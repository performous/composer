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
#include "song.hh"
#include "util.hh"


namespace {
	static Operation opFromNote(const NoteLabel& note, int id) {
		Operation op("NEW");
		op << id << note.lyric() << note.x() << note.y() << note.width() << note.height() << note.isFloating();
		return op;
	}
}

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: QLabel(parent), m_noteHalfHeight(), m_panHotSpot(), m_selectedNote(), m_selectedAction(NONE), m_seeking(), m_actionHappened(), m_pitch(), m_seekHandle(this), m_pixelsPerSecond(200.0), m_duration(10.0)
{
	// Determine NoteLabel height
	NoteLabel templabel(Note(" "), NULL);
	m_noteHalfHeight = templabel.height()/2;
	templabel.close();

	setProperty("darkBackground", true);
	setStyleSheet("QLabel[darkBackground=\"true\"] { background: rgb(32, 32, 32); }");

	// Initially expanding horizontally to fill the space
	QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setSizePolicy(sp);
	setFixedHeight(768);

	setFocusPolicy(Qt::StrongFocus);
	setWhatsThis(tr("Note graph that displays the song notes and allows you to manipulate them."));

	updateNotes();
}

void NoteGraphWidget::selectNote(NoteLabel* note, bool clearPrevious)
{
	if (!note) clearPrevious = true; // NULL means allways clear all

	if (m_selectedNote) {
		// Clear all selections
		if (clearPrevious) {
			// Assumes m_selectedNote is the first note in the selection chain
			for (NoteLabel *n = m_selectedNote, *nn; n; n = nn) {
				nn = n->nextSelected; // Need to put this into temp variable because it is cleared in setSelected(false)
				n->setSelected(false);
			}
		} else {
			// Add at the beginning of the chain
			if (note && note != m_selectedNote && !note->isSelected()) {
				m_selectedNote->prevSelected = note;
				note->nextSelected = m_selectedNote;
			}
		}
	}

	// Assign new selection as the first selected note
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
	bool firstNote = true;
	while (!ts.atEnd()) {
		// We want to loop one line at the time to insert line breaks
		bool sentenceStart = true;
		QString sentence = ts.readLine();
		QTextStream ts2(&sentence, QIODevice::ReadOnly);
		while (!ts2.atEnd()) {
			QString word;
			ts2 >> word;
			if (!word.isEmpty()) {
				m_notes.push_back(new NoteLabel(Note(word), this, QPoint(0, n2px(24) - m_noteHalfHeight), QSize(), !firstNote));
				doOperation(opFromNote(*m_notes.back(), m_notes.size()-1), Operation::NO_EXEC);
				if (sentenceStart) setLineBreak(m_notes.back(), true);
				firstNote = false;
				sentenceStart = false;
			}
		}
	}

	finalizeNewLyrics();
}

void NoteGraphWidget::setLyrics(const VocalTrack &track)
{
	clearNotes();
	m_duration = std::max(m_duration, track.endTime);
	const Notes &notes = track.notes;
	for (Notes::const_iterator it = notes.begin(); it != notes.end(); ++it) {
		if (it->type == Note::SLEEP) continue;
		m_notes.push_back(new NoteLabel(*it, this, QPoint(s2px(it->begin), n2px(it->note) - m_noteHalfHeight), QSize(s2px(it->length()), 0), false));
		doOperation(opFromNote(*m_notes.back(), m_notes.size()-1), Operation::NO_EXEC);
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
		emit analyzeProgress(1000 * m_pitch->getProgress(), 1000);
		if (m_pitch->newDataAvailable()) {
			m_duration = std::max(m_duration, m_pitch->getDuration());
			update();
		}
		if (m_pitch->isFinished()) killTimer(m_analyzeTimer);
	}
}

void NoteGraphWidget::paintEvent(QPaintEvent*) {
	setFixedSize(s2px(m_duration), height());
	QScrollArea *scrollArea = NULL;
	int x1 = 0, x2 = 0;
	if (parentWidget())
		scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parent());
	if (scrollArea && scrollArea->horizontalScrollBar()) {
		x1 = scrollArea->horizontalScrollBar()->value();
		x2 = x1 + scrollArea->width();
	}
	if (m_pitch) m_pitch->paint(this, x1, x2);
}

int NoteGraphWidget::getNoteLabelId(NoteLabel* note) const
{
	for (int i = 0; i < m_notes.size(); ++i)
		if (m_notes[i] == note) return i;
	return -1;
}

