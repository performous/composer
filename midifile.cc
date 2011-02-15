#include "midifile.hh"
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace mid;

#if 0
if (event & 0x80) {
	// Store current status, with exceptions:
	// * Not stored for RealTime Category messages (0xF8..0xFF)
	// * Running status cleared for System Common Category (0xF0..0xF7)
	if (event < 0xF8) runningstatus = (event < 0xF0 ? event : 0);
} else {
	riff.seek_back();
	if (!runningstatus) throw std::runtime_error("Invalid MIDI file (first MIDI Event of a track wants running status)");
	event = runningstatus;
}

if (event < 0xF0) {
	// Midi event
	uint8_t arg1 = riff.read_uint8();
	uint8_t arg2 = 0;
	uint8_t ev = event >> 4;
	switch (ev) {
	case 0x8: case 0x9: case 0xA: case 0xB: case 0xE: arg2 = riff.read_uint8(); break;
	case 0xC: case 0xD: break;  // These only take one argument
	default: throw std::runtime_error("Unknown MIDI event");  // Quite possibly this is impossible, but I am too tired to prove it.
	}
	process_midi_event(track, ev, arg1, arg2, miditime);
}
if (event == 0xFF) {
	// Meta event
	uint8_t type = riff.read_uint8();
	std::string data = riff.read_bytes(riff.read_varlen());
	switch (type) {
	  // 0x00: Sequence Number
	  case 0x01: { // Text Event
		const std::string sect_pfx = "[section ";
		// Lyrics are hidden here, only [text] are orders
		if (data[0] != '[') m_lyric = data;
		else if (!data.compare(0, sect_pfx.length(), sect_pfx)) {// [section verse_1]
			std::string sect_name = data.substr(sect_pfx.length(), data.length()-sect_pfx.length()-1);
			if (sect_name != "big_rock_ending") {
				bool space = true;
				for (std::string::iterator it = sect_name.begin(); it != sect_name.end(); ++it) {
					if (space) *it = toupper(*it);        // start in uppercase
					if (*it == '_') {*it = ' '; space = true;} // underscores to spaces
					else space = false;
				}
				// replace gtr => guitar
				midisections.push_back(MidiSection(sect_name, get_seconds(miditime)));
			} else cmdevents.push_back(std::string(data)); // see songparser-ini.cc: we need to keep the BRE in cmdevents
		}
		else cmdevents.push_back(std::string(data));
	  } break;
	  // 0x02: Copyright Notice
	  case 0x03: // Sequence or Track Name
		track.name = data;
		break;
	  // 0x04: Instrument Name
	  case 0x05: // Lyric Text
		m_lyric = data;
		break;
	  // 0x06: Marker Text
	  // 0x07: Cue point
	  // 0x20: MIDI Channel Prefix Assignment
	  case 0x2F: // End of Track
		end = true;
		break;
	  case 0x51: // Tempo Setting
		if (data.size() != 3) throw std::runtime_error("Invalid tempo change event");
		add_tempo_change(miditime, static_cast<unsigned char>(data[0]) << 16 | static_cast<unsigned char>(data[1]) << 8 | static_cast<unsigned char>(data[2])); break;
	  // 0x54: SMPTE Offset
	  case 0x58: // Time Signature
		if (data.size() != 4) throw std::runtime_error("Invalid time signature event");
		break;
	  // 0x59: Key Signature
	  // 0x7f: Sequencer Specific Event
	  default: break;  // Unknown meta event (skipped)
	}
} else if (event==0xF0 || event==0xF7) {
	// System exclusive event
	uint32_t size = riff.read_varlen();
	riff.ignore(size);
}

#endif

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
