#pragma once

#include "types.hh"
#include <stdexcept>
#include <string>
#include <vector>

namespace mid {
	typedef uint8_t value_type;
	typedef uint8_t* iterator;
	typedef uint8_t const* const_iterator;

	struct Event {
		enum Type {
			NOTE_OFF = 0x80,
			NOTE_ON = 0x90,
			NOTE_AFTERTOUCH = 0xA0,
			CONTROLLER = 0xB0,
			PROGRAM_CHANGE = 0xC0,
			CHANNEL_AFTERTOUCH = 0xD0,
			PITCH_BEND = 0xE0,
			SPECIAL = 0xF0
		};
		enum Meta {
			META_SEQNUMBER = 0,
			META_TEXT,
			META_COPYRIGHT,
			META_SEQNAME,
			META_INSTRNAME,
			META_LYRIC,
			META_MARKERTEXT,
			META_CUEPOINT,
			META_CHPREFIX = 0x20,
			META_ENDOFTRACK = 0x2F,
			META_TEMPO = 0x51,
			META_SMTPEOFFSET = 0x54,
			META_TIMESIGNATURE = 0x58,
			META_KEYSIGNATURE = 0x59,
			META_SEQUENCERSPECIFIC = 0x7F
		};
		static char const* metaName(Meta meta);
		unsigned timecode;  // Relative to previous event
		Type type;
		unsigned channel;
		unsigned arg1;
		unsigned arg2;
		const_iterator begin, end;  // Data belonging to the event (including any terminating 0x7F)
		Meta getMeta() const { return static_cast<Meta>(arg1); }
		std::string getDataStr() const { return std::string(begin, end); }
		void print() const;  ///< Prints to std::cout (for debugging only)
		Event(): timecode(), type(), channel(), arg1(), arg2(), begin(), end() {}
	};

	class Reader {
	public:
		static const unsigned margin = 20;  ///< The minimum number of extra bytes required after the end of the buffer for more efficient processing
		Reader(char const* filename);
		unsigned numTracks() const { return m_tracks; }
		unsigned Tracks() const { return m_tracks; }
		/// Switch to the next track, returns false if end of file reached (all track processed)
		bool nextTrack();
		/// Parse the next event of the current track, returns false if at the end of track
		bool parseEvent(Event& ev);
		unsigned getDivision() const { return m_division; }
	private:
		void parseMThd();
		void parseMTrk();
		void parseRiff(char const* name);
		template <unsigned N> unsigned read() {
			unsigned value = 0;
			for (unsigned i = N - 1; i < N; --i) value |= *m_pos++ << (8 * i);
			return value;
		}

		unsigned read_varlen() {
			unsigned value = 0;
			unsigned len = 0;
			unsigned c = 0;
			do {
				if (++len > 4) throw std::runtime_error("MIDI parse error (too long varlen)");
				c = *m_pos++;
				value = (value << 7) | (c & 0x7F);
			} while (c & 0x80);
			return value;
		}
		std::vector<value_type> m_data;  ///< Only used when reading a file, not used if the user provides a buffer
		const_iterator m_pos;
		const_iterator m_riffEnd;
		const_iterator m_fileEnd;
		unsigned m_tracks;
		unsigned m_division;
		unsigned m_runningStatus;
	};

	class Writer {
	public:
	private:
		void writeMThd(unsigned tracks, unsigned division) {
			beginRiff("MThd");
			write<2>(1); // fmt	1 (multitrack)
			write<2>(tracks);
			write<2>(division);
			endRiff();
		}
		void writeMTrk() {
			beginRiff("MTrk");
		}
		void beginRiff(char const* name) {
			m_riffBegin = m_data.size();
			m_data.resize(m_riffBegin + 8);  // Add space for header
			std::copy(name, name + 4, m_data.begin() + m_riffBegin);  // Set RIFF name
		}
		void endRiff() {
			unsigned size = m_data.size() - 8 - m_riffBegin;
			m_data[m_riffBegin + 4] = size >> 24;
			m_data[m_riffBegin + 5] = size >> 16;
			m_data[m_riffBegin + 6] = size >> 8;
			m_data[m_riffBegin + 7] = size;
		}
		template <unsigned N> void write(unsigned value) {
			for (unsigned i = N - 1; i < N; --i) m_data.push_back(value >> (8 * i));
		}
		void write_varlen(unsigned value) {
			if (value >= 0x10000000U) throw std::logic_error("Value cannot be MIDI varlen encoded");
			if (value >= 0x200000U) m_data.push_back(0x80 | (value >> 21));
			if (value >= 0x4000U) m_data.push_back(0x80 | (value >> 14));
			if (value >= 0x80U) m_data.push_back(0x80 | (value >> 7));
			m_data.push_back(value & 0x7F);
		}
		std::vector<value_type> m_data;
		unsigned m_riffBegin;
	};
}

