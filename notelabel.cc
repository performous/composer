#include <QtGui>
#include "notelabel.hh"

namespace {
	static const int text_margin = 12; // Margin of the label texts
}

NoteLabel::NoteLabel(const QString &text, QWidget *parent, const QPoint &position, const QSize &size, bool floating)
	: QLabel(parent), m_labelText(text), m_floating(floating), m_resizing(0)
{
	createPixmap(size);
	if (!position.isNull())
		move(position);

	show();
	setMouseTracking(true);
	setAttribute(Qt::WA_DeleteOnClose);
}

void NoteLabel::createPixmap(QSize size)
{
	QFontMetrics metric(font());
	if (size.width() <= 0) {
		size.rwidth() = 50; // FIXME: Size is fixed
	}
	if (size.height() <= 0) {
		size.rheight() = metric.size(Qt::TextSingleLine, m_labelText).height() + text_margin;
	}

	QImage image(size.width(), size.height(),
				 QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(0, 0, 0, 0));

	QFont font;
	font.setStyleStrategy(QFont::ForceOutline);

	QLinearGradient gradient(0, 0, 0, image.height()-1);
	gradient.setColorAt(0.0, Qt::white);
	if (m_floating) {
		gradient.setColorAt(0.2, QColor(160, 160, 180));
		gradient.setColorAt(0.8, QColor(160, 160, 180));
		gradient.setColorAt(1.0, QColor(100, 100, 120));
	} else {
		gradient.setColorAt(0.2, QColor(100, 100, 255));
		gradient.setColorAt(0.8, QColor(100, 100, 255));
		gradient.setColorAt(1.0, QColor(100, 100, 200));
	}

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

void NoteLabel::mouseMoveEvent(QMouseEvent *event)
{
	QToolTip::showText(event->globalPos(), getText(), this);
	if (m_resizing != 0) {
		if (m_resizing < 0)
			setGeometry(x() + event->pos().x(), y(), width() - event->pos().x(), height());
		else
			resize(event->pos().x(), height());
	}
}
