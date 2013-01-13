#include "songparser.hh"
#include <stdexcept>
#include <iostream>

namespace {
	int linecounter = 0;
	VocalTrack vocal(TrackName::LEAD_VOCAL);
	Notes& notes = vocal.notes;
}

bool  SongParser::smmTxtCheck(QString const& data)
{
	return data[0] == '[';
}

void SongParser::smmParse()
{
	notes.clear();
	QString line;
	while(getline(line))
	{
		linecounter = 0;
		while (smmNoteParse(line)) {}
	}

	if (!notes.empty()) {
		vocal.beginTime = notes.front().begin;
		vocal.endTime = notes.back().end;
		// Insert notes
		m_song.insertVocalTrack(TrackName::LEAD_VOCAL, vocal);
	} else throw std::runtime_error(QT_TR_NOOP("Couldn't find any notes"));
}

bool SongParser::smmNoteParse(QString line)
{
	if (line.isEmpty())
	{
		return false;
	}

	int j = 0;
	int sizeofLine = line.count();
	char nextChar;
	Note n;
	n.note = 5;
	if (line[0] != '[')
	{
		throw std::runtime_error("not a soramimi file!");
		return false;
	}
	else
	{
		QString noteTimeBegin = "";
		QString noteTimeEnd = "";
		QString noteText;
		n.type =  Note::NORMAL;
		while(linecounter<sizeofLine)
		{
			if(line[linecounter] == '[')
			{
				linecounter++;
				nextChar = line.at(linecounter).toLatin1();
				while(nextChar != ']')
				{
					noteTimeBegin+=nextChar;
					linecounter++;
					nextChar = line.at(linecounter).toLatin1();
					//now we've got the starttime as string seperated by :
				}
				if(line.at(linecounter+1) == '\0') //see if we' re at the end of the line
				{
					Note e;
					e.type = Note::SLEEP; //add sleep note to mark end of line
					e.note = 0;
					e.begin = convertSMMTimestampToDouble(noteTimeBegin);
					e.end = e.begin;
					notes.push_back(e);
					return false;
				}
			}
			else if(line[linecounter] == ']') //confirm end of time indication
			{
				linecounter++;
				nextChar = line.at(linecounter).toLatin1();
				do
				{
					noteText+=nextChar;
					linecounter++;
					nextChar = line.at(linecounter).toLatin1();

				}
				while(nextChar != '[');

				n.syllable = noteText; //now we've got the note text as string;
				j = linecounter;
				j++;
				nextChar = line.at(j).toLatin1();
				while(nextChar != ']')
				{
					noteTimeEnd+=nextChar;
					j++;
					nextChar = line.at(j).toLatin1();
					/*now we've got the endtime as string seperated by :
					but we start a different counter because the end time
					of one note is the start time of the following exept for the last note in the line*/
				}
				n.begin = convertSMMTimestampToDouble(noteTimeBegin);
				n.end = convertSMMTimestampToDouble(noteTimeEnd);
				notes.push_back(n);

				if(line.at(j+1) == '\0') //see if we' re at the end of the line
				{

					Note e;
					e.type = Note::SLEEP; //add sleep note to mark end of line
					e.note = 0;
					e.begin = convertSMMTimestampToDouble(noteTimeEnd);
					e.end = e.begin;
					notes.push_back(e);
					return false;
				}

				return true;
			}
		}

		return true;

	}

}

double SongParser::convertSMMTimestampToDouble(QString timeStamp)
{
    bool ok = false;
    timeStamp.replace(QString(":"), QString("."));
    QString minutes = timeStamp.mid(0,2);
    QString seconds = timeStamp.mid(3,5);
    double Min = minutes.toDouble(&ok);
    if(!ok)
    {
        throw std::runtime_error("double conversion went wrong");
    }
    double Sec = seconds.toDouble(&ok);
    if(!ok)
    {
        throw std::runtime_error("double conversion went wrong");
    }
    double append = Min*60;
    append +=Sec;
    return append;
}
