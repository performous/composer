#include <QtGui>
#include <iostream>
#include <algorithm>
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

	// FIXME: Width should come from song length * pixPerSec
	setFixedSize(std::max(width, (unsigned)1024), height);

	setFocusPolicy(Qt::StrongFocus);
	setWhatsThis(tr("Note graph that displays the song notes and allows you to manipulate them."));
	setLyrics(tr("Please add music file and lyrics text."));

	// We want the initial text to be completely visible on screen
	// FIXME: This should be handled with more robustness and elegance
	m_notes.back()->move(600, m_notes.back()->y());
	updateNotes();

	// Signal the UI to update itself
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
		NoteLabel *child = qobject_cast<NoteLabel*>(*it);
		if (child) child->close();
	}
	m_notes.clear();
}

void NoteGraphWidget::setLyrics(QString lyrics)
{
	QTextStream ts(&lyrics, QIODevice::ReadOnly);

	clear();
	while (!ts.atEnd()) {
		QString word;
		ts >> word;
		if (!word.isEmpty())
			m_notes.push_back(new NoteLabel(Note(word.toStdString()), this, QPoint(0, 2 * noteYStep)));
	}

	finalizeNewLyrics();
}

void NoteGraphWidget::setLyrics(const Notes &notes)
{
	clear();
	for (Notes::const_iterator it = notes.begin(); it != notes.end(); ++it) {
		m_notes.push_back(new NoteLabel(*it, this, QPoint(0, 2 * noteYStep)));
	}

	finalizeNewLyrics();
}

void NoteGraphWidget::finalizeNewLyrics()
{
	// Set first and last to non-floating and put the last one to the end of the song
	if (m_notes.size() > 0) {
		m_notes.front()->setFloating(false);
		if (m_notes.size() > 1) {
			m_notes.back()->setFloating(false);
			m_notes.back()->move(width() - m_notes.back()->width(), m_notes.back()->y());
		}
		// Make sure there is enough room
		setFixedWidth(std::max<int>(width(), m_notes.size() * NoteLabel::min_width + m_notes.front()->width() * 2));
	}
	// Calculate floating note positions
	updateNotes();
}

void NoteGraphWidget::updateNotes()
{
	// Here happens the magic that adjusts the floating
	// notes according to the fixed ones.
	FloatingGap gap(0);

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

			if (gap.width() <= gap.minWidth()) {
				// We are at minimum width, enforce it
				int x = gap.begin;
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					(*it2)->move(x, (*it2)->y());
					(*it2)->resize(NoteLabel::min_width, (*it2)->height());
					x += NoteLabel::min_width;
				}
				// FIXME: Enforcing fixed note position can be cheated by rapid mouse movement
				// Also, left & right side behave differently
				child->move(gap.begin + gap.minWidth(), child->y());

			} else {
				// Calculate position and size
				double w = gap.width() / double(gap.notes.size()) * 0.9;
				double step = (gap.width() - w * gap.notes.size()) / double(gap.notes.size() + 1);
				double x = gap.begin + step;
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					(*it2)->move(x, (*it2)->y());
					(*it2)->resize(w, (*it2)->height());
					x += w + step;
				}
			}

			// Start a new gap
			gap = FloatingGap(child->x() + child->width());
		}
	}
}

void NoteGraphWidget::mousePressEvent(QMouseEvent *event)
{
	NoteLabel *child = qobject_cast<NoteLabel*>(childAt(event->pos()));
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
		NoteLabel *newLabel2 = new NoteLabel(Note(secondst.toStdString()), this, newLabel1->pos() + QPoint(newLabel1->width(), 0), QSize(child->width() - w1, 0));
		// Insert them to the list removing the old one
		NoteLabels::iterator it = std::find(m_notes.begin(), m_notes.end(), child);
		if (it != m_notes.end()) {
			m_notes.insert(it, newLabel1);
			m_notes.insert(it, newLabel2);
			m_notes.erase(it);
		} else {
			m_notes.push_back(newLabel1);
			m_notes.push_back(newLabel2);
		}

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
	event->ignore();
}

void NoteGraphWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	NoteLabel *child = qobject_cast<NoteLabel*>(childAt(event->pos()));
	if (!child)
		return;

	// Spawn an input dialog
	bool ok;
	QString text = QInputDialog::getText(this, tr("Edit lyric"),
										tr("Lyric:"), QLineEdit::Normal,
										child->lyric(), &ok);
	if (ok && !text.isEmpty()) {
		child->setLyric(text);
		child->createPixmap(child->size());
	}
}

void NoteGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
	// Pan
	if (!m_panHotSpot.isNull()) {
		setCursor(QCursor(Qt::ClosedHandCursor));
		QScrollArea *scrollArea = NULL;
		if (parentWidget() && parentWidget()->parent())
			scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parent()->parent());
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

void NoteGraphWidget::doOperation(const Operation& op, Operation::OperationFlags flags)
{
	if (!(flags & Operation::NO_EXEC)) {
		// TODO: This should perform the operation
	}
	if (!(flags & Operation::NO_EMIT))
		emit operationDone(op);
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
