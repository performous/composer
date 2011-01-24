
#include "pitchvis.hh"
#include "pitch.hh"
#include "ffmpeg.hh"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <QProgressDialog>
#include <QLabel>

PitchVis::PitchVis(QString const& filename, QWidget *parent)
	: QWidget(parent), QThread(), mutex(), step(1024), height(768), fileName(filename), moreAvailable(), cancelled(), curX(), m_width()
{
	start(); // Launch the thread
}

void PitchVis::setWidth(std::size_t w) {
	m_width = w;
	QLabel *ngw = qobject_cast<QLabel*>(QWidget::parent());
	if (ngw) ngw->setFixedSize(w, height);
}

void PitchVis::run()
{
	try {

		unsigned int rate = 44100;
		Analyzer analyzer(rate, "");
		{
			// Initialize FFmpeg decoding
			FFmpeg mpeg(false, true, fileName.toStdString(), rate);
			while(std::isnan(mpeg.duration()) || std::isinf(mpeg.duration())) {
				if (mpeg.terminating()) // Check if FFMPEG has failed
					return;
				msleep(1000); // Wait for ffmpeg to be ready
			}
			msleep(1000); // Wait some more
			setWidth(mpeg.duration() * rate / step); // Estimation

			curX = 0;
			for (std::vector<float> data(step*2); mpeg.audioQueue(&*data.begin(), &*data.end(), curX * step * 2); ++curX) {
				// Mix stereo into mono
				for (unsigned i = 0; i < step; ++i) data[i] = 0.5 * (data[2*i] + data[2*i + 1]);
				// Process
				analyzer.input(&data[0], &data[step]);
				analyzer.process();
				if (cancelled) return;
			}
		}
		// Draw, TODO: Draw somewhere else (on demand) without a huge bitmap
		Analyzer::Moments const& moments = analyzer.getMoments();
		setWidth(moments.size());
		// Create image
		{
			QMutexLocker locker(&mutex);
			image = QImage(width(), height, QImage::Format_ARGB32_Premultiplied);
			memset(image.bits(), 0, width() * height * 4);
		}
		curX = 0;
		for (Analyzer::Moments::const_iterator it = moments.begin(), itend = moments.end(); it != itend && curX < width(); ++it, ++curX) {
			QMutexLocker locker(&mutex);
			unsigned* rgba = reinterpret_cast<unsigned*>(image.bits());
			moreAvailable = true;
			Moment::Tones const& tones = it->m_tones;
			for (Moment::Tones::const_iterator it2 = tones.begin(), it2end = tones.end(); it2 != it2end; ++it2) {
				if (it2->prev) continue;  // The tone doesn't begin at this moment, skip
				// Copy the linked list into vector for easier access and calculate max level
				std::vector<Tone const*> tones;
				double lmax = 0.0;
				for (Tone const* n = &*it2; n; n = n->next) { tones.push_back(n); lmax = std::max(lmax, n->level); }
				if (tones.size() < 5) continue;  // Too short or weak tone, ignored
				std::cout << tones.size() << ", " << lmax << std::endl;
				// Render
				for (unsigned i = 0; i < tones.size(); ++i) {
					unsigned x = curX + i;
					if (x >= width()) throw std::logic_error("Tone past the end of moments");
					float value = 0.006 * (level2dB(tones[i]->level) + 60.0);
					if (value <= 0.0) continue;
					unsigned int pix = Pixel(0.0f, 1.0f, 0.0f, value).rgba();
					unsigned y = freq2px(tones[i]->freq);
					for (int j = std::max<int>(0, int(y) - 2), jend = std::min<int>(height, int(y) + 3); j < jend; ++j) rgba[j * width() + x] = pix;
				}
			}
		}

	} catch (std::exception& e) {
		std::cerr << std::string("Error loading audio: ") + e.what() + '\n' << std::flush;
	}
	QMutexLocker locker(&mutex);
	moreAvailable = true;
	curX = width();
		
#if 0
		for (unsigned x = 0; x < width; ++x) {
			if (cancelled) return;
			curX = x;

			// Peaks
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

		}
#endif
}

unsigned PitchVis::freq2px(double freq) const { return note2px(scale.getNote(freq)); }
unsigned PitchVis::note2px(double tone) const { return height - static_cast<unsigned>(16.0 * tone); }
double PitchVis::px2note(unsigned px) const { return (height - px) / 16.0; }
unsigned PitchVis::time2px(double t) const { return t * 44100.0 / step; }
double PitchVis::px2time(double px) const { return px / 44100.0 * step; }

