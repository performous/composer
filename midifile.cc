#include "midifile.hh"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>

using namespace midifile;

Reader::Reader(char const* filename) {
	// Read the entire file into a buffer
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
}

bool Reader::startTrack() {
	m_pos = m_riffEnd;  // Jump to the end of the current riff
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

Writer::Writer(unsigned fmt, unsigned tracks, unsigned division) {
	beginRiff("MThd");
	if (fmt == 0 && tracks != 1) throw std::logic_error("Format 0 MIDI must have exactly one track");
	if (fmt == 1 && tracks < 2) throw std::logic_error("Format 1 MIDI must have a separate timing track");
	if (division == 0) throw std::logic_error("Division must be set to a positive value");
	write<2>(fmt);
	write<2>(tracks);
	write<2>(division);
}

void Writer::save(char const* filename) {
	endRiff();
	std::ofstream f(filename, std::ios::binary);
	f.write(reinterpret_cast<char const*>(&m_data[0]), m_data.size());
}

void Writer::startTrack() {
	endRiff();
	beginRiff("MTrk");
}

void Writer::writeEvent(Event const& ev) {
	write_varlen(ev.timecode);
	if (ev.type & ~0xF0 || ev.type < 0x80) throw std::logic_error("Invalid MIDI event type");
	if (ev.type & ~0x0F) throw std::logic_error("Invalid MIDI channel number");
	write<1>(ev.type | ev.channel);
	if (ev.type != Event::SPECIAL || ev.channel >= 8) write<1>(ev.arg1);  // Everything except System Common takes one argument
	switch (ev.type) {
	case Event::NOTE_ON:
	case Event::NOTE_OFF:
	case Event::NOTE_AFTERTOUCH:
	case Event::CONTROLLER:
	case Event::PITCH_BEND:
		write<1>(ev.arg2);
		break;
	case Event::PROGRAM_CHANGE:
	case Event::CHANNEL_AFTERTOUCH:
		if (ev.arg2 != 0) throw std::logic_error("MIDI event with non-zero arg2 for event that only takes one arg");
		break;  // No arg2 for these
	case Event::SPECIAL:  // Special category (system exclusive or meta event)
		write_varlen(ev.end - ev.begin);  // data size
		std::copy(ev.begin, ev.end, std::back_inserter(m_data));
		break;
	}	
}

void Writer::beginRiff(char const* name) {
	m_riffBegin = m_data.size();
	m_data.resize(m_riffBegin + 8);  // Add space for header
	std::copy(name, name + 4, m_data.begin() + m_riffBegin);  // Set RIFF name
}

void Writer::endRiff() {
	unsigned size = m_data.size() - m_riffBegin;
	if (size == 0) return;  // Nothing to close
	size -= 8;
	m_data[m_riffBegin + 4] = size >> 24;
	m_data[m_riffBegin + 5] = size >> 16;
	m_data[m_riffBegin + 6] = size >> 8;
	m_data[m_riffBegin + 7] = size;
	m_riffBegin = m_data.size();
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

