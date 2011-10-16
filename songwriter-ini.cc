#include "songwriter.hh"

#include "midifile.hh"
#include "util.hh"
#include <QTextStream>

void FoFMIDIWriter::writeMIDI() const {
	Notes const& notes = s.getVocalTrack().notes;
	if (notes.empty()) throw std::runtime_error("No notes");
	double tempo = s.bpm > 0 ? s.bpm : 120.0;
	unsigned division = 256;  // Allow for very precise timing
	unsigned endtc = round(tempo / 60.0 * division * notes.back().end);
	midifile::Writer writer(1, 2, division);
	using midifile::Event;
	writer.startTrack();
	Event ev;
	ev.type = Event::SPECIAL;
	ev.channel = 0x0F;
	unsigned char buf[16];
	ev.begin = ev.end = buf;
	// Write tempo info
	ev.arg1 = Event::META_TEMPO;
	ev.end = ev.begin + 3;
	unsigned val = 6e+7 / tempo;  // Microseconds per beat
	buf[0] = val >> 16;
	buf[1] = val >> 8;
	buf[2] = val;
	writer.writeEvent(ev);
	// TODO: write Performous Composer and Title= & Artist= like FoF lyric converter does (note: it does on PART VOCALS but timing track seems more appropriate)
	// End the timing track
	ev.arg1 = Event::META_ENDOFTRACK;
	writer.writeEvent(ev);
	// Vocals track begins
	writer.startTrack();
	// Write track name
	ev.arg1 = Event::META_SEQNAME;
	std::string partvocals = "PART VOCALS";
	std::copy(partvocals.begin(), partvocals.end(), buf);
	ev.end = ev.begin + partvocals.size();
	writer.writeEvent(ev);
	ev.begin = ev.end = NULL;
	// Write notes
	bool sentenceOn = false;  // Sentence note 105 not playing
	unsigned timecode = 0;
	for (Notes::const_iterator it = notes.begin(), itend = notes.end(); it != itend; ++it) {
		ev.timecode = round(tempo / 60.0 * division * it->begin) - timecode;
		timecode += ev.timecode;
		if (it->lineBreak) {
			ev.type = Event::NOTE_ON;
			ev.channel = 0;
			ev.arg1 = 105;   // Special note value for starting a new sentence
			// End the previous sentence (if not at the beginning)
			if (sentenceOn) {
				ev.arg2 = 0;
				writer.writeEvent(ev);
				ev.timecode = 0;  // Reset time for the next event
			}  // Note OFF
			sentenceOn = true;
			// Begin a new one
			ev.arg2 = 127; writer.writeEvent(ev);  // Note ON
		}
		// Write lyric
		QByteArray bytes = it->syllable.toUtf8();
		ev.type = Event::SPECIAL;
		ev.channel = 0x0F;
		ev.arg1 = Event::META_LYRIC;
		ev.begin = reinterpret_cast<unsigned char*>(bytes.data());
		ev.end = ev.begin + bytes.size();
		writer.writeEvent(ev);
		ev.end = ev.begin = NULL;
		// Prepare for writing note on/off events
		ev.timecode = 0;  // Same timecode as the lyric
		ev.type = Event::NOTE_ON;
		ev.channel = 0;
		// Note begin
		ev.arg1 = 36 + it->note;  // FoF format uses offset for note values :(
		ev.arg2 = 127;  // Note ON
		writer.writeEvent(ev);
		// Note end
		ev.timecode = round(tempo / 60.0 * division * it->end) - timecode;
		timecode += ev.timecode;
		ev.arg2 = 0;  // Note OFF
		writer.writeEvent(ev);
	}
	ev.timecode = 0;
	// Terminate the sentence note
	if (sentenceOn) {
		ev.type = Event::NOTE_ON;
		ev.channel = 0;
		ev.arg1 = 105;
		ev.arg2 = 0; writer.writeEvent(ev);  // Note OFF
	}
	// Terminate the vocal track
	ev.type = Event::SPECIAL;
	ev.channel = 0x0F;
	ev.arg1 = Event::META_ENDOFTRACK;
	writer.writeEvent(ev);
	// Write to file
	QByteArray name = (path + "/notes.mid").toLocal8Bit();
	writer.save(std::string(name.data(), name.size()).c_str());
}

void FoFMIDIWriter::writeINI() const {
	QFile f(path + "/song.ini");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
	out.setCodec("UTF-8");

	out << "[song]\n";
	out << "name = " << (s.title.isEmpty() ? "Unknown" : s.title) << '\n';
	out << "artist = " << (s.artist.isEmpty() ? "Unknown" : s.artist) << '\n';
	if (!s.genre.isEmpty()) out << "genre = " << s.genre << '\n';
	if (!s.year.isEmpty()) out << "year = " << s.year << '\n';
}

