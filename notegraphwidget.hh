#pragma once

#include "pitchvis.hh"
#include "notes.hh"
#include "operation.hh"
#include <QLabel>
#include <list>

class NoteLabel;
typedef std::list<NoteLabel*> NoteLabels;

class NoteGraphWidget: public QLabel
{
	Q_OBJECT

public:

	static const int noteYStep;

	NoteGraphWidget(QWidget *parent = 0);

	void clear();
	void setLyrics(QString lyrics);
	void setLyrics(const Notes &notes);
	void updateNotes();

	void selectNote(NoteLabel* note);
	NoteLabel* selectedNote() const { return m_selectedNote; }

	void doOperation(const Operation& op, Operation::OperationFlags flags = Operation::NORMAL);

signals:
	void updateNoteInfo(NoteLabel*);
	void operationDone(const Operation&);

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);
	void mouseMoveEvent(QMouseEvent * event);
	void keyPressEvent(QKeyEvent *event);

private:
	void finalizeNewLyrics();

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
