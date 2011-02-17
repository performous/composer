#include "songwriter.hh"
#include "config.hh"
#include "util.hh"
#include <QTextStream>


void LRCWriter::writeLRC() {
	QFile f(path + "/song.lrc");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
	out.setCodec("UTF-8");

	// Meta fields
	out << "[ti:" << (s.title.isEmpty() ? "Unknown" : s.title) << "]\n";
	out << "[ar:" << (s.artist.isEmpty() ? "Unknown" : s.artist) << "]\n";
	out << "[re:" << PACKAGE << "]\n";
	if (!s.creator.isEmpty()) out << s.creator << "]\n";

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
	return QString("[%1:%2]")
			.arg(int(sec/60), 2, 10, QChar('0'))
			.arg(int(sec) % 60, 5, 'f', 2, QChar('0'));
}
