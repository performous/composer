#include "midifile.hh"
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace mid;

void Event::print() const {
	std::ostringstream oss;
	oss << "Midi event:" << std::setw(12) << timecode << "  ";
	switch (type) {
	  case 0x8: oss << "note off   note=" << arg1 << " velocity=" << arg2; break;
	  case 0x9: oss << "note on   note=" << arg1 << " velocity=" << arg2; break;
	  case 0xA: oss << "aftertouch pitch=" << arg1 << " value=" << arg2; break;
	  case 0xB: oss << "controller num=" << arg1 << " value=" << arg2; break;
	  case 0xC: oss << "program change num=" << arg1; break;
	  case 0xD: oss << "channel value =" << arg1; break;
	  case 0xE: oss << "pitch bend value=" << (arg2 << 8 | arg1); break;
	  default: oss << "UNKNOWN EVENT=0x" << std::hex << type << std::dec << ")"; break;
	}
	oss << '\n';
	std::cerr << oss.str();
}
