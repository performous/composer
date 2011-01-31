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
	enum { NONE, TXT, XML, INI, SM } type = NONE;
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
	else throw SongParserException("Does not look like a song file (wrong header)", 1, true);

	m_stream.setString(&data);

	// Parse
	try {
		if (type == TXT) txtParse();
		else if (type == XML) xmlParse();
		else if (type == INI) iniParse();
		else if (type == SM) smParse();
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
		return (1006 + target - note) / 12 * 12 - 1006; // 1006 for mathematical rounding (always positive, round up from 6)
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
		// Note normalization
		const int limLow = 0, limHigh = 48;
		if (vocal.noteMin <= limLow || vocal.noteMax >= limHigh) {
			std::vector<int> fsNotes, regNotes;
			for (Notes::iterator it = vocal.notes.begin(); it != vocal.notes.end(); ++it) {
				(it->type == Note::FREESTYLE ? fsNotes : regNotes).push_back(it->note);
			}
			std::sort(regNotes.begin(), regNotes.end());
			std::sort(fsNotes.begin(), fsNotes.end());
			// Center the entire song to the middle of the permitted range
			int shift = regNotes.empty() ? 0 : nearestOctave(fsNotes[fsNotes.size() / 2], 24);
			int shiftFS = fsNotes.empty() ? 0 : nearestOctave(fsNotes[fsNotes.size() / 2], 24);
			for (Notes::iterator it = vocal.notes.begin(); it != vocal.notes.end(); ++it) {
				int s = (it->type == Note::FREESTYLE ? shiftFS : shift);
				it->note += s;
				it->notePrev += s;
			}
		}
	}
	if (m_tsPerBeat) {
		// Add song beat markers
		for (unsigned ts = 0; ts < m_tsEnd; ts += m_tsPerBeat) m_song.beats.push_back(tsTime(ts));
	}
}
