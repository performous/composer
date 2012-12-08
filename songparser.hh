#pragma once

#include "song.hh"
#include <QTextStream>

namespace SongParserUtil {
	/// Parse a boolean from string and assign it to a variable
	void assign(bool& var, QString const& str);
}

/// parses songfiles
class SongParser {
  public:
	/// constructor
	SongParser(Song& s);

	static bool looksLikeSongFile(QString const& data) {
		return txtCheck(data) || xmlCheck(data) || iniCheck(data) || smCheck(data);
	}

  private:
	void finalize();

	Song& m_song;
	QTextStream m_stream;
	unsigned int m_linenum;
	bool getline(QString& line);
	bool m_relative;
	double m_gap;

	// UltraStar TXT
	static bool txtCheck(QString const& data);
	void txtParse();
	bool txtParseField(QString const& line);
	bool txtParseNote(QString line, VocalTrack &vocal);

	// SingStar XML
	static bool xmlCheck(QString const& data);
	void xmlParse();

	// Frets on Fire MIDI
	static bool iniCheck(QString const& data);
	static bool midiCheck(QString const& data);
	void iniParse();
	void iniParseField(QString const& line);
	void midParse();

    //SoramimiTXT
    void smmParse();
    bool smmNoteParse(QString line);
    int convertTimeToBpm(int bpm,QString time);

	// FIXME: Dummy funcs
	static bool smCheck(QString const& data) { (void)data; return false; }
	void smParse() { }
	bool smParseField(std::string line) { (void)line; return false; }
	Notes smParseNotes(std::string line) { (void)line; return Notes(); }

	double m_prevtime;
	unsigned int m_prevts;
	unsigned int m_relativeShift;
	double m_maxScore;
	struct BPM {
		BPM(double _begin, double _ts, double _bpm, double division): begin(_begin), step(60.0 / _bpm / division), ts(_ts) {}
		double begin; // Time in seconds
		double step; // Seconds per quarter note
		double ts;
	};
	typedef std::vector<BPM> bpms_t;
	bpms_t m_bpms;
	unsigned m_tsPerBeat;  ///< The ts increment per beat
	unsigned m_tsEnd;  ///< The ending ts of the song
	void addBPM(double ts, double bpm, double division = 4.0) {
		if (!(bpm >= 1.0 && bpm < 1e12)) throw std::runtime_error("Invalid BPM value");
		if (!m_bpms.empty()) {
			double diff = ts - m_bpms.back().ts;
			if (diff == 0.0) m_bpms.pop_back();  // Avoid zero-length BPM definitions
			else if (!(diff > 0.0)) throw std::runtime_error("Invalid BPM timestamp");
		}
		m_bpms.push_back(BPM(tsTime(ts), ts, bpm, division));
	}
	/// Convert a timestamp (beats) into time (seconds)
	double tsTime(double ts) const {
		if (m_bpms.empty()) {
			if (ts != 0) throw std::runtime_error("BPM data missing");
			return m_gap;
		}
		for (std::vector<BPM>::const_reverse_iterator it = m_bpms.rbegin(); it != m_bpms.rend(); ++it) {
			if (it->ts <= ts) return it->begin + (ts - it->ts) * it->step;
		}
		throw std::logic_error("INTERNAL ERROR: BPM data invalid");
	}
	/// Stops stored in <ts, duration> format
	Song::Stops m_stops;
	/// Convert a stop into <time, duration> (as stored in the song)
	std::pair<double, double> stopConvert(std::pair<double, double> s) {
		s.first = tsTime(s.first);
		return s;
	}
};
