#pragma once

#include <QWidget>
#include <list>

class NoteLabel;
typedef std::list<NoteLabel*> NoteLabels;

class NoteGraphWidget: public QWidget
{
public:

	static const int noteYStep;

	NoteGraphWidget(QWidget *parent = 0);

	void clear();
	void setLyrics(QString lyrics);
	void updateWidth();
	void updateNotes();
	void rebuildNoteList();

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);
	void mouseMoveEvent(QMouseEvent * event);

private:
	int  m_requiredWidth;
	QPoint m_panHotSpot;
	NoteLabel* m_selectedNote;
	enum NoteAction { NONE, RESIZE, MOVE } m_selectedAction;
	NoteLabels m_notes;
};


struct FloatingGap
{
	FloatingGap(int x): begin(x), end(x) {}

	void addNote(NoteLabel* n);
	int width() const { return end - begin; }
	bool isEmpty() const { return notes.empty(); }

	int begin;
	int end;

	NoteLabels notes;
};
