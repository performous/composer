#pragma once

#include "notes.hh"
#include "util.hh"
#include <QWidget>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QPainterPath>
#include <cmath>
#include <string>
#include <vector>

class QAudioDecoder;

struct PitchFragment {
	float time, note, level;  // seconds, MIDI note, dB
	PitchFragment(float time, float note, float level): time(time), note(note), level(level) {}
};

struct PitchPath {
	typedef std::vector<PitchFragment> Fragments;
	Fragments fragments;
	unsigned channel;
	PitchPath(unsigned channel): channel(channel) {}
};

class NoteGraphWidget;

class PitchVis: public QThread
{
	Q_OBJECT
public:
	typedef std::vector<PitchPath> Paths;
	QMutex mutex;

	PitchVis(QString const& filename, QWidget *parent = NULL, int visId = 0);
	~PitchVis() { stop(); wait(); }

	void stop();
	void cancel();
	void paint(int x1, int y1, int x2, int y2);
	bool newDataAvailable() const { return moreAvailable; }
	double getProgress() const { return position / duration; }
	double getDuration() const { return duration; }
	int guessNote(double begin, double end, int initial);

signals:
	void renderedImage(const QImage &image, const QPoint &position, int visId);

public slots:
	void bufferReady() { start(); }

protected:
	void run(); // Thread runs here

private:
	void renderer();
	Paths const& getPaths() { moreAvailable = false; return paths; }

	QAudioDecoder* decoder;
	MusicalScale scale;
	QString fileName;
	Paths paths;
	double position;  ///< Position while analyzing
	double duration;  ///< Song duration (or estimation while analyzing)
	bool moreAvailable;
	bool quit;  ///< Quit at the frst chance
	bool cancelled;  ///< Cancel analyzing, but use what was done so far
	bool restart;  ///< Should we start the rendering again?
	QWaitCondition condition;
	int m_x1, m_y1, m_x2, m_y2;
	int m_visId;
};

