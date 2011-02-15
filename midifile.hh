#pragma once

#include "types.hh"
#include <stdexcept>
#include <string>

namespace mid {

#if 0
	class Reader {
	public:
		typedef uint8_t const* iterator;
		Reader(iterator begin, iterator end): m_beginFile(begin), m_endFile(end) {
		parseFile() {
			parseRiff();
			riff.end = begin + 8 + read<uint32_t>();  // Read content size
		}

			Riff(iterator begin, iterator end): begin(begin), end(end) {
				riff.end = begin + 8 + read<uint32_t>();
			}
			uint8_t* begin;
			uint8_t* end;
			std::string name() const { return std::string(begin, begin + 4); }


		int bytesLeft() const { return riff.end - pos; }
		void requireBytes(unsigned num) const {
			if (bytesLeft() < num) throw std::runtime_error("Read past the end of RIFF chunk " + riff.name());
		}
		template <typename T> T read() {
			requireBytes(sizeof(T));
			T value = 0;
			for (int i = sizeof(T) - 1; i >= 0; --i) value |= *pos++ << (8 * i);
			return value;
		}
		uint32_t readVar() {
			unsigned value = 0;
			unsigned len = 0;
			unsigned c = 0;
			do {
				if (++len > 4) throw std::runtime_error("Too long varlen sequence in RIFF chunk " + riff.name());
				requireBytes(1);
				c = *pos++;
				value = (value << 7) | (c & 0x7F);
			} while (c & 0x80);
			return value;
		}
		
	private:
		Riff riff;
		uint8_t* pos;
	};

#endif
	struct Event {
		unsigned timecode;
		int type;
		int arg1;
		int arg2;
		void print() const;
	};
	
}

