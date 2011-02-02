#pragma once

#include "pitchvis.hh"
#include "notes.hh"
#include "operation.hh"
#include <QLabel>
#include <QList>
#include <QScopedPointer>

class NoteLabel;
typedef QList<NoteLabel*> NoteLabels;


class SeekHandle: public QLabel
{
	Q_OBJECT
public:
	SeekHandle(QWidget *parent = 0);
	int curx() const { return x() + width() / 2; }
	int moveTimerId;
protected:
	void mouseMoveEvent(QMouseEvent *event);
	void timerEvent(QTimerEvent *event);
};



class NoteLabelManager: public QLabel
{
	Q_OBJECT
public:
	NoteLabelManager(QWidget *parent = 0);

	virtual void updateNotes(bool leftToRight = true) {}

	void selectNote(NoteLabel *note, bool clearPrevious = true);
	NoteLabel* selectedNote() const { return m_selectedNotes.isEmpty() ? NULL : m_selectedNotes.front(); }
	NoteLabels& selectedNotes() { return m_selectedNotes; }
	NoteLabels const& selectedNotes() const { return m_selectedNotes; }

	int getNoteLabelId(NoteLabel* note) const;
	NoteLabels& noteLabels() { return m_notes; }

	void split(NoteLabel *note, float ratio = 0.5f);
	void del(NoteLabel *note);
	void move(NoteLabel *note, int value);
	void editLyric(NoteLabel *note);
	void setFloating(NoteLabel *note, bool state);
	void setLineBreak(NoteLabel *note, bool state);
	void setType(NoteLabel *note, int newtype);

	void doOperation(const Operation& op, Operation::OperationFlags flags = Operation::NORMAL);

	int s2px(double sec) const;
	double px2s(int px) const;
	int n2px(double note) const;
	double px2n(int px) const;

signals:
	void updateNoteInfo(NoteLabel*);
	void operationDone(const Operation&);

public slots:
	void selectNextSyllable(bool backwards = false, bool addToSelection = false);
	void selectNextSentenceStart();

protected:
	NoteLabels m_notes;
	NoteLabels m_selectedNotes;
	enum NoteAction { NONE, RESIZE, MOVE } m_selectedAction;
	int m_noteHalfHeight;
	double m_pixelsPerSecond;
};



class NoteGraphWidget: public NoteLabelManager
{
	Q_OBJECT

public:

	NoteGraphWidget(QWidget *parent = 0);

	void clearNotes();
	void setLyrics(QString lyrics);
	void setLyrics(const VocalTrack &track);
	void analyzeMusic(QString filepath);

	void updateNotes(bool leftToRight = true);
	void updateMusicPos(qint64 time, bool smoothing);
	void stopMusic();
	void seek(int x);

	VocalTrack getVocalTrack() const;
	QString getCurrentSentence() const;
	QString dumpLyrics() const;

public slots:
	void timeSyllable();
	void timeSentence();

signals:
	void analyzeProgress(int, int);
	void seeked(qint64 time);

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent * event);
	void keyPressEvent(QKeyEvent *event);
	void timerEvent(QTimerEvent *event);
	void paintEvent(QPaintEvent*);

private:
	void finalizeNewLyrics();
	void timeCurrent();

	int m_requiredWidth;
	QPoint m_panHotSpot;
	bool m_seeking;
	bool m_actionHappened;
	QScopedPointer<PitchVis> m_pitch;
	SeekHandle m_seekHandle;
	int m_analyzeTimer;
	double m_duration;
};


struct FloatingGap
{
	FloatingGap(int x): begin(x), end(x), m_notesWidth() {}

	void addNote(NoteLabel* n);
	bool isEmpty() const { return notes.empty(); }
	int width() const { return abs(end - begin); }
	int minWidth() const;
	int notesWidth() const { return m_notesWidth; }

	int begin;
	int end;

	NoteLabels notes;

private:
	int m_notesWidth;
};
