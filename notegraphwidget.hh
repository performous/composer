#pragma once

#include <QWidget>

class NoteGraphWidget: public QWidget
{
public:
	NoteGraphWidget(QWidget *parent = 0);

	void updateWidth();

protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);

private:
	int requiredWidth;
};
