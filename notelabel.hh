#pragma once

#include <QLabel>
#include <QCloseEvent>
#include <QTimer>
#include "notes.hh"
#include "operation.hh"

class NoteLabel: public QLabel
{
	Q_OBJECT

public:
	static const int render_delay;
	static const int resize_margin;
	static const double default_length;
	static const double min_length;

	NoteLabel(const Note &note, QWidget *parent, bool floating = true);

	QString lyric() const { return m_note.syllable; }
	void setLyric(const QString &text) { m_note.syllable = text; QTimer::singleShot(render_delay, this, SLOT(updatePixmap())); }
	QString description(bool multiline) const;

	bool isSelected() const { return m_selected; }
	void setSelected(bool state = true);

	Note& note() { return m_note; }
	Note note() const { return m_note; }
	void updateLabel();
	void updateTips();

	bool isFloating() const { return m_floating; }
	void setFloating(bool state) { m_floating = state; QTimer::singleShot(render_delay, this, SLOT(updatePixmap())); }
	bool isLineBreak() const { return m_note.lineBreak; }
	void setLineBreak(bool state) { m_note.lineBreak = state; QTimer::singleShot(render_delay, this, SLOT(updatePixmap())); }
	void setType(int newtype) { m_note.type = Note::types[newtype]; QTimer::singleShot(render_delay, this, SLOT(updatePixmap())); }

	void startResizing(int dir);
	void startDragging(const QPoint& point);

	/// Create Operation from NoteLabel
	operator Operation() const;

	bool operator<(const NoteLabel &rhs) const { return m_note.begin < rhs.note().begin; }

public slots:
	bool createPixmap() { if (isVisible()) return false; show(); updatePixmap(); return true; }
	void updatePixmap();

protected:
	void resizeEvent(QResizeEvent *event);
	void moveEvent(QMoveEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void closeEvent(QCloseEvent *event) { deleteLater(); event->accept(); }

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
