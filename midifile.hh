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
		unsigned timecode;  // Relative to previous event
		Type type;
		unsigned channel;
		unsigned arg1;
		unsigned arg2;
		const_iterator begin, end;  // Data belonging to the event (including any terminating 0x7F)
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
	private:
		void parseMThd();
		void parseMTrk();
		void parseRiff(char const* name);
		template <typename T> T read() {
			T value = 0;
			for (int i = sizeof(T) - 1; i >= 0; --i) value |= *m_pos++ << (8 * i);
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
	
}

