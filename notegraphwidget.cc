#include <QtGui>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "notelabel.hh"
#include "notegraphwidget.hh"
#include "util.hh"


namespace {
	static Operation opFromNote(const NoteLabel& note, int id) {
		Operation op("NEW");
		op << id << note.lyric() << note.x() << note.y() << note.width() << note.height() << note.isFloating();
		return op;
	}
}


const int NoteGraphWidget::noteYStep = 28;

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: QLabel(parent), m_pixPerSec(200), m_lowestNote(MusicalScale::getBaseId()), m_octaves(3),
	m_panHotSpot(), m_selectedNote(), m_selectedAction(NONE), m_actionHappened(), m_pitch("music.raw")
{
	unsigned width = m_pitch.width(), height = m_pitch.height;
	QProgressDialog progress(tr("Rendering pitch data..."), tr("&Abort"), 0, width, this);
	progress.setWindowModality(Qt::WindowModal);

	QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
	unsigned* rgba = reinterpret_cast<unsigned*>(image.bits());
	for (unsigned x = 0; x < width; ++x) {
		progress.setValue(x);
		if (progress.wasCanceled()) break;

		for (unsigned y = 0; y < height; ++y) {
			rgba[y * width + x] = m_pitch(x, y).rgba();
		}
	}
	setPixmap(QPixmap::fromImage(image));
	progress.setValue(width);

	// FIXME: Width should come from song length * pixPerSec
	setFixedSize(std::max(width, (unsigned)1024), h());

	setFocusPolicy(Qt::StrongFocus);
	setWhatsThis(tr("Note graph that displays the song notes and allows you to manipulate them."));

	updateNotes();
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
	bool first = true;
	while (!ts.atEnd()) {
		QString word;
		ts >> word;
		if (!word.isEmpty()) {
			m_notes.push_back(new NoteLabel(Note(word.toStdString()), this, QPoint(0, n2px(m_lowestNote + 12 * m_octaves - 6)), QSize(), !first));
			doOperation(opFromNote(*m_notes.back(), m_notes.size()-1), Operation::NO_EXEC);
			first = false;
		}
	}

	finalizeNewLyrics();
}

void NoteGraphWidget::setLyrics(const VocalTrack &track)
{
	clear();

	// Determine how many octaves are needed and what is the base line
	int diff = track.noteMax - track.noteMin;
	m_octaves = std::ceil(diff / 12.0f) + 1;
	m_lowestNote = clamp(track.noteMin - 6, 0, 1000);
	std::cout << "--: " << m_octaves << " " << m_lowestNote << std::endl;
	setFixedSize(s2px(track.endTime), ndiff2px(m_octaves*12));

	bool first = true;
	const Notes &notes = track.notes;
	for (Notes::const_iterator it = notes.begin(); it != notes.end(); ++it) {
		if (it->type == Note::NORMAL || it->type == Note::GOLDEN || it->type == Note::FREESTYLE) {
			m_notes.push_back(new NoteLabel(*it, this, QPoint(s2px(it->begin), n2px(it->note)),
				QSize(s2px(it->length()), 0), !first));
			doOperation(opFromNote(*m_notes.back(), m_notes.size()-1), Operation::NO_EXEC);
			first = false;
		}
	}

	updateNotes();
}

void NoteGraphWidget::finalizeNewLyrics()
{
	// Set first and last to non-floating and put the last one to the end of the song
	if (m_notes.size() > 1) {
		Operation floatop("FLOATING"); floatop << (int)m_notes.size()-1 << false;
		doOperation(floatop);
		Operation moveop("MOVE");
		moveop << (int)m_notes.size()-1 << width() - m_notes.back()->width() << m_notes.back()->y();
		doOperation(moveop);
		// Make sure there is enough room
		setFixedWidth(std::max<int>(width(), m_notes.size() * NoteLabel::min_width + m_notes.front()->width() * 2));
	}
	// Calculate floating note positions
	updateNotes();
}

int NoteGraphWidget::getNoteLabelId(NoteLabel* note) const
{
	for (int i = 0; i < m_notes.size(); ++i)
		if (m_notes[i] == note) return i;
	return -1;
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

		// Create operations for adding the new labels and deleting the old one
		int id = getNoteLabelId(child);
		Operation new1("NEW"), new2("NEW");
		new1 << id << firstst << child->pos().x() << child->pos().y() << w1 << 0 << child->isFloating();
		new2 << id+1 << secondst << child->pos().x() + w1 << child->pos().y() << child->width() - w1 << 0 << child->isFloating();
		Operation del("DEL"); del << id+2;
		Operation combiner("COMBINER"); combiner << 3; // This will combine the previous ones to one undo action
		std::cout << new1.dump() << std::endl << new2.dump() << std::endl << del.dump() << std::endl;
		doOperation(new1); doOperation(new2); doOperation(del); doOperation(combiner);

		m_selectedNote = NULL;
	}
}

