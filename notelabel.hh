#pragma once

#include <QLabel>

class NoteLabel: public QLabel
{
public:
	NoteLabel(const QString &text, QWidget *parent, const QPoint &position = QPoint());
	void createPixmap(QSize size = QSize());
	QString getText() const;
	void setText(const QString &text);

	void resizeEvent(QResizeEvent *event);

private:
	QString m_labelText;
};
