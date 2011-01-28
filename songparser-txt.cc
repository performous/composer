#include "songparser.hh"

#include <stdexcept>
#include <iostream>

/// @file
/// Functions used for parsing the UltraStar TXT song format

using namespace SongParserUtil;

/// 'Magick' to check if this file looks like correct format
bool SongParser::txtCheck(QString const& data) {
	return data[0] == '#' && data[1] >= 'A' && data[1] <= 'Z';
}

/// Parser
void SongParser::txtParse() {
	QString line;
	// Parse header
	while (getline(line) && txtParseField(line)) {}
	if (m_song.title.isEmpty() || m_song.artist.isEmpty())
		throw std::runtime_error("Required header fields missing");
	if (m_song.bpm != 0.0) addBPM(0, m_song.bpm);

	// Parse notes
	VocalTrack vocal(TrackName::LEAD_VOCAL);
	while (txtParseNote(line, vocal) && getline(line)) {}

	// Workaround for the terminating : 1 0 0 line, written by some converters
	if (!vocal.notes.empty() && vocal.notes.back().type != Note::SLEEP
	  && vocal.notes.back().begin == vocal.notes.back().end) vocal.notes.pop_back();

	m_song.insertVocalTrack(TrackName::LEAD_VOCAL, vocal);
}

bool SongParser::txtParseField(QString const& line) {
	if (line.isEmpty()) return true;
	if (line[0] != '#') return false;
	int pos = line.indexOf(':');
	if (pos < 0) throw std::runtime_error("Invalid txt format, should be #key:value");
	QString key = line.left(pos).trimmed().mid(1);
	QString value = line.mid(pos + 1).trimmed();
	bool ok = true;
	if (value.isEmpty()) return true;
	if (key == "TITLE") m_song.title = value;
	else if (key == "ARTIST") m_song.artist = value;
	else if (key == "EDITION") m_song.edition = value;
	else if (key == "GENRE") m_song.genre = value;
	else if (key == "CREATOR") m_song.creator = value;
	else if (key == "COVER") m_song.cover = value;
	else if (key == "MP3") m_song.music["background"] = m_song.path + value;
	else if (key == "VOCALS") m_song.music["vocals"] = m_song.path + value;
	else if (key == "VIDEO") m_song.video = value;
	else if (key == "BACKGROUND") m_song.background = value;
	else if (key == "START") m_song.start = value.toDouble(&ok);
	else if (key == "VIDEOGAP") m_song.videoGap = value.toDouble(&ok);
	else if (key == "PREVIEWSTART") m_song.preview_start = value.toDouble(&ok);
	else if (key == "RELATIVE") assign(m_relative, value);
	else if (key == "GAP") { m_gap = value.toDouble(&ok); m_gap *= 1e-3; }
	else if (key == "BPM") m_song.bpm = value.toDouble(&ok);
	else if (key == "LANGUAGE") m_song.language= value;
	else if (key == "YEAR") m_song.year = value;

	if (!ok) throw std::runtime_error(QString("Invalid value for %1: %2").arg(key).arg(value).toStdString());
	return true;
}

bool SongParser::txtParseNote(QString line, VocalTrack &vocal) {
	if (line.isEmpty() || line == "\r") return true;
	line = line.trimmed();
	if (line[0] == '#') throw std::runtime_error("Key found in the middle of notes");
	if (line[0] == 'E') return false;
	QTextStream iss(&line);
	if (line[0] == 'B') {
		unsigned int ts;
		double bpm;
		QChar ignore; iss >> ignore;
		iss >> ts >> bpm;
		if (iss.status() != QTextStream::Ok) throw std::runtime_error("Invalid BPM line format");
		addBPM(ts, bpm);
		return true;
	}
	if (line[0] == 'P') return true; //We ignore player information for now (multiplayer hack)
	Note n;
	n.type = Note::Type(iss.read(1)[0].toAscii());
	unsigned int ts = m_prevts;
	switch (n.type) {
	  case Note::NORMAL:
	  case Note::FREESTYLE:
	  case Note::GOLDEN:
		{
			unsigned int length = 0;
			iss >> ts >> length >> n.note;
			if (iss.status() != QTextStream::Ok) throw std::runtime_error("Invalid note line format");
			n.notePrev = n.note; // No slide notes in TXT yet.
			if (m_relative) ts += m_relativeShift;
			if (iss.read(1)[0].toAscii() == ' ') n.syllable = iss.readLine().trimmed();
			n.end = tsTime(ts + length);
		}
		break;
	  case Note::SLEEP:
		{
			unsigned int end;
			iss >> ts >> end;
			if (iss.status() != QTextStream::Ok) end = ts;
			if (m_relative) {
				ts += m_relativeShift;
				end += m_relativeShift;
				m_relativeShift = end;
			}
			n.end = tsTime(end);
		}
		break;
	  default: throw std::runtime_error("Unknown note type");
	}
	n.begin = tsTime(ts);
	Notes& notes = vocal.notes;
	if (m_relative && notes.empty()) m_relativeShift = ts;
	m_prevts = ts;
	if (n.begin < m_prevtime) {
		// Oh no, overlapping notes (b0rked file)
		// Can't do this because too many songs are b0rked: throw std::runtime_error("Note overlaps with previous note");
		if (notes.size() >= 1) {
			Note& p = notes.back();
			// Workaround for songs that use semi-random timestamps for sleep
			if (p.type == Note::SLEEP) {
				p.end = p.begin;
				Notes::reverse_iterator it = notes.rbegin();
				Note& p2 = *++it;
				if (p2.end < n.begin) p.begin = p.end = n.begin;
			}
			// Can we just make the previous note shorter?
			if (p.begin <= n.begin) p.end = n.begin;
			else { // Nothing to do, warn and skip
				std::cout << "Skipping overlapping note in " << m_song.path.toStdString() << m_song.filename.toStdString() << std::endl;
				return true;
			}
		} else throw std::runtime_error("The first note has negative timestamp");
	}
	double prevtime = m_prevtime;
	m_prevtime = n.end;
	if (n.type != Note::SLEEP && n.end > n.begin) {
		vocal.noteMin = std::min(vocal.noteMin, n.note);
		vocal.noteMax = std::max(vocal.noteMax, n.note);
	}
	if (n.type == Note::SLEEP) {
		if (notes.empty()) return true; // Ignore sleeps at song beginning
		n.begin = n.end = prevtime; // Normalize sleep notes
		notes.back().lineBreak = true; // lineBreak flag for notes preceding SLEEPs
	}
	notes.push_back(n);
	return true;
}

