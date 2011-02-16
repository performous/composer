#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include "notes.hh"

#include <stdexcept>
#include <string>

/// parsing of songfile failed
struct SongParserException: public std::runtime_error {
	/// constructor
	SongParserException(QString const& msg, unsigned int linenum, bool sil = false)
		: runtime_error(msg.toStdString()), m_linenum(linenum), m_silent(sil) {}
	unsigned int line() const { return m_linenum; } ///< line in which the error occured
	bool silent() const { return m_silent; } ///< if the error should not be printed to user (file skipped)
  private:
	unsigned int m_linenum;
	bool m_silent;
};

class SongParser;

namespace TrackName {
	const QString GUITAR = "Guitar";
	const QString GUITAR_COOP = "Coop guitar";
	const QString GUITAR_RHYTHM = "Rhythm guitar";
	const QString BASS = "Bass";
	const QString DRUMS = "Drums";
	const QString LEAD_VOCAL = "Vocals";
	const QString HARMONIC_1 = "Harmonic 1";
	const QString HARMONIC_2 = "Harmonic 2";
	const QString HARMONIC_3 = "Harmonic 3";
}

/// class to load and parse songfiles
class Song {
	Q_DISABLE_COPY(Song);
	friend class SongParser;
	VocalTracks vocalTracks; ///< notes for the sing part
	VocalTrack dummyVocal; ///< notes for the sing part
  public:
	/// constructors
	Song(): dummyVocal(TrackName::LEAD_VOCAL) { reload(true); }
	Song(QString const& path_, QString const& filename_): dummyVocal(TrackName::LEAD_VOCAL), path(path_), filename(filename_) { reload(false); }
	/// reload song
	void reload(bool errorIgnore = true);
	/// parse field
	bool parseField(QString const& line);
	/// drop notes (to conserve memory), but keep info about available tracks
	void dropNotes();
	/** Get formatted song label. **/
	QString str() const { return title + "  by  " + artist; }
	/** Get full song information (used by the search function). **/
	QString strFull() const { return title + "\n" + artist + "\n" + genre + "\n" + edition + "\n" + path; }
	/// Is the song parsed from the file yet?
	enum LoadStatus { NONE, HEADER, FULL } loadStatus;
	/// status of song
	enum Status { NORMAL, INSTRUMENTAL_BREAK, FINISHED };
	/** Get the song status at a given timestamp **/
	Status status(double time);
	int randomIdx; ///< sorting index used for random order
	void insertVocalTrack(QString vocalTrack, VocalTrack track) {
		vocalTracks.erase(vocalTrack);
		vocalTracks.insert(std::make_pair<QString, VocalTrack>(vocalTrack, track));
	}
	// Get a selected track, or LEAD_VOCAL if not found or the first one if not found
	VocalTrack& getVocalTrack(QString vocalTrack = TrackName::LEAD_VOCAL) {
		if(vocalTracks.find(vocalTrack) != vocalTracks.end()) {
			return vocalTracks.find(vocalTrack)->second;
		} else {
			if(vocalTracks.find(TrackName::LEAD_VOCAL) != vocalTracks.end()) {
				return vocalTracks.find(TrackName::LEAD_VOCAL)->second;
			} else {
				if(!vocalTracks.empty()) {
					return vocalTracks.begin()->second;
				} else {
					return dummyVocal;
				}
			}
		}
	}
	VocalTrack getVocalTrack(QString vocalTrack = TrackName::LEAD_VOCAL) const {
		if(vocalTracks.find(vocalTrack) != vocalTracks.end()) {
			return vocalTracks.find(vocalTrack)->second;
		} else {
			if(vocalTracks.find(TrackName::LEAD_VOCAL) != vocalTracks.end()) {
				return vocalTracks.find(TrackName::LEAD_VOCAL)->second;
			} else {
				if(!vocalTracks.empty()) {
					return vocalTracks.begin()->second;
				} else {
					return dummyVocal;
				}
			}
		}
	}

	std::vector<QString> getVocalTrackNames() {
		std::vector<QString> result;
		for (VocalTracks::const_iterator it = vocalTracks.begin(); it != vocalTracks.end(); ++it) {
			result.push_back(it->first);
		}
		return result;
	}
	//InstrumentTracks instrumentTracks; ///< guitar etc. notes for this song
	//DanceTracks danceTracks; ///< dance tracks
	//bool hasDance() const { return !danceTracks.empty(); }
	//bool hasDrums() const { return instrumentTracks.find(TrackName::DRUMS) != instrumentTracks.end(); }
	//bool hasGuitars() const { return instrumentTracks.size() - hasDrums(); }
	bool hasVocals() const { return !vocalTracks.empty(); }
	QString path; ///< path of songfile
	QString filename; ///< name of songfile
	QString midifilename; ///< name of midi file in FoF format
	std::vector<QString> category; ///< category of song
	QString genre; ///< genre
	QString edition; ///< license
	QString title; ///< songtitle
	QString artist; ///< artist
	QString text; ///< songtext
	QString creator; ///< creator
	QString language; ///< language
	QString year; ///< year
	QMap<QString,QString> music; ///< music files (background, guitar, rhythm/bass, drums, vocals)
	QString cover; ///< cd cover
	QString background; ///< background image
	QString video; ///< video
	double bpm; ///< used for more accurate import --> export cycle
	/// Variables used for comparisons (sorting)
	QString collateByTitle;
	QString collateByTitleOnly;
	/// Variables used for comparisons (sorting)
	QString collateByArtist;
	QString collateByArtistOnly;
	/** Rebuild collate variables from other strings **/
	void collateUpdate();
	/** Convert a string to its collate form **/
	static QString collate(QString const& str);
	double videoGap; ///< gap with video
	double start; ///< start of song
	double preview_start; ///< starting time for the preview

	typedef std::vector<std::pair<double,double> > Stops;
	Stops stops; ///< related to dance
	typedef std::vector<double> Beats;
	Beats beats; ///< related to instrument and dance
	bool hasBRE; ///< is there a Big Rock Ending? (used for drums only)
	bool b0rkedTracks; ///< are some tracks broken? (so that user can be notified)
	struct SongSection {
		QString name;
		double begin;
		SongSection(QString const& name, const double begin): name(name), begin(begin) {}
	};
	typedef std::vector<SongSection> SongSections;
	SongSections songsections; ///< vector of song sections
	bool getNextSection(double pos, SongSection &section);
	bool getPrevSection(double pos, SongSection &section);
};

static inline bool operator<(Song const& l, Song const& r) { return l.collateByArtist < r.collateByArtist; }

