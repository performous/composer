#include "pitch.hh"

#include "libda/fft.hpp"
#include <cmath>
#include <numeric>

static const unsigned FFT_P = 11;  // FFT size setting, will use 2^FFT_P sample FFT
static const std::size_t FFT_N = 1 << FFT_P;  // FFT size in samples
static const std::size_t FFT_STEP = 512;  // Step size in samples, should be <= 0.25 * FFT_N. Low values cause high CPU usage.

// Limit the range to avoid noise and useless computation
static const double FFT_MINFREQ = 45.0;
static const double FFT_MAXFREQ = 3000.0;

Tone::Tone(): freq(), level() {
	for (std::size_t i = 0; i < MAXHARM; ++i) harmonics[i] = 0.0;
}

bool Tone::operator==(double f) const {
	return std::abs(freq / f - 1.0) < 0.03;  // Half semitone
}

Analyzer::Analyzer(double rate, std::string id):
  m_rate(rate),
  m_id(id),
  m_window(FFT_N),
  m_bufRead(0),
  m_bufWrite(0),
  m_fftLastPhase(FFT_N / 2),
  m_level(-getInf()),
  m_oldfreq(0.0)
{
  	// Hamming window
	for (size_t i=0; i < FFT_N; i++) {
		m_window[i] = 0.53836 - 0.46164 * std::cos(2.0 * M_PI * i / (FFT_N - 1));
	}
}

namespace {
	bool sqrLT(float a, float b) { return a * a < b * b; }
}

bool Analyzer::calcFFT() {
	float pcm[FFT_N];
	size_t r = m_bufRead;
	// Test if there is enough audio available
	if ((BUF_N + m_bufWrite - r) % BUF_N <= FFT_N) return false;
	// Copy audio to local buffer
	for (size_t i = 0; i < FFT_N; ++i) pcm[i] = m_buf[(r + i) % BUF_N];
	m_bufRead = (r + FFT_STEP) % BUF_N;
	// Calculate FFT
	m_fft = da::fft<FFT_P>(pcm, m_window);
	return true;
}

namespace {
	bool matchFreq(double f1, double f2) {
		return std::abs(f1 / f2 - 1.0) < 0.06;  // Semitone difference
	}
	bool matchHarm(double ff, double hf) {
		for (std::size_t n = 1; n < Tone::MAXHARM; ++n) if (matchFreq(n*ff, hf)) return true;
		return false;
	}
	struct Combo {
		double freq;
		double level;
		Combo(): freq(), level() {}
		void combine(Peak const& p) {
			freq += p.level * p.freq;  // Multiplication for weighted average
			level += p.level;
		}
		bool match(double freqOther) const { return matchFreq(freq, freqOther); }
	};
	bool operator<(Combo const& a, Combo const& b) { return a.level < b.level; }
}

