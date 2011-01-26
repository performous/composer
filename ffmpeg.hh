#pragma once

#include "util.hh"
#include "libda/sample.hpp"
#include <boost/circular_buffer.hpp>
#include <boost/ptr_container/ptr_set.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <vector>

using boost::uint8_t;
using boost::int16_t;
using boost::int64_t;

/// single audio frame
struct AudioFrame {
	/// timestamp of audio frame
	double timestamp;
	/// audio data
	std::vector<int16_t> data;
	/// constructor
	template <typename InIt> AudioFrame(double ts, InIt begin, InIt end): timestamp(ts), data(begin, end) {}
	AudioFrame(): timestamp(getInf()) {} // EOF marker
};

class AudioBuffer {
  public:
	AudioBuffer(size_t size = 2000000): m_data(size), m_pos(), m_posReq(), m_sps(), m_duration(getNaN()), m_quit() {}
	/// Reset from FFMPEG side (seeking to beginning or terminate stream)
	void reset() {
		boost::mutex::scoped_lock l(m_mutex);
		m_data.clear();
		m_pos = 0;
		l.unlock();
		m_cond.notify_one();
	}
	void quit() {
		boost::mutex::scoped_lock l(m_mutex);
		m_quit = true;
		l.unlock();
		m_cond.notify_one();
	}
	/// set samples per second
	void setSamplesPerSecond(unsigned sps) { m_sps = sps; }
	/// get samples per second
	unsigned getSamplesPerSecond() { return m_sps; }
	void push(std::vector<int16_t> const& data, double timestamp) {
		boost::mutex::scoped_lock l(m_mutex);
		while (!condition()) m_cond.wait(l);
		if (m_quit) return;
		if (m_pos == 0 && timestamp != 0.0) {
			//std::cerr << "Warning: The first audio frame begins at " << timestamp << " seconds instead of zero, compensating." << std::endl;
			m_pos = timestamp * m_sps;
		}
		m_data.insert(m_data.end(), data.begin(), data.end());
		m_pos += data.size();
	}
	bool prepare(int64_t pos) {
		boost::mutex::scoped_try_lock l(m_mutex);
		if (!l.owns_lock()) return false;
		if (pos < 0) pos = 0;
		m_posReq = pos;
		return m_pos > m_posReq + m_data.capacity() / 4 && m_pos - m_data.size() <= m_pos;
	}
	bool operator()(float* begin, float* end, int64_t pos, float volume = 1.0f) {
		boost::mutex::scoped_lock l(m_mutex);
		size_t idx = pos + m_data.size() - m_pos;
		size_t samples = end - begin;
		for (size_t s = 0; s < samples; ++s, ++idx) {
			if (idx < m_data.size()) begin[s] += volume * da::conv_from_s16(m_data[idx]);
		}
		m_posReq = pos + samples;
		l.unlock();
		if (wantSeek()) reset();
		if (condition()) m_cond.notify_one();
		return !eof(pos);
	}
	bool eof(int64_t pos) const { return double(pos) / m_sps >= m_duration; }
	double duration() const { return m_duration; }
	void setDuration(double seconds) { m_duration = seconds; }
	bool wantSeek() {
		size_t oldest = m_pos - m_data.size();
		return oldest > 0 && m_posReq < int64_t(oldest);
	}
  private:
	bool wantMore() { return int64_t(m_pos) - int64_t(m_data.capacity() / 2) < m_posReq; }
	bool condition() { return m_quit || wantMore() || wantSeek(); }
	mutable boost::mutex m_mutex;
	boost::condition m_cond;
	boost::circular_buffer<int16_t> m_data;
	size_t m_pos;
	int64_t m_posReq;
	unsigned m_sps;
	double m_duration;
	bool m_quit;
};

// ffmpeg forward declarations
extern "C" {
  struct AVCodec;
  struct AVCodecContext;
  struct AVFormatContext;
  struct ReSampleContext;
  struct SwsContext;
}

/// ffmpeg class
class FFmpeg {
  public:
	/// constructor
	FFmpeg(std::string const& file, unsigned int rate = 48000);
	~FFmpeg();
	/**
	* This function is called by the crash handler to indicate that FFMPEG has
	* crashed or has gotten stuck, and that the destructor should not wait for
	* it to finish before exiting.
	**/
	void crash() { m_thread.reset(); m_quit = true; }
	void operator()(); ///< Thread runs here, don't call directly
	/// queue for audio
	AudioBuffer  audioQueue;
	/** Seek to the chosen time. Will block until the seek is done, if wait is true. **/
	void seek(double time, bool wait = true);
	/// duration
	double duration() const;
	bool terminating() const { return m_quit; }

  private:
	class eof_error: public std::exception {};
	void seek_internal();
	void open();
	void decodeNextFrame();
	std::string m_filename;
	unsigned int m_rate;
	volatile bool m_quit;
	volatile bool m_running;
	volatile bool m_eof;
	volatile double m_seekTarget;
	AVFormatContext* pFormatCtx;
	ReSampleContext* pResampleCtx;
	SwsContext* img_convert_ctx;

	AVCodecContext* pAudioCodecCtx;
	AVCodec* pAudioCodec;

	int audioStream;
	double m_position;
	boost::scoped_ptr<boost::thread> m_thread;
	static boost::mutex s_avcodec_mutex; // Used for avcodec_open/close (which use some static crap and are thus not thread-safe)
};

