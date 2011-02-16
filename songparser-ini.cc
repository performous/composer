#include "songparser.hh"

#include "midifile.hh"
#include <stdexcept>
#include <iostream>

/// @file
/// Functions used for parsing the FoF INI/MID song format

using namespace SongParserUtil;

/// 'Magick' to check if this file looks like correct format
bool SongParser::iniCheck(QString const& data) {
	return data.startsWith("[song]");
}

/// Parser
void SongParser::iniParse() {
	// Some defaults
	m_song.music["background"] = m_song.path + "song.ogg";
	m_song.music["vocals"] = m_song.path + "vocals.ogg";
	m_song.music["guitar"] = m_song.path + "guitar.ogg";
	m_song.music["rhythm"] = m_song.path + "rhythm.ogg";
	m_song.music["drums"] = m_song.path + "drums.ogg";
	// Parse the INI file
	QString line;
	if (!getline(line) || line != "[song]") throw std::runtime_error("INI should begin with [song]");
	while (getline(line)) iniParseField(line);
	midParse();
}

void SongParser::iniParseField(QString const& line) {
	if (line.trimmed().isEmpty()) return;
	int pos = line.indexOf('=');
	if (pos < 0) throw std::runtime_error("Invalid ini format, should be key=value");
	QString key = line.left(pos).trimmed().toUpper();
	QString value = line.mid(pos + 1).trimmed();
	bool ok = true;
	if (key == "NAME") m_song.title = value;
	else if (key == "ARTIST") m_song.artist = value;
	else if (key == "EDITION") m_song.edition = value;
	else if (key == "GENRE") m_song.genre = value;
	else if (key == "CREATOR") m_song.creator = value;
	else if (key == "LANGUAGE") m_song.language= value;
	else if (key == "YEAR") m_song.year = value;
	else if (key == "COVER") m_song.cover = value;
	else if (key == "VIDEO") m_song.video = value;
	else if (key == "BACKGROUND") m_song.background = value;
	else if (key == "START") m_song.start = value.toDouble(&ok);
	else if (key == "VIDEO_START_TIME") m_song.videoGap = 1e-3 * value.toDouble(&ok);
	else if (key == "PREVIEW_START_TIME") m_song.preview_start = 1e-3 * value.toDouble(&ok);

	if (!ok) throw std::runtime_error(QString("Invalid value for %1: %2").arg(key).arg(value).toStdString());
	return;
}

void SongParser::midParse() {
	QByteArray name = (m_song.path + "song.mid").toLocal8Bit();
	using namespace mid;
	Reader reader(std::string(name.data(), name.size()).c_str());
	unsigned track = 0;
	do {
		std::cout << "Track " << track << " begins" << std::endl;
		unsigned timecode = 0;
		std::string trackName, lyric;
		for (Event ev; reader.parseEvent(ev); ) {
			timecode += ev.timecode;
			if (ev.type == Event::NOTE_ON && ev.arg2 == 0) ev.type = Event::NOTE_OFF;  // Note on with velocity 0 actually means off.
			// Process any interesting events
			if (ev.type == Event::NOTE_ON) {
			}
			if (ev.type == Event::NOTE_OFF) {
			}
			if (ev.type == Event::SPECIAL) {
				Event::Meta meta = ev.getMeta();
				if (meta == Event::META_LYRIC || (meta == Event::META_TEXT && *ev.begin != '[')) {
					lyric = ev.getDataStr();
					continue;
				}
				if (meta == Event::META_TEXT) {  // Non-lyric text event such as "[section Verse-1a]"
					continue;
				}
				if (meta == Event::META_SEQNAME) { trackName = ev.getDataStr(); continue; }
				if (meta == Event::META_ENDOFTRACK) break;  // Not really necessary as the track would normally end after this anyway
				if (meta == Event::META_TEMPO) {
					if (ev.end - ev.begin != 3) throw std::runtime_error("Invalid tempo change event");
					unsigned microSecPerBeat = ev.begin[0] << 16 | ev.begin[1] << 8 | ev.begin[2];
					addBPM(timecode, 6e+7 / microSecPerBeat);
					break;
				}
				if (meta == Event::META_TIMESIGNATURE) {
					if (ev.end - ev.begin != 4) throw std::runtime_error("Invalid time signature event");
					break;
				}
				ev.print();
			}
		}
	} while (++track, reader.nextTrack());
}

#if 0
// Text event handling
const std::string sect_pfx = "[section ";
if (!data.compare(0, sect_pfx.length(), sect_pfx)) {// [section verse_1]
	std::string sect_name = data.substr(sect_pfx.length(), data.length()-sect_pfx.length()-1);
	if (sect_name != "big_rock_ending") {
		bool space = true;
		for (std::string::const_iterator it = sect_name.begin(); it != sect_name.end(); ++it) {
			if (space) *it = toupper(*it);        // start in uppercase
			if (*it == '_') {*it = ' '; space = true;} // underscores to spaces
			else space = false;
		}
		// replace gtr => guitar
		midisections.push_back(MidiSection(sect_name, get_seconds(miditime)));
	} else cmdevents.push_back(std::string(data)); // see songparser-ini.cc: we need to keep the BRE in cmdevents
}
else cmdevents.push_back(std::string(data));
#endif


