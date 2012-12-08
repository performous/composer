#include "songparser.hh"
#include <stdexcept>
#include <iostream>


const int bpm = 6000; //this way we'll have 100 bpm every second, so 1bpm every milisecond!
VocalTrack vocal(TrackName::LEAD_VOCAL);
Notes& notes = vocal.notes;

void SongParser::smmParse()
{
    QString line;
    while (getline(line) && smmNoteParse(line)) {}
    m_song.bpm = bpm;
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
                    n.begin = convertTimeToBpm(bpm,noteTimeBegin);
                    n.end = convertTimeToBpm(bpm, noteTimeEnd);
                    notes.push_back(n);

                    if(line.at(j+1) == '\0') //see if we' re at the end of the line
                    {

                        Note e;
                        e.type = Note::SLEEP;
                        e.note = 0;
                        e.begin = convertTimeToBpm(bpm,noteTimeEnd);
                        e.end = e.begin;
                        notes.push_back(e);
                        return true;
                    }


            }
         }

        return true;

   }

}

int SongParser::convertTimeToBpm(int bpm, QString time)
{
    int beats = 0;
    char Minutes [2];
    Minutes [0] = time.at(0).toLatin1();
    Minutes [1] = time.at(1).toLatin1();
    int minutes = atoi(Minutes);
    beats += (bpm * minutes);
    char Seconds [2];
    Seconds [0] = time.at(3).toLatin1();
    Seconds [1] = time.at(4).toLatin1();
    int seconds = atoi(Seconds);
    beats += ((bpm/60)*seconds);
    char MiliSeconds [2];
    MiliSeconds [0] = time.at(6).toLatin1();
    MiliSeconds [1] = time.at(7).toLatin1();
    int miliseconds = atoi(MiliSeconds);
    beats += ((bpm/6000)*miliseconds);
    return beats;
}
