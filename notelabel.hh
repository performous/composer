#pragma once

#include <QLabel>

class NoteLabel: public QLabel
{
public:
	NoteLabel(const QString &text, QWidget *parent, const QPoint &position = QPoint(), const QSize &size = QSize(), bool floating = true);

	void createPixmap(QSize size = QSize());
	QString getText() const;
	void setText(const QString &text);

	bool isFloating() const { return m_floating; }
	void disableFloating() { m_floating = false; }

	void startResizing(int dir) { m_resizing = dir; m_hotspot = QPoint(); }
	void startDragging(const QPoint& point) { m_hotspot = point; m_resizing = 0; }

	void resizeEvent(QResizeEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

	bool operator<(const NoteLabel &rhs) const { return x() < rhs.x(); }

private:
	QString m_labelText;
	bool m_floating;
	int m_resizing;
	QPoint m_hotspot;
};

bool inline cmpNoteLabelPtr(const NoteLabel *lhs, const NoteLabel *rhs)
{
	return *lhs < *rhs;
}
