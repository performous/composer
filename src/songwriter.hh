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
	int sec2dur(double sec) const;
	double dur2sec(int ts) const;
	int tempo;
	QString res;
};

struct UltraStarTXTWriter: public SongWriter
{
	UltraStarTXTWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_), tempo(s_.bpm > 0 ? s_.bpm : 180) { writeTXT(); }
private:
	void writeTXT() const;
	int sec2dur(double sec) const;
	int tempo;
};

struct FoFMIDIWriter: public SongWriter
{
	FoFMIDIWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_) {  writeINI(); writeMIDI(); }

private:
	void writeINI() const;
	void writeMIDI() const;
};

struct LRCWriter: public SongWriter
{
	LRCWriter(const Song& s_, const QString& path_)
		: SongWriter(s_, path_) { writeLRC(); }
private:
	void writeLRC() const;
	QString sec2timestamp(double sec) const;
};

struct SMMWriter: public SongWriter
{
	SMMWriter(const Song& s_, const QString& path_)
		: SongWriter(s_,path_) { writeSMM(); }
private:
	void writeSMM() const;
	QString sec2timestamp(double sec) const;
};
