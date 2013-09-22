#include "songwriter.hh"
#include "config.hh"
#include "util.hh"
#include <QTextStream>


void UltraStarTXTWriter::writeTXT() const {
	QFile f(path + "/notes.txt");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	// Figure out song filename
	QString mp3 = "NO_SONG";
	if (!s.music["EDITOR"].isEmpty()) {
		QFileInfo finfo(s.music["EDITOR"]);
		mp3 = finfo.fileName();
	}

	QTextStream out(&f);
	out.setCodec("UTF-8");

	// Required fields
	out << "#TITLE:" << (s.title.isEmpty() ? "Unknown" : s.title) << '\n';
	out << "#ARTIST:" << (s.artist.isEmpty() ? "Unknown" : s.artist) << '\n';
	out << "#MP3:" << mp3 << '\n';
	out << "#BPM:" << tempo << '\n';
	out << "#GAP:" << "0" << '\n'; // Time to first lyric in milliseconds

	// Additional fields
	out << "#CREATOR:" << (s.creator.isEmpty() ? PACKAGE : s.creator) << '\n';
	if (!s.genre.isEmpty()) out << "#GENRE:" << s.genre << '\n';
	if (!s.year.isEmpty()) out << "#YEAR:" << s.year << '\n';
	if (!s.language.isEmpty()) out << "#LANGUAGE:" << s.language << '\n';
	if (!s.edition.isEmpty()) out << "#EDITION:" << s.edition << '\n';
	if (!s.cover.isEmpty()) out << "#COVER:" << s.cover << '\n';
	if (!s.background.isEmpty()) out << "#BACKGROUND:" << s.background << '\n';
	if (!s.video.isEmpty()) out << "#VIDEO:" << s.video << '\n';

	// The following are not useful, at least for now
	//if (s.videoGap != 0) out << "#VIDEOGAP:" << s.videoGap << '\n';
	//if (start != 0) out << "#START:" << s.start << '\n';
	//out << "#RELATIVE:" << "no" << '\n';
	//out << "#PREVIEWSTART:" << s.preview_start << '\n';
	//if (!s.music["vocals"].isEmpty()) out << "#VOCALS:" << s.music["vocals"] << '\n'; // FIXME: remove full path

	// Loop through the notes
	const Notes& notes = s.getVocalTrack().notes;
	for (int i = 0; i < notes.size(); ++i) {
		const Note& n = notes[i];
		if (n.type == Note::SLEEP) continue;

		// Put sleeps before new phrases
		if (i > 0 && n.lineBreak)
			out << "- " << sec2dur(notes[i-1].end) << '\n';

		// Output the note
		out << (char)n.type << ' '<< sec2dur(n.begin) << ' ' << sec2dur(n.length()) << ' ' << n.note << ' ' << n.syllable << '\n';
	}

	out << "E"; // End indicator
}


int UltraStarTXTWriter::sec2dur(double sec) const {
	return round(tempo / 60.0 * sec * 4);
}
