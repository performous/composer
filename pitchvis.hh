#pragma once

#include "util.hh"
#include <QWidget>
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

class PitchVis: public QWidget {
	std::vector<Pixel> img;
public:
	const std::size_t height;
	Pixel& operator()(std::size_t x, std::size_t y) { return img[x * height + y]; }
	std::size_t width() const { return img.size() / height; }
	PitchVis(std::string const& filename);
};

