#pragma once

#include "notes.hh"
#include "util.hh"
#include <QWidget>
#include <QThread>
#include <QMutex>
#include <cmath>
#include <string>
#include <vector>

struct Pixel {
	float r,g,b,a;
	Pixel(float r, float g, float b, float a = 1.0f): r(r), g(g), b(b), a(a) {}
	Pixel(): r(), g(), b(), a(1.0f) {}
	static unsigned char conv(float c, float a) {
		return static_cast<unsigned char>(255.0 * a * std::sqrt(c)); // sqrt(c) is gamma correction
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
		r = clamp(r + pix.r);
		g = clamp(g + pix.g);
		b = clamp(b + pix.b);
		a = clamp(a + pix.a);
		return *this;
	}
};

class PitchVis: public QWidget, public QThread {
	unsigned step;
	std::vector<Pixel> img;
	MusicalScale scale;
	QString fileName;
	QImage image;
	bool moreAvailable;
	QMutex mutex;
public:
	const std::size_t height;

	PitchVis(QString const& filename, QWidget *parent = NULL);

	void run(); // Thread runs here

	QImage getImage() { QMutexLocker locker(&mutex); moreAvailable = false; return image; }
	bool newDataAvailable() const { return moreAvailable; }

	Pixel& pixel(std::size_t x, std::size_t y) { return img[x * height + y]; }
	std::size_t width() const { return img.size() / height; }

	unsigned freq2px(double freq) const;
	unsigned note2px(double note) const;
	double px2note(unsigned px) const;
	unsigned time2px(double t) const;
	double px2time(double px) const;
};

