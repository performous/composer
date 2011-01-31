
#include "pitchvis.hh"
#include "pitch.hh"
#include "ffmpeg.hh"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <QPainter>
#include <QProgressDialog>
#include <QLabel>
#include <QSettings>

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
		unsigned pixScale = 8;
		unsigned rate = 44100;
		Analyzer analyzer(rate, "");
		{
			unsigned step = 1024;
			pixelsPerSecond = pixScale * rate / step;
			// Initialize FFmpeg decoding
			FFmpeg mpeg(fileName.toStdString(), rate);
			setWidth(pixScale * mpeg.duration() * rate / step); // Estimation
			unsigned x = 0;
			for (std::vector<float> data(step*2); mpeg.audioQueue(&*data.begin(), &*data.end(), x * step * 2); ++x) {
				curX = pixScale * x;
				// Mix stereo into mono
				for (unsigned j = 0; j < step; ++j) data[j] = 0.5 * (data[2*j] + data[2*j + 1]);
				// Process
				analyzer.input(&data[0], &data[step]);
				analyzer.process();
				if (cancelled) return;
				std::fill(data.begin(), data.end(), 0.0f);
			}
		}
		// Filter the analyzer output data into QPainterPaths.
		Analyzer::Moments const& moments = analyzer.getMoments();
		setWidth(pixScale * moments.size());
		curX = 0;
		for (Analyzer::Moments::const_iterator it = moments.begin(), itend = moments.end(); it != itend; ++it) {
			curX += pixScale;
			Moment::Tones const& tones = it->m_tones;
			for (Moment::Tones::const_iterator it2 = tones.begin(), it2end = tones.end(); it2 != it2end; ++it2) {
				if (it2->prev) continue;  // The tone doesn't begin at this moment, skip
				// Copy the linked list into vector for easier access and calculate max level
				std::vector<Tone const*> tones;
				for (Tone const* n = &*it2; n; n = n->next) { tones.push_back(n); }
				if (tones.size() < 3) continue;  // Too short tone, ignored
				PitchPath path;
				Analyzer::Moments::const_iterator momit = it;
				// Render
				for (unsigned i = 0; i < tones.size(); ++i, ++momit) {
					float t = momit->m_time;
					float n = scale.getNote(tones[i]->freq);
					float level = level2dB(tones[i]->level);
					path.push_back(PitchFragment(t, n, level));
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

void PitchVis::paint(QPaintDevice* widget, int x1, int x2) {
	QSettings settings; // Default QSettings parameters given in main()
	bool aa = settings.value("anti-aliasing", true).toBool();
	QMutexLocker locker(&mutex);
	QPainter painter;
	painter.begin(widget);
	if (aa) painter.setRenderHint(QPainter::Antialiasing);
	QPen pen;
	pen.setWidth(8);
	pen.setCapStyle(Qt::RoundCap);
	PitchVis::Paths const& paths = getPaths();
	for (PitchVis::Paths::const_iterator it = paths.begin(), itend = paths.end(); it != itend; ++it) {
		int oldx, oldy;
		// Only render paths in view
		if (time2px(it->back().time) < x1) continue;
		else if (time2px(it->front().time) > x2) break;
		// Iterate through the path points
		for (PitchPath::const_iterator it2 = it->begin(), it2end = it->end(); it2 != it2end; ++it2) {
			int x = time2px(it2->time);
			int y = note2px(it2->note);
			pen.setColor(QColor(32, clamp<int>(127 + it2->level, 32, 255), 32));
			painter.setPen(pen);
			if (it2 != it->begin()) painter.drawLine(oldx, oldy, x, y);
			oldx = x; oldy = y;
		}
	}
	painter.end();
}

int PitchVis::guessNote(double begin, double end, int note) {
	const unsigned scoreSz = 48;
	double score[scoreSz] = {};
	if (note >= 0 || note < 48) score[note] = 10.0;  // Slightly prefer the current note
	// Score against paths
	for (PitchVis::Paths::const_iterator it = paths.begin(), itend = paths.end(); it != itend; ++it) {
		for (PitchPath::const_iterator it2 = it->begin(), it2end = it->end(); it2 != it2end; ++it2) {
			if (it2->time < begin) continue;
			if (it2->time > end) break;
			unsigned n = round(it2->note);
			if (n < scoreSz) score[n] += 100 + it2->level;
		}
	}
	// Return the idx with best score
	return std::max_element(score + 1, score + scoreSz) - score;
}

unsigned PitchVis::freq2px(double freq) const { return note2px(scale.getNote(freq)); }
/* static */ unsigned PitchVis::note2px(double tone) { return height - static_cast<unsigned>(16.0 * tone); }
/* static */ double PitchVis::px2note(unsigned px) { return (height - px) / 16.0; }
unsigned PitchVis::time2px(double t) const { return t * pixelsPerSecond; }
double PitchVis::px2time(double px) const { return px / pixelsPerSecond; }

