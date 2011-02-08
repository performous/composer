#include "pitch.hh"

#include "libda/fft.hpp"
#include <cmath>
#include <numeric>

static const unsigned FFT_P = 12;  // FFT size setting, will use 2^FFT_P sample FFT
static const std::size_t FFT_N = 1 << FFT_P;  // FFT size in samples
static const std::size_t FFT_STEP = 1024;  // Step size in samples, should be <= 0.25 * FFT_N. Low values cause high CPU usage.

// Limit the range to avoid noise and useless computation
static const double FFT_MINFREQ = 45.0;
static const double FFT_MAXFREQ = 3000.0;

Tone::Tone(): freq(), level(), prev(), next() {
	for (std::size_t i = 0; i < MAXHARM; ++i) harmonics[i] = 0.0;
}

bool Tone::operator==(double f) const {
	return std::abs(freq / f - 1.0) < 0.06;  // Half semitone
}

Analyzer::Analyzer(double rate, std::string id):
  m_rate(rate),
  m_id(id),
  m_window(FFT_N),
  m_fftLastPhase(FFT_N / 2),
  m_oldfreq(0.0)
{
  	// Hamming window
	for (size_t i=0; i < FFT_N; i++) {
		m_window[i] = 0.53836 - 0.46164 * std::cos(2.0 * M_PI * i / (FFT_N - 1));
	}
}

unsigned Analyzer::processSize() const { return FFT_N; }
unsigned Analyzer::processStep() const { return FFT_STEP; }

void Analyzer::calcFFT(float* pcm) {
	m_fft = da::fft<FFT_P>(pcm, m_window);
}

namespace {
	bool sqrLT(float a, float b) { return a * a < b * b; }
	bool matchFreq(double f1, double f2) {
		return std::abs(f1 / f2 - 1.0) < 0.06;
	}
}

void Combo::combine(Peak const& p) {
	freq += p.level * p.freq;  // Multiplication for weighted average
	level += p.level;
}

bool Combo::match(double freqOther) const { return matchFreq(freq, freqOther); }

void Analyzer::calcTones() {
	// Precalculated constants
	const double freqPerBin = m_rate / FFT_N;
	const double phaseStep = 2.0 * M_PI * FFT_STEP / FFT_N;
	const double normCoeff = 1.0 / FFT_N;
	// Limit frequency range of processing
	const size_t kMin = std::max(size_t(3), size_t(FFT_MINFREQ / freqPerBin));
	const size_t kMax = std::min(FFT_N / 2, size_t(FFT_MAXFREQ / freqPerBin));
	m_peaks.resize(kMax);
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
	}
	// Filter peaks and combine adjacent peaks pointing at the same frequency into one
	typedef std::vector<Combo> Combos;
	Combos combos;
	for (size_t k = kMin; k < kMax; ++k) {
		Peak const& p = m_peaks[k];
		bool ok = p.level > 1e-3 && p.freq >= FFT_MINFREQ && p.freq <= FFT_MAXFREQ && std::abs(p.freqFFT - p.freq) < freqPerBin;
		if (!ok) continue;
		// Do we need to add a new Combo (rather than using the last one)?
		if (combos.empty() || !combos.back().match(p.freq)) combos.push_back(Combo());
		combos.back().combine(p);
	}
	// Convert sum frequencies into averages
	for (Combos::iterator it = combos.begin(), itend = combos.end(); it != itend; ++it) {
		it->freq /= it->level;
	}
	// Only keep a reasonable amount of strongest combos
	std::sort(combos.begin(), combos.end(), Combo::cmpByLevel);
	if (combos.size() > 30) combos.resize(30);
	// The order may not be strictly correct, fix it...
	std::sort(combos.begin(), combos.end(), Combo::cmpByFreq);
	// Try to combine combos into tones (collections of harmonics)
	Tones tones;
	for (Combos::const_iterator it = combos.begin(), itend = combos.end(); it != itend; ++it) {
		Tone tone;
		for (int div = 1; div <= 3; ++div) {  // Missing fundamental processing
			int plausibleHarmonics = 0;
			double basefreq = it->freq / div;
			if (basefreq < FFT_MINFREQ) break;  // Do not try any lower frequencies
			for (Combos::const_iterator harm = it; harm != itend; ++harm) {
				double ratio = harm->freq / basefreq;
				unsigned n = round(ratio);
				if (n > Tone::MAXHARM) break; // No more harmonics can be found
				if (std::abs(ratio - n) > 0.03) continue; // Frequency doesn't match
				if (n == 0) throw std::logic_error("combos not correctly sorted");
				if (n % div != 0) ++plausibleHarmonics;
				double l = harm->level;
				tone.harmonics[n - 1] += l;
				tone.level += l;
				tone.freq += l * harm->freq / n;  // The sum of all harmonics' fundies (weighted by l)
			}
			if (div > 1 && plausibleHarmonics < 3) continue;  // Not a proper missing fundamental
			tone.freq /= tone.level;  // Average instead of sum
			tones.push_back(tone);
		}
	}
	// Clean harmonics misdetected as fundamental
	tones.sort();
	for (Tones::iterator it = tones.begin(); it != tones.end(); ++it) {
		Tones::iterator it2 = it;
		++it2;
		while (it2 != tones.end()) {
			double ratio = it2->freq / it->freq;
			double diff = std::abs(ratio - round(ratio));
			bool erase = false;
			if (diff < 0.02 && it2->level < 2.0 * it->level) erase = true;  // Precisely harmonic and not much stronger than fundamental
			// Perform the action
			if (erase) it2 = tones.erase(it2); else ++it2;
		}
	}
	temporalMerge(tones);
}

void Analyzer::temporalMerge(Tones& tones) {
	if (!m_moments.empty()) {
		Tones& old = m_moments.back().m_tones;
		Tones::iterator it = tones.begin();
		// Iterate over old tones
		for (Tones::iterator oldit = old.begin(); oldit != old.end(); ++oldit) {
			// Try to find a matching new tone
			while (it != tones.end() && *it < *oldit) ++it;
			// If match found
			if (it != tones.end() && *it == *oldit) {
				// Link together the old and the new tones
				oldit->next = &*it;
				it->prev = &*oldit;
			}
		}
	}
	m_moments.push_back(Moment(m_moments.size() * processStep() / m_rate));
	m_moments.back().stealTones(tones);  // No pointers are invalidated
}

Moment::Moment(double t): m_time(t) {}

void Moment::stealTones(Tones& tones) {
	m_tones.swap(tones);
}