void NoteGraphWidget::mouseReleaseEvent(QMouseEvent *event)
{
	(void)*event;
	if (m_selectedAction != NONE) {
		if (m_selectedNote) {
			m_selectedNote->startResizing(0);
			m_selectedNote->startDragging(QPoint());
			m_selectedNote->move(m_selectedNote->pos().x(), n2px(px2n(m_selectedNote->pos().y())));
			if (m_actionHappened) {
				// Operation for undo stack & saving
				Operation op("SETGEOM");
				op << getNoteLabelId(m_selectedNote)
					<< m_selectedNote->x() << m_selectedNote->y()
					<< m_selectedNote->width() << m_selectedNote->height();
				doOperation(op, Operation::NO_EXEC);
			}
		}
		m_selectedAction = NONE;
	}
	m_actionHappened = false;
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
		Operation op("LYRIC");
		op << getNoteLabelId(child) << text;
		doOperation(op, Operation::NO_EXEC);
	}
}

void NoteGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
	m_actionHappened = true; // We have movement, so resize/move can be accepted

	// Pan
	if (!m_panHotSpot.isNull()) {
		setCursor(QCursor(Qt::ClosedHandCursor));
		QScrollArea *scrollArea = NULL;
		if (parentWidget())
			scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parent());
		if (scrollArea) {
			QPoint diff = event->pos() - m_panHotSpot;
			QScrollBar *scrollHor = scrollArea->horizontalScrollBar();
			scrollHor->setValue(scrollHor->value() - diff.x());
			QScrollBar *scrollVer = scrollArea->verticalScrollBar();
			scrollVer->setValue(scrollVer->value() - diff.y());
			m_panHotSpot = event->pos() - diff;
		}
	}

	emit updateNoteInfo(m_selectedNote);
}


void NoteGraphWidget::keyPressEvent(QKeyEvent *event)
 {
	switch (event->key()) {
	case Qt::Key_Left: // Select note on the left
		// FIXME!
/*		if (m_selectedNote && m_notes.size() > 1  && m_selectedNote != m_notes.front()) {
			for (NoteLabels::reverse_iterator it = m_notes.rbegin(); it != m_notes.rend(); ++it) {
				if (m_selectedNote == *it) { selectNote(*(++it)); break; }
			}
		}*/
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
			Operation op("DEL");
			op << getNoteLabelId(m_selectedNote);
			doOperation(op);
			m_selectedNote = NULL;
		}
		break;
	default:
		QWidget::keyPressEvent(event);
	 }
 }

void NoteGraphWidget::doOperation(const Operation& op, Operation::OperationFlags flags)
{
	if (!(flags & Operation::NO_EXEC)) {
		std::string action = op.op().toStdString();
		if (action == "BLOCK") {
			; // No op
		} else if (action == "NEW") {
			NoteLabel *newLabel = new NoteLabel(
				Note(op.s(2).toStdString()), // Note(lyric)
				this, // parent
				QPoint(op.i(3), op.i(4)), // x,y
				QSize(op.i(5), op.i(6)), // w,h
				op.b(7) // floating
				);
			if (m_notes.isEmpty()) m_notes.push_back(newLabel);
			else m_notes.insert(op.i(1), newLabel);
		} else {
			NoteLabel *n = m_notes.at(op.i(1));
			if (n) {
				if (action == "DEL") {
					n->close();
					m_notes.removeAt(op.i(1));
				} else if (action == "SPLIT") {
					// TODO
				} else if (action == "SETGEOM") {
					n->setGeometry(op.i(2), op.i(3), op.i(4), op.i(5));
				} else if (action == "RESIZE") {
					n->resize(op.i(2), op.i(3));
				} else if (action == "MOVE") {
					n->move(op.i(2), op.i(3));
				} else if (action == "FLOATING") {
					n->setFloating(op.b(2));
				} else if (action == "LYRIC") {
					n->setLyric(op.s(2));
				} else if (action == "TYPE") {
					n->setType(op.i(2));
				}
				n->createPixmap(n->size());
			}
		}
		updateNotes();
	}
	if (!(flags & Operation::NO_EMIT))
		emit operationDone(op);
}


int NoteGraphWidget::s2px(double sec) const {
	return sec * m_pixPerSec;
}
double NoteGraphWidget::px2s(int px) const {
	return px / m_pixPerSec;
}
int NoteGraphWidget::ndiff2px(int dn) const {
	return dn * noteYStep;
}
int NoteGraphWidget::n2px(int note) const {
	int highestNote = m_lowestNote + 12 * m_octaves;
	return (highestNote - note) * noteYStep;
}
int NoteGraphWidget::px2n(int px) const {
	int highestNote = m_lowestNote + 12 * m_octaves;
	return highestNote - round(px / (float)noteYStep);
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
