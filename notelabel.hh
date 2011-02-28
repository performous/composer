#pragma once

#include <QLabel>
#include <QCloseEvent>
#include <QTimer>
#include "notes.hh"
#include "operation.hh"

/**
 * @brief Widget representing a single note.
 *
 * Notes:
 * - Is rather useless without a parent NoteGraphWidget-object
 * - Widget is initially hidden and without a pixmap to allow quick creation
 * - Pixmap updates are generally delayed a little
 *   - The idea is to allow some time to apply the base operation to every note
 *     and then do the gfx updates asynchronously
 * - NoteLabel has its own mouse handling for moving, resizing, cursors, tooltips etc,
	 but requires the parent NoteGraphWidget to update some internal states
 * - Geometry & position is calculated from the underlying Note attributes (i.e. time and pitch)
 *   - Setting size or pos manually will be overridden so the Note must be manipulated instead
 * - NoteLabel can be serialized to Operation-class
 */
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
	/// Shows the widget and creates the pixmap; if already visible, do nothing
	/// @return true if pixmap was actually created, false if the widget was already visible
	bool createPixmap() { if (isVisible()) return false; show(); updatePixmap(); return true; }
	/// Updates the pixmap but only if the widget is visible (i.e. createPixmap has been called)
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
