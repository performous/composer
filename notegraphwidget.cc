#include <QMouseEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
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

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: QLabel(parent), m_noteHalfHeight(), m_panHotSpot(), m_selectedNote(), m_selectedAction(NONE), m_seeking(), m_actionHappened(), m_pitch(), m_seekHandle(this)
{
	// Determine NoteLabel height
	NoteLabel templabel(Note(" "), NULL);
	m_noteHalfHeight = templabel.height()/2;
	templabel.close();

	setProperty("darkBackground", true);
	setStyleSheet("QLabel[darkBackground=\"true\"] { background: #222; }");

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

void NoteGraphWidget::clearNotes()
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

	clearNotes();
	bool first = true;
	while (!ts.atEnd()) {
		QString word;
		ts >> word;
		if (!word.isEmpty()) {
			m_notes.push_back(new NoteLabel(Note(word), this, QPoint(0, n2px(24)), QSize(), !first));
			doOperation(opFromNote(*m_notes.back(), m_notes.size()-1), Operation::NO_EXEC);
			first = false;
		}
	}

	finalizeNewLyrics();
}

void NoteGraphWidget::setLyrics(const VocalTrack &track)
{
	clearNotes();

	setFixedSize(s2px(track.endTime), h());

	const Notes &notes = track.notes;
	for (Notes::const_iterator it = notes.begin(); it != notes.end(); ++it) {
		if (it->type == Note::NORMAL || it->type == Note::GOLDEN || it->type == Note::FREESTYLE) {
			m_notes.push_back(new NoteLabel(*it, this, QPoint(s2px(it->begin), n2px(it->note)),
				QSize(s2px(it->length()), 0), false));
			doOperation(opFromNote(*m_notes.back(), m_notes.size()-1), Operation::NO_EXEC);
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

void NoteGraphWidget::analyzeMusic(QString filepath)
{
	m_pitch.reset(new PitchVis(filepath, this));
	m_analyzeTimer = startTimer(100);
}

void NoteGraphWidget::timerEvent(QTimerEvent*)
{
	if (m_pitch) {
		QMutexLocker locker(&m_pitch->mutex);
		emit analyzeProgress(m_pitch->getXValue(), width());
		if (m_pitch->newDataAvailable()) update();
	}
}

void NoteGraphWidget::paintEvent(QPaintEvent*) {
	if (m_pitch) {
		QMutexLocker locker(&m_pitch->mutex);
		QPainter painter;
		painter.begin(this);
		painter.setRenderHint(QPainter::Antialiasing);
		QPen pen;
		pen.setColor(Qt::gray);
		pen.setWidth(8);
		pen.setCapStyle(Qt::RoundCap);
		painter.setPen(pen);
		PitchVis::Paths const& paths = m_pitch->getPaths();
		for (PitchVis::Paths::const_iterator it = paths.begin(), itend = paths.end(); it != itend; ++it) {
			painter.drawPath(*it);
		}
		painter.end();
	}
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

void NoteGraphWidget::updateMusicPos(qint64 time, bool smoothing)
{
	int x = s2px(time / 1000.0) - m_seekHandle.width() / 2;
	m_seekHandle.killTimer(m_seekHandle.moveTimerId);
	m_seekHandle.move(x, 0);
	if (smoothing)
		m_seekHandle.moveTimerId = m_seekHandle.startTimer(px2s(1) * 1000);
}

void NoteGraphWidget::stopMusic()
{
	m_seekHandle.killTimer(m_seekHandle.moveTimerId);
}

void NoteGraphWidget::seek(int x)
{
	m_seekHandle.move(x - m_seekHandle.width()/2, 0);
	emit seeked(1000 * px2s(x));
}

void NoteGraphWidget::timeCurrent()
{
	if (m_selectedNote) {
		// TODO: Automatic pitch detection
		Operation op("MOVE");
		op <<  getNoteLabelId(m_selectedNote)
			<< m_seekHandle.curx() << m_selectedNote->y();
		doOperation(op);
	}
}

void NoteGraphWidget::timeSyllable()
{
	timeCurrent();
	selectNextSyllableAfterSeekHandle();
}

void NoteGraphWidget::timeSentence()
{
	timeCurrent();
	selectNextSentenceStart();
}

void NoteGraphWidget::selectNextSyllableAfterSeekHandle()
{
	bool currentFound = (m_selectedNote ? false : true);
	if (m_notes.size() > 1 && m_selectedNote != m_notes.back()) {
		for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); ++it) {
			if (m_selectedNote == *it) currentFound = true;
			else if (currentFound && (*it)->x() > m_seekHandle.curx()) {
				selectNote(*it); break;
			}
		}
	}
}

void NoteGraphWidget::selectNextSentenceStart()
{
	bool currentFound = (m_selectedNote ? false : true);
	NoteLabel *prev = NULL;
	if (m_notes.size() > 1 && m_selectedNote != m_notes.back()) {
		for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); prev = *it, ++it) {
			if (m_selectedNote == *it) currentFound = true;
			else if (currentFound && (*it)->x() > m_seekHandle.curx()
					 && prev && prev->note().lineBreak) {
				selectNote(*it); break;
			}
		}
	}
}


