#pragma once
#include <string>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QBuffer>
#include <QFile>
#include <QAudioOutput>
#include <QAudioFormat>
#include "notes.hh"


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
	static const int SampleRate = 22050; ///< Sample rate

	Synth(QObject *parent = NULL) : QThread(parent), m_delay(), m_pos(), m_noteBegin(), m_curBuffer(), m_quit()
	{
		qRegisterMetaType<QByteArray>("QByteArray"); // Register type for use with queued connections
	}
	~Synth() { stop(); wait(); }

	/// Updates the synth
	void tick(qint64 pos, const SynthNotes& notes);
	/// Stop synthesizing
	void stop();
	/// Creates the sound
	static void createBuffer(QByteArray &buffer, int note, double length);

signals:
	void playBuffer(const QByteArray&);

protected:
	/// Thread runs here
	void run();

private:
	/// Calculates the next values
	void calcNext();
	/// WAV header writer
	static std::string writeWavHeader(unsigned bits, unsigned ch, unsigned sr, unsigned samples);

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
	Q_DISABLE_COPY(BufferPlayer)
public:
	BufferPlayer(QObject *parent);

	bool play(const QByteArray& ba);

public slots:
	void handleStateChanged(QAudio::State newState);

private:
	QBuffer *m_buffer;
	QAudioOutput *m_player;
};
