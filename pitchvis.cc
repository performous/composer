
#include "pitchvis.hh"
#include "pitch.hh"
#include "ffmpeg.hh"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <QProgressDialog>
#include <QLabel>

PitchVis::PitchVis(QString const& filename, QWidget *parent)
	: QWidget(parent), QThread(), mutex(), pixelsPerSecond(), fileName(filename), moreAvailable(), cancelled(), curX(), m_width()
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
		unsigned scale = 8;
		unsigned int rate = 44100;
		Analyzer analyzer(rate, "");
		{
			unsigned step = 1024;
			pixelsPerSecond = scale * rate / step;
			// Initialize FFmpeg decoding
			FFmpeg mpeg(fileName.toStdString(), rate);
			setWidth(scale * mpeg.duration() * rate / step); // Estimation
			curX = 0;
			for (std::vector<float> data(step*2); mpeg.audioQueue(&*data.begin(), &*data.end(), curX * step * 2); ++curX) {
				// Mix stereo into mono
				for (unsigned i = 0; i < step; ++i) data[i] = 0.5 * (data[2*i] + data[2*i + 1]);
				// Process
				analyzer.input(&data[0], &data[step]);
				analyzer.process();
				if (cancelled) return;
				std::fill(data.begin(), data.end(), 0.0f);
			}
		}
		// Filter the analyzer output data into QPainterPaths.
		Analyzer::Moments const& moments = analyzer.getMoments();
		curX = 0;
		setWidth(scale * moments.size());
		for (Analyzer::Moments::const_iterator it = moments.begin(), itend = moments.end(); it != itend && curX < width(); ++it, ++curX) {
			moreAvailable = true;
			Moment::Tones const& tones = it->m_tones;
			for (Moment::Tones::const_iterator it2 = tones.begin(), it2end = tones.end(); it2 != it2end; ++it2) {
				if (it2->prev) continue;  // The tone doesn't begin at this moment, skip
				// Copy the linked list into vector for easier access and calculate max level
				std::vector<Tone const*> tones;
				double lmax = 0.0;
				for (Tone const* n = &*it2; n; n = n->next) { tones.push_back(n); lmax = std::max(lmax, n->level); }
				if (tones.size() < 8) continue;  // Too short or weak tone, ignored
				QPainterPath path;
				path.lineTo(20, 30);
				// Render
				for (unsigned i = 0; i < tones.size(); ++i) {
					//float value = 0.006 * (level2dB(tones[i]->level) + 60.0);
					unsigned x = scale * (curX + i);
					unsigned y = freq2px(tones[i]->freq);
					if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
				}
				QMutexLocker locker(&mutex);
				paths.push_back(path);
				moreAvailable = true;
			}
		}

	} catch (std::exception& e) {
		std::cerr << std::string("Error loading audio: ") + e.what() + '\n' << std::flush;
	}
	QMutexLocker locker(&mutex);
	moreAvailable = true;
	curX = width();
}

unsigned PitchVis::freq2px(double freq) const { return note2px(scale.getNote(freq)); }
/* static */ unsigned PitchVis::note2px(double tone) { return height - static_cast<unsigned>(16.0 * tone); }
/* static */ double PitchVis::px2note(unsigned px) { return (height - px) / 16.0; }
unsigned PitchVis::time2px(double t) const { return t * pixelsPerSecond; }
double PitchVis::px2time(double px) const { return px / pixelsPerSecond; }

