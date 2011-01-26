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

struct Pixel {
	float r,g,b,a;
	Pixel(float r, float g, float b, float a = 1.0f): r(r), g(g), b(b), a(a) {}
	Pixel(): r(), g(), b(), a(1.0f) {}
	static unsigned char conv(float c, float a) {
		return static_cast<unsigned char>(0.5 + 255.0 * clamp(a * std::sqrt(c))); // sqrt(c) is gamma correction
	}
	unsigned rgba() const {
		unsigned char red = conv(r, a);
		unsigned char green = conv(g, a);
		unsigned char blue = conv(b, a);
		unsigned char alpha = conv(1.0f, a);
		return alpha << 24 | red << 16 | green << 8 | blue;
	}
	float& operator[](unsigned idx) { return (&r)[idx]; }
	Pixel& operator+=(Pixel const& pix) {
		r += pix.r;
		g += pix.g;
		b += pix.b;
		a += pix.a;
		return *this;
	}
};

class PitchVis: public QWidget, public QThread {
public:
	static const std::size_t height = 768;
	typedef std::vector<QPainterPath> Paths;
	QMutex mutex;

	PitchVis(QString const& filename, QWidget *parent = NULL);
	~PitchVis() { stop(); wait(); }

	void run(); // Thread runs here
	void stop() { cancelled = true; }

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

