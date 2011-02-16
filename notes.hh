#pragma once

#include <map>
#include <string>
#include <vector>
#include <QString>

/// musical scale, defaults to C major
class MusicalScale {
  private:
	double m_baseFreq;
	static const int m_baseId = 33;

  public:
	/// constructor
	MusicalScale(double baseFreq = 440.0): m_baseFreq(baseFreq) {}
	/// get name of note
	QString getNoteStr(double freq) const;
	/// get note number for id
	unsigned int getNoteNum(int id) const;
	/// true if sharp note
	bool isSharp(int id) const;
	/// get frequence for note id
	double getNoteFreq(int id) const;
	/// get note id for frequence
	int getNoteId(double freq) const;
	/// get note for frequence
	double getNote(double freq) const;
	/// get note offset for frequence
	double getNoteOffset(double freq) const;
	/// get octave number

	/// get base id
	static int getBaseId() { return m_baseId; }
};


/// stores duration of a note
struct Duration {
	double begin, ///< beginning timestamp in seconds
	       end;   ///< ending timestamp in seconds
	Duration();
	/// create a new Duration object and initialize begin and end
	Duration(double b, double e): begin(b), end(e) {}
	/// compares begin timestamps of two Duration structs
	static bool ltBegin(Duration const& a, Duration const& b) { return a.begin < b.begin; }
	/// compares end timestamps of two Duration structs
	static bool ltEnd(Duration const& a, Duration const& b) { return a.end < b.end; }
};

typedef std::vector<Duration> Durations;
typedef std::map<int, Durations> NoteMap;

/// note read from songfile
struct Note {
	Note(QString lyric = "");
	/// note type - NOTE! Keep the types array below in sync with the enum!
	enum Type { FREESTYLE = 'F', NORMAL = ':', GOLDEN = '*', SLIDE = '+', SLEEP = '-',
		TAP = '1', HOLDBEGIN = '2', HOLDEND = '3', ROLL = '4', MINE = 'M', LIFT = 'L'} type;
	static const Type types[];
	int getTypeInt() const;

	//Duration duration; ///< note begin/end
	double begin; // FIXME: Should use duration but it is pain to change everywhere
	double end;
	double phase; ///< position within a measure, [0, 1)
	int note; ///< MIDI pitch of the note (at the end for slide notes)
	int notePrev; ///< MIDI pitch of the previous note (should be same as note for everything but SLIDE)
	QString syllable; ///< lyrics syllable for that note
	bool lineBreak; ///< is this note ending a syllable?
	/// move beginning without changing length
	void move(double newBegin) { double l = length(); begin = newBegin; end = newBegin + l; }
	/// note length
	double length() const { return end - begin; }
	/// difference of n from note
	double diff(double n) const { return diff(note, n); }
	/// difference of n from note, so that note + diff(note, n) is n (mod 12)
	static double diff(double note, double n);
	/// compares begin of two notes
	static bool ltBegin(Note const& a, Note const& b) { return a.begin < b.begin; }
	/// compares end of two notes
	static bool ltEnd(Note const& a, Note const& b) { return a.end < b.end; }

	/// human-readable description of note type
	QString typeString() const;
};

typedef std::vector<Note> Notes;

struct VocalTrack {
	VocalTrack(QString name);
	void reload();
	QString name;
	Notes notes;
	int noteMin, noteMax; ///< lowest and highest note
	double beginTime, endTime; ///< the period where there are notes
	double m_scoreFactor; ///< normalization factor for the scoring system
	MusicalScale scale; ///< scale in which song is sung
};

typedef std::map<QString, VocalTrack> VocalTracks;
