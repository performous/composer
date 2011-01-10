#include <QtGui>
#include <iostream>
#include <cmath>
#include "notelabel.hh"
#include "notegraphwidget.hh"
#include "util.hh"


const int NoteGraphWidget::noteYStep = 40;

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: QWidget(parent), m_panHotSpot(), m_selectedNote(), m_selectedAction(NONE)
{
	setFocusPolicy(Qt::StrongFocus);
	setLyrics("Please add music file and lyrics text.");
	emit updateNoteInfo(NULL);
}

void NoteGraphWidget::selectNote(NoteLabel* note)
{
	if (m_selectedNote) // Reset old pixmap
		m_selectedNote->setSelected(false);
	// Assign new selection
	m_selectedNote = note;
	if (m_selectedNote)
		m_selectedNote->setSelected();
	else m_selectedAction = NONE;
	// Signal UI about the change
	emit updateNoteInfo(m_selectedNote);
}

void NoteGraphWidget::clear()
{
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
	while (!ts.atEnd()) {
		QString word;
		ts >> word;
		if (!word.isEmpty()) {
			NoteLabel *wordLabel = new NoteLabel(word, this, QPoint(x, y));
			x += wordLabel->width() + gap;
		}
	}

	rebuildNoteList();
	// Set first and last to non-floating
	m_notes.front()->disableFloating();
	m_notes.back()->disableFloating();

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
		NoteLabel *child = *it; //dynamic_cast<NoteLabel*>(*it);
		if (!child) continue;
		//std::cout << "Child, floating: " << child->isFloating() << std::endl;
		if (child->isFloating()) {
			// Add floating note to gap
			gap.addNote(child);
		} else {
			// Fixed note encountered, handle the gap (divide notes evenly into it)
			std::cout << "Gap end, width: " << gap.width() << ", notes: " << gap.notes.size() << std::endl;
			if (!gap.isEmpty()) {
				gap.end = child->x();
				int step = gap.width() / (gap.notes.size() + 1);
				int x = gap.begin + step * 0.5;
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2, x += step) {
					(*it2)->move(x, (*it2)->y());
				}
			}
			// Start a new gap
			gap = FloatingGap(child->x() + child->width());
			std::cout << "New gap from: " << gap.begin << std::endl;
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
		return;
	}

	QPoint hotSpot = event->pos() - child->pos();

	// Left Click
	if (event->button() == Qt::LeftButton) {

		selectNote(child);
		// Determine if it is drag or resize
		if (hotSpot.x() < NoteLabel::resize_margin || hotSpot.x() > child->width() - NoteLabel::resize_margin) {
			// Start a resize
			m_selectedAction = RESIZE;
			child->startResizing( (hotSpot.x() < NoteLabel::resize_margin) ? -1 : 1 );
			child->disableFloating();

		} else {
			// Start a drag
			m_selectedAction = MOVE;
			child->startDragging(hotSpot);
			child->disableFloating();
		}
		child->createPixmap(child->size());

	// Right Click
	} else if (event->button() == Qt::RightButton) {

		// Cut the text in a position proportional to the click point
		float relRatio = float(hotSpot.x()) / child->width();
		int cutpos = int(std::ceil(child->getText().length() * relRatio));
		QString firstst = child->getText().left(cutpos);
		QString secondst = child->getText().right(child->getText().length() - cutpos);
		int w1 = relRatio * child->width();

		// Create new labels
		NoteLabel *newLabel1 = new NoteLabel(firstst, this, child->pos(), QSize(w1, 0));
		new NoteLabel(secondst, this, newLabel1->pos() + QPoint(newLabel1->width(), 0), QSize(child->width() - w1, 0));

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
										  child->getText(), &ok);
	if (ok && !text.isEmpty()) {
		child->setText(text);
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
			scrollHor->setValue(scrollHor->value() + diff.x());
			QScrollBar *scrollVer = scrollArea->horizontalScrollBar();
			scrollVer->setValue(scrollVer->value() + diff.y());
			m_panHotSpot = event->pos() + diff;
		}
	}
}


void NoteGraphWidget::keyPressEvent(QKeyEvent *event)
 {
	 switch (event->key()) {
	 case Qt::Key_Delete:
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
}
