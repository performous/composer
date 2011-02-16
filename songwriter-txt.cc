#include "songwriter.hh"
#include "config.hh"
#include "util.hh"
#include <QTextStream>


void UltraStarTXTWriter::writeTXT() {
	QFile f(path + "/notes.txt");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
	out.setCodec("UTF-8");
	out << "#TITLE:" << (s.title.isEmpty() ? "Unknown" : s.title) << '\n';
	out << "#ARTIST:" << (s.artist.isEmpty() ? "Unknown" : s.artist) << '\n';

	if (!s.edition.isEmpty()) out << "#EDITION:" << s.edition << '\n';
	if (!s.genre.isEmpty()) out << "#GENRE:" << s.genre << '\n';
	out << "#CREATOR:" << (s.creator.isEmpty() ? PACKAGE : s.creator) << '\n';
	if (!s.cover.isEmpty()) out << "#COVER:" << s.cover << '\n';
//	if (!s.music["background"].second.isEmpty()) out << "#MP3:" << s.music["background"].second << '\n'; // FIXME: remove full path
//	if (!s.music["vocals"].isEmpty()) out << "#VOCALS:" << s.music["vocals"] << '\n'; // FIXME: remove full path
	if (!s.video.isEmpty()) out << "#VIDEO:" << s.video << '\n';
	if (!s.background.isEmpty()) out << "#BACKGROUND:" << s.background << '\n';
	out << "#START:" << s.start << '\n';
	out << "#VIDEOGAP:" << s.videoGap << '\n';
	out << "#PREVIEWSTART:" << s.preview_start << '\n';
	out << "#RELATIVE:" << "0" << '\n'; // TODO: Check that this outputs in a good format
	//if (!s.gap.isEmpty()) out << "#GAP:" << s.gap << '\n';
	if (s.bpm != 0) out << "#BPM:" << s.bpm << '\n';
	if (!s.language.isEmpty()) out << "#LANGUAGE:" << s.language << '\n';
	if (!s.year.isEmpty()) out << "#YEAR:" << s.year << '\n';

	// Loop through the notes
	const Notes& notes = s.getVocalTrack().notes;
	for (int i = 0; i < notes.size(); ++i) {
		const Note& n = notes[i];
		if (n.type == Note::SLEEP) continue;

		// Put sleeps before new phrases
		if (i > 0 && n.lineBreak)
			out << "- " << sec2dur(notes[i-1].end) << ' ' << sec2dur(notes[i-1].end)+1 << '\n';

		// Output the note
		out << (char)n.type << ' '<< sec2dur(n.begin) << ' ' << sec2dur(n.length()) << ' ' << n.note << ' ' << n.syllable << '\n';
	}

	out << "E"; // End indicator
}


int UltraStarTXTWriter::sec2dur(double sec) {
	return round(tempo / 60.0 * sec * 4);
}
