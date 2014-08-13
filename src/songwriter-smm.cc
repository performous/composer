#include "songwriter.hh"
#include "util.hh"
#include <cmath>
#include <QTextStream>
#define PACKAGE "composer"
#define VERSION 2

void SMMWriter::writeSMM() const {
	QFile f(path + "/" + s.artist + " - " + s.title + ".txt");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
	out.setCodec("UTF-8");

	// Loop through the notes
	const Notes& notes = s.getVocalTrack().notes;
	for (int i = 0; i < notes.size(); ++i) {
		const Note& n = notes[i];
		if (i == 0 || n.lineBreak)
			out << '\n' << sec2timestamp(n.begin);
		// Put timestamp between notes
			out << n.syllable << sec2timestamp(n.begin);
		if (n.type == Note::SLEEP)
			out << '\n';
	}
	out << '\n';
}



QString SMMWriter::sec2timestamp(double sec) const {
	double modsec = std::fmod(sec, 60.0);
	return QString("[%1:%2:%3]")
			.arg(int(sec/60), 2, 10, QChar('0'))
			.arg(int(modsec), 2, 10, QChar('0'))
			.arg(int(100 * (modsec - int(modsec))), 2, 10, QChar('0'));
}
