#include "pitchvis.hh"

#include "pitch.hh"
#include "ffmpeg.hh"
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

PitchVis::PitchVis(std::string const& filename, QWidget *parent): QWidget(parent), step(512), height(768) {
	QProgressDialog progress(tr("Initializing audio decoding..."), tr("&Abort"), 0, 1, this);
	progress.setWindowModality(Qt::WindowModal);
	progress.setValue(0);

	// Initialize FFmpeg decoding
	FFmpeg mpeg(false, true, filename, 44100);
	while(std::isnan(mpeg.duration())); // Wait for ffmpeg to be ready
	usleep(1000000); // Wait some more

	unsigned width = mpeg.duration() * 44100 / step;
	img.resize(width * height);
	Analyzer analyzer(44100, "");

	progress.setMaximum(width);
	progress.setLabelText(tr("Analyzing pitch data..."));
	for (unsigned x = 0; x < width; ++x) {
		progress.setValue(x);
		if (progress.wasCanceled()) return;

		// Get decoded samples from ffmpeg
		std::vector<float> data(step*2);
		mpeg.audioQueue(&*data.begin(), &*data.end(), x * step * 2);

		// Sample iterators for getting only one channel
		da::step_iterator<float> beginIt(&*data.begin(), 2);
		da::step_iterator<float> endIt(&*data.end(), 2);

		// Analyze
		analyzer.input(beginIt, endIt);
		analyzer.process();
		Analyzer::Peaks peaks = analyzer.getPeaks();
		for (unsigned i = 0; i < peaks.size(); ++i) {
			unsigned y = freq2px(peaks[i].freq);
			if (y == 0 || y >= height - 1) continue;
			float value = 0.003 * (level2dB(peaks[i].level) + 80.0);
			if (value <= 0.0) continue;
			Pixel p(0.0f, 0.0f, value);
			pixel(x, y) += p;
			p.r *= 0.5;
			p.g *= 0.5;
			p.b *= 0.5;
			pixel(x, y + 1) += p;
			pixel(x, y - 1) += p;
		}
		Analyzer::Tones tones = analyzer.getTones();
		unsigned int i = 0;
		for (Analyzer::Tones::const_iterator it = tones.begin(), itend = tones.end(); it != itend && i < 3; ++it) {
			unsigned y = freq2px(it->freq);
			if (y == 0 || y >= height - 1) continue;
			float value = 0.003 * (level2dB(it->level) + 80.0);
			if (value <= 0.0) continue;
			Pixel p(0.0f, value, 0.0f);
			for (int j = int(y) - 2; j <= int(y) + 2; ++j) pixel(x, j) += p;
		}
	}
	progress.setValue(width);
}

unsigned PitchVis::freq2px(double freq) const { return note2px(scale.getNote(freq)); }
unsigned PitchVis::note2px(double note) const { return height - static_cast<unsigned>(16.0 * note); }
double PitchVis::px2note(unsigned px) const { return (height - px) / 16.0; }
unsigned PitchVis::time2px(double t) const { return t * 44100.0 / step; }
double PitchVis::px2time(double px) const { return px / 44100.0 * step; }

