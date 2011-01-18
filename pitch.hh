#pragma once

#include "util.hh"
#include <complex>
#include <vector>
#include <list>
#include <algorithm>
#include <cmath>

static inline double magn2dB(double magnitude) { return 20.0 * std::log10(magnitude); }
static inline double dB2magn(double db) { return std::pow(10.0, db / 20.0); }

/// A tone is a collection of a base frequency (freq) and all its harmonics
struct Tone {
	static const std::size_t MAXHARM = 16; ///< The maximum number of harmonics tracked
	static const std::size_t MINAGE = 6; ///< The minimum age required for a tone to be output
	double freq; ///< Frequency (Hz)
	double db; ///< Level (dB)
	double stabledb; ///< Stable level, useful for graphics rendering
	double harmonics[MAXHARM]; ///< Harmonics' levels
	std::size_t age; ///< How many times the tone has been detected in row
	Tone(); 
	void print() const; ///< Prints Tone to std::cout
	bool operator==(double f) const; ///< Compare for rough frequency match
	/// Less-than compare by levels (instead of frequencies like operator< does)
	static bool dbCompare(Tone const& l, Tone const& r) { return l.stabledb < r.stabledb; }
};

/// A peak contains information about a single frequency
struct Peak {
	double freqFFT;
	double freq;
	double magnitude;
	Peak(): freqFFT(), freq(), magnitude() {}
};

static inline bool operator==(Tone const& lhs, Tone const& rhs) { return lhs == rhs.freq; }
static inline bool operator!=(Tone const& lhs, Tone const& rhs) { return !(lhs == rhs); }
static inline bool operator<=(Tone const& lhs, Tone const& rhs) { return lhs.freq < rhs.freq || lhs == rhs; }
static inline bool operator>=(Tone const& lhs, Tone const& rhs) { return lhs.freq > rhs.freq || lhs == rhs; }
static inline bool operator<(Tone const& lhs, Tone const& rhs) { return lhs.freq < rhs.freq && lhs != rhs; }
static inline bool operator>(Tone const& lhs, Tone const& rhs) { return lhs.freq > rhs.freq && lhs != rhs; }

static const std::size_t BUF_N = 100000;  // Ringbuffer size in samples, major b0rkage will happen if this is too small; it won't cause latency, only wasted RAM

/// analyzer class
 /** class to analyze input audio and transform it into useable data
 */
class Analyzer {
public:
	typedef std::vector<std::complex<float> > Fourier;  ///< FFT vector (the first level of detection)
	typedef std::vector<Peak> Peaks;  ///< Peaks (the second level of detection)
	typedef std::list<Tone> Tones; ///< Tones (the final level of detection)
	/// constructor
	Analyzer(double rate, std::string id);
	/** Add input data to buffer. This is thread-safe (against other functions). **/
	template <typename InIt> void input(InIt begin, InIt end) {
		size_t r = m_bufRead;  // The read position
		size_t w = m_bufWrite;  // The write position
		bool overflow = false;
		while (begin != end) {
			float s = *begin++;  // Read input sample
			if (sizeof(*begin) == 2) s /= 32767.0;  // Integer samples (normalize)
			// Peak level calculation
			float p = s * s;
			if (p > m_level) m_level = p; else m_level *= 0.9995;
			m_buf[w] = s;
			// Cursor updates
			w = (w + 1) % BUF_N;
			if (w == r) overflow = true;
		}
		m_bufWrite = w;
		if (overflow) m_bufRead = (w + 1) % BUF_N;  // Reset read pointer on overflow
	}
	/** Call this to process all data input so far. **/
	void process();
	/** Get the fourier transform. **/
	Fourier const& getFourier() const { return m_fft; }
	/** Get the peak frequencies. **/
	Peaks const& getPeaks() const { return m_peaks; }
	/** Get the peak level in dB (negative value, 0.0 means clipping, sometimes clipping may occur at lower levels too). **/
	double getLevel() const { return 10.0 * log10(m_level); }
	/** Get a list of all tones detected. **/
	Tones const& getTones() const { return m_tones; }
	/** Find a tone within the singing range; prefers strong tones around 200-400 Hz. **/
	Tone const* findTone(double minfreq = 70.0, double maxfreq = 700.0) const;
	std::string const& getId() const { return m_id; }

private:
	double m_rate;
	std::string m_id;
	std::vector<float> m_window;
	float m_buf[2 * BUF_N];
	volatile size_t m_bufRead, m_bufWrite;
	Fourier m_fft;
	std::vector<float> m_fftLastPhase;
	double m_level;
	Peaks m_peaks;
	Tones m_tones;
	mutable double m_oldfreq;
	bool calcFFT();
	void calcTones();
	void mergeWithOld(Tones& tones) const;
};

