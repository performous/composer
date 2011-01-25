#include "notes.hh"

#include "util.hh"
#include <QtGlobal>
#include <QTextStream>
#include <cmath>
#include <stdexcept>

QString MusicalScale::getNoteStr(double freq) const {
	int id = getNoteId(freq);
	if (id == -1) return QString();
	static const char * note[12] = {"C ","C#","D ","D#","E ","F ","F#","G ","G#","A ","A#","B "};
	QString buf;
	QTextStream ts(&buf);
	// Acoustical Society of America Octave Designation System
	//int octave = 2 + id / 12;
	ts << note[id%12] << " " << int(round(freq)) << " Hz";
	return ts.readAll();
}

unsigned int MusicalScale::getNoteNum(int id) const {
	// C major scale
	int n = id % 12;
	return (n + (n > 4)) / 2;
}

bool MusicalScale::isSharp(int id) const {
	if (id < 0) throw std::logic_error("MusicalScale::isSharp: Invalid note ID");
	// C major scale
	switch (id % 12) {
	  case 1: case 3: case 6: case 8: case 10: return true;
	}
	return false;
}

double MusicalScale::getNoteFreq(int id) const {
	if (id == -1) return 0.0;
	return m_baseFreq * std::pow(2.0, (id - m_baseId) / 12.0);
}

int MusicalScale::getNoteId(double freq) const {
	double note = getNote(freq);
	if (note >= 0.0 && note < 100.0) return int(note + 0.5);
	return -1;
}

double MusicalScale::getNote(double freq) const {
	if (freq < 1.0) return getNaN();
	return m_baseId + 12.0 * std::log(freq / m_baseFreq) / std::log(2.0);
}

double MusicalScale::getNoteOffset(double freq) const {
	double frac = freq / getNoteFreq(getNoteId(freq));
	return 12.0 * std::log(frac) / std::log(2.0);
}

Duration::Duration(): begin(getNaN()), end(getNaN()) {}


const Note::Type Note::types[] = { NORMAL, GOLDEN, FREESTYLE, SLIDE, SLEEP, TAP, HOLDBEGIN, HOLDEND, ROLL, MINE, LIFT };

Note::Note(QString lyric): syllable(lyric), begin(), end(), phase(getNaN()), type(NORMAL), note(), notePrev(), lineBreak() {}

double Note::diff(double note, double n) { return remainder(n - note, 12.0); }

int Note::getTypeInt() const {
	switch (type) {
		case NORMAL:    return 0;
		case GOLDEN:    return 1;
		case FREESTYLE: return 2;
		case SLIDE:     return 3;
		case SLEEP:     return 4;
		default:        return 255;
	}
}

QString Note::typeString() const {
	switch (type) {
		case NORMAL:    return QT_TR_NOOP("Normal");
		case GOLDEN:    return QT_TR_NOOP("Bonus");
		case FREESTYLE: return QT_TR_NOOP("Freestyle");
		case SLIDE:     return QT_TR_NOOP("Slide");
		case SLEEP:     return QT_TR_NOOP("Sleep");
		default:        return QT_TR_NOOP("Unknown");
	}
}


VocalTrack::VocalTrack(QString name) : name(name) {reload();}

void VocalTrack::reload() {
	notes.clear();
	m_scoreFactor = 0.0;
	noteMin = std::numeric_limits<int>::max();
	noteMax = std::numeric_limits<int>::min();
	beginTime = endTime = getNaN();
}
