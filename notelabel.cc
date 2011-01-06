#include <QtGui>
#include "notelabel.hh"

namespace {
	static const int text_margin = 12; // Margin of the label texts
}

NoteLabel::NoteLabel(const QString &text, QWidget *parent, const QPoint &position)
	: QLabel(parent), m_labelText(text)
{
	createPixmap();
	if (!position.isNull())
		move(position);
	show();
	setAttribute(Qt::WA_DeleteOnClose);
}

void NoteLabel::createPixmap(QSize size)
{
	if (!size.isValid()) {
		QFontMetrics metric(font());
		size = metric.size(Qt::TextSingleLine, m_labelText);
		size.rwidth() += text_margin;
		size.rheight() += text_margin;
	}

	QImage image(size.width(), size.height(),
				 QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(0, 0, 0, 0));

	QFont font;
	font.setStyleStrategy(QFont::ForceOutline);

	QLinearGradient gradient(0, 0, 0, image.height()-1);
	gradient.setColorAt(0.0, Qt::white);
	gradient.setColorAt(0.2, QColor(200, 200, 255));
	gradient.setColorAt(0.8, QColor(200, 200, 255));
	gradient.setColorAt(1.0, QColor(127, 127, 200));

	QPainter painter;
	painter.begin(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setBrush(gradient);
	painter.drawRoundedRect(QRectF(0.5, 0.5, image.width()-1, image.height()-1),
							25, 25, Qt::RelativeSize);

	painter.setFont(font);
	painter.setBrush(Qt::black);
	painter.drawText(QRect(QPoint(6, 6), QSize(size.width()-text_margin, size.height()-text_margin)), Qt::AlignCenter, m_labelText);
	painter.end();

	setPixmap(QPixmap::fromImage(image));

	setToolTip(m_labelText);
	setStatusTip(QString("Lyric: ") + m_labelText);
}

QString NoteLabel::getText() const
{
    return m_labelText;
}

void NoteLabel::setText(const QString &text)
{
	m_labelText = text;
}

void NoteLabel::resizeEvent(QResizeEvent *event)
{
	createPixmap(event->size());
}
