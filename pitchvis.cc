#include "pitchvis.hh"

#include "notes.hh"
#include "pitch.hh"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <QProgressDialog>

/** Read the entire file into a vector of some type.
The file needs to be in the machine-native endianess. **/
template <typename T> void readVec(std::string const& filename, std::vector<T>& c) {
	std::ifstream f(filename.c_str(), std::ios::binary);
	if (!f) throw std::runtime_error("Cannot open " + filename);
	f.seekg(0, std::ios::end);
	unsigned size = f.tellg();
	if (size > 1e+8) throw std::runtime_error("File too large: " + filename);
	f.seekg(0);
	c.resize(size / sizeof(T));
	f.read(reinterpret_cast<char*>(&c[0]), c.size() * sizeof(T));
	if (!f) throw std::runtime_error("Error reading " + filename);
}

PitchVis::PitchVis(std::string const& filename): height(1024) {
	std::vector<float> data;
	try { readVec(filename, data); } catch(std::exception& e) { std::cerr << e.what() << std::endl; return; }
	unsigned step = 1024;
	unsigned width = data.size() / step;
	img.resize(width * height, Pixel(0.5f, 0.5f, 0.5f, 1.0f));
	Analyzer analyzer(44100, "");
	MusicalScale scale;
	QProgressDialog progress(tr("Analyzing and rendering pitch data..."), tr("&Abort"), 0, width, this);
	progress.setWindowModality(Qt::WindowModal);

	for (unsigned x = 0; x < width; ++x) {
		progress.setValue(x);
		if (progress.wasCanceled()) return;

		analyzer.input(data.begin() + x * step, data.begin() + (x + 1) * step);
		analyzer.process();
		Analyzer::Peaks peaks = analyzer.getPeaks();
		for (unsigned i = 0; i < peaks.size(); ++i) {
			for (unsigned div = 1; div < 4; ++div) {
				unsigned y = height - static_cast<unsigned>(16.0 * (scale.getNote(peaks[i].freq * div)));
				if (y == 0 || y >= height - 1) continue;
				float value = 0.003 * (magn2dB(peaks[i].magnitude) + 80.0);
				if (value <= 0.0) continue;
				if (div > 1) value *= -0.5;
				Pixel p(value, value, value);
				(*this)(x, y) += p;
				p.r *= 0.5;
				p.g *= 0.5;
				p.b *= 0.5;
				(*this)(x, y + 1) += p;
				(*this)(x, y - 1) += p;
			}
		}
	}
	progress.setValue(width);
}

