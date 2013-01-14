#include "songparser.hh"
#include <stdexcept>
#include <iostream>
#include <QRegExp>

// LRC has many variations: http://en.wikipedia.org/wiki/LRC_(file_format)
// We also support Soramimi format, which differs as follows:
//   * Per word timing, [] instead of <> (in contrast to "Enhanced LRC")
//   * Centisecond separator is ':' instead of '.'


bool SongParser::lrcCheck(QString const& data) {
	return data[0] == '[';
}

void SongParser::lrcParse() {
	VocalTrack vocal(TrackName::LEAD_VOCAL);
	Notes& notes = vocal.notes;
	QString line;

	while (getline(line)) {
		// LRC header tags
		if (line.startsWith("[ar:", Qt::CaseInsensitive)) {
			m_song.artist = line.mid(4).trimmed().remove(QRegExp("\\]$"));
		} else if (line.startsWith("[ti:", Qt::CaseInsensitive)) {
			m_song.title = line.mid(4).trimmed().remove(QRegExp("\\]$"));
		} else if (line.startsWith("[by:", Qt::CaseInsensitive)) {
			m_song.creator = line.mid(4).trimmed().remove(QRegExp("\\]$"));
		// TODO: Gap, [offset: ?
		} else if (line.length() >= 2 && line[1].isLetter()) {
			// Skip unknown header tags
			continue;
		} else { // Note parsing
			// These replacements are compatibility between Soramimi and "enhanced" LRC
			line.replace("<", "[").replace(">", "]");
			lrcNoteParse(line, vocal);
		}
	}

	if (!notes.empty()) {
		vocal.beginTime = notes.front().begin;
		vocal.endTime = notes.back().end;
		// Insert notes
		m_song.insertVocalTrack(TrackName::LEAD_VOCAL, vocal);
	} else throw std::runtime_error(QT_TR_NOOP("Couldn't find any notes"));
}

bool SongParser::lrcNoteParse(QString line, VocalTrack& vocal) {
	if (line.isEmpty()) return false;
	if (line[0] != '[') throw std::runtime_error("Unexpected character at line start");

	Notes& notes = vocal.notes;
	bool parsingTime = true, createNote = false;
	QString timeStr = "", lyric = "";
	double time = 0, prevTime = 0;
	// Start traversing from 1 because we already know 0 is '['
	for (int i = 1; i < line.length(); ++i) {
		// Two state parser: either parsing timestamp or lyrics
		if (parsingTime) {
			if (line[i].toAscii() == ']') { // Timestamp end
				parsingTime = false;
				prevTime = time;
				time = convertLRCTimestampToDouble(timeStr);
				timeStr.clear();
				if (!lyric.isEmpty()) {
					createNote = true;
				} else if (!notes.empty()) {
					// Adjust last sentence's end to fix possible overlaps if there is no line end timestamp
					notes.back().end = std::min(notes.back().end, time);
				}
			} else { // Accumulate timestamp string
				timeStr += line[i];
			}
		} else { // Lyrics parsing mode
			if (line[i].toAscii() == '[') parsingTime = true; // Lyric end
			else lyric += line[i]; // Accumulate lyric string
		}

		// Create the note
		if (createNote || i == line.length() - 1) {
			Note n(lyric);
			n.begin = prevTime;
			n.end = time;
			n.note = 33;
			notes.push_back(n);
			lyric.clear();
			createNote = false;
		}
	}

	// Add SLEEP to line end
	Note n;
	n.type = Note::SLEEP;
	n.begin = time;
	n.end = time;
	n.note = 33;
	notes.push_back(n);

	return true;
}

double SongParser::convertLRCTimestampToDouble(QString timeStamp) {
	bool ok = false;
	// This replacing is also LRC <-> Soramimi compatibility
	timeStamp.replace(QString(":"), QString("."));
	QString minutes = timeStamp.mid(0,2);
	QString seconds = timeStamp.mid(3,5);
	double min = minutes.toDouble(&ok);
	if (!ok) throw std::runtime_error("Invalid minutes in timestamp " + timeStamp.toStdString());
	double sec = seconds.toDouble(&ok);
	if (!ok) throw std::runtime_error("Invalid seconds in timestamp " + timeStamp.toStdString());
	return min * 60 + sec;
}
