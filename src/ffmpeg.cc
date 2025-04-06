#include "ffmpeg.hh"
#include "config.hh"
#include "util.hh"
#include <iostream>
#include <stdexcept>
#include <QtGlobal>

// Somehow ffmpeg headers give errors that these are not defined...
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
extern "C" {
	#include AVCODEC_INCLUDE
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58,87,100) // FFmpeg 4.3
	#include AVCODEC_CODEC_PAR_INCLUDE
#endif
	#include AVFORMAT_INCLUDE
	#include SWSCALE_INCLUDE
	#include SWRESAMPLE_INCLUDE
	#include AVUTIL_INCLUDE
	#include AVUTIL_OPT_INCLUDE
	#include AVUTIL_MATH_INCLUDE
	#include AVUTIL_ERROR_INCLUDE
}

/*static*/ QMutex FFmpeg::s_avcodec_mutex;

void AVFormatContextDeleter::operator ()(AVFormatContext* context) {
	avformat_close_input(&context);
}

void AVCodecContextDeleter::operator ()(AVCodecContext* context) {
	avcodec_free_context(&context);
}

void SwrContextDeleter::operator ()(SwrContext* context) {
	swr_free(&context);
}

static std::string stringFromErrorCode(int code) {
	char buffer[AV_ERROR_MAX_STRING_SIZE];
	av_make_error_string(buffer, sizeof(buffer), code);
	return buffer;
}

FFmpeg::FFmpeg(std::string const& _filename):
  m_filename(_filename), m_quit(), m_running(), m_eof(),
  pFormatCtx(), pAudioCodecCtx(), m_rate(48000),
  audioStream(-1)
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
	}

double FFmpeg::duration() const {
	double d = m_running ? pFormatCtx->duration / double(AV_TIME_BASE) : getNaN();
	return d >= 0.0 ? d : getInf();
}

void FFmpeg::open() {
	QMutexLocker l(&s_avcodec_mutex);
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58,9,100) // Got deprecated in 4.0 but needed for earlier versions
	av_register_all();
#endif
	av_log_set_level(AV_LOG_ERROR);

	AVFormatContext *ps{};

	if (auto err = avformat_open_input(&ps, m_filename.c_str(), NULL, NULL); err != 0)
		throw std::runtime_error(std::string("Cannot open input file. Error: ") + std::to_string(err) + " " + stringFromErrorCode(err));

	pFormatCtx = {ps, {}};

	if (avformat_find_stream_info(pFormatCtx.get(), NULL) < 0) throw std::runtime_error("Cannot find stream information");
	pFormatCtx->flags |= AVFMT_FLAG_GENPTS;
	audioStream = -1;

	// Take the first video/audio streams
	for (unsigned int i=0; i<pFormatCtx->nb_streams; i++) {
		auto * codecpar = pFormatCtx->streams[i]->codecpar;

		if (codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
		{
			audioStream = i;
			break;
		}
	}
	if (audioStream == -1) throw std::runtime_error("No audio stream found");

	auto* codecpar = pFormatCtx->streams[audioStream]->codecpar;
	const auto* pAudioCodec = avcodec_find_decoder(codecpar->codec_id);
	audioQueue.setRateChannels(m_rate, 2);
	if (!pAudioCodec) throw std::runtime_error("Cannot find audio codec");

	pAudioCodecCtx = {avcodec_alloc_context3(pAudioCodec), {}};
	avcodec_parameters_to_context(pAudioCodecCtx.get(), codecpar);
	pAudioCodecCtx->workaround_bugs = 1;

	if (avcodec_open2(pAudioCodecCtx.get(), pAudioCodec, NULL) < 0) throw std::runtime_error("Cannot open audio codec");
	m_resampleContext = {swr_alloc(), {}};

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100) // FFmpeg 5.1 and up
	AVChannelLayout in_chlayout{};
	if (av_channel_layout_copy(&in_chlayout, &pAudioCodecCtx->ch_layout) < 0)
	{
		av_channel_layout_default(&in_chlayout, pAudioCodecCtx->ch_layout.nb_channels);
	};
	AVChannelLayout out_chlayout{};
	av_channel_layout_default(&out_chlayout, 2);
	// av_get_default_channel_layout(2)
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100) // FFmpeg 5.1 and up
	av_opt_set_chlayout(m_resampleContext.get(), "in_chlayout", &in_chlayout, 0);
	av_opt_set_chlayout(m_resampleContext.get(), "out_chlayout", &out_chlayout, 0);