void Analyzer::calcTones() {
	// Precalculated constants
	const double freqPerBin = m_rate / FFT_N;
	const double phaseStep = 2.0 * M_PI * FFT_STEP / FFT_N;
	const double normCoeff = 1.0 / FFT_N;
	// Limit frequency range of processing
	const size_t kMin = std::max(size_t(3), size_t(FFT_MINFREQ / freqPerBin));
	const size_t kMax = std::min(FFT_N / 2, size_t(FFT_MAXFREQ / freqPerBin));
	m_peaks.resize(kMax);
	std::vector<double> levels;
	// Process FFT into peaks
	for (size_t k = 1; k < kMax; ++k) {
		double level = normCoeff * std::abs(m_fft[k]);
		double phase = std::arg(m_fft[k]);
		// Use the reassignment method for calculating precise frequencies
		double delta = phase - m_fftLastPhase[k];
		m_fftLastPhase[k] = phase;
		delta -= k * phaseStep;  // Subtract the expected phase difference
		delta = remainder(delta, 2.0 * M_PI);  // Map the delta phase into +/- M_PI interval
		delta /= phaseStep;  // Calculate how much difference that makes during a step
		m_peaks[k].freqFFT = k * freqPerBin;  // Calculate the simple FFT frequency
		m_peaks[k].freq = (k + delta) * freqPerBin;  // Calculate the true frequency
		m_peaks[k].level = level;
		levels.push_back(level);
	}
	std::sort(levels.begin(), levels.end());
	double l80 = levels[levels.size() * 80 / 100];  // l80 > 80 % peaks
	double lmax = levels.back();  // Max level of peaks (note: combos will combine peaks and go higher)
	// Filter peaks and combine adjacent peaks pointing at the same frequency into one
	typedef std::vector<Combo> Combos;
	Combos combos;
	for (size_t k = kMin; k < kMax; ++k) {
		Peak const& p = m_peaks[k];
		if (p.level < l80) continue;
		if (p.freq < FFT_MINFREQ || p.freq > FFT_MAXFREQ) continue;
		// Do we need to add a new Combo (rather than using the last one)?
		if (combos.empty() || !combos.back().match(p.freq)) combos.push_back(Combo());
		combos.back().combine(p);
	}
	// Convert sum frequencies into averages
	for (Combos::iterator it = combos.begin(), end = combos.end(); it != end; ++it) it->freq /= it->level;
	// Strongest first
	std::sort(combos.rbegin(), combos.rend());
	// Keep only a reasonable amount of strongest frequencies.
	//if (combos.size() > 10) combos.resize(10);
	// Try to combine combos into tones (collections of harmonics)
	Tones tones;
	for (Combos::const_iterator it = combos.begin(), end = combos.end(); it != end; ++it) {
		if (it->level < 0.1 * lmax) break;
		Tone bestTone;
		int bestScore = 0;
		for (std::size_t div = 1; div <= Tone::MAXHARM; ++div) {
			// Assume that the *it is the div'th harmonic
			Tone t;
			double freq = it->freq / div;  // Assumed fundamental frequency
			int score = 0;
			size_t misses = 0;
			t.level = 0.0;
			for (std::size_t n = 1; n <= Tone::MAXHARM && misses < 3; ++n) {
				double hfreq = n * freq;
				bool miss = true;
				for (Combos::const_iterator harm = combos.begin(), harmend = combos.end(); harm != harmend; ++harm) {
					if (!harm->match(hfreq)) continue;  // Skip current combo if it doesn't match
					// Possible matching harmonic found, update t and scoring
					score += (n == 1 ? 3 : 2);  // Score for finding a harmonic (extra for fundamental)
					double l = harm->level;
					t.harmonics[n - 1] += l;
					t.level += l;
					t.freq += l * harm->freq / n;  // The sum of all harmonics' fundies (weighted by m)
				}
				if (miss) ++misses;
			}
			t.freq /= t.level;  // Average instead of sum
			if (score > bestScore) {
				bestScore = score;
				bestTone = t;
			}
		}
		tones.push_back(bestTone);
	}
	// Clean harmonics misdetected as fundamental
	tones.sort();
	for (Tones::iterator it = tones.begin(); it != tones.end(); ++it) {
		Tones::iterator it2 = it;
		++it2;
		while (it2 != tones.end()) {
			if (matchHarm(it->freq, it2->freq)) it2 = tones.erase(it2);
			else ++it2;
		}
	}
	temporalMerge(tones);
}

void Analyzer::temporalMerge(Tones& tones) {
	/*Tones::iterator oldit;
	for (Tones::const_iterator it = tones.begin(), itend = tones.end(); it != itend; ++it) {
		if () oldit = std::find(
	}
	// Iterate over old tones
	for (ToneSeries::reverse_iterator oldit = m_toneSeries.rbegin(); oldit != m_toneSeries.rend(); ++oldit) {
		if (
		// Try to find a matching new tone
		while (it != tones.end() && *it < *oldit) ++it;
		// If match found
		if (it != tones.end() && *it == *oldit) {
			// Merge the old tone into the new tone
			it->age = oldit->age + 1;
			it->levelSlow = 0.5 * oldit->levelSlow + 0.5 * it->level;
			it->freqSlow = 0.5 * oldit->freqSlow + 0.5 * it->freq;
		} else if (oldit->levelSlow > 1e-8) {
			// Insert a decayed version of the old tone into new tones
			Tone& t = *tones.insert(it, *oldit);
			t.level = 0.0;
			t.levelSlow *= 0.01;
		}
	}*/
	m_moments.push_back(Moment(0.0, tones));
}

void Analyzer::process() {
	// Try calculating FFT and calculate tones until no more data in input buffer
	while (calcFFT()) calcTones();
}

/*Tone const* Analyzer::findTone(double minfreq, double maxfreq) const {
	Tone const* best = NULL;
	double bestscore = -getInf();
	for (Tones::const_iterator it = m_tones.begin(); it != m_tones.end(); ++it) {
		if (it->freq > maxfreq || it->freq < minfreq || it->age < Tone::MINAGE) continue;
		double score = it->levelSlow * (400.0 - std::max(180.0, std::abs(it->freq - 300.0)));
		if (m_oldfreq != 0.0 && std::abs(it->freq/m_oldfreq - 1.0) < 0.10) score += 30.0;  // Stability is a really good thing
		if (it->harmonics[0] > 0.0) score += 10.0;  // fundamental is always good to have
		if (it->harmonics[2] > 0.0) score += 10.0;  // third harmonic is nearly always detected for vocals
		if (score > bestscore) {
			best = &*it;
			bestscore = score;
		}
	}
	m_oldfreq = (best ? best->freq : 0.0);
	return best;
}*/

Moment::Moment(double t, Tones& tones): m_time(t) {
	m_tones.swap(tones);
}

