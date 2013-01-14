#pragma once
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cmath>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QBuffer>
#include <QFile>
#include <phonon/MediaObject>
#include "notes.hh"
#include "notegraphwidget.hh"
#include "notelabel.hh"


#ifndef M_PI
	#define M_PI 3.141592653589793
#endif

struct SynthNote {
	SynthNote(): note(24), begin(), length() {}
	SynthNote(const Note& n): note(n.note), begin(n.begin), length(n.length()) {}
	bool operator<(const SynthNote& rhs) { return begin < rhs.begin; }
	int note;
	double begin;
	double length;
};

typedef QList<SynthNote> SynthNotes;

/**
 * @brief Threaded WAV buffer creator.
 *
 * Synthesizes and schedules notes in a thread and sends them to the main thread when its time to play them.
 */
class Synth: public QThread
{
	Q_OBJECT
public:
	Synth(QObject *parent = NULL) : QThread(parent), m_delay(), m_pos(), m_noteBegin(), m_curBuffer(), m_quit()
	{
		qRegisterMetaType<QByteArray>("QByteArray"); // Register type for use with queued connections
	}

	~Synth() { stop(); wait(); }

	/// Updates the synth
	void tick(qint64 pos, const SynthNotes& notes) {
		QMutexLocker locker(&m_mutex);
		m_pos = pos / 1000.0;
		m_notes = notes;
		if (m_notes.isEmpty()) m_quit = true;

		if (isRunning()) m_condition.wakeOne();
		else start();
	}

	void stop() {
		QMutexLocker locker(&m_mutex);
		m_quit = true;
		m_condition.wakeOne();
	}

	/// Creates the sound
	static void createBuffer(QByteArray &buffer, int note, double length) {
		// This is simple beep, so we use mono and lowish sample rate
		// --> quick to create and small memory footprint
		// Going to 8 bits seems to create weird samples on Windows though
		std::string header = writeWavHeader(16, 1, sampleRate, length * sampleRate);
		buffer = QByteArray(header.c_str(), header.size());
		double d = (note + 1) / 13.0;
		double freq = MusicalScale().getNoteFreq(note + 12);
		double phase = 0;
		// Synthesize tones
		for (size_t i = 0; i < length * sampleRate; ++i) {
			float fvalue = d * 0.2 * std::sin(phase) + 0.2 * std::sin(2 * phase) + (1.0 - d) * 0.2 * std::sin(4 * phase);
			phase += 2.0 * M_PI * freq / sampleRate;

			// Convert float to 16-bit integer and push to buffer
			qint16 svalue = fvalue * 32768;
			char* value = reinterpret_cast<char*>(&svalue);
			buffer.push_back(value[0]);
			buffer.push_back(value[1]);
		}

		//std::ofstream of("/tmp/wavdump.wav");
		//of.write(buf.data(), buf.size());
	}

signals:
	void playBuffer(const QByteArray&);

protected:
	/// Thread runs here
	void run() {
		calcNext();
		while (!m_quit) {
			m_mutex.lock();
			// Wait here until wake up or time out
			if (m_condition.wait(&m_mutex, m_delay * 1000)) {
				// We were woken up, so let's see if an update is in order
				m_mutex.unlock();
				if (m_quit) break;
				calcNext();

			} else {
				// Time-out: time to play the music
				m_mutex.unlock();
				if (m_quit) break;
				emit playBuffer(m_soundData[m_curBuffer]);
				m_curBuffer = (m_curBuffer+1) % 2;
				// Slightly hacky stuff follows:
				// We advance the time a bit to make sure we are over the note beginning.
				// Then cache the next note, but put longer delay (which will be corrected
				// with the next tick) so that we don't accidentally play wrong note(
				// in case we advanced the time too much).
				m_pos += 0.2;
				calcNext();
				m_delay = std::max(m_delay, 1.0);
			}
		}
	}

private:
	/// Calculates the next values
	void calcNext() {
		QElapsedTimer timer; timer.start();
		SynthNote n;
		{
			QMutexLocker locker(&m_mutex);
			SynthNotes::const_iterator it = m_notes.begin();
			while (it != m_notes.end() && it->begin < m_pos) ++it;
			if (it == m_notes.end()) { m_delay = 1000.0; return; }
			n = *it;
		}

		m_delay = n.begin - m_pos;
		if (n.begin != m_noteBegin) {
			// Need to create a new buffer
			m_noteBegin = n.begin;
			createBuffer(m_soundData[m_curBuffer], n.note % 12, n.length);
		}
		// Compensate for the time spent in this function
		m_delay -= timer.elapsed() / 1000.0;
		if (m_delay <= 0.001) m_delay = 0.001;
	}

	/// WAV header writer
	static std::string writeWavHeader(unsigned bits, unsigned ch, unsigned sr, unsigned samples) {
		std::ostringstream out;
		unsigned bps = ch * bits / 8; // Bytes per sample
		unsigned datasize = bps * samples;
		unsigned size = datasize + 0x2C;
		out.write("RIFF" ,4); // RIFF chunk
		{ unsigned int tmp=size-0x8 ; out.write((char*)(&tmp),4); } // RIFF chunk size
		out.write("WAVEfmt ",8); // WAVEfmt header
		{ int   tmp=0x00000010 ; out.write((char*)(&tmp),4); } // Always 0x10
		{ short tmp=0x0001     ; out.write((char*)(&tmp),2); } // Always 1
		{ short tmp = ch; out.write((char*)(&tmp),2); } // Number of channels
		{ int   tmp = sr; out.write((char*)(&tmp),4); } // Sample rate
		{ int   tmp = bps * sr; out.write((char*)(&tmp),4); } // Bytes per second
		{ short tmp = bps; out.write((char*)(&tmp),2); } // Bytes per frame
		{ short tmp = bits; out.write((char*)(&tmp),2); } // Bits per sample
		out.write("data",4); // data chunk
		{ int   tmp = datasize; out.write((char*)(&tmp),4); }
		return out.str();
	}

	static const int sampleRate = 22050; ///< Sample rate

	SynthNotes m_notes; ///< Notes to synthesize
	double m_delay; ///< How many seconds until the next sound must be played
	double m_pos; ///< Position where we are now
	double m_noteBegin; ///< Position of the next note
	QByteArray m_soundData[2]; ///< The WAV buffers
	int m_curBuffer; ///< Which buffer we are currently using
	bool m_quit; ///< Flag to signal the thread should quit
	QMutex m_mutex; ///< Mutex for protecting resource access
	QWaitCondition m_condition; ///< For signaling the thread
};


/**
 * @brief Class for playing a WAV buffer from memory.
 *
 * Designed to be reused, but won't play the buffer if the previous hasn't finished.
 */
class BufferPlayer: public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(BufferPlayer);
public:
	BufferPlayer(QObject *parent): QObject(parent) {
		m_player = Phonon::createPlayer(Phonon::MusicCategory);
		m_player->setParent(this);
		m_buffer = new QBuffer(this);
		connect(m_player, SIGNAL(finished()), this, SLOT(finished()));
	}

	bool play(const QByteArray& ba) {
		if (m_player->state() != Phonon::PlayingState) {
			m_player->clear();
			m_buffer->close();
			m_buffer->setData(ba);
			m_player->setCurrentSource(m_buffer);
			m_player->play();
			return true;
		}
		return false;
	}

public slots:
	void finished() {
		m_player->clear();
	}

private:
	QBuffer *m_buffer;
	Phonon::MediaObject *m_player;
};