#else // FFmpeg 5.0 and earlier
	av_opt_set_int(m_resampleContext.get(), "in_channel_layout", pAudioCodecCtx->channel_layout ? pAudioCodecCtx->channel_layout : av_get_default_channel_layout(pAudioCodecCtx->channels), 0);
	av_opt_set_int(m_resampleContext.get(), "out_channel_layout", av_get_default_channel_layout(2), 0);
#endif
	av_opt_set_int(m_resampleContext.get(), "in_sample_rate", pAudioCodecCtx->sample_rate, 0);
	av_opt_set_int(m_resampleContext.get(), "out_sample_rate", m_rate, 0);
	av_opt_set_int(m_resampleContext.get(), "in_sample_fmt", pAudioCodecCtx->sample_fmt, 0);
	av_opt_set_int(m_resampleContext.get(), "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	swr_init(m_resampleContext.get());
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
	av_seek_frame(pFormatCtx.get(), stream, target, flags);
	m_seekTarget = getNaN(); // Signal that seeking is done
}

void FFmpeg::decodeNextFrame() {
	struct ReadFramePacket: public AVPacket {
		AVFormatContext* m_s;
		ReadFramePacket(AVFormatContext* s): m_s(s) {
			if (av_read_frame(s, this) < 0) throw eof_error();
		}
        ~ReadFramePacket() { av_packet_unref(this); }
		double time() {
			return uint64_t(dts) == uint64_t(AV_NOPTS_VALUE) ?
			  getNaN() : double(dts) * av_q2d(m_s->streams[stream_index]->time_base);
		}
	};

	struct ReadFrame
	{
		AVFrame* m_frame;

		ReadFrame(): m_frame(av_frame_alloc()) {
			if (m_frame == nullptr) throw eof_error();
		}
        ~ReadFrame() { av_frame_free(&m_frame); }

		operator AVFrame*() {return m_frame;}
		AVFrame operator*() {return *m_frame;}
		AVFrame* operator->() {return m_frame;}
	};

	bool frameFinished{};
	while (!frameFinished) {
		ReadFramePacket packet(pFormatCtx.get());
		if (packet.stream_index==audioStream) {
			auto err = avcodec_send_packet(pAudioCodecCtx.get(), &packet);
			if (err == AVERROR_EOF) break; // Nothing we can do
			if (err != 0 && err != AVERROR(EAGAIN)) throw std::runtime_error(std::string("Can't send packet. Error: ") + std::to_string(err) + " " + stringFromErrorCode(err));

			do
			{
				if (m_quit || m_seekTarget == m_seekTarget) return;
				ReadFrame m_frame{};

				err = avcodec_receive_frame(pAudioCodecCtx.get(), m_frame);
				if (err == 0);
				else if (err == AVERROR(EAGAIN)) break;
				else if (err == AVERROR_EOF) return; // Finished file
				else throw std::runtime_error(std::string("cannot decode audio frame. Error: ") + std::to_string(err) + " " + stringFromErrorCode(err));

				//resample here!
				int16_t * output;
				int out_linesize;
				int out_samples = swr_get_out_samples(m_resampleContext.get(), m_frame->nb_samples);
				av_samples_alloc((uint8_t**)&output, &out_linesize, 2, out_samples,AV_SAMPLE_FMT_S16, 0);
				out_samples = swr_convert(m_resampleContext.get(), (uint8_t**)&output, out_samples, (const uint8_t**)&m_frame->data[0], m_frame->nb_samples);
				std::vector<int16_t> m_output(output, output+out_samples*2);
				// Output samples
				int outsize = m_output.size(); /* Convert bytes into samples */ \
				short* samples = reinterpret_cast<short*>(&m_output[0]);\
				audioQueue.input(samples, samples + outsize, 1.0 / 32767.0);\
			}
			while (true);

			// Audio frames are always finished
			frameFinished = true;
		}
	}
}

