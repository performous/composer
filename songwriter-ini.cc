#include "songwriter.hh"

#include "midifile.hh"
#include <QTextStream>


void FoFMIDIWriter::writeMIDI() const {
	Notes const& notes = s.getVocalTrack().notes;
	unsigned division = 16;
	midifile::Writer writer(1, 2, division);
	writer.startTrack();
	// TODO: write timing info
	writer.startTrack();
	// TODO: write notes
	QByteArray name = (path + "/notes.mid").toLocal8Bit();
	writer.save(std::string(name.data(), name.size()).c_str());
}

void FoFMIDIWriter::writeINI() const {
	QFile f(path + "/song.ini");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
	out.setCodec("UTF-8");

	out << "[song]\n";
	out << "name = " << (s.title.isEmpty() ? "Unknown" : s.title) << '\n';
	out << "artist = " << (s.artist.isEmpty() ? "Unknown" : s.artist) << '\n';
	if (!s.genre.isEmpty()) out << "genre = " << s.genre << '\n';
	if (!s.year.isEmpty()) out << "year = " << s.year << '\n';
}

