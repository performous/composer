#include <QtGui>
#include <iostream>
#include <cmath>
#include "notelabel.hh"
#include "notegraphwidget.hh"
#include "util.hh"


const int NoteGraphWidget::noteYStep = 40;

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: QLabel(parent), m_panHotSpot(), m_selectedNote(), m_selectedAction(NONE), m_pitch("music.raw")
{
	unsigned width = m_pitch.width(), height = m_pitch.height;
	QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
	unsigned* rgba = reinterpret_cast<unsigned*>(image.bits());
	for (unsigned x = 0; x < width; ++x) {
		for (unsigned y = 0; y < height; ++y) {
			rgba[y * width + x] = m_pitch(x, y).rgba();
		}
	}
	setPixmap(QPixmap::fromImage(image));

	setFocusPolicy(Qt::StrongFocus);
	setWhatsThis(tr("Note graph that displays the song notes and allows you to manipulate them."));
	setLyrics(tr("Please add music file and lyrics text."));
	emit updateNoteInfo(NULL);
}

void NoteGraphWidget::selectNote(NoteLabel* note)
{
	if (m_selectedNote) // Reset old pixmap
		m_selectedNote->setSelected(false);

	// Assign new selection
	m_selectedNote = note;
	if (m_selectedNote) {
		m_selectedNote->setSelected();
	} else m_selectedAction = NONE;

	// Signal UI about the change
	emit updateNoteInfo(m_selectedNote);
}

void NoteGraphWidget::clear()
{
	selectNote(NULL);
	// Clear NoteLabels
	const QObjectList &childlist = children();
	for (QObjectList::const_iterator it = childlist.begin(); it != childlist.end(); ++it) {
		NoteLabel *child = dynamic_cast<NoteLabel*>(*it);
		if (child) child->close();
	}
	m_notes.clear();
}

void NoteGraphWidget::setLyrics(QString lyrics)
{
	QTextStream ts(&lyrics, QIODevice::ReadOnly);
	const int gap = 10;
	int x = gap, y = 2 * noteYStep;

	clear();
	NoteLabel *first = NULL, *last = NULL;
	while (!ts.atEnd()) {
		QString word;
		ts >> word;
		if (!word.isEmpty()) {
			NoteLabel *wordLabel = new NoteLabel(Note(word.toStdString()), this, QPoint(x, y));
			last = wordLabel;
			x += wordLabel->width() + gap;
			if (!first) first = wordLabel;
		}
	}

	// Set first and last to non-floating
	if (first) first->setFloating(false);
	if (last) last->setFloating(false);

	rebuildNoteList();
	m_requiredWidth = x + gap;
	updateWidth();
}

void NoteGraphWidget::setLyrics(const Notes &notes)
{
	const int gap = 10;
	int x = gap, y = 2 * noteYStep;

	clear();
	NoteLabel *first = NULL, *last = NULL;
	for (Notes::const_iterator it = notes.begin(); it != notes.end(); ++it) {
		NoteLabel *label = new NoteLabel(*it, this, QPoint(x, y));
		last = label;
		x += label->width() + gap;
		if (!first) first = label;
	}

	// Set first and last to non-floating
	if (first) first->setFloating(false);
	if (last) last->setFloating(false);

	rebuildNoteList();
	m_requiredWidth = x + gap;
	updateWidth();
}

void NoteGraphWidget::updateWidth()
{
	setFixedWidth(m_requiredWidth);
}

void NoteGraphWidget::updateNotes()
{
	// Here happens the magic that adjusts the floating
	// notes according to the fixed ones.
	FloatingGap gap(0);
	rebuildNoteList();

	// Determine gaps between non-floating notes
	for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); ++it) {
		NoteLabel *child = *it;
		if (!child) continue;

		if (child->isFloating() && child != m_notes.back()) {
			// Add floating note to gap
			gap.addNote(child);

		} else {
			// Fixed note encountered, handle the gap (divide notes evenly into it)
			gap.end = child->x();
			int x = gap.begin;

			if (gap.width() >= gap.notesWidth()) {
				// Plenty of space - no resizing needed
				int step = (gap.width() - gap.notesWidth()) / (gap.notes.size() + 1);
				x += step;
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					(*it2)->move(x, (*it2)->y());
					x += step + (*it2)->width();
				}

			} else if (gap.width() <= gap.minWidth()) {
				// We are at minimum width, enforce it
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					(*it2)->move(x, (*it2)->y());
					(*it2)->resize(NoteLabel::min_width, (*it2)->height());
					x += NoteLabel::min_width;
				}
				// FIXME: Enforcing fixed note position can be cheated by rapid mouse movement
				// Also, left & right side behaves differently
				child->move(gap.begin + gap.minWidth(), child->y());

			} else {
				// Make the notes smaller so that they fit
				float sf = gap.width() / float(gap.notesWidth());
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					(*it2)->move(x, (*it2)->y());
					(*it2)->resize((*it2)->width() * sf, (*it2)->height());
					x += (*it2)->width();
				}
			}

			// Start a new gap
			gap = FloatingGap(child->x() + child->width());
		}
	}
}

void NoteGraphWidget::rebuildNoteList()
{
	m_notes.clear();
	const QObjectList &childlist = children();
	for (QObjectList::const_iterator it = childlist.begin(); it != childlist.end(); ++it) {
		NoteLabel *child = dynamic_cast<NoteLabel*>(*it);
		if (child) m_notes.push_back(child);
	}
	m_notes.sort(cmpNoteLabelPtr);
}

