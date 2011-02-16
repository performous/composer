#include "songwriter.hh"
#include <QTextStream>


void FoFMIDIWriter::writeMIDI() {
	throw std::runtime_error("MIDI export is not implemented.");
	/*std::ofstream f((path + "notes.mid").c_str(), std::ios::binary);
	// FIXME: The following is just an example and doesn't actually output MID format
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

void FoFMIDIWriter::writeINI() {
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

