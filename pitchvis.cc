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

PitchVis::PitchVis(std::string const& filename): step(512), height(768) {
#if 0 // Change this to 1 to use old raw loading
	std::vector<float> data;
	try { readVec("music.raw", data); } catch(std::exception& e) { std::cerr << e.what() << std::endl; return; }
#else
	// FIXME: This is rather horrible and needs clean-up
	const unsigned buffersize = 512;
	std::vector<float> datas(buffersize);
	FFmpeg mpeg(false, true, filename, 44100);
	uint64_t pos = 0;
	while(std::isnan(mpeg.duration())); // Wait for ffmpeg to be ready
	usleep(3000000); // Wait some more
	while (!mpeg.audioQueue.eof(pos)) {
		// TODO: Should probably analyze buffer-by-buffer
		mpeg.audioQueue(&datas[pos], &*datas.end(), pos);
		datas.resize(datas.size() + buffersize);
		pos += buffersize;
		usleep(10); // HACKHACK
	}
	// We want only one channel
	std::vector<float> data;
	data.reserve(datas.size()/2);
	for (uint64_t i = 0; i < datas.size() / 2; ++i) {
		data.push_back(datas[i*2]);
	}
#endif
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
			Pixel p(0.0f, value, 0.0f);
			for (int j = int(y) - 2; j <= int(y) + 2; ++j) (*this)(x, j) += p;
		}
	}
	progress.setValue(width);
}

unsigned PitchVis::freq2px(double freq) const { return note2px(scale.getNote(freq)); }
unsigned PitchVis::note2px(double note) const { return height - static_cast<unsigned>(16.0 * note); }
double PitchVis::px2note(unsigned px) const { return (height - px) / 16.0; }
unsigned PitchVis::time2px(double t) const { return t * 44100.0 / step; }
double PitchVis::px2time(double px) const { return px / 44100.0 * step; }

