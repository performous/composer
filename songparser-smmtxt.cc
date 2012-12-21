#include "songparser.hh"
#include <stdexcept>
#include <iostream>


VocalTrack vocal(TrackName::LEAD_VOCAL);
Notes& notes = vocal.notes;

void SongParser::smmParse()
{
    QString line;
    while (getline(line) && smmNoteParse(line)) {}
    m_song.insertVocalTrack(TrackName::LEAD_VOCAL, vocal);

    if (!notes.empty()) {
        vocal.beginTime = notes.front().begin;
        vocal.endTime = notes.back().end;
        // Insert notes
        m_song.insertVocalTrack(TrackName::LEAD_VOCAL, vocal);
    } else throw std::runtime_error(QT_TR_NOOP("Couldn't find any notes"));
}



bool SongParser::smmNoteParse(QString line)
{
    int i = 0;
    int j = 0;
    int sizeofLine = sizeof(line);
    char nextChar;
    Note n;
    if (line.isEmpty()) return true;
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
        while(i<sizeofLine)
        {
            if(line[i] == '[')
            {
                i++;
                nextChar = line.at(i).toLatin1();
                while(nextChar != ']')
                {
                    noteTimeBegin+=nextChar;
                    i++;
                    nextChar = line.at(i).toLatin1();
                    //now we've got the starttime as string seperated by :
                }
            }
             else if(line[i] == ']') //confirm end of time indication
            {
                    i++;
                    while(nextChar != '[')
                    {
                        noteText+=nextChar;
                        i++;
                        nextChar = line.at(i).toLatin1();
                        //now we've got the note text as string;
                        n.syllable = noteText;
                    }
                    j = i;
                    j++;
                    nextChar = line.at(j).toLatin1();
                    while(nextChar != ']')
                    {
                    noteTimeEnd+=nextChar;
                    i++;
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
                        return true;
                    }


            }
         }

        return true;

   }

}

double  SongParser::convertSMMTimestampToDouble(QString timeStamp)
{
    double append = 0;
    char minutes [2];
    char seconds [2];
    char miliseconds[2];
    minutes[0] = timeStamp.at(0).toLatin1();
    minutes[1] = timeStamp.at(1).toLatin1();
    seconds[0] = timeStamp.at(3).toLatin1();
    seconds[1] = timeStamp.at(4).toLatin1();
    miliseconds[0] = timeStamp.at(6).toLatin1();
    miliseconds[1] = timeStamp.at(7).toLatin1();
    int Min = atoi(minutes);
    int Sec = atoi(seconds);
    int Mil = atoi(miliseconds);
    append += (Min*60);
    append += (Sec);
    append += (Mil/100);
    return append;
}
