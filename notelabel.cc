#include <QMouseEvent>
#include <QResizeEvent>
#include <QToolTip>
#include <QPainter>
#include <QMenu>
#include <iostream>
#include "notelabel.hh"
#include "notegraphwidget.hh"

namespace {
	static const int text_margin = 3; // Margin of the label texts
}

const int NoteLabel::resize_margin = 5; // How many pixels is the resize area
const int NoteLabel::default_size = 100; // The preferred size of notes
const double NoteLabel::min_length = 0.01; // How many seconds minimum

NoteLabel::NoteLabel(const Note &note, QWidget *parent, bool floating)
	: QLabel(parent), m_note(note), m_selected(false), m_floating(floating), m_resizing(0), m_hotspot()
{
	QSize size;
	NoteGraphWidget *ngw = qobject_cast<NoteGraphWidget*>(parent);
	if (ngw) size.setWidth(ngw->s2px(note.length()));
	createPixmap(size);
	show(); // Already here so that height() is calculated
	updateLabel();
	setMouseTracking(true);
	setAttribute(Qt::WA_DeleteOnClose);
}

void NoteLabel::createPixmap(QSize size)
{
	QFont font;
	font.setStyleStrategy(QFont::ForceOutline);
	QFontMetrics metric(font);
	if (size.width() <= 0) {
		size.rwidth() = default_size;
	}
	if (size.height() <= 0) {
		size.rheight() = metric.size(Qt::TextSingleLine, lyric()).height() + 2 * text_margin;
	}

	QImage image(size.width(), size.height(),
				 QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(0, 0, 0, 0));

	QLinearGradient gradient(0, 0, 0, image.height()-1);
	float ff = m_floating ? 1.0f : 0.6f;
	int alpha = m_floating ? 160 : ( isSelected() ? 80 : 220 );
	gradient.setColorAt(0.0, m_floating ? QColor(255, 255, 255, alpha) : QColor(50, 50, 50, alpha));
	if (m_note.type == Note::NORMAL) {
		gradient.setColorAt(0.2, QColor(100 * ff, 100 * ff, 255 * ff, alpha));
		gradient.setColorAt(0.8, QColor(100 * ff, 100 * ff, 255 * ff, alpha));
		gradient.setColorAt(1.0, QColor(100 * ff, 100 * ff, 200 * ff, alpha));
	} else if (m_note.type == Note::GOLDEN) {
		gradient.setColorAt(0.2, QColor(255 * ff, 255 * ff, 100 * ff, alpha));
		gradient.setColorAt(0.8, QColor(255 * ff, 255 * ff, 100 * ff, alpha));
		gradient.setColorAt(1.0, QColor(160 * ff, 160 * ff, 100 * ff, alpha));
	} else if (m_note.type == Note::FREESTYLE) {
		gradient.setColorAt(0.2, QColor(100 * ff, 180 * ff, 100 * ff, alpha));
		gradient.setColorAt(0.8, QColor(100 * ff, 180 * ff, 100 * ff, alpha));
		gradient.setColorAt(1.0, QColor(100 * ff, 120 * ff, 100 * ff, alpha));
	}

	QPainter painter;
	painter.begin(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(isSelected() ? Qt::red : Qt::black); // Hilight selected note
	painter.setBrush(gradient);
	painter.drawRoundedRect(QRectF(0.5, 0.5, image.width()-1, image.height()-1), 8, 8);

	painter.setFont(font);
	painter.drawText(QRect(QPoint(text_margin, text_margin), QSize(size.width()-text_margin, size.height()-text_margin)), Qt::AlignCenter, lyric());

	// Render sentence end indicator
	if (m_note.lineBreak) {
		painter.setPen(QPen(QBrush(QColor(255, 0, 0)), 4));
		painter.drawLine(2, 0, 2, image.height()-1);
	}

	painter.end();

	setPixmap(QPixmap::fromImage(image));

	setStatusTip(tr("Lyric: ") + lyric());
}

void NoteLabel::setSelected(bool state) {
	if (m_selected != state) {
		m_selected = state; createPixmap(size());
		if (!m_selected) {
			startResizing(0); // Reset
			startDragging(QPoint()); // Reset
		}
	}
}

void NoteLabel::resizeEvent(QResizeEvent *event)
{
	createPixmap(event->size());
}

void NoteLabel::mouseMoveEvent(QMouseEvent *event)
{
	NoteGraphWidget* ngw = qobject_cast<NoteGraphWidget*>(parent());
	if (m_resizing != 0 && ngw) {
		// Resizing
		if (m_resizing < 0 && width() - event->pos().x() > ngw->s2px(min_length))
			setGeometry(x() + event->pos().x(), y(), width() - event->pos().x(), height());
		else if (m_resizing > 0 && event->pos().x() > ngw->s2px(min_length))
			resize(event->pos().x(), height());
		updateNote();
		ngw->updateNotes(m_resizing > 0);

	} else if (!m_hotspot.isNull() && ngw) {
		// Moving
		QPoint newpos = pos() + event->pos() - m_hotspot;
		int dx = newpos.x() - pos().x(), dy = newpos.y() - pos().y();
		NoteLabels& labels = ngw->selectedNotes();
		for (int i = 0; i < labels.size(); ++i) {
			NoteLabel *n = labels[i];
			n->move(n->x() + dx, ngw->n2px(int(round(ngw->px2n(n->y() + dy + height() / 2)))) - height() / 2);
			n->updateNote();
		}
		ngw->updateNotes((event->pos() - m_hotspot).x() < 0);
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
	updateNote();
	QToolTip::showText(event->globalPos(), toolTip(), this);
	event->ignore(); // Propagate event to parent
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

// FIXME: This should be removed and updateLabel used everywhere instead
void NoteLabel::updateNote()
{
	NoteGraphWidget* ngw = qobject_cast<NoteGraphWidget*>(parent());
	if (ngw) {
		// Update note pos
		m_note.note = round(ngw->px2n(y() + height() / 2));
		m_note.begin = ngw->px2s(x());
		// Update note length
		m_note.end = m_note.begin + ngw->px2s(width());
	}
	MusicalScale ms;
	setToolTip(QString("\"%1\"\n%2\n%3\n%4 s - %5 s")
		.arg(lyric())
		.arg(m_note.typeString())
		.arg(ms.getNoteStr(ms.getNoteFreq(m_note.note)))
		.arg(QString::number(m_note.begin, 'f', 3))
		.arg(QString::number(m_note.end, 'f', 3))
		);
}

void NoteLabel::updateLabel()
{
	NoteGraphWidget* ngw = qobject_cast<NoteGraphWidget*>(parent());
	if (ngw) {
		// Update label geometry
		resize(ngw->s2px(m_note.length()), height());
		move(ngw->s2px(m_note.begin), ngw->n2px(m_note.note) - height() / 2);
	}
	MusicalScale ms;
	setToolTip(QString("\"%1\"\n%2\n%3\n%4 s - %5 s")
		.arg(lyric())
		.arg(m_note.typeString())
		.arg(ms.getNoteStr(ms.getNoteFreq(m_note.note)))
		.arg(QString::number(m_note.begin, 'f', 3))
		.arg(QString::number(m_note.end, 'f', 3))
		);
}

NoteLabel::operator Operation() const
{
	Operation op("NEW", -1); // -1 for id means auto-calculate based on position
	op << m_note.syllable << m_note.begin << m_note.end << m_note.note << m_floating << m_note.lineBreak << m_note.getTypeInt();
	return op;
}
