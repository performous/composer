#pragma once

#include "notes.hh"
#include "util.hh"
#include <QWidget>
#include <QThread>
#include <QMutex>
#include <QPainterPath>
#include <cmath>
#include <string>
#include <vector>

struct PitchFragment {
	float time, note, level;  // seconds, MIDI note, dB
	PitchFragment(float time, float note, float level): time(time), note(note), level(level) {}
};

typedef std::vector<PitchFragment> PitchPath;

class NoteGraphWidget;

class PitchVis: public QWidget, public QThread {
public:
	typedef std::vector<PitchPath> Paths;
	QMutex mutex;

	PitchVis(QString const& filename, QWidget *parent = NULL);
	~PitchVis() { stop(); wait(); }

	void run(); // Thread runs here
	void stop() { cancelled = true; }
	void paint(NoteGraphWidget* widget, int x1, int x2);
	Paths const& getPaths() { moreAvailable = false; return paths; }
	bool newDataAvailable() const { return moreAvailable; }
	double getProgress() const { return position / duration; }
	double getDuration() const { return duration; }
	int guessNote(double begin, double end, int initial);

private:
	MusicalScale scale;
	QString fileName;
	Paths paths;
	double position;  ///< Position while analyzing
	double duration;  ///< Song duration (or estimation while analyzing)
	bool moreAvailable;
	bool cancelled;
};

