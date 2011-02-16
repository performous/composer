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
		: SongWriter(s_, path_), tempo(s_.bpm > 0 ? s_.bpm : 180), res("Semiquaver") { writeXML(); }
private:
	void writeXML();
	int sec2dur(double sec);
	double dur2sec(int ts);
	int tempo;
	QString res;
};

struct UltraStarTXTWriter: public SongWriter
{
	UltraStarTXTWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_), tempo(s_.bpm > 0 ? s_.bpm : 180) { writeTXT(); }
private:
	void writeTXT();
	int sec2dur(double sec);
	int tempo;
};

struct FoFMIDIWriter: public SongWriter
{
	FoFMIDIWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_) {  writeINI(); writeMIDI(); }

private:
	void writeINI();
	void writeMIDI();
};
