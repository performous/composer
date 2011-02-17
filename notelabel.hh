#pragma once

#include <QLabel>
#include "notes.hh"
#include "operation.hh"

class NoteLabel: public QLabel
{
	Q_OBJECT

public:
	static const int resize_margin;
	static const int default_size;
	static const double min_length;

	NoteLabel(const Note &note, QWidget *parent, bool floating = true);

	void createPixmap(QSize size = QSize());
	QString lyric() const { return m_note.syllable; }
	void setLyric(const QString &text) { m_note.syllable = text; createPixmap(size()); }

	bool isSelected() const { return m_selected; }
	void setSelected(bool state = true);

	Note& note() { return m_note; }
	Note note() const { return m_note; }
	void updateLabel();

	bool isFloating() const { return m_floating; }
	void setFloating(bool state) { m_floating = state; createPixmap(size()); }
	bool isLineBreak() const { return m_note.lineBreak; }
	void setLineBreak(bool state) { m_note.lineBreak = state; createPixmap(size()); }
	void setType(int newtype) { m_note.type = Note::types[newtype]; createPixmap(size()); }

	void startResizing(int dir);
	void startDragging(const QPoint& point);

	/// Create Operation from NoteLabel
	operator Operation() const;

	bool operator<(const NoteLabel &rhs) const { return m_note.begin < rhs.note().begin; }

protected:
	void resizeEvent(QResizeEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

private:
	Note m_note;
	bool m_selected;
	bool m_floating;
	int m_resizing;
	QPoint m_hotspot;
};

bool inline cmpNoteLabelPtr(const NoteLabel *lhs, const NoteLabel *rhs)
{
	return (*lhs) < (*rhs);
}
