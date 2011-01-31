#include "songparser.hh"
#include <QtXml/QDomDocument>
#include <QFile>
#include <QtGlobal>
#include <iostream>



int ts = 0;
int sleepts = -1;


/// 'Magick' to check if this file looks like correct format
bool SongParser::xmlCheck(QString const& data)
{
	return (data[0] == '<' && data[1] == '?' && data[2] == 'x' && data[3] == 'm' && data[4] == 'l')
			|| (data[0] == '<' && data[1] == 'M' && data[2] == 'E' && data[3] == 'L');
}

void SongParser::xmlParse()
{
	// Build DOM tree from the xml file
	QDomDocument doc("MELODY");
	if (!doc.setContent(m_stream.readAll())) {
		throw std::runtime_error(QT_TR_NOOP("XML parse error"));	}

	VocalTrack vocal(TrackName::LEAD_VOCAL);
	Notes& notes = vocal.notes;

	// Parse meta
	QDomElement root = doc.documentElement();
	m_song.bpm = root.attribute("Tempo").toDouble();
	if (m_song.bpm == 0)
		throw std::runtime_error(QT_TR_NOOP("Invalid tempo"));
	if (root.attribute("Resolution") == QString("Demisemiquaver"))
		m_song.bpm *= 2;
	addBPM(0, m_song.bpm);
	m_song.genre = root.attribute("Genre");
	m_song.year = root.attribute("Year");

	bool track_found = false; // FIXME: HACK: We only parse the first track

	// Loop through the child elements
	QDomElement elem = root.firstChildElement();
	while (!elem.isNull()) {

		if (elem.tagName() == "TRACK") {
			// Track found
			if (track_found) break; // FIXME: HACK: We only parse the first track
			track_found = true;
			m_song.artist = elem.attribute("Artist");

		} else if (elem.tagName() == "SENTENCE") {
			// Sentence found
			// Loop through the notes in the sentence
			QDomElement noteElem = elem.firstChildElement();
			while (!noteElem.isNull()) {

				// We are only interested in NOTE elements
				if (noteElem.tagName() == "NOTE") {
					// Note found
					int length = noteElem.attribute("Duration").toInt();
					unsigned int ts = m_prevts;

					// See if it is an actual note and not sleep
					QString lyric = noteElem.attribute("Lyric").isEmpty()
						? noteElem.attribute("Rap") : noteElem.attribute("Lyric");
					if (noteElem.attribute("MidiNote") != "0" || !lyric.isEmpty()) {
						// TODO: Prettify lyric? (as ss_extract)
						Note n(lyric);
						if (noteElem.attribute("Bonus") == QString("Yes"))
							n.type = Note::GOLDEN;
						else if (noteElem.attribute("FreeStyle") == QString("Yes"))
							n.type = Note::FREESTYLE;
						else
							n.type = Note::NORMAL;

						n.note = noteElem.attribute("MidiNote").toInt();
						n.notePrev = n.note; // No slide notes
						n.begin = tsTime(ts);
						n.end = tsTime(ts + length);

						// Track note meta
						vocal.noteMin = std::min(vocal.noteMin, n.note);
						vocal.noteMax = std::max(vocal.noteMax, n.note);

						// Save note
						notes.push_back(n);
					}

					// Update time
					m_prevts += length;
					m_prevtime = tsTime(ts + length);
				}
				noteElem = noteElem.nextSiblingElement();
			}

			// Now add sentence end indicators
			if (!notes.empty()) notes.back().lineBreak = true;
			Note n;
			n.type = Note::SLEEP;
			n.note = 0;
			n.begin = m_prevtime;
			n.end = n.begin;
			notes.push_back(n);
		}
		elem = elem.nextSiblingElement();
	}

	if (!notes.empty()) {
		vocal.beginTime = notes.front().begin;
		vocal.endTime = notes.back().end;
		// Insert notes
		m_song.insertVocalTrack(TrackName::LEAD_VOCAL, vocal);
	} else throw std::runtime_error(QT_TR_NOOP("Couldn't find any notes"));
}


/*
	// Some extra formatting to make lyrics look better (hyphen removal & whitespace)
	if (lyric.size() > 0 && lyric[lyric.size() - 1] == '-') {
		if (lyric.size() > 1 && lyric[lyric.size() - 2] == ' ') lyric.erase(lyric.size() - 2);
		else lyric[lyric.size() - 1] = '~';
	} else {
		lyric += ' ';
	}
*/
