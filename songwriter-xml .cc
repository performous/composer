#include "songwriter.hh"
#include <fstream>
#include <iostream>

void SingStarXMLWriter::writeXML() {
	throw std::runtime_error("XML export is not implemented.");
	// FIXME: Use QtXML
	std::ofstream f((path + "notes.xml").c_str(), std::ios::binary);
	// FIXME: The following is just an example and doesn't actually output XML format
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
