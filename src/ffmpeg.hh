#pragma once

#include "util.hh"
#include "libda/sample.hpp"
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QScopedPointer>
#include <memory>
#include <vector>

class AudioQueue {
public:
	void reset() {
		QMutexLocker lock(&m_mutex);
		m_size = 0;
		m_needData.wakeOne();
		m_needSpace.wakeOne();
	}
	template <typename Iterator> void input(Iterator begin, Iterator end, double scale) {
		QMutexLocker lock(&m_mutex);
		unsigned count = end - begin;
		unsigned capacity = m_ring.size();
		if (capacity < count) throw std::logic_error("AudioQueue input chunk is bigger than capacity");
		while (capacity - m_size < count) m_needSpace.wait(&m_mutex);
		for (unsigned i = 0; i < count; ++i) {
			m_ring[m_position + m_size++] = *begin++ * scale;
		}
		m_needData.wakeOne();
	}
	void setEof(bool eof = true) {
		QMutexLocker lock(&m_mutex);
		m_eof = eof;
		m_needData.wakeOne();
	}
	bool output(std::vector<da::sample_t>& out) {
		QMutexLocker lock(&m_mutex);
		while (m_size == 0) {
			if (m_eof) return false;
			m_needData.wait(&m_mutex);
		}
		std::size_t outsz = out.size();
		out.resize(outsz + m_size);
		da::sample_t* outptr = &out[outsz];
		Ring::iterator b = m_ring.begin() + m_position,
		  e = m_ring.begin() + (m_position + m_size) % m_ring.size();
		if (e <= b) {
			// Copy the first part
			std::copy(b, m_ring.end(), outptr);
			outptr += m_ring.end() - b;
			b = m_ring.begin();
		}
		// Copy the only/last part
		std::copy(b, e, outptr);
		m_size = 0;
		m_needSpace.wakeOne();
		return true;
	}
	unsigned samplesPerSecond() const { return m_channels * m_rate; }
	void setRateChannels(unsigned rate, unsigned channels) { m_rate = rate; m_channels = channels; }
	unsigned getRate() { return m_rate; }
	unsigned getChannels() { return m_channels; }
	AudioQueue(unsigned capacity = 32768): m_ring(capacity), m_channels(), m_position(), m_size(), m_eof() {}
	
private:
	QMutex m_mutex;
	QWaitCondition m_needData, m_needSpace;
	typedef std::vector<da::sample_t> Ring;
	Ring m_ring;
	unsigned m_rate;
	unsigned m_channels;
	unsigned m_position;
	unsigned m_size;
	bool m_eof;
};

// ffmpeg forward declarations
// ffmpeg forward declarations
extern "C" {
  struct AVCodec;
  struct AVCodecContext;
  struct AVFormatContext;
  struct AVFrame;
  struct SwrContext;
  struct SwsContext;
}

/// ffmpeg class
class FFmpeg: public QThread {
  public:
	/// constructor
	FFmpeg(std::string const& file);
	~FFmpeg();
	/// Thread runs here, don't call directly
	void run();
	/// Queue for audio
	AudioQueue audioQueue;
	/** Seek to the chosen time. Will block until the seek is done, if wait is true. **/
	void seek(double time, bool wait = true);
	/// Duration
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
	SwrContext* m_resampleContext;
	AVCodecContext* pAudioCodecCtx;
	AVCodec* pAudioCodec;

	int audioStream;
	double m_position;
	static QMutex s_avcodec_mutex; // Used for avcodec_open/close (which use some static crap and are thus not thread-safe)
};

