#include <QMouseEvent>
#include <QKeyEvent>
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
	static Operation opFromNote(const Note& note, int id, bool floating) {
		Operation op("NEW");
		op << id << note.syllable << note.begin << note.end << note.note << floating << note.lineBreak;
		return op;
	}
}

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: NoteLabelManager(parent), m_panHotSpot(), m_seeking(), m_actionHappened(),
	m_pitch(), m_seekHandle(this), m_analyzeTimer(), m_playbackTimer(), m_playbackPos(), m_duration(10.0)
{
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
				Note note(word); note.note = 24;
				if (sentenceStart) note.lineBreak = true;
				doOperation(opFromNote(note, m_notes.size(), !firstNote));
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
		doOperation(opFromNote(*it, m_notes.size(), false));
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
		moveop << (int)m_notes.size()-1
		  << px2s(width() - m_notes.back()->width()) << px2s(width() - m_notes.back()->width() + m_notes.back()->x())
		  << px2n(m_notes.back()->y() + m_noteHalfHeight);
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

void NoteGraphWidget::timerEvent(QTimerEvent* event)
{
	if (event->timerId() == m_playbackTimer) {
		m_playbackPos += m_playbackInterval.restart();
		updateMusicPos(m_playbackPos);

	} else if (event->timerId() == m_analyzeTimer && m_pitch) {
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
	m_playbackPos = time;
	int x = s2px(m_playbackPos / 1000.0) - m_seekHandle.width() / 2;
	killTimer(m_playbackTimer);
	m_seekHandle.move(x, 0);
	if (smoothing)
		m_playbackTimer = startTimer(17); // Hope for 60 fps
	m_playbackInterval.restart();
}

void NoteGraphWidget::stopMusic()
{
	killTimer(m_playbackTimer);
}

void NoteGraphWidget::seek(int x)
{
	m_seekHandle.move(x - m_seekHandle.width()/2, 0);
	emit seeked(1000 * px2s(x));
}

void NoteGraphWidget::timeCurrent()
{
	if (selectedNote()) {
		Operation op("MOVE");
		op << getNoteLabelId(selectedNote())
			<< px2s(m_seekHandle.curx()) << px2s(m_seekHandle.curx() + selectedNote()->width())
			<< int(round(px2n(selectedNote()->y() + m_noteHalfHeight)));
		doOperation(op);
	}
}

void NoteGraphWidget::timeSyllable()
{
	timeCurrent();
	selectNextSyllable();
}

void NoteGraphWidget::timeSentence()
{
	timeCurrent();
	selectNextSentenceStart();
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
		if (selectedNote()) {
			int movecount = 0;
			for (int i = 0; i < m_selectedNotes.size(); ++i) {
				NoteLabel *n = m_selectedNotes[i];
				n->startResizing(0);
				n->startDragging(QPoint());
				if (m_actionHappened) {
					// Operation for undo stack & saving
					Operation op("MOVE");
					op << getNoteLabelId(n)
					  << px2s(n->x()) << px2s(n->x() + n->width())
					  << int(round(px2n(n->y() + m_noteHalfHeight)));
					doOperation(op, Operation::NO_EXEC);
					++movecount;
				}
			}
			// Combine to one undo operation
			if (movecount > 1) {
				Operation op("COMBINER"); op << movecount; doOperation(op);
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
		selectNextSyllable();
		return;
	}

	editLyric(child);
}

void NoteGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (!m_actionHappened) {
		m_actionHappened = true; // We have movement, so resize/move can be accepted
		// See if the note needs to be unfloated
		// TODO: Maybe unfloat all selected notes?
		if (m_selectedAction != NONE && selectedNote() && selectedNote()->isFloating()) {
			// Undo op is handled later by the MOVE constructed at drop
			selectedNote()->setFloating(false);
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

	emit updateNoteInfo(selectedNote());
}

void NoteGraphWidget::keyPressEvent(QKeyEvent *event)
{
	int k = event->key(), m = event->modifiers();

	if (k == Qt::Key_A && (m & Qt::ControlModifier)) { // Select all
		selectNote(NULL); // Clear previous
		for (int i = m_notes.size()-1; i >= 0; --i) // Traverse in reverse order to get the first note first
			selectNote(m_notes[i], false);
	} else if (k == Qt::Key_Return) { // Edit lyric
		editLyric(selectedNote());
	} else if (k == Qt::Key_Left) { // Select note on the left
		selectNextSyllable(true, (m & Qt::ControlModifier));
	} else if (k == Qt::Key_Right) { // Select note on the right
		selectNextSyllable(false, (m & Qt::ControlModifier));
	} else if (k == Qt::Key_Up) { // Move note up
		move(selectedNote(), 1);
	} else if (k == Qt::Key_Down) { // Move note down
		move(selectedNote(), -1);
	} else if (k == Qt::Key_Delete) { // Delete selected note(s)
		del(selectedNote());
	} else {
		QWidget::keyPressEvent(event);
	}
}


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
	if (!m_notes.isEmpty() && selectedNote()) {
		int id = getNoteLabelId(selectedNote());
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
			if(!lyrics.isEmpty() && m_notes[i]->note().lineBreak) lyrics += '\n';
			lyrics += m_notes[i]->lyric() + " ";
		}
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

void SeekHandle::moveEvent(QMoveEvent*)
{
	// Make handle always visible in the ScrollArea
	if (wrapToViewport && parentWidget() && parentWidget()->parentWidget()) {
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
