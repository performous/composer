namespace mid {

	typedef unsigned char uint8_t;
	typedef std::vector<uint8_t> Data;

	struct Riff {
		Riff(uint8_t* begin, uint8_t* end): begin(begin), end(end) {}
		uint8_t* begin;
		uint8_t* end;
		std::string name() const { return std::string(begin, begin + 4); }
	};

	class Reader {
	public:
		Reader(uint8_t*& ptr): riff(ptr, ptr + 8), pos(ptr + 4) {
			ptr = riff.end += read<uint32_t>();  // Read content size
		}
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
			do {
				if (++len > 4) throw std::runtime_error("Too long varlen sequence in RIFF chunk " + riff.name());
				requireBytes(1);
				value = (value << 7) | (*pos++ & 0x7F);
			} while (c & 0x80);
			return value;
		}
		
	private:
		Riff riff;
		uint8_t* pos;
	};

	struct Command {
		unsigned timecode;
		int command;
		int channel;
		int note;
	};

}