void NoteGraphWidget::mousePressEvent(QMouseEvent *event)
{
	NoteLabel *child = qobject_cast<NoteLabel*>(childAt(event->pos()));
	if (!child) {
		SeekHandle *seekh = qobject_cast<SeekHandle*>(childAt(event->pos()));
		if (!seekh) {
			// Left click empty area = pan
			if (event->button() == Qt::LeftButton)
				m_panHotSpot = event->pos();
			// Right click empty area = deselect
			if (event->button() == Qt::RightButton)
				selectNote(NULL);
		} else {
			// Seeking
			m_seeking = true;
			setCursor(QCursor(Qt::SizeHorCursor));
		}
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
		event->ignore();
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
				op <<  getNoteLabelId(m_selectedNote)
					<< m_selectedNote->x() << m_selectedNote->y()
					<< m_selectedNote->width() << m_selectedNote->height();
				doOperation(op, Operation::NO_EXEC);
			}
		}
		m_selectedAction = NONE;
	}
	m_actionHappened = false;
	m_panHotSpot = QPoint();
	m_seeking = false;
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
	if (!child) {
		// Double click empty space = seek there
		seek(event->x());
		selectNextSyllableAfterSeekHandle();
		return;
	}

	editLyric(child);
}

void NoteGraphWidget::split(NoteLabel *note)
{
	if (!note) return;

	// Cut the text in half
	float relRatio = 0.5; //float(hotSpot.x()) / note->width();
	int cutpos = int(std::ceil(note->lyric().length() * relRatio));
	QString firstst = note->lyric().left(cutpos);
	QString secondst = note->lyric().right(note->lyric().length() - cutpos);
	int w1 = relRatio * note->width();

	// Create operations for adding the new labels and deleting the old one
	int id = getNoteLabelId(note);
	Operation new1("NEW"), new2("NEW");
	new1 << id << firstst << note->pos().x() << note->pos().y() << w1 << 0 << note->isFloating();
	new2 << id+1 << secondst << note->pos().x() + w1 << note->pos().y() << note->width() - w1 << 0 << note->isFloating();
	Operation del("DEL"); del << id+2;
	Operation combiner("COMBINER"); combiner << 3; // This will combine the previous ones to one undo action
	doOperation(new1); doOperation(new2); doOperation(del); doOperation(combiner);

	m_selectedNote = NULL;
}

void NoteGraphWidget::del(NoteLabel *note)
{
	if (!note) return;

	Operation op("DEL");
	op << getNoteLabelId(m_selectedNote);
	doOperation(op);
	m_selectedNote = NULL;
}

void NoteGraphWidget::editLyric(NoteLabel *note) {
	if (!note) return;

	// Spawn an input dialog
	bool ok;
	QString text = QInputDialog::getText(this, tr("Edit lyric"),
										tr("Lyric:"), QLineEdit::Normal,
										note->lyric(), &ok);
	if (ok && !text.isEmpty()) {
		note->setLyric(text);
		// Create undo operation
		Operation op("LYRIC");
		op << getNoteLabelId(note) << text;
		doOperation(op, Operation::NO_EXEC);
	}
}

void NoteGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (!m_actionHappened) {
		m_actionHappened = true; // We have movement, so resize/move can be accepted
		// See if the note needs to be unfloated
		if (m_selectedAction != NONE && m_selectedNote && m_selectedNote->isFloating()) {
			// Undo op is handled later by the SETGEOM constructed at drop
			m_selectedNote->setFloating(false);
		}
	}

	// Seeking
	if (m_seeking)
		seek(event->x());

	// Pan
	else if (!m_panHotSpot.isNull()) {
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
	case Qt::Key_Return: // Edit lyric
		editLyric(m_selectedNote);
		break;
	case Qt::Key_Left: // Select note on the left
		if (m_selectedNote && m_notes.size() > 1  && m_selectedNote != m_notes.front()) {
			for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); ++it) {
				if (m_selectedNote == *it) { selectNote(*(--it)); break; }
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
			Operation op("MOVE");
			op << getNoteLabelId(m_selectedNote);
			op << m_selectedNote->x() << n2px(px2n(m_selectedNote->y()) + 1);
			doOperation(op);
		}
		break;
	case Qt::Key_Down: // Move note down
		if (m_selectedNote) {
			Operation op("MOVE");
			op << getNoteLabelId(m_selectedNote);
			op << m_selectedNote->x() << n2px(px2n(m_selectedNote->y()) - 1);
			doOperation(op);
		}
		break;
	case Qt::Key_Delete: // Delete selected note
		del(m_selectedNote);
		break;
	default:
		QWidget::keyPressEvent(event);
	 }
 }

