#pragma once

#include <string>
#include <vector>

struct Pixel {
	float r,g,b,a;
	Pixel(float r, float g, float b, float a = 1.0f): r(r), g(g), b(b), a(a) {}
	Pixel(): r(), g(), b(), a(1.0f) {}
	unsigned rgba() {
		unsigned ret = 0;
		for (unsigned i = 0; i < 4; ++i) {
			float c = (*this)[i];
			unsigned char byte = static_cast<unsigned char>(0.5 + 255.0 * c * c); // c² is gamma correction
			ret |= byte << (i * 8);
		}
		return ret;
	}
	float& operator[](unsigned idx) { return (&r)[idx]; }
};

class PitchVis {
	std::size_t height;
	std::vector<Pixel> img;
public:
	Pixel& operator()(std::size_t x, std::size_t y) { return img[x * height + y]; }
	std::size_t width() const { return img.size() / height; }
	PitchVis(std::string const& filename);
};

