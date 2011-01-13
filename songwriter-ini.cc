#include "songwriter.hh"
#include <fstream>
#include <iostream>

void FoFMIDIWriter::writeMIDI() {
	throw std::runtime_error("MIDI export is not implemented.");
	std::ofstream f((path + "notes.mid").c_str(), std::ios::binary);
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
	}
}

void FoFMIDIWriter::writeINI() {
	std::ofstream f((path + "song.ini").c_str(), std::ios::binary);
	f << "[song]\n";
	f << "name = " << s.title << std::endl;
	f << "artist = " << s.artist << std::endl;
	f << "genre = " << s.genre << std::endl;
	f << "year = " << s.year << std::endl;
}

