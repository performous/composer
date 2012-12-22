#include "songparser.hh"
#include "textcodecselector.hh"
#include <QFile>
#include <QFileInfo>
#include <algorithm>


namespace SongParserUtil {
	void assign(bool& var, QString const& str) {
		if (str == "YES" || str == "yes" || str == "1") var = true;
		else if (str == "NO" || str == "no" || str == "0") var = false;
		else throw std::runtime_error("Invalid boolean value: " + str.toStdString());
	}
}


/// constructor
SongParser::SongParser(Song& s):
  m_song(s),
  m_stream(),
  m_linenum(),
  m_relative(),
  m_gap(),
  m_prevtime(),
  m_prevts(),
  m_relativeShift(),
  m_maxScore(),
  m_tsPerBeat(),
  m_tsEnd()
{
    enum { NONE, TXT, XML, INI, MIDI, SM, SMMTXT } type = NONE;
	// Read the file, determine the type and do some initial validation checks
	QFile file(m_song.path + m_song.filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		throw SongParserException(QT_TR_NOOP("Could not open song file"), 0);

	QFileInfo finfo(file);
	if (finfo.size() < 10 || finfo.size() > 100000) throw SongParserException("Does not look like a song file (wrong size)", 1, true);

	// Determine encoding
	QString data = TextCodecSelector::readAllAndHandleEncoding(file);
	file.close();

	if (smCheck(data)) type = SM;
	else if (txtCheck(data)) type = TXT;
	else if (xmlCheck(data)) type = XML;
	else if (iniCheck(data)) type = INI;
	else if (midiCheck(data)) type = MIDI;
    else if (smmTxtCheck(data)) type = SMMTXT;

	m_stream.setString(&data);

	// Parse
	try {
		if (type == TXT) txtParse();
		else if (type == XML) xmlParse();
		else if (type == INI) iniParse();
		else if (type == MIDI) midParse();
		else if (type == SM) smParse();
        else if (type == SMMTXT) smmParse();
	} catch (std::runtime_error& e) {
		throw SongParserException(e.what(), m_linenum);
	}

	// Remove bogus entries
	if (!QFileInfo(m_song.path + m_song.cover).exists()) m_song.cover = "";
	if (!QFileInfo(m_song.path + m_song.background).exists()) m_song.background = "";
	if (!QFileInfo(m_song.path + m_song.video).exists()) m_song.video = "";

	// TODO: Should we try to guess images and stuff?

	finalize(); // Do some adjusting to the notes
	s.loadStatus = Song::FULL;
}

bool SongParser::getline(QString &line)
{
	++m_linenum;
	line = m_stream.readLine();
	return !m_stream.atEnd();
}

namespace {
	/// Return the amount of shift (in notes) required for note to make put it in the nearest octave of the target note
	int nearestOctave(int note, int target) {
		return (1200 + 6 + target - note) / 12 * 12 - 1200; // 1200 for always positive, 6 for mathematical rounding
	}
	void normalize(Notes& notes, int limLow, int limHigh) {
		// Analyze the entire song first
		int defaultShift = 0;
		int shiftFS = 0;
		{
			std::vector<int> fsNotes, regNotes;
			for (Notes::iterator it = notes.begin(); it != notes.end(); ++it) {
				(it->type == Note::FREESTYLE ? fsNotes : regNotes).push_back(it->note);
			}
			if (!regNotes.empty()) {
				std::sort(regNotes.begin(), regNotes.end());
				// Find a good starting value for default shift
				defaultShift = nearestOctave(regNotes[regNotes.size() / 2], 0.5 * (limLow + limHigh));
				// Find the additional correction required for freestyle notes 
				if (!fsNotes.empty()) {
					std::sort(fsNotes.begin(), fsNotes.end());
					shiftFS = nearestOctave(fsNotes[fsNotes.size() / 2], regNotes[regNotes.size() / 2]);
				}
			}
		}
		// Process sentence by sentence
		for (Notes::iterator it = notes.begin(), itnext = it; it != notes.end();) {
			int low, high;
			low = high = it->note;
			// Analyze the sentence and find the end of it
			while (++itnext != notes.end() && !itnext->lineBreak) {
				if (itnext->type == Note::SLEEP) continue;
				int n = itnext->note;
				if (itnext->type == Note::FREESTYLE) n += shiftFS;
				low = std::min(low, n);
				high = std::max(high, n);
			}
			// Per-sentence shift
			int shift = nearestOctave(0.5 * (low + high), 0.5 * (limLow + limHigh));
			if (std::abs(defaultShift - shift) <= 12) shift = defaultShift;
			// Shift the notes into position
			for (; it != itnext; ++it) {
				if (it->type == Note::SLEEP) continue;
				int s = shift;
				if (it->type == Note::FREESTYLE) s += shiftFS;
				// The last resort if everything else fails (per-note shifting)
				while (it->note + s < limLow) s += 12;
				while (it->note + s > limHigh) s -= 12;
				it->note += s;
				it->notePrev += s;
			}
		}
	}
}

void SongParser::finalize() {
	std::vector<QString> tracks = m_song.getVocalTrackNames();
	for(std::vector<QString>::const_iterator it = tracks.begin() ; it != tracks.end() ; ++it) {
		VocalTrack& vocal = m_song.getVocalTrack(*it);
		vocal.m_scoreFactor = 1.0 / m_maxScore;
		if (vocal.notes.empty()) continue;
		// Set begin/end times
		vocal.beginTime = vocal.notes.front().begin, vocal.endTime = vocal.notes.back().end;
		// Setup sentence start indicators and remove sleep notes
		bool sentenceStart = true;
		for (Notes::iterator it = vocal.notes.begin(); it != vocal.notes.end();) {
			if (sentenceStart) it->lineBreak = true;
			sentenceStart = false;
			if (it->type == Note::SLEEP) {
				sentenceStart = true;
				it = vocal.notes.erase(it);
			} else ++it;
		}
		// Note normalization
		const int limLow = 1, limHigh = 47;
		if (vocal.noteMin < limLow || vocal.noteMax > limHigh) normalize(vocal.notes, limLow, limHigh);
	}
	if (m_tsPerBeat) {
		// Add song beat markers
		for (unsigned ts = 0; ts < m_tsEnd; ts += m_tsPerBeat) m_song.beats.push_back(tsTime(ts));
	}
}
