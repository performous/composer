#include "songwriter.hh"
#include "util.hh"
#include <QtXml/QDomDocument>
#include <QTextStream>
#include <iostream>


void SingStarXMLWriter::writeXML() {
	QDomDocument doc("");
	QDomElement root = doc.createElement("MELODY");
	root.setAttribute("xmlns", "http://www.singstargame.com");
	root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
	root.setAttribute("Version", "1");
	root.setAttribute("Tempo", QString::number(tempo));
	root.setAttribute("FixedTempo", "Yes");
	root.setAttribute("Resolution", "Semiquaver"); // Demisemiquaver = 2x tempo
	root.setAttribute("Genre", QString::fromStdString(s.genre));
	root.setAttribute("Year", QString::fromStdString(s.year));
	root.setAttribute("xsi:schemaLocation", "http://www.singstargame.com http://15GMS-SINGSQL/xml_schema/melody.xsd");
	root.setAttribute("m2xVersion", "060110"); //?
	root.setAttribute("audioVersion", "2"); //?
	doc.appendChild(root);

	QDomComment artistComment = doc.createComment(QString("Artist: ") + QString::fromStdString(s.artist));
	QDomComment titleComment = doc.createComment(QString("Title: ") + QString::fromStdString(s.title));
	root.appendChild(artistComment);
	root.appendChild(titleComment);

	int tracknum = 1;
	QDomElement trackElem = doc.createElement("TRACK");
	trackElem.setAttribute("Name", "Player1");
	trackElem.setAttribute("Artist", QString::fromStdString(s.artist));

	int sentencenum = 1;
	QDomElement sentenceElem = doc.createElement("SENTENCE"); // FIXME: Should there be Singer and Part attributes?
	QDomComment sentenceComment = doc.createComment(QString("Track %1, Sentence %2").arg(tracknum).arg(sentencenum));
	trackElem.appendChild(sentenceComment);

	// Iterate all notes
	Notes const& notes = s.getVocalTrack().notes;
	for (unsigned int i = 0; i < notes.size(); ++i) {
		Note const& n = notes[i];

		// SLEEP notes indicate sentence end
		if (n.type == Note::SLEEP) {
			trackElem.appendChild(sentenceElem);
			++sentencenum;
			sentenceElem = doc.createElement("SENTENCE");
			sentenceComment = doc.createComment(QString("Track %1, Sentence %2").arg(tracknum).arg(sentencenum));
			sentenceElem.appendChild(sentenceComment);

		} else { // Regular note handling

			// Construct the note element
			QDomElement noteElem = doc.createElement("NOTE");
			noteElem.setAttribute("MidiNote", QString::number(n.note));
			noteElem.setAttribute("Duration", QString::number(sec2dur(n.length())));
			noteElem.setAttribute("Lyric", QString::fromStdString(n.syllable));
			if (n.type == Note::GOLDEN) noteElem.setAttribute("Bonus", "Yes");
			if (n.type == Note::FREESTYLE) noteElem.setAttribute("FreeStyle", "Yes");
			sentenceElem.appendChild(noteElem);

			// Construct a second note element, indicationg the pause before next note
			// This is only done if the pause has duration
			int pauseLen = 0;
			if (i < notes.size() - 1)
				pauseLen = sec2dur(notes[i+1].begin - n.end); // Difference to next note
			if (pauseLen > 0) {
				QDomElement pauseElem = doc.createElement("NOTE");
				pauseElem.setAttribute("MidiNote", "0");
				pauseElem.setAttribute("Duration", QString::number(sec2dur(pauseLen)));
				pauseElem.setAttribute("Lyric", "");
				sentenceElem.appendChild(pauseElem);
			}
		}
	}

	// TODO: Needs a last dummy sentence

	root.appendChild(trackElem);

	// Get the xml data
	QString xml = doc.toString(4);
	// Write to file
	QFile f(path + "/notes.xml");
	if (f.open(QFile::WriteOnly)) {
		QTextStream out(&f);
		out << xml;
	} else throw std::runtime_error("Couldn't open target file notes.xml");
}

int SingStarXMLWriter::sec2dur(double sec) {
	return round(tempo / 60.0 * sec);
}
