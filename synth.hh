#pragma once
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cmath>
#include <QBuffer>
#include <QFile>
#include <phonon/MediaObject>
#include "notes.hh"
#include "notegraphwidget.hh"
#include "notelabel.hh"


#ifndef M_PI
	#define M_PI 3.141592653589793
#endif


inline std::string writeWavHeader(unsigned bits, unsigned ch, unsigned sr, unsigned samples) {
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

class Synth: public QObject
{
	Q_OBJECT
public:
	Synth(NoteLabels& notes) : m_notes(notes), m_phase(0), m_player(Phonon::createPlayer(Phonon::MusicCategory)), m_curBuffer()
	{
		m_player->setParent(this);
	}

	//~Synth() { stop(); }

	/// Starts/updates the synth
	void start(qint64 pos) {
		calcNext(pos / 1000.0);
	}

	/// Stops the synth
	void stop(bool killPlayback = true) {
		killTimer(m_timerId);
		if (killPlayback) {
			m_player->clear();
			m_soundData[0].close();
			m_soundData[1].close();
		}
		m_phase = 0;
	}

protected:
	/// Plays next sound
	void timerEvent(QTimerEvent*) {
		killTimer(m_timerId);
		m_player->clear();
		m_soundData[m_curBuffer].close();
		m_soundData[m_curBuffer].setData(createBuffer());
		m_player->setCurrentSource(&(m_soundData[m_curBuffer]));
		m_player->play();
		m_curBuffer = (m_curBuffer+1) % 2;
		calcNext(m_pos);
	}

private:
	/// Calculates the next values
	void calcNext(double pos) {
		NoteLabels::const_iterator it = m_notes.begin();
		while (it != m_notes.end() && (*it)->note().begin < pos) ++it;
		Note n = (*it)->note();
		if (it == m_notes.end() || n.type == Note::SLEEP) { stop(false); return; }
		m_note = n.note; // % 12;
		qint64 nextTime = (n.begin - pos + m_length) * 1000;
		m_length = n.length(); //((n.begin > pos) ? n.length() : (n.end - pos));
		m_pos = n.end; // Where to start the search for next note
		m_timerId = startTimer(nextTime > 0 ? nextTime : 1);
		std::cout << "lyr: " << n.syllable.toStdString() << " len: " << m_length  << " dela: " << (int)nextTime << std::endl;
	}

	/// Creates the sound
	QByteArray createBuffer() {
		// This is simple beep, so we use mono, low sample rate and only 8 bits resolution
		// --> quick to create and small memory foot print
		std::string header = writeWavHeader(8, 1, srate, m_length * srate);
		QByteArray buf(header.c_str(), header.size());
		double d = (m_note + 1) / 13.0;
		double freq = MusicalScale().getNoteFreq(m_note + 12);
		// Synthesize tones
		for (size_t i = 0; i < m_length * srate; ++i) {
			float fvalue = d * 0.2 * std::sin(m_phase) + 0.2 * std::sin(2 * m_phase) + (1.0 - d) * 0.2 * std::sin(4 * m_phase);
			m_phase += 2.0 * M_PI * freq / srate;
			
			// 8-bit
			quint8 value = (fvalue + 1) * 0.5 * 255;
			buf.push_back(value);
		}

		//std::ofstream of("/tmp/wavdump.wav");
		//of.write(buf.data(), buf.size());

		return buf;
	}

	static const int srate = 8000; ///< Sample rate
	NoteLabels& m_notes; ///< Notes
	double m_phase; ///< Phase
	int m_note; ///< Next note to synthesize
	double m_length; ///< Next note length
	double m_pos; ///< Position from where to start looking the next note
	Phonon::MediaObject *m_player;
	QBuffer m_soundData[2];
	int m_curBuffer;
	int m_timerId; ///< Timer ID
};
