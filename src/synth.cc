#include <sstream>
#include <fstream>
#include <string>
#include <cmath>
#include <QDebug>
#include <QElapsedTimer>
#include "synth.hh"

#ifndef M_PI
	#define M_PI 3.141592653589793
#endif


void Synth::tick(qint64 pos, qreal playbackRate, const SynthNotes& notes) {
	QMutexLocker locker(&m_mutex);
	m_pos = pos / 1000.0;
	m_rate = playbackRate;
	m_notes = notes;
	if (m_notes.isEmpty()) m_quit = true;

	if (isRunning()) m_condition.wakeOne();
	else start();
}

void Synth::stop() {
	QMutexLocker locker(&m_mutex);
	m_quit = true;
	m_condition.wakeOne();
}

void Synth::createBuffer(QByteArray &buffer, int note, double length) {
	// This is simple beep, so we use mono and lowish sample rate
	// --> quick to create and small memory footprint
	// Going to 8 bits seems to create weird samples on Windows though
	//std::string header = writeWavHeader(16, 1, SampleRate, length * SampleRate);
	//buffer = QByteArray(header.c_str(), header.size());
	const int bytesPerSample = 2;
	const quint64 samples = length * SampleRate;
	buffer.clear();
	buffer.reserve(samples * bytesPerSample);
	double d = (note + 1) / 13.0;
	double freq = MusicalScale().getNoteFreq(note + 12);
	double phase = 0;
	// Synthesize tones
	for (size_t i = 0; i < samples; ++i) {
		float fvalue = d * 0.2 * std::sin(phase) + 0.2 * std::sin(2 * phase) + (1.0 - d) * 0.2 * std::sin(4 * phase);
		phase += 2.0 * M_PI * freq / SampleRate;

		// Convert float to 16-bit integer and push to buffer
		qint16 svalue = fvalue * 32768;
		char* value = reinterpret_cast<char*>(&svalue);
		buffer.push_back(value[0]);
		buffer.push_back(value[1]);
	}

	//std::ofstream of("/tmp/wavdump.wav");
	//of.write(buf.data(), buf.size());
}

void Synth::run() {
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

void Synth::calcNext() {
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
		createBuffer(m_soundData[m_curBuffer], n.note % 12, n.length / m_rate);
	}
	// Compensate for the time spent in this function
	m_delay -= timer.elapsed() / 1000.0;
	if (m_delay <= 0.001) m_delay = 0.001;
}


std::string Synth::writeWavHeader(unsigned bits, unsigned ch, unsigned sr, unsigned samples) {
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



// BufferPlayer

BufferPlayer::BufferPlayer(QObject *parent)
: QObject(parent)
{
	m_buffer = new QBuffer(this);

	QAudioFormat format;
	format.setChannelCount(1);
	format.setSampleRate(Synth::SampleRate);
	format.setSampleSize(16);
	format.setSampleType(QAudioFormat::SignedInt);
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setCodec("audio/pcm");

	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	if (!info.isFormatSupported(format)) {
		qWarning() << "Synth output format not supported, trying nearest";
		format = info.nearestFormat(format);
	}
	m_player = new QAudioOutput(format, this);
	m_player->setVolume(1.0f);
	//m_player->setNotifyInterval(100);
	//connect(m_player, SIGNAL(notify()), this, SLOT(debugDumpStats()));
	connect(m_player, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
}

bool BufferPlayer::play(const QByteArray& ba)
{
	if (m_player->state() != QAudio::ActiveState) {
		m_player->stop();
		m_buffer->close();
		m_buffer->setData(ba);
		m_buffer->open(QIODevice::ReadOnly);
		m_player->setBufferSize(ba.size());
		m_player->start(m_buffer);
		return true;
	}
	return false;
}

void BufferPlayer::handleStateChanged(QAudio::State newState)
{
	//qDebug() << "Synth" << newState;
	switch (newState) {
	case QAudio::IdleState: // Finished playing (no more data)
		m_player->stop();
		break;
	case QAudio::StoppedState: // Stopped for other reasons
		if (m_player->error() != QAudio::NoError) {
			qWarning() << "Synth audio error code " << m_player->error();
		}
		break;
	default:
		break;
	}
}

void BufferPlayer::debugDumpStats()
{
	qWarning() << "bytesFree =" << m_player->bytesFree()
		<< "- elapsedUSecs =" << m_player->elapsedUSecs()
		<< "- processedUSecs =" << m_player->processedUSecs();
}
