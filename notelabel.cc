#include <QtGui>
#include "notelabel.hh"
#include "notegraphwidget.hh"

namespace {
	static const int text_margin = 12; // Margin of the label texts
}

const int NoteLabel::resize_margin = 5; // How many pixels is the resize area
const int NoteLabel::min_width = 10; // How many pixels is the resize area
const int NoteLabel::default_size = 50; // The preferred size of notes

NoteLabel::NoteLabel(const Note &note, QWidget *parent, const QPoint &position, const QSize &size, bool floating)
	: QLabel(parent), m_note(note), m_selected(false), m_floating(floating), m_resizing(0), m_hotspot()
{
	createPixmap(size);
	if (!position.isNull())
		move(position);

	setMouseTracking(true);
	setMinimumSize(min_width, 10);
	setAttribute(Qt::WA_DeleteOnClose);
	show();
}

void NoteLabel::createPixmap(QSize size)
{
	QFontMetrics metric(font());
	if (size.width() <= 0) {
		size.rwidth() = default_size;
	}
	if (size.height() <= 0) {
		size.rheight() = metric.size(Qt::TextSingleLine, lyric()).height() + text_margin;
	}

	QImage image(size.width(), size.height(),
				 QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(0, 0, 0, 0));

	QFont font;
	font.setStyleStrategy(QFont::ForceOutline);

	QLinearGradient gradient(0, 0, 0, image.height()-1);
	gradient.setColorAt(0.0, Qt::white);
	float ff = m_floating ? 1.0f : 0.5f;
	if (m_selected) {
		gradient.setColorAt(0.2, QColor(180, 100, 100));
		gradient.setColorAt(0.8, QColor(180, 100, 100));
		gradient.setColorAt(1.0, QColor(120, 100, 100));
	} else if (m_note.type == Note::NORMAL) {
		gradient.setColorAt(0.2, QColor(100 * ff, 100 * ff, 255 * ff));
		gradient.setColorAt(0.8, QColor(100 * ff, 100 * ff, 255 * ff));
		gradient.setColorAt(1.0, QColor(100 * ff, 100 * ff, 200 * ff));
	} else if (m_note.type == Note::GOLDEN) {
		gradient.setColorAt(0.2, QColor(255 * ff, 255 * ff, 100 * ff));
		gradient.setColorAt(0.8, QColor(255 * ff, 255 * ff, 100 * ff));
		gradient.setColorAt(1.0, QColor(160 * ff, 160 * ff, 100 * ff));
	} else if (m_note.type == Note::FREESTYLE) {
		gradient.setColorAt(0.2, QColor(100 * ff, 180 * ff, 100 * ff));
		gradient.setColorAt(0.8, QColor(100 * ff, 180 * ff, 100 * ff));
		gradient.setColorAt(1.0, QColor(100 * ff, 120 * ff, 100 * ff));
	}

	QPainter painter;
	painter.begin(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setBrush(gradient);
	painter.drawRoundedRect(QRectF(0.5, 0.5, image.width()-1, image.height()-1),
							25, 25, Qt::RelativeSize);

	painter.setFont(font);
	painter.setBrush(Qt::black);
	painter.drawText(QRect(QPoint(6, 6), QSize(size.width()-text_margin, size.height()-text_margin)), Qt::AlignCenter, lyric());

	// Render sentence end indicator
	if (m_note.lineBreak) {
		// TODO
	}

	painter.end();

	setPixmap(QPixmap::fromImage(image));

	setToolTip(QString("\"%1\"\n%2").arg(lyric()).arg(QString::fromStdString(m_note.typeString())));
	setStatusTip(QString("Lyric: ") + lyric());
}

void NoteLabel::resizeEvent(QResizeEvent *event)
{
	createPixmap(event->size());
}

void NoteLabel::mouseMoveEvent(QMouseEvent *event)
{
	QToolTip::showText(event->globalPos(), toolTip(), this);

	NoteGraphWidget* ngw = dynamic_cast<NoteGraphWidget*>(parent());
	if (m_resizing != 0) {
		// Resizing
		if (m_resizing < 0)
			setGeometry(x() + event->pos().x(), y(), width() - event->pos().x(), height());
		else
			resize(event->pos().x(), height());
		if (ngw) ngw->updateNotes();

	} else if (!m_hotspot.isNull()) {
		// Moving
		QPoint newpos = pos() + event->pos() - m_hotspot;
		move(newpos);
		if (ngw) ngw->updateNotes();
		// Check if we need a new hotspot, because the note was constrained
		if (pos().x() != newpos.x()) m_hotspot.rx() = event->x();

	} else {
		// Hover cursors
		if (event->pos().x() < NoteLabel::resize_margin || event->pos().x() > width() - NoteLabel::resize_margin) {
			setCursor(QCursor(Qt::SizeHorCursor));
		} else {
			setCursor(QCursor(Qt::OpenHandCursor));
		}
	}
}

void NoteLabel::startResizing(int dir)
{
	m_resizing = dir;
	m_hotspot = QPoint(); // Reset
	if (dir != 0) setCursor(QCursor(Qt::SizeHorCursor));
	else setCursor(QCursor());
}

void NoteLabel::startDragging(const QPoint& point)
{
	m_hotspot = point;
	m_resizing = 0;

	if (!point.isNull()) setCursor(QCursor(Qt::ClosedHandCursor));
	else setCursor(QCursor());
}

