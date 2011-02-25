#include "songwriter.hh"

#include "midifile.hh"
#include "util.hh"
#include <QTextStream>

void FoFMIDIWriter::writeMIDI() const {
	Notes const& notes = s.getVocalTrack().notes;
	if (notes.empty()) throw std::runtime_error("No notes");
	double tempo = s.bpm > 0 ? s.bpm : 120.0;
	unsigned division = 64;  // Allow for very precise timing
	unsigned endtc = round(tempo / 60.0 * division * notes.back().end);
	midifile::Writer writer(1, 2, division);
	using midifile::Event;
	writer.startTrack();
	Event ev;
	ev.type = Event::SPECIAL;
	ev.channel = 0x0F;
	unsigned char buf[16];
	ev.begin = ev.end = buf;
	if (tempo != 120.0) {   // MIDI defaults to 120 BPM
		ev.arg1 = Event::META_TEMPO;
		ev.end = ev.begin + 3;
		unsigned val = 6e+7 / tempo;  // Microseconds per beat
		buf[0] = val >> 16;
		buf[1] = val >> 8;
		buf[2] = val;
		writer.writeEvent(ev);
	}
	// TODO: write Performous Composer and title= & artist= like EoF does
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
	// Write notes
	unsigned timecode = 0;
	for (Notes::const_iterator it = notes.begin(), itend = notes.end(); it != itend; ++it) {
		// Lyric
		QByteArray bytes = it->syllable.toUtf8();
		ev.timecode = round(tempo / 60.0 * division * it->begin) - timecode;
		timecode += ev.timecode;
		ev.type = Event::SPECIAL;
		ev.channel = 0x0F;
		ev.arg1 = Event::META_LYRIC;
		ev.begin = reinterpret_cast<unsigned char*>(bytes.data());
		ev.end = ev.begin + bytes.size();
		writer.writeEvent(ev);
		// Note begin
		ev.channel = 0;
		ev.timecode = 0;  // Same timecode as the lyric
		ev.type = Event::NOTE_ON;
		ev.arg1 = it->note;
		ev.arg2 = 64;
		writer.writeEvent(ev);
		// Note end
		ev.timecode = round(tempo / 60.0 * division * it->end) - timecode;
		timecode += ev.timecode;
		ev.type = Event::NOTE_OFF;
		ev.arg2 = 0;
		writer.writeEvent(ev);
	}
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

