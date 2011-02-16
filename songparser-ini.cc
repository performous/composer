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
	mid::Reader reader(std::string(name.data(), name.size()).c_str());
	do {
		std::cout << "Track begins" << std::endl;
		for (mid::Event ev; reader.parseEvent(ev); ) {
			ev.print();
		}
	} while (reader.nextTrack());
}

