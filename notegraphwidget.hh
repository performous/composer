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
	int moveTimerId;
protected:
	void mouseMoveEvent(QMouseEvent *event);
	void timerEvent(QTimerEvent *event);
};


class NoteGraphWidget: public QLabel
{
	Q_OBJECT

public:

	NoteGraphWidget(QWidget *parent = 0);

	void clearNotes();
	void setLyrics(QString lyrics);
	void setLyrics(const VocalTrack &track);
	void analyzeMusic(QString filepath);
	void updateNotes();

	void updateMusicPos(qint64 time, bool smoothing);
	void stopMusic();

	void selectNote(NoteLabel *note);
	NoteLabel* selectedNote() const { return m_selectedNote; }

	int getNoteLabelId(NoteLabel* note) const;
	NoteLabels& noteLabels() { return m_notes; }
	void doOperation(const Operation& op, Operation::OperationFlags flags = Operation::NORMAL);

	int s2px(double sec) const;
	double px2s(int px) const;
	int n2px(int note) const;
	int px2n(int px) const;
	int h() const { return m_pitch->height; }

signals:
	void updateNoteInfo(NoteLabel*);
	void operationDone(const Operation&);
	void seek(qint64 time);

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent * event);
	void keyPressEvent(QKeyEvent *event);
	void timerEvent(QTimerEvent *event);

private:
	void finalizeNewLyrics();

	int m_requiredWidth;
	int m_noteHalfHeight;
	QPoint m_panHotSpot;
	NoteLabel* m_selectedNote;
	enum NoteAction { NONE, RESIZE, MOVE } m_selectedAction;
	bool m_seeking;
	bool m_actionHappened;
	NoteLabels m_notes;
	QScopedPointer<PitchVis> m_pitch;
	SeekHandle m_seekHandle;
	int m_analyzeTimer;
};


struct FloatingGap
{
	FloatingGap(int x): begin(x), end(x), m_notesWidth() {}

	void addNote(NoteLabel* n);
	bool isEmpty() const { return notes.empty(); }
	int width() const { return end - begin; }
	int minWidth() const;
	int notesWidth() const { return m_notesWidth; }

	int begin;
	int end;

	NoteLabels notes;

private:
	int m_notesWidth;
};