void NoteGraphWidget::updateNotes(bool leftToRight)
{
	// Here happens the magic that adjusts the floating
	// notes according to the fixed ones.
	FloatingGap gap(leftToRight ? 0 : width());

	// Determine gaps between non-floating notes
	// Variable leftToRight controls the iteration direction.
	for (int i = (leftToRight ? 0 : m_notes.size()-1); i >= 0 && i < m_notes.size(); i += (leftToRight ? 1 : -1)) {
		NoteLabel *child = m_notes[i];
		if (!child) continue;

		if (child->isFloating() && i != (leftToRight ? m_notes.size()-1 : 0)) {
			// Add floating note to gap
			gap.addNote(child);

		} else {
			// Fixed note encountered, handle the gap (divide notes evenly into it)
			gap.end = child->x() + (leftToRight ? 0 : child->width());

			int d = (gap.end - gap.begin) * (leftToRight ? 1 : -1);
			if (d <= gap.minWidth()) {
				// We are at minimum width, enforce it
				int x = gap.begin - (leftToRight ? 0 : NoteLabel::min_width);
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					(*it2)->move(x, (*it2)->y());
					(*it2)->resize(NoteLabel::min_width, (*it2)->height());
					x += NoteLabel::min_width * (leftToRight ? 1 : -1);
				}
				// Also move the fixed one (probably the one being moved by user)
				child->move(gap.begin + (leftToRight ? gap.minWidth() : (-gap.minWidth() - child->width())), child->y());

			} else {
				// Calculate position and size
				double w = gap.width() / double(gap.notes.size()) * 0.9;
				double step = (gap.width() - w * gap.notes.size()) / double(gap.notes.size() + 1);
				double x = gap.begin + (leftToRight ? step : (-step - w));
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					double y = (*it2)->y();
					// Try to find optimal pitch
					if (m_pitch) y = n2px(m_pitch->guessNote(px2s(x), px2s(x + w + step), 24)) - m_noteHalfHeight;
					(*it2)->move(x, y);
					(*it2)->resize(w, (*it2)->height());
					x += (w + step) * (leftToRight ? 1 : -1);
				}
			}

			// Start a new gap
			gap = FloatingGap(child->x() + (leftToRight ? child->width() : 0));
		}
	}
}