void NoteGraphWidget::doOperation(const Operation& op, Operation::OperationFlags flags)
{
	if (!(flags & Operation::NO_EXEC)) {
		QString action = op.op();
		if (action == "BLOCK" || action == "COMBINER") {
			; // No op
		} else if (action == "NEW") {
			NoteLabel *newLabel = new NoteLabel(
				Note(op.s(2)), // Note(lyric)
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
				} else if (action == "SETGEOM") {
					n->setGeometry(op.i(2), op.i(3), op.i(4), op.i(5));
					n->setFloating(false);
				} else if (action == "RESIZE") {
					n->resize(op.i(2), op.i(3));
					n->setFloating(false);
				} else if (action == "MOVE") {
					n->move(op.i(2), op.i(3));
					n->setFloating(false);
				} else if (action == "FLOATING") {
					n->setFloating(op.b(2));
				} else if (action == "LINEBREAK") {
					n->setLineBreak(op.b(2));
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
	if (!(flags & Operation::NO_EMIT)) {
		emit operationDone(op);
		emit updateNoteInfo(m_selectedNote);
	}
}

// FIXME: duplicated functionality with PitchVis and funny handling for m_pitch == NULL
int NoteGraphWidget::s2px(double sec) const { return m_pitch ? m_pitch->time2px(sec) : sec * 345.0; }
double NoteGraphWidget::px2s(int px) const { return m_pitch ? m_pitch->px2time(px) : px / 345.0; }
int NoteGraphWidget::n2px(int note) const { return PitchVis::note2px(note) - m_noteHalfHeight; }
int NoteGraphWidget::px2n(int px) const { return PitchVis::px2note(px + m_noteHalfHeight); }


QString NoteGraphWidget::dumpLyrics() const
{
	QString lyrics;
	if (!m_notes.isEmpty()) {
		for (int i = 0; i < m_notes.size(); ++i) {
			lyrics += m_notes[i]->lyric() + " ";
			if (m_notes[i]->note().lineBreak)
				lyrics.replace(lyrics.size()-1, 1, QString("\n"));
		}
		lyrics.replace(lyrics.size()-1, 1, QString("\n"));
	}
	return lyrics;
}



SeekHandle::SeekHandle(QWidget *parent)
	: QLabel(parent)
{
	// FIXME: height from notegraph
	QImage image(16, 768, QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(0, 0, 0, 0));
	QLinearGradient gradient(0, 0, image.width()-1, 0);
	gradient.setColorAt(0.00, QColor(255,255,0,0));
	gradient.setColorAt(0.25, QColor(255,255,0,0));
	gradient.setColorAt(0.50, QColor(255,255,0,200));
	gradient.setColorAt(0.75, QColor(255,255,0,0));
	gradient.setColorAt(1.00, QColor(255,255,0,0));

	QPainter painter;
	painter.begin(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setBrush(gradient);
	painter.setPen(Qt::NoPen);
	painter.drawRect(QRect(0, 0, image.width(), image.height()));

	setPixmap(QPixmap::fromImage(image));
	setMouseTracking(true);
	setStatusTip(tr("Seek by dragging"));
}

void SeekHandle::mouseMoveEvent(QMouseEvent *event)
{
	setCursor(QCursor(Qt::SizeHorCursor));
	event->ignore();
}

void SeekHandle::timerEvent(QTimerEvent*)
{
	move(x() + 1, 0);

	// Make handle always visible in the ScrollArea
	if (parentWidget() && parentWidget()->parentWidget()) {
		QScrollArea *scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parentWidget()->parent());
		if (scrollArea) {
			QScrollBar *scrollVer = scrollArea->verticalScrollBar();
			int y = 0;
			if (scrollVer) y = scrollVer->value();
			scrollArea->ensureVisible(x() + scrollArea->width()/3, y, scrollArea->width()/3, 0);
		}
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
