#include "pitchvis.hh"

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

PitchVis::PitchVis(std::string const& filename): step(1024), height(768) {
	std::vector<float> data;
	try { readVec(filename, data); } catch(std::exception& e) { std::cerr << e.what() << std::endl; return; }
	unsigned width = data.size() / step;
	img.resize(width * height);
	Analyzer analyzer(44100, "");
	QProgressDialog progress(tr("Analyzing pitch data..."), tr("&Abort"), 0, width, this);
	progress.setWindowModality(Qt::WindowModal);

	for (unsigned x = 0; x < width; ++x) {
		progress.setValue(x);
		if (progress.wasCanceled()) return;

		analyzer.input(data.begin() + x * step, data.begin() + (x + 1) * step);
		analyzer.process();
		Analyzer::Peaks peaks = analyzer.getPeaks();
		for (unsigned i = 0; i < peaks.size(); ++i) {
			unsigned y = freq2px(peaks[i].freq);
			if (y == 0 || y >= height - 1) continue;
			float value = 0.003 * (level2dB(peaks[i].level) + 80.0);
			if (value <= 0.0) continue;
			Pixel p(0.0f, 0.0f, value);
			(*this)(x, y) += p;
			p.r *= 0.5;
			p.g *= 0.5;
			p.b *= 0.5;
			(*this)(x, y + 1) += p;
			(*this)(x, y - 1) += p;
		}
		Analyzer::Tones tones = analyzer.getTones();
		unsigned int i = 0;
		for (Analyzer::Tones::const_iterator it = tones.begin(), itend = tones.end(); it != itend && i < 3; ++it) {
			if (it->age < Tone::MINAGE) continue;
			unsigned y = freq2px(it->freqSlow);
			if (y == 0 || y >= height - 1) continue;
			float value = 0.003 * (level2dB(it->levelSlow) + 80.0);
			if (value <= 0.0) continue;
			Pixel p(value, value, value);
			switch(i) {
				case 0:
					p.r = 0.0; p.g = 1.0; p.b = 0.0;
					break;
				case 1:
					p.r = 0.15; p.g = 0.15; p.b = 0.0;
					break;
				case 2:
					p.r = 0.05; p.g = 0.05; p.b = 0.0;
					break;
			}
			(*this)(x, y) = p;
			for(int j = -3 ; j < 4 ; ++j) {
				if(y + j < 0 || y + j >= height) continue;
				(*this)(x, y + j) = p;
			}
			++i;
		}
	}
	progress.setValue(width);
}

unsigned PitchVis::freq2px(double freq) const { return note2px(scale.getNote(freq)); }
unsigned PitchVis::note2px(double note) const { return height - static_cast<unsigned>(16.0 * note); }
double PitchVis::px2note(unsigned px) const { return (height - px) / 16.0; }
unsigned PitchVis::time2px(double t) const { return t * 44100.0 / step; }
double PitchVis::px2time(double px) const { return px / 44100.0 * step; }

