
#include "notegraphwidget.hh"
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
	: QWidget(parent), QThread(), mutex(), fileName(filename), moreAvailable(), cancelled()
{
	start(); // Launch the thread
}

void PitchVis::run()
{
	try {
		unsigned rate = 44100;
		Analyzer analyzer(rate, "");
		// Process the entire song
		{
			unsigned step = 1024;
			// Initialize FFmpeg decoding
			FFmpeg mpeg(fileName.toStdString(), rate);
			{
				QMutexLocker locker(&mutex);
				paths.clear();
				position = 0.0;
				duration = mpeg.duration(); // Estimation
			}
			unsigned x = 0;
			for (std::vector<float> data(step*2); mpeg.audioQueue(&*data.begin(), &*data.end(), x * step * 2); ++x, std::fill(data.begin(), data.end(), 0.0f)) {
				// Mix stereo into mono (FIXME: FFmpeg converting into stereo and then converting back here is really stupid)
				for (unsigned j = 0; j < step; ++j) data[j] = 0.5 * (data[2*j] + data[2*j + 1]);
				// Process
				analyzer.input(&data[0], &data[step]);
				analyzer.process();
				QMutexLocker locker(&mutex);
				if (cancelled) return;
				Analyzer::Moments const& moments = analyzer.getMoments();
				if (!moments.empty()) {
					double t = moments.back().time();
					position = t;
					duration = std::max(duration, t + 0.01);
				}
			}
		}
		// Filter the analyzer output data into QPainterPaths.
		Analyzer::Moments const& moments = analyzer.getMoments();
		if (moments.empty()) return;
		for (Analyzer::Moments::const_iterator it = moments.begin(), itend = moments.end(); it != itend; ++it) {
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
	position = duration;
}

void PitchVis::paint(NoteGraphWidget* widget, int x1, int x2) {
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
		if (widget->s2px(it->back().time) < x1) continue;
		else if (widget->s2px(it->front().time) > x2) break;
		// Iterate through the path points
		for (PitchPath::const_iterator it2 = it->begin(), it2end = it->end(); it2 != it2end; ++it2) {
			int x = widget->s2px(it2->time);
			int y = widget->n2px(it2->note);
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
		// Discard paths completely outside the window
		if (it->back().time < begin) continue;
		if (it->front().time > end) break;
		for (PitchPath::const_iterator it2 = it->begin(), it2end = it->end(); it2 != it2end; ++it2) {
			// Discard path points outside the window
			if (it2->time < begin) continue;
			if (it2->time > end) break;
			unsigned n = round(it2->note);
			if (n < scoreSz) score[n] += 100 + it2->level;
		}
	}
	// Return the idx with best score
	return std::max_element(score + 1, score + scoreSz) - score;
}

