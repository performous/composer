#include "songwriter.hh"
#include "util.hh"
#include <cmath>
#include <QTextStream>

#define PACKAGE "composer"
#define VERSION 2


void LRCWriter::writeLRC() const {
	QFile f(path + "/song.lrc");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
	out.setCodec("UTF-8");

	// Meta fields
	out << "[ti:" << (s.title.isEmpty() ? "Unknown" : s.title) << "]\n";
	out << "[ar:" << (s.artist.isEmpty() ? "Unknown" : s.artist) << "]\n";
	out << "[re:" << PACKAGE << "]\n";
	out << "[ve:" << VERSION << "]\n";
	if (!s.creator.isEmpty()) out << "[by:" << s.creator << "]\n";

	// Loop through the notes
	const Notes& notes = s.getVocalTrack().notes;
	for (int i = 0; i < notes.size(); ++i) {
		const Note& n = notes[i];
		if (n.type == Note::SLEEP) continue;

		// Put timestamp before new phrases
		if (i == 0 || n.lineBreak)
			out << '\n' << sec2timestamp(n.begin);

		// Output the lyrics
		out << n.syllable;
	}
	out << '\n';
}


QString LRCWriter::sec2timestamp(double sec) const {
	double modsec = std::fmod(sec, 60.0);
	return QString("[%1:%2.%3]")
			.arg(int(sec/60), 2, 10, QChar('0'))
			.arg(int(modsec), 2, 10, QChar('0'))
			.arg(int(100 * (modsec - int(modsec))), 2, 10, QChar('0'));
}


