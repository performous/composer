#include "songwriter.hh"
#include <QtXml/QDomDocument>
#include <QTextStream>
#include <iostream>

void SingStarXMLWriter::writeXML() {
	QDomDocument doc("");
	QDomElement root = doc.createElement("MELODY");
	root.setAttribute("xmlns", "http://www.singstargame.com");
	root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
	root.setAttribute("Version", "1");
	root.setAttribute("Tempo", "120"); // FIXME!
	root.setAttribute("FixedTempo", "Yes");
	root.setAttribute("Resolution", "Demisemiquaver"); //?
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

	QString xml = doc.toString(4);

	QFile f(path + "/notes.xml");
	if (f.open(QFile::WriteOnly)) {
		QTextStream out(&f);
		out << xml;
	} else throw std::runtime_error("Couldn't open target file notes.xml");

	// FIXME: The following is just an example and doesn't actually output XML format
	/*
	char buf[1024] = {};
	Notes const& notes = s.getVocalTrack().notes;
	std::cout << notes.size() << std::endl;
	for (unsigned int i = 0; i < notes.size(); ++i) {
		Note const& n = notes[i];
		buf[0] = 0xFF;
		buf[1] = n.note; // MIDI note value
		// Others are n.begin, n.end, n.type etc. (see notes.hh)
		f.write(buf, 1024);
	}*/

}
