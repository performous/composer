
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

PitchVis::PitchVis(QString const& filename, QWidget *parent, int visId)
	: QThread(parent), mutex(), fileName(filename), duration(), moreAvailable(), quit(),
	  cancelled(), restart(), m_x1(), m_y1(), m_x2(), m_y2(), m_visId(visId), condition()
{
	start(); // Launch the thread
}

void PitchVis::stop()
{
	QMutexLocker locker(&mutex);
	quit = true;
	condition.wakeOne();
}

void PitchVis::cancel()
{
	QMutexLocker locker(&mutex);
	cancelled = true;
}

void PitchVis::run()
{
	bool analyzingSuccess = false;
	try {
		// Initialize FFmpeg decoding
		std::string file(fileName.toLocal8Bit().data(), fileName.toLocal8Bit().size());
		FFmpeg mpeg(file);
		{
			QMutexLocker locker(&mutex);
			paths.clear();
			position = 0.0;
			duration = mpeg.duration(); // Estimation
		}
		unsigned rate = mpeg.audioQueue.getRate();
		unsigned channels = mpeg.audioQueue.getChannels();
		if (channels == 0) throw std::runtime_error("No audio channels found");
		std::vector<Analyzer> analyzers(channels, Analyzer(rate, ""));
		// Process the entire song
		std::vector<float> data;
		data.reserve((duration + 1.0) * rate * channels);
		unsigned x = 0;
		while (mpeg.audioQueue.output(data)) {
			// Process as much as can be processed at this point
			while (data.size() / channels - x >= analyzers[0].processSize()) {
				// Pitch detection
				for (unsigned ch = 0; ch < channels; ++ch) {
					analyzers[ch].process(da::step_iterator<float>(&data[x * channels + ch], channels));
				}
				x += analyzers[0].processStep();
				// Update progress and check for quit flag
				QMutexLocker locker(&mutex);
				if (quit) return;
				else if (cancelled) break;
				double t = analyzers[0].getTime();
				position = t;
				duration = std::max(duration, t + 0.01);
			}
		}
		// DEBUG: std::ofstream("audio.raw", std::ios::binary).write(reinterpret_cast<char*>(&data[0]), data.size() * sizeof(float));
		// Filter the analyzer output data into QPainterPaths.
		std::vector<Analyzer::Moments::const_iterator> mit(channels), mend(channels);
		for (unsigned ch = 0; ch < channels; ++ch) {
			Analyzer::Moments const& moments = analyzers[ch].getMoments();
			mit[ch] = moments.begin();
			mend[ch] = moments.end();
		}
		while (mit[0] != mend[0]) {
			for (unsigned ch = 0; ch < channels; ++mit[ch++]) {
				Moment::Tones const& tones = mit[ch]->m_tones;  // Take tones then move forward the iterator
				for (Moment::Tones::const_iterator it2 = tones.begin(), it2end = tones.end(); it2 != it2end; ++it2) {
					if (it2->prev) continue;  // The tone doesn't begin at this moment, skip
					// Copy the linked list into vector for easier access and calculate max level
					std::vector<Tone const*> tones;
					for (Tone const* n = &*it2; n; n = n->next) { tones.push_back(n); }
					if (tones.size() < 3) continue;  // Too short tone, ignored
					PitchPath path(ch);
					double score = 0.0;
					Analyzer::Moments::const_iterator momit = mit[ch];
					// Store path used for rendering
					for (unsigned i = 0; i < tones.size(); ++i, ++momit) {
						float t = momit->m_time;
						float n = scale.getNote(tones[i]->freq);
						float level = level2dB(tones[i]->level);
						score += tones[i]->level;
						path.fragments.push_back(PitchFragment(t, n, level));
					}
					QMutexLocker locker(&mutex);
					if (score > 1.0) paths.push_back(path);
				}
			}
		}
		analyzingSuccess = true;

	} catch (std::exception& e) {
		std::cerr << std::string("Error loading audio: ") + e.what() + '\n' << std::flush;
	}
	{
		QMutexLocker locker(&mutex);
		moreAvailable = true;
		position = duration;
	}

	// Start the renderer loop
	if (analyzingSuccess) renderer();
}

