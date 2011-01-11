#pragma once

#include <map>
#include <string>
#include <vector>

/// musical scale, defaults to C major
class MusicalScale {
  private:
	double m_baseFreq;
	static const int m_baseId = 33;

  public:
	/// constructor
	MusicalScale(double baseFreq = 440.0): m_baseFreq(baseFreq) {}
	/// get name of note
	std::string getNoteStr(double freq) const;
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
	Note(std::string lyric = "");
	Duration duration; ///< note begin/end
	double phase; ///< position within a measure, [0, 1)
	/// note type
	enum Type { NORMAL = 0, GOLDEN = 1, FREESTYLE = 2 } type;
	int note; ///< MIDI pitch of the note (at the end for slide notes)
	int notePrev; ///< MIDI pitch of the previous note (should be same as note for everything but SLIDE)
	std::string syllable; ///< lyrics syllable for that note
	/// note begin time access
	double& begin() { return duration.begin; }
	double begin() const { return duration.begin; }
	/// note end time access
	double& end() { return duration.end; }
	double end() const { return duration.end; }
	/// difference of n from note
	double diff(double n) const { return diff(note, n); }
	/// difference of n from note, so that note + diff(note, n) is n (mod 12)
	static double diff(double note, double n);
	/// compares begin of two notes
	static bool ltBegin(Note const& a, Note const& b) { return a.duration.begin < b.duration.begin; }
	/// compares end of two notes
	static bool ltEnd(Note const& a, Note const& b) { return a.duration.end < b.duration.end; }

	/// human-readable description of note type
	std::string typeString() const;
};

typedef std::vector<Note> Notes;

struct VocalTrack {
	VocalTrack(std::string name);
	void reload();
	std::string name;
	Notes notes;
	int noteMin, noteMax; ///< lowest and highest note
	double beginTime, endTime; ///< the period where there are notes
	double m_scoreFactor; ///< normalization factor for the scoring system
	MusicalScale scale; ///< scale in which song is sung
};

typedef std::map<std::string, VocalTrack> VocalTracks;
