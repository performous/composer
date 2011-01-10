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

PitchVis::PitchVis(std::string const& filename): height(512) {
	std::vector<float> data;
	try { readVec(filename, data); } catch(std::exception& e) { std::cerr << e.what() << std::endl; return; }
	unsigned step = 1024;
	unsigned width = data.size() / step;
	img.resize(width * height);
	Analyzer analyzer(44100, "");
	MusicalScale scale;
	QProgressDialog progress("Analyzing and rendering pitch...", "Abort", 0, width, this);
		progress.setWindowModality(Qt::WindowModal);

	for (unsigned x = 0; x < width; ++x) {
		progress.setValue(x);
		if (progress.wasCanceled()) return;

		analyzer.input(data.begin() + x * step, data.begin() + (x + 1) * step);
		analyzer.process();
		Analyzer::Peaks peaks = analyzer.getPeaks();
		for (unsigned i = 0; i < peaks.size(); ++i) {
			unsigned y = static_cast<unsigned>(8.0 * (scale.getNote(peaks[i].freq)));
			if (y >= height) continue;
			float value = magn2dB(peaks[i].magnitude) + 80.0;
			if (value > 0.0) (*this)(x, y).r += 0.01 * value;
		}
	}
	progress.setValue(width);
}

