#pragma once

#include "util.hh"
#include <complex>
#include <vector>
#include <list>
#include <algorithm>
#include <cmath>

static inline double level2dB(double level) { return 20.0 * std::log10(level); }
static inline double dB2level(double db) { return std::pow(10.0, db / 20.0); }

/// A tone is a collection of a base frequency (freq) and all its harmonics
struct Tone {
	static const std::size_t MAXHARM = 16; ///< The maximum number of harmonics tracked
	double freq; ///< Frequency (Hz)
	double level; ///< Level (linear)
	double harmonics[MAXHARM]; ///< Harmonics' levels
	Tone(); 
	bool operator==(double f) const; ///< Compare for rough frequency match
	// Linked list of a continuous tone
	Tone* prev;
	Tone* next;
	static bool cmpByLevel(Tone const& a, Tone const& b) { return a.level > b.level; }
};

static inline bool operator==(Tone const& lhs, Tone const& rhs) { return lhs == rhs.freq; }
static inline bool operator!=(Tone const& lhs, Tone const& rhs) { return !(lhs == rhs); }
static inline bool operator<=(Tone const& lhs, Tone const& rhs) { return lhs.freq < rhs.freq || lhs == rhs; }
static inline bool operator>=(Tone const& lhs, Tone const& rhs) { return lhs.freq > rhs.freq || lhs == rhs; }
static inline bool operator<(Tone const& lhs, Tone const& rhs) { return lhs.freq < rhs.freq && lhs != rhs; }
static inline bool operator>(Tone const& lhs, Tone const& rhs) { return lhs.freq > rhs.freq && lhs != rhs; }

struct Moment {
	typedef std::list<Tone> Tones;
	Tones m_tones;
	double m_time;
	Moment(double t);
	double time() const { return m_time; }
	void stealTones(Tones& tones);
};

/// A peak contains information about a single frequency
struct Peak {
	double freqFFT;
	double freq;
	double level;
	Peak(): freqFFT(), freq(), level() {}
};

/// A combo combines multiple FFT peaks that all display the same frequency into one
struct Combo {
	double freq;
	double level;
	Combo(): freq(), level() {}
	void combine(Peak const& p);
	bool match(double freqOther) const;
	static bool cmpByFreq(Combo const& a, Combo const& b) { return a.freq < b.freq; }
	static bool cmpByLevel(Combo const& a, Combo const& b) { return a.level > b.level; }
};


/// analyzer class
 /** class to analyze input audio and transform it into useable data
 */
class Analyzer {
public:
	typedef std::vector<std::complex<float> > Fourier;  ///< FFT vector (the first level of detection)
	using Peaks = std::vector<Peak>;  ///< Peaks (the second level of detection)
	typedef std::list<Tone> Tones; ///< Tones (the final level of detection)
	typedef std::list<Moment> Moments; ///< Time-serie history of time and tones
	/// constructor
	Analyzer(double rate, std::string id);
	/** Get the fourier transform. **/
	Fourier const& getFourier() const { return m_fft; }
	/** Get the peak frequencies. **/
	Peaks const& getPeaks() const { return m_peaks; }
	/** Get a list of all tones detected. **/
	Moments const& getMoments() const { return m_moments; }
	/** Find a tone within the singing range; prefers strong tones around 200-400 Hz. **/
	//Tone const* findTone(double minfreq = 70.0, double maxfreq = 700.0) const;
	std::string const& getId() const { return m_id; }
	/// Process processSize() samples from RndIt input
	template<typename RndIt> void process(RndIt input) {
		std::vector<float> pcm(input, input + processSize());  // Needs local modifyable copy for calculations
		calcFFT(&pcm[0]);
		calcTones();
	}
	unsigned processSize() const;  ///< The number of samples required by process()
	unsigned processStep() const;  ///< The number of samples to increment the input position after each call to process()
	double getTime() const { return m_moments.empty() ? 0.0 : m_moments.back().time(); }
private:
	double m_rate;
	std::string m_id;
	std::vector<float> m_window;
	Fourier m_fft;
	std::vector<float> m_fftLastPhase;
	Peaks m_peaks;
	Moments m_moments;
	mutable double m_oldfreq;
	void calcFFT(float* pcm);
	void calcTones();
	void temporalMerge(Tones& tones);
};