void PitchVis::paint(int x1, int y1, int x2, int y2)
{
	QMutexLocker locker(&mutex);
	m_x1 = x1; m_y1 = y1;
	m_x2 = x2; m_y2 = y2;

	// Wake the thread
	restart = true;
	condition.wakeOne();
}

void PitchVis::renderer() {
	forever {
		int x1, x2, y1, y2;
		{
			QMutexLocker locker(&mutex);
			if (quit) return;
			x1 = m_x1, x2 = m_x2, y1 = m_y1, y2 = m_y2;
		}

		// Rendering
		// QImage allows drawing in non-main/non-GUI thread
		QImage image(x2-x1, y2-y1, QImage::Format_ARGB32_Premultiplied);
		NoteGraphWidget *widget = qobject_cast<NoteGraphWidget*>(parent());
		if (!widget) continue;
		QSettings settings; // Default QSettings parameters given in main()
		bool aa = settings.value("anti-aliasing", true).toBool();

		{
			QPainter painter(&image);
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			if (aa) painter.setRenderHint(QPainter::Antialiasing);
			// Fill the background, otherwise the image will have all kinds of carbage
			painter.fillRect(image.rect(), QColor(0,0,0,0));

			QPen pen;
			pen.setWidth(8);
			pen.setCapStyle(Qt::RoundCap);

			PitchVis::Paths const& paths = getPaths();
			for (PitchVis::Paths::const_iterator it = paths.begin(), itend = paths.end(); it != itend; ++it) {
				PitchPath::Fragments const& fragments = it->fragments;
				int oldx, oldy;
				// Only render paths in view
				if (widget->s2px(fragments.back().time) < x1) continue;
				else if (widget->s2px(fragments.front().time) > x2) break;
				// Iterate through the path points
				for (PitchPath::Fragments::const_iterator it2 = fragments.begin(), it2end = fragments.end(); it2 != it2end; ++it2) {
					// TODO: Take y-size into account (change also the paint calls in NoteGraphWidget)
					int x = widget->s2px(it2->time) - x1;
					int y = widget->n2px(it2->note);
					if (m_visId == 0)
						pen.setColor(QColor(32 + 64 * it->channel, clamp<int>(127 + it2->level, 32, 255), 32, 128));
					else
						pen.setColor(QColor(clamp<int>(127 + it2->level, 32, 255), 32, 32 + 32 * it->channel, 100));
					painter.setPen(pen);
					if (it2 != fragments.begin()) painter.drawLine(oldx, oldy, x, y);
					oldx = x; oldy = y;
				}
			}
		}

		// Send the image
		// This is actually delivered by the reciever's event loop thread, and not called directly from here
		emit renderedImage(image, QPoint(x1, y1), m_visId);

		mutex.lock();
		// If nothing to do, sleep here
		if (!restart) condition.wait(&mutex);
		restart = false;
		mutex.unlock();
	}
}

int PitchVis::guessNote(double begin, double end, int note) {
	const unsigned scoreSz = 48;
	double score[scoreSz] = {};
	if (note >= 0 || note < 48) score[note] = 10.0;  // Slightly prefer the current note
	// Score against paths
	for (PitchVis::Paths::const_iterator it = paths.begin(), itend = paths.end(); it != itend; ++it) {
		PitchPath::Fragments const& fragments = it->fragments;
		// Discard paths completely outside the window
		if (fragments.back().time < begin) continue;
		if (fragments.front().time > end) break;
		for (PitchPath::Fragments::const_iterator it2 = fragments.begin(), it2end = fragments.end(); it2 != it2end; ++it2) {
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