void NoteGraphWidget::mousePressEvent(QMouseEvent *event)
{
	NoteLabel *child = static_cast<NoteLabel*>(childAt(event->pos()));
	if (!child) {
		// Left click empty area = pan
		if (event->button() == Qt::LeftButton)
			m_panHotSpot = event->pos();
		// Right click empty area = deselect
		if (event->button() == Qt::RightButton)
			selectNote(NULL);
		return;
	}
	if (child->isHidden()) return;

	QPoint hotSpot = event->pos() - child->pos();

	// Left Click
	if (event->button() == Qt::LeftButton) {

		selectNote(child);
		// Determine if it is drag or resize
		if (hotSpot.x() < NoteLabel::resize_margin || hotSpot.x() > child->width() - NoteLabel::resize_margin) {
			// Start a resize
			m_selectedAction = RESIZE;
			child->startResizing( (hotSpot.x() < NoteLabel::resize_margin) ? -1 : 1 );

		} else {
			// Start a drag
			m_selectedAction = MOVE;
			child->startDragging(hotSpot);
		}
		child->createPixmap(child->size());

	// Right Click
	} else if (event->button() == Qt::RightButton) {

		// Cut the text in a position proportional to the click point
		float relRatio = float(hotSpot.x()) / child->width();
		int cutpos = int(std::ceil(child->lyric().length() * relRatio));
		QString firstst = child->lyric().left(cutpos);
		QString secondst = child->lyric().right(child->lyric().length() - cutpos);
		int w1 = relRatio * child->width();

		// Create new labels
		NoteLabel *newLabel1 = new NoteLabel(Note(firstst.toStdString()), this, child->pos(), QSize(w1, 0));
		new NoteLabel(Note(secondst.toStdString()), this, newLabel1->pos() + QPoint(newLabel1->width(), 0), QSize(child->width() - w1, 0));

		// Delete the old one
		child->close();
	}
}

void NoteGraphWidget::mouseReleaseEvent(QMouseEvent *event)
{
	(void)*event;
	if (m_selectedAction != NONE) {
		if (m_selectedNote) {
			m_selectedNote->startResizing(0);
			m_selectedNote->startDragging(QPoint());
			m_selectedNote->move(m_selectedNote->pos().x(), int(round(m_selectedNote->pos().y() / float(noteYStep))) * noteYStep);
		}
		m_selectedAction = NONE;
	}
	m_panHotSpot = QPoint();
	setCursor(QCursor());
	updateNotes();
}

void NoteGraphWidget::wheelEvent(QWheelEvent *event)
{
	NoteLabel *child = static_cast<NoteLabel*>(childAt(event->pos()));
	if (!child)
		return;

	// Figure out new size and apply it
	int neww = child->size().width() + event->delta() * 0.1;
	child->resize(neww, child->size().height());

	updateNotes();

	event->accept();
}

void NoteGraphWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	NoteLabel *child = static_cast<NoteLabel*>(childAt(event->pos()));
	if (!child)
		return;

	// Spawn an input dialog
	bool ok;
	QString text = QInputDialog::getText(this, tr("Edit lyric"),
										  tr("Lyric:"), QLineEdit::Normal,
										  child->lyric(), &ok);
	if (ok && !text.isEmpty()) {
		child->setLyric(text);
		child->createPixmap();
	}

	event->accept();
}

void NoteGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
	// Pan
	if (!m_panHotSpot.isNull()) {
		setCursor(QCursor(Qt::ClosedHandCursor));
		QScrollArea *scrollArea = NULL;
		if (parentWidget() && parentWidget()->parent())
			scrollArea = dynamic_cast<QScrollArea*>(parentWidget()->parent()->parent());
		if (scrollArea) {
			QPoint diff = event->pos() - m_panHotSpot;
			QScrollBar *scrollHor = scrollArea->horizontalScrollBar();
			scrollHor->setValue(scrollHor->value() - diff.x());
			QScrollBar *scrollVer = scrollArea->verticalScrollBar();
			scrollVer->setValue(scrollVer->value() - diff.y());
			m_panHotSpot = event->pos() - diff;
		}
	}
}


void NoteGraphWidget::keyPressEvent(QKeyEvent *event)
 {
	switch (event->key()) {
	case Qt::Key_Left: // Select note on the left
		if (m_selectedNote && m_notes.size() > 1  && m_selectedNote != m_notes.front()) {
			for (NoteLabels::reverse_iterator it = m_notes.rbegin(); it != m_notes.rend(); ++it) {
				if (m_selectedNote == *it) { selectNote(*(++it)); break; }
			}
		}
		break;
	case Qt::Key_Right: // Select note on the right
		if (m_selectedNote && m_notes.size() > 1 && m_selectedNote != m_notes.back()) {
			for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); ++it) {
				if (m_selectedNote == *it) { selectNote(*(++it)); break; }
			}
		}
		break;
	case Qt::Key_Up: // Move note up
		if (m_selectedNote) {
			m_selectedNote->move(m_selectedNote->x(), m_selectedNote->y() - noteYStep);
		}
		break;
	case Qt::Key_Down: // Move note down
		if (m_selectedNote) {
			m_selectedNote->move(m_selectedNote->x(), m_selectedNote->y() + noteYStep);
		}
		break;
	case Qt::Key_Delete: // Delete selected note
		if (m_selectedNote) {
			m_selectedNote->close();
			m_selectedNote = NULL;
			updateNotes();
		}
		break;
	default:
		QWidget::keyPressEvent(event);
	 }
 }



void FloatingGap::addNote(NoteLabel* n)
{
	notes.push_back(n);
	end = n->x();
	m_notesWidth += n->width();
}

int FloatingGap::minWidth() const
{
	return notes.size() * NoteLabel::min_width;
}
