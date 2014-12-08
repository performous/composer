#include "ffmpeg.hh"
#include "config.hh"
#include "util.hh"
#include <iostream>
#include <stdexcept>
#include <QtGlobal>

// Somehow ffmpeg headers give errors that these are not defined...
#define INT64_C Q_INT64_C
#define UINT64_C Q_UINT64_C
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
extern "C" {
#include AVCODEC_INCLUDE
#include AVFORMAT_INCLUDE
#include SWSCALE_INCLUDE
#include AVRESAMPLE_INCLUDE
#include AVUTIL_INCLUDE
#include AVUTIL_OPT_INCLUDE
#include AVUTIL_MATH_INCLUDE
}

/// A custom allocator that uses av_malloc for aligned buffers
template <typename T> class AvMalloc: public std::allocator<T> {
public:
/*	pointer allocate(size_type count, allocator<void>::const_pointer* = 0) {
		pointer ptr = static_cast<pointer>(m(count * sizeof(T)));
		if (!ptr) throw std::bad_alloc();
		return ptr;
	}
	void deallocate(pointer ptr, size_type) { f(ptr); }*/
private:
	void* m(size_t n) { return av_malloc(n); }
	void f(void* ptr) { av_free(ptr); }
};


/*static*/ QMutex FFmpeg::s_avcodec_mutex;

FFmpeg::FFmpeg(std::string const& _filename):
  m_filename(_filename), m_quit(), m_running(), m_eof(),
  pFormatCtx(), pAudioCodecCtx(), pAudioCodec(), m_rate(48000),
  audioStream(-1), m_position()
{
	open(); // Throws on error
	m_running = true;
	start();
}

FFmpeg::~FFmpeg() {
	m_quit = true;
	audioQueue.setEof();
	audioQueue.reset();
	wait();
	// TODO: use RAII for freeing resources (to prevent memory leaks)
	QMutexLocker l(&s_avcodec_mutex); // avcodec_close is not thread-safe
	if (pAudioCodecCtx) avcodec_close(pAudioCodecCtx);
	if (pFormatCtx) avformat_close_input(&pFormatCtx);
	}

double FFmpeg::duration() const {
	double d = m_running ? pFormatCtx->duration / double(AV_TIME_BASE) : getNaN();
	return d >= 0.0 ? d : getInf();
}

// FFMPEG has fluctuating API
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(64<<8)+0)
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

void FFmpeg::open() {
	QMutexLocker l(&s_avcodec_mutex);
	av_register_all();
	av_log_set_level(AV_LOG_ERROR);
	if (avformat_open_input(&pFormatCtx, m_filename.c_str(), NULL, NULL)) throw std::runtime_error("Cannot open input file");
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) throw std::runtime_error("Cannot find stream information");
	pFormatCtx->flags |= AVFMT_FLAG_GENPTS;
	audioStream = -1;
	// Take the first video/audio streams
	for (unsigned int i=0; i<pFormatCtx->nb_streams; i++) {
		AVCodecContext* cc = pFormatCtx->streams[i]->codec;
		cc->workaround_bugs = FF_BUG_AUTODETECT;
		if (audioStream == -1 && cc->codec_type==AVMEDIA_TYPE_AUDIO) audioStream = i;
	}
	if (audioStream == -1) throw std::runtime_error("No audio stream found");
	AVCodecContext* cc = pFormatCtx->streams[audioStream]->codec;
	pAudioCodec = avcodec_find_decoder(cc->codec_id);
	audioQueue.setRateChannels(cc->sample_rate, cc->channels);
	if (!pAudioCodec) throw std::runtime_error("Cannot find audio codec");
	if (avcodec_open2(cc, pAudioCodec, NULL) < 0) throw std::runtime_error("Cannot open audio codec");
	pAudioCodecCtx = cc;
	m_resampleContext = avresample_alloc_context();
	av_opt_set_int(m_resampleContext, "in_channel_layout", pAudioCodecCtx->channel_layout ? pAudioCodecCtx->channel_layout : av_get_default_channel_layout(pAudioCodecCtx->channels), 0);
	av_opt_set_int(m_resampleContext, "out_channel_layout", av_get_default_channel_layout(2), 0);
	av_opt_set_int(m_resampleContext, "in_sample_rate", pAudioCodecCtx->sample_rate, 0);
	av_opt_set_int(m_resampleContext, "out_sample_rate", m_rate, 0);
	av_opt_set_int(m_resampleContext, "in_sample_fmt", pAudioCodecCtx->sample_fmt, 0);
	av_opt_set_int(m_resampleContext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	avresample_open(m_resampleContext);
	if (!m_resampleContext) throw std::runtime_error("Cannot create resampling context");

}

