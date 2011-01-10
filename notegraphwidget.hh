#pragma once

#include "pitchvis.hh"
#include <QWidget>
#include <list>

class NoteLabel;
typedef std::list<NoteLabel*> NoteLabels;

class NoteGraphWidget: public QWidget
{
	Q_OBJECT

public:

	static const int noteYStep;

	NoteGraphWidget(QWidget *parent = 0);

	void clear();
	void setLyrics(QString lyrics);
	void updateWidth();
	void updateNotes();
	void rebuildNoteList();

	void selectNote(NoteLabel* note);

signals:
	void updateNoteInfo(NoteLabel*);

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);
	void mouseMoveEvent(QMouseEvent * event);
	void keyPressEvent(QKeyEvent *event);

private:
	int  m_requiredWidth;
	QPoint m_panHotSpot;
	NoteLabel* m_selectedNote;
	enum NoteAction { NONE, RESIZE, MOVE } m_selectedAction;
	NoteLabels m_notes;
	PitchVis m_pitch;
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
