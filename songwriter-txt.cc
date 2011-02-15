#include "songwriter.hh"
#include "config.hh"
#include <QTextStream>

void UltraStarTXTWriter::writeTXT() {
	QFile f(path + "/notes.txt");
	if (!f.open(QFile::WriteOnly | QFile::Truncate))
		throw std::runtime_error("Couldn't open target file");

	QTextStream out(&f);
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

	throw std::runtime_error("TXT export is not completely implemented.");
}
