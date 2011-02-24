#include "midifile.hh"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace mid;

Reader::Reader(char const* filename) {
	{
		std::ifstream file(filename, std::ios::binary);
		if (!file.is_open()) throw std::runtime_error("Unable to open " + std::string(filename));
		file.exceptions(std::ios::failbit);
		file.seekg(0, std::ios::end);
		unsigned size = file.tellg();
		m_data.resize(size + margin);
		file.seekg(0);
		file.read(reinterpret_cast<char*>(&m_data[0]), size);
		m_pos = &m_data[0];
		m_fileEnd = m_pos + size;
	}
	parseMThd();
	parseMTrk();
}

bool Reader::nextTrack() {
	m_pos = m_riffEnd;
	if (m_pos == m_fileEnd) return false;
	parseMTrk();
	return true;
}

void Reader::parseRiff(char const* name) {
	if (!std::equal(name, name + 4, m_pos)) throw std::runtime_error("MIDI header " + std::string(name) + " not found, instead found " + std::string(m_pos, m_pos + 4));
	m_pos += 4;
	unsigned size = read<4>();
	if (m_fileEnd - m_pos < size) throw std::runtime_error("Unexpected end of file within MIDI header " + std::string(name));
	m_riffEnd = m_pos + size;
}

void Reader::parseMThd() {
	parseRiff("MThd");
	unsigned fmt = read<2>();
	m_tracks = read<2>();
	m_division = read<2>();
	if (fmt == 2) throw std::runtime_error("MIDI format 2 not supported.");
	if (fmt == 0 && m_tracks == 1) return;  // Old single track format
	if (fmt == 1 && m_tracks > 1) return;  // New format (timing info on first track)
	throw std::runtime_error("MIDI format and track number combination is invalid.");
	m_runningStatus = 0;
}

void Reader::parseMTrk() {
	m_pos = m_riffEnd;  // Skip any unread bytes of the previous
	parseRiff("MTrk");
}

bool Reader::parseEvent(Event& ev) {
	if (m_pos == m_riffEnd) return false;
	ev = Event();
	// Event header
	{
		ev.timecode = read_varlen();
		unsigned event = read<1>();
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
	if (ev.type != Event::SPECIAL || ev.channel >= 8) ev.arg1 = read<1>();  // Everything except System Common takes one argument
	unsigned tmp;
	switch (ev.type) {
	case Event::NOTE_ON:
	case Event::NOTE_OFF:
	case Event::NOTE_AFTERTOUCH:
	case Event::CONTROLLER:
	case Event::PITCH_BEND:
		ev.arg2 = read<1>();
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
	if (m_pos > m_riffEnd) throw std::runtime_error("MIDI event went past the end of Mtrk");
	return true;
}

// Debugging facilities follow

namespace {
	std::string escapeString(const_iterator begin, const_iterator end) {
		std::string ret;
		while (begin != end) {
			value_type ch = *begin++;
			if (ch >= 0x20 && ch < 0x7F) { ret += ch; continue; }
			// Print escaped character
			std::ostringstream oss;
			oss << '<' << std::hex << std::setfill('0') << std::setw(2) << int(ch) << '>';
			ret += oss.str();
		}
		return ret;
	}
}

void Event::print() const {
	std::ostringstream oss;
	oss << "Midi event:" << std::setw(12) << timecode << "  ";
	switch (type) {
	  case NOTE_OFF: oss << "note off   note=" << arg1 << " velocity=" << arg2; break;
	  case NOTE_ON: oss << "note on   note=" << arg1 << " velocity=" << arg2; break;
	  case NOTE_AFTERTOUCH: oss << "aftertouch pitch=" << arg1 << " value=" << arg2; break;
	  case CONTROLLER: oss << "controller num=" << arg1 << " value=" << arg2; break;
	  case PROGRAM_CHANGE: oss << "program change num=" << arg1; break;
	  case CHANNEL_AFTERTOUCH: oss << "channel aftertouch value =" << arg1; break;
	  case PITCH_BEND: oss << "pitch bend value=" << (arg2 << 8 | arg1); break;
	  case SPECIAL:
		if (channel == 0x0F) oss << "meta <" << metaName(static_cast<Event::Meta>(arg1)) << ">";
		else if (channel >= 0x08) oss << "unknown realtime category message, arg1=" << arg1;
		else oss << "system common message";
		break;
	}
	if (begin != end) oss << " data='" << escapeString(begin, end) << "'";  // TODO: filter non-printable
	oss << '\n';
	std::cerr << oss.str();
}

char const* Event::metaName(Meta meta) {
	switch (meta) {
	case META_SEQNUMBER: return "sequence number";
	case META_TEXT: return "text";
	case META_COPYRIGHT: return "copyright";
	case META_SEQNAME: return "sequence or track name";
	case META_INSTRNAME: return "instrument name";
	case META_LYRIC: return "lyric";
	case META_MARKERTEXT: return "marker text";
	case META_CUEPOINT: return "cue point";
	case META_CHPREFIX: return "channel prefix";
	case META_ENDOFTRACK: return "end of track";
	case META_TEMPO: return "tempo change";
	case META_SMTPEOFFSET: return "SMTPE offset";
	case META_TIMESIGNATURE: return "time signature";
	case META_KEYSIGNATURE: return "key signature";
	case META_SEQUENCERSPECIFIC: return "sequencer specific";
	}
	return "unknown";
}

