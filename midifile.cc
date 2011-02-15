#include "midifile.hh"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace mid;

Reader::Reader(char const* filename) {
	std::ifstream file(filename, std::ios::binary);
	file.exceptions(std::ios::failbit);
	file.seekg(std::ios::end);
	unsigned size = file.tellg();
	m_data.resize(size + margin);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(&m_data[0]), size);
	m_pos = &m_data[0];
	m_fileEnd = m_pos + size;
	parseMHdr();
}

void Reader::parseRiff(char const* name) {
	if (!std::equal(name, name + 4, m_pos)) throw std::runtime_error("MIDI header " + std::string(name) + " not found.");
	m_pos += 4;
	unsigned size = read<uint32_t>();
	if (m_fileEnd - m_pos < size) throw std::runtime_error("Unexpected end of file within MIDI header " + std::string(name));
	m_riffEnd = m_pos + size;
}

void Reader::parseMHdr() {
	parseRiff("MHdr");
	unsigned fmt = read<uint16_t>();
	m_tracks = read<uint16_t>();
	m_division = read<uint16_t>();
	if (fmt == 2) throw std::runtime_error("MIDI format 2 not supported.");
	if (fmt == 0 && m_tracks == 1) return;  // Old single track format
	if (fmt == 1 && m_tracks > 1) return;  // New format (timing info on first track)
	throw std::runtime_error("MIDI format and track number combination is invalid.");
}

void Reader::parseMTrk() {
	m_pos = m_riffEnd;  // Skip any unread bytes of the previous
	parseRiff("MTrk");
}

Event Reader::parseEvent() {
	Event ev;
	// Event header
	{
		ev.timecode = read_varlen();
		unsigned event = read<uint8_t>();
		if (event & 0x80) {
			if (event < 0xF0) m_runningStatus = event;  // Stored for regular (0x00..0xEF)
			else if (event < 0xF8) m_runningStatus = 0;  // Cleared for System Common Category (0xF0..0xF7)
			// No-op for RealTime Category messages (0xF8..0xFF)
		} else {
			--m_pos;  // The "event" we read is actually part of the content so put it back
			if (m_runningStatus == 0) throw std::runtime_error("Invalid MIDI file (no stored running status available)");
			event = m_runningStatus;
		}
		ev.type = static_cast<Event::Type>(event & 0xF0);
		ev.channel = event & 0x0F;
	}

	if (!m_runningStatus) ev.arg1 = read<uint8_t>();  // Everything except System Common take one argument
	unsigned tmp;
	switch (ev.type) {
	case Event::NOTE_ON:
	case Event::NOTE_OFF:
	case Event::NOTE_AFTERTOUCH:
	case Event::CONTROLLER:
	case Event::PITCH_BEND:
		ev.arg2 = read<uint8_t>();
		break;
	case Event::PROGRAM_CHANGE:
	case Event::CHANNEL_AFTERTOUCH:
		break;  // No arg2 for these
	case Event::SPECIAL:  // Special category (system exclusive or meta event)
		tmp = read_varlen();  // data size
		ev.begin = m_pos;
		m_pos += tmp;
		ev.end = m_pos;
		break;
	}
	return ev;
}

#if 0
{
	switch (ev.arg1) {
	  case META_TEXT: { // Text Event
		const std::string sect_pfx = "[section ";
		// Lyrics are hidden here, only [text] are orders
		if (data[0] != '[') m_lyric = data;
		else if (!data.compare(0, sect_pfx.length(), sect_pfx)) {// [section verse_1]
			std::string sect_name = data.substr(sect_pfx.length(), data.length()-sect_pfx.length()-1);
			if (sect_name != "big_rock_ending") {
				bool space = true;
				for (std::string::const_iterator it = sect_name.begin(); it != sect_name.end(); ++it) {
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
	  case META_SEQNAME: // Sequence or Track Name
		track.name = data;
		break;
	  case META_LYRIC: // Lyric Text
		m_lyric = data;
		break;
	  case META_ENDOFTRACK: // End of Track
		end = true;
		break;
	  case META_TEMPO: // Tempo Setting
		if (data.size() != 3) throw std::runtime_error("Invalid tempo change event");
		add_tempo_change(miditime, static_cast<unsigned char>(data[0]) << 16 | static_cast<unsigned char>(data[1]) << 8 | static_cast<unsigned char>(data[2])); break;
	  case META_TIMESIGNATURE: // Time Signature
		if (data.size() != 4) throw std::runtime_error("Invalid time signature event");
		break;
	}
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
	if (begin != end) oss << " data='" << std::string(begin, end) << "'";  // TODO: filter non-printable
	oss << '\n';
	std::cerr << oss.str();
}