void NoteGraphWidget::updateMusicPos(qint64 time, bool smoothing)
{
	int x = s2px(time / 1000.0) - m_seekHandle.width() / 2;
	m_seekHandle.killTimer(m_seekHandle.moveTimerId);
	m_seekHandle.move(x, 0);
	if (smoothing)
		m_seekHandle.moveTimerId = m_seekHandle.startTimer(std::ceil(px2s(1) * 1000));
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

		// Determine if it is drag or resize
		if (hotSpot.x() < NoteLabel::resize_margin || hotSpot.x() > child->width() - NoteLabel::resize_margin) {
			// Start a resize
			selectNote(child); // Resizing will deselect everything but one
			m_selectedAction = RESIZE;
			child->startResizing( (hotSpot.x() < NoteLabel::resize_margin) ? -1 : 1 );

		} else {
			// Ctrl allows selecting multiple notes for dragging
			selectNote(child, !(event->modifiers() & Qt::ControlModifier));
			m_selectedAction = MOVE;
			child->startDragging(hotSpot);
		}
		child->createPixmap(child->size());

	// Middle Click
	} else if (event->button() == Qt::MiddleButton) {
		split(child, float(hotSpot.x()) / child->width());

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
			for (NoteLabel *n = m_selectedNote; n; n = n->nextSelected) {
				n->startResizing(0);
				n->startDragging(QPoint());
			}
			if (m_actionHappened) {
				// TODO: Undo multiple note move
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

void NoteGraphWidget::split(NoteLabel *note, float ratio)
{
	if (!note) return;

	if (m_selectedNote == note)
		selectNote(NULL);

	// Cut the text
	int cutpos = int(std::ceil(note->lyric().length() * ratio));
	QString firstst = note->lyric().left(cutpos);
	QString secondst = note->lyric().right(note->lyric().length() - cutpos);
	int w1 = ratio * note->width();

	// Create operations for adding the new labels and deleting the old one
	int id = getNoteLabelId(note);
	Operation new1("NEW"), new2("NEW");
	new1 << id << firstst << note->pos().x() << note->pos().y() << w1 << 0 << note->isFloating();
	new2 << id+1 << secondst << note->pos().x() + w1 << note->pos().y() << note->width() - w1 << 0 << note->isFloating();
	Operation del("DEL"); del << id+2;
	Operation combiner("COMBINER"); combiner << 3; // This will combine the previous ones to one undo action
	doOperation(new1); doOperation(new2); doOperation(del); doOperation(combiner);
}

void NoteGraphWidget::del(NoteLabel *note)
{
	if (!note) return;

	// If delete is directed to a selected note, all selected notes will be deleted
	if (note->isSelected() && m_selectedNote) {
		note = m_selectedNote;
		m_selectedNote = NULL;
	}

	int delcount = 0;
	for (NoteLabel *n = note, *nn; n; n = nn, ++delcount) {
		nn = n->nextSelected; // Need to put this into temp variable because it is cleared in setSelected(false)
		Operation op("DEL");
		op << getNoteLabelId(n);
		doOperation(op);
	}

	// Combine to one undo operation
	if (delcount > 1) {
		Operation op("COMBINER"); op << delcount; doOperation(op);
	}
}

void NoteGraphWidget::move(NoteLabel *note, int value)
{
	if (!note) return;

	int movecount = 0;
	for (NoteLabel *n = note ; n; n = n->nextSelected, ++movecount) {
		Operation op("MOVE");
		op << getNoteLabelId(n);
		op << n->x() << int(round(px2n(n->y() + m_noteHalfHeight))) + value;
		doOperation(op);
	}

	// Combine to one undo operation
	if (movecount > 1) {
		Operation op("COMBINER"); op << movecount; doOperation(op);
	}
}

void NoteGraphWidget::setType(NoteLabel *note, int index)
{
	if (note && note->note().getTypeInt() != index) {
		Operation op("TYPE");
		op << getNoteLabelId(note) << index;
		doOperation(op);
	}
}

void NoteGraphWidget::setFloating(NoteLabel *note, bool state)
{
	if (note && note->isFloating() != state) {
		Operation op("FLOATING");
		op << getNoteLabelId(note) << state;
		doOperation(op);
	}
}

void NoteGraphWidget::setLineBreak(NoteLabel *note, bool state)
{
	if (note && note->isLineBreak() != state) {
		Operation op("LINEBREAK");
		op << getNoteLabelId(note) << state;
		doOperation(op);
	}
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
	int k = event->key(), m = event->modifiers();

	if (k == Qt::Key_A && (m & Qt::ControlModifier)) { // Select all
		selectNote(NULL); // Clear previous
		for (int i = m_notes.size()-1; i >= 0; --i) // Traverse in reverse order to get the first note first
			selectNote(m_notes[i], false);
	} else if (k == Qt::Key_Return) { // Edit lyric
		editLyric(m_selectedNote);
	} else if (k == Qt::Key_Left) { // Select note on the left
		if (m_selectedNote && m_notes.size() > 1  && m_selectedNote != m_notes.front()) {
			for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); ++it) {
				if (m_selectedNote == *it) { selectNote(*(--it), !(m & Qt::ControlModifier)); break; }
			}
		}
	} else if (k == Qt::Key_Right) { // Select note on the right
		if (m_selectedNote && m_notes.size() > 1 && m_selectedNote != m_notes.back()) {
			for (NoteLabels::iterator it = m_notes.begin(); it != m_notes.end(); ++it) {
				if (m_selectedNote == *it) { selectNote(*(++it), !(m & Qt::ControlModifier)); break; }
			}
		}
	} else if (k == Qt::Key_Up) { // Move note up
		move(m_selectedNote, 1);
	} else if (k == Qt::Key_Down) { // Move note down
		move(m_selectedNote, -1);
	} else if (k == Qt::Key_Delete) { // Delete selected note(s)
		del(m_selectedNote);
	} else {
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
					n->setSelected(false); // Remove from selection list
					n->close();
					m_notes.removeAt(op.i(1));
				} else if (action == "SETGEOM") {
					n->setGeometry(op.i(2), op.i(3), op.i(4), op.i(5));
					n->setFloating(false);
				} else if (action == "RESIZE") {
					n->resize(op.i(2), op.i(3));
					n->setFloating(false);
				} else if (action == "MOVE") {
					n->move(op.i(2), n2px(op.i(3)) - m_noteHalfHeight);
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

int NoteGraphWidget::s2px(double sec) const { return sec * m_pixelsPerSecond; }
double NoteGraphWidget::px2s(int px) const { return px / m_pixelsPerSecond; }
int NoteGraphWidget::n2px(double note) const { return height() - 16.0 * note; }
double NoteGraphWidget::px2n(int px) const { return (height() - px) / 16.0; }

VocalTrack NoteGraphWidget::getVocalTrack() const
{
	VocalTrack track(TrackName::LEAD_VOCAL);
	Notes& notes = track.notes;
	if (!m_notes.isEmpty()) {
		for (int i = 0; i < m_notes.size(); ++i) {
			notes.push_back(m_notes[i]->note());
			track.noteMin = std::min(notes.back().note, track.noteMin);
			track.noteMax = std::max(notes.back().note, track.noteMin);
		}
		track.beginTime = notes.front().begin;
		track.endTime = notes.back().end;
	}
	return track;
}

QString NoteGraphWidget::getCurrentSentence() const
{
	QString lyrics;
	if (!m_notes.isEmpty() && m_selectedNote && !m_selectedNote->nextSelected) {
		int id = getNoteLabelId(m_selectedNote);
		for (int i = id; i < m_notes.size(); ++i) {
			if (i != id && m_notes[i]->note().lineBreak) break;
			lyrics += m_notes[i]->lyric() + " ";
		}
		lyrics = lyrics.left(lyrics.size() - 1);
	}
	return lyrics;
}

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




/// SeekHandle

SeekHandle::SeekHandle(QWidget *parent)
	: QLabel(parent)
{
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




/// FloatingGap

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