void FFmpeg::run() {
	int errors = 0;
	while (!m_quit) {
		try {
			if (m_seekTarget == m_seekTarget) seek_internal();
			decodeNextFrame();
			m_eof = false;
			errors = 0;
		} catch (eof_error&) {
			audioQueue.setEof();
			m_eof = true;
			msleep(100);
		} catch (std::exception& e) {
			std::cerr << "FFMPEG error: " << e.what() << std::endl;
			if (++errors > 2) { std::cerr << "FFMPEG terminating due to errors" << std::endl; m_quit = true; }
		}
	}
	audioQueue.setEof();
	m_running = false;
	m_eof = true;
}

void FFmpeg::seek(double time, bool wait) {
	m_seekTarget = time;
	audioQueue.reset(); // Empty these to unblock the internals in case buffers were full
	if (wait) while (!m_quit && m_seekTarget == m_seekTarget) msleep(10);
}

void FFmpeg::seek_internal() {
	audioQueue.reset();
	int flags = 0;
	// FIXME: if (m_seekTarget < position()) flags |= AVSEEK_FLAG_BACKWARD;
	int stream = -1;
	stream = audioStream;
	int64_t target = m_seekTarget * AV_TIME_BASE;
	// FIXME: av_rescale_q not declared on recent ffmpeg
	// Latest Performous code does not have these lines
	//const AVRational time_base_q = { 1, AV_TIME_BASE };  // AV_TIME_BASE_Q is the same thing with C99 struct literal (not supported by MSVC)
	//if (stream != -1) target = av_rescale_q(target, time_base_q, pFormatCtx->streams[stream]->time_base);
	av_seek_frame(pFormatCtx, stream, target, flags);
	m_seekTarget = getNaN(); // Signal that seeking is done
}

void FFmpeg::decodeNextFrame() {
	struct ReadFramePacket: public AVPacket {
		AVFormatContext* m_s;
		ReadFramePacket(AVFormatContext* s): m_s(s) {
			if (av_read_frame(s, this) < 0) throw eof_error();
		}
		~ReadFramePacket() { av_free_packet(this); }
		double time() {
			return uint64_t(dts) == uint64_t(AV_NOPTS_VALUE) ?
			  getNaN() : double(dts) * av_q2d(m_s->streams[stream_index]->time_base);
		}
	};

	int frameFinished=0;
	while (!frameFinished) {
		ReadFramePacket packet(pFormatCtx);
		unsigned packetPos = 0;
		if (packet.stream_index==audioStream) {
			while (packetPos < packet.size) {
				if (m_quit || m_seekTarget == m_seekTarget) return;
				int bytesUsed;
				#if (LIBAVCODEC_VERSION_INT) > (AV_VERSION_INT(55,0,0))
				AVFrame* m_frame = av_frame_alloc();
				#else
				AVFrame* m_frame = avcodec_alloc_frame();
				#endif
				int gotFramePointer = 0;
				bytesUsed = avcodec_decode_audio4(pAudioCodecCtx,m_frame,&gotFramePointer, &packet);
				if (bytesUsed < 0) throw std::runtime_error("cannot decode audio frame");
				packetPos += bytesUsed;
				// Update position if timecode is available
				if (packet.time() == packet.time()) m_position = packet.time();
				//resample here!
				int16_t * output;
				int out_linesize;
				int out_samples = avresample_available(m_resampleContext) +
					av_rescale_rnd(avresample_get_delay(m_resampleContext) +
					m_frame->nb_samples, m_frame->sample_rate, m_rate, AV_ROUND_UP);
				av_samples_alloc((uint8_t**)&output, &out_linesize, 2, out_samples,AV_SAMPLE_FMT_S16, 0);
				out_samples = avresample_convert(m_resampleContext, (uint8_t**)&output, 0, out_samples, &m_frame->data[0], 0, m_frame->nb_samples);
				std::vector<int16_t> m_output(output, output+out_samples*2);
				// Output samples
				int outsize = m_output.size(); /* Convert bytes into samples */ \
				short* samples = reinterpret_cast<short*>(&m_output[0]);\
				audioQueue.input(samples, samples + outsize, 1.0 / 32767.0);\
				m_position += double(out_samples)/m_rate;  // New position in case the next packet doesn't have packet.time()
			}
			// Audio frames are always finished
			frameFinished = 1;
		}
	}
}

