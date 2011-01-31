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

class PitchVis: public QWidget, public QThread {
public:
	static const std::size_t height = 768;
	typedef std::vector<PitchPath> Paths;
	QMutex mutex;

	PitchVis(QString const& filename, QWidget *parent = NULL);
	~PitchVis() { stop(); wait(); }

	void run(); // Thread runs here
	void stop() { cancelled = true; }
	void paint(QPaintDevice* widget, int x1, int x2);
	Paths const& getPaths() { moreAvailable = false; return paths; }
	bool newDataAvailable() const { return moreAvailable; }
	int getXValue() const { return curX; }

	std::size_t width() const { return m_width; }

	unsigned freq2px(double freq) const;
	static unsigned note2px(double note);
	static double px2note(unsigned px);
	unsigned time2px(double t) const;
	double px2time(double px) const;
private:
	MusicalScale scale;
	QString fileName;
	Paths paths;
	double pixelsPerSecond;
	bool moreAvailable;
	bool cancelled;
	int curX;
	std::size_t m_width;
	void setWidth(std::size_t w);
};

