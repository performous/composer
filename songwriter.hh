#include "song.hh"
#include <QDir>
#include <fstream>

struct SongWriter
{
	SongWriter(const Song& s_, const QString& path_)
		: s(s_), path(path_) { QDir dir; dir.mkpath(path_); }
	const Song& s;
	QString path;
};

struct SingStarXMLWriter: public SongWriter
{
	SingStarXMLWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_), tempo(160) { writeXML(); }
private:
	void writeXML();
	int sec2dur(double sec);
	int tempo;
};

struct UltraStarTXTWriter: public SongWriter
{
	UltraStarTXTWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_) { writeTXT(); }
private:
	void writeTXT();
};

struct FoFMIDIWriter: public SongWriter
{
	FoFMIDIWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_) {  writeINI(); writeMIDI(); }

private:
	void writeINI();
	void writeMIDI();
};
