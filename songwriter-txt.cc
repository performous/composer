#include "songwriter.hh"
#include <fstream>
#include <iostream>

void UltraStarTXTWriter::writeTXT() {
	throw std::runtime_error("TXT export is not implemented.");
/*
	std::ofstream f((path + "notes.txt").c_str(), std::ios::binary);
	// FIXME: The following is just an example and doesn't actually output TXT format
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
