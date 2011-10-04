#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QMenu>
#include <QToolTip>
#include <QMessageBox>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "notelabel.hh"
#include "notegraphwidget.hh"
#include "song.hh"
#include "util.hh"
#include "busydialog.hh"


namespace {
	static Operation opFromNote(const Note& note, int id, bool floating) {
		Operation op("NEW");
		op << id << note.syllable << note.begin << note.end << note.note << floating << note.lineBreak << note.getTypeInt();
		return op;
	}

	static const double endMarginSeconds = 5.0;
}

/*static*/ const int NoteGraphWidget::Height = 768;
/*static*/ const QString NoteGraphWidget::BGColor = "#222";

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: NoteLabelManager(parent), m_mouseHotSpot(), m_seeking(), m_actionHappened(),
	m_seekHandle(this), m_nextNotePixmap(), m_notePixmapTimer(), m_analyzeTimer(), m_playbackTimer(), m_playbackPos(), m_pixmap(), m_pixmapPos()
{
	setProperty("darkBackground", true);
	setStyleSheet("QLabel[darkBackground=\"true\"] { background: " + BGColor + "; }");

	// Initially expanding horizontally to fill the space
	QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setSizePolicy(sp);
	setFixedHeight(Height);

	setFocusPolicy(Qt::StrongFocus);
	setAcceptDrops(true);
	setMouseTracking(true);
	setWhatsThis(tr("Note graph that displays the song notes and allows you to manipulate them."));

	// Context menu
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));

	qRegisterMetaType<QImage>("QImage"); // Needed for queued connections from other threads

	updateNotes();
}


void NoteGraphWidget::setLyrics(QString lyrics)
{
	BusyDialog busy(this, 2);
	QTextStream ts(&lyrics, QIODevice::ReadOnly);

	doOperation(Operation("CLEAR"));
	bool firstNote = true;
	while (!ts.atEnd()) {
		busy();
		// We want to loop one line at the time to insert line breaks
		bool sentenceStart = true;
		QString sentence = ts.readLine();
		QTextStream ts2(&sentence, QIODevice::ReadOnly);
		while (!ts2.atEnd()) {
			QString word;
			ts2 >> word;
			if (!word.isEmpty()) {
				Note note(word + " "); note.end = NoteLabel::default_length; note.note = 24;
				if (sentenceStart) note.lineBreak = true;
				doOperation(opFromNote(note, m_notes.size(), !firstNote), Operation::NO_UPDATE);
				firstNote = false;
				sentenceStart = false;
			}
		}
	}
	// Set duration
	m_duration = std::max(m_duration, NoteLabel::default_length * m_notes.size() * 1.1 + endMarginSeconds);

	finalizeNewLyrics();
}

void NoteGraphWidget::setLyrics(const VocalTrack &track)
{
	BusyDialog busy(this, 10);
	doOperation(Operation("CLEAR"));
	m_duration = std::max(m_duration, track.endTime + endMarginSeconds);
	const Notes &notes = track.notes;
	for (Notes::const_iterator it = notes.begin(); it != notes.end(); ++it) {
		if (it->type == Note::SLEEP) continue;
		doOperation(opFromNote(*it, m_notes.size(), false), Operation::NO_UPDATE);
		busy();
	}

	finalizeNewLyrics();
}

void NoteGraphWidget::finalizeNewLyrics()
{
	int ops = m_notes.size();
	// Set the last note to non-floating and to the end of the song
	if (ops > 1 && m_notes[ops-1]->isFloating()) {
		doOperation(Operation("FLOATING", (int)m_notes.size()-1, false));
		Operation moveop("MOVE");
		moveop << (int)m_notes.size()-1
			<< m_duration - NoteLabel::default_length - endMarginSeconds
			<< m_duration - endMarginSeconds
			<< m_notes.back()->note().note;
		doOperation(moveop);
		ops += 2; // Amount of extra Operations added here
	}

	// Combine the import into one undo action
	doOperation(Operation("COMBINER", ops));

	// Make sure there is enough room
	if (!m_notes.isEmpty())
		setFixedWidth(std::max<int>(width(), s2px(m_duration)));

	// Calculate floating note positions
	updateNotes();

	// Scroll to show the first note
	scrollToFirstNote();

	// Start creating pixmaps for notes
	startNotePixmapUpdates();
}

void NoteGraphWidget::scrollToFirstNote()
{
	QScrollArea* scrollArea = getScrollArea();
	if (scrollArea && !m_notes.isEmpty()) {
		const Note& n = m_notes.front()->note();
		int w = scrollArea->width();
		scrollArea->ensureVisible(s2px(n.begin) + w/3, n2px(n.note), w/2, scrollArea->height()/2);
	}
}

void NoteGraphWidget::analyzeMusic(QString filepath, int visId)
{
	m_pitch[visId].reset(new PitchVis(filepath, this, visId));
	connect(m_pitch[visId].data(), SIGNAL(renderedImage(QImage,QPoint,int)), this, SLOT(updatePixmap(QImage,QPoint,int)));
	m_analyzeTimer = startTimer(100);
}

void NoteGraphWidget::timerEvent(QTimerEvent* event)
{
	if (event->timerId() == m_playbackTimer) {
		// Playback position update
		m_playbackPos += m_playbackInterval.restart();
		updateMusicPos(m_playbackPos);

	// FIXME: Only considers first pitchvis
	} else if (event->timerId() == m_analyzeTimer && m_pitch[0]) {
		// PitchVis stuff
		double progress, duration;
		{
			QMutexLocker locker(&m_pitch[0]->mutex);
			progress = m_pitch[0]->getProgress();
			duration = m_pitch[0]->getDuration();
		}
		emit analyzeProgress(1000 * progress, 1000); // Update progress bar
		m_duration = std::max(m_duration, duration);
		// Analyzing has ended?
		if (progress == 1.0) {
			killTimer(m_analyzeTimer);
			updatePitch();
		}

	} else if (event->timerId() == m_notePixmapTimer) {
		// Here we create a pixmap for a NoteLabel
		if (m_nextNotePixmap >= m_notes.size()) {
			killTimer(m_notePixmapTimer);
			m_notePixmapTimer = 0;
			m_nextNotePixmap = 0;
			return;
		}
		// Loop until a pixmap-to-create is found - we only create one at a time to not block the UI
		while (m_nextNotePixmap < m_notes.size() && !m_notes[m_nextNotePixmap]->createPixmap())
			++m_nextNotePixmap;
		++m_nextNotePixmap;
	}
}

void NoteGraphWidget::startNotePixmapUpdates()
{
	// With 0-delay, note pixmaps are created whenever there is not events to process
	// This means fast performance while keeping snappy interface
	if (!m_notePixmapTimer) m_notePixmapTimer = startTimer(0);
	m_nextNotePixmap = 0;
}

void NoteGraphWidget::paintEvent(QPaintEvent*)
{
	setFixedSize(s2px(m_duration), height());

	// Find out the viewport
	int x1, y1, x2, y2;
	calcViewport(x1, y1, x2, y2);

	QPainter painter(this);

	// PitchVis pixmap
	for (int i = 0; i < MaxPitchVis; ++i) {
		if (!m_pixmap[i].isNull())
			painter.drawPixmap(m_pixmapPos[i], m_pixmap[i]);
	}

	// Octave lines
	QPen pen; pen.setWidth(1); pen.setColor(QColor("#666"));
	painter.setPen(pen);
	for (int i = 1; i < 4; ++i)
		painter.drawLine(x1, n2px(i*12), x2, n2px(i*12));

	// Selection box
	if (!m_mouseHotSpot.isNull()) {
		QPoint mousep = mapFromGlobal(QCursor::pos());
		pen.setWidth(2); pen.setColor(QColor("#800"));
		painter.setPen(pen);
		painter.drawRect(QRect(m_mouseHotSpot, mousep));
	}
}

void NoteGraphWidget::updatePixmap(const QImage &image, const QPoint &position, int visId)
{
	// PitchVis sends its renderings here, let's save & draw them
	// This gets actually called in our own thread by our own event loop (queued connection)
	m_pixmap[visId] = QPixmap::fromImage(image);
	m_pixmapPos[visId] = position;
	update();
}

void NoteGraphWidget::updatePitch()
{
	// Called whenever pitch needs updating
	// Note that the scrollbar change signals are connected here, so no need to call this from everywhere
	if (!m_pitch[0]) return;
	// Find out the viewport
	int x1, y1, x2, y2;
	calcViewport(x1, y1, x2, y2);
	// Ask for a new render
	for (int i = 0; i < MaxPitchVis; ++i)
		if (m_pitch[i]) m_pitch[i]->paint(x1, 0, x2, height());
}

void NoteGraphWidget::updateNotes(bool leftToRight)
{
	// Here happens the magic that adjusts the floating
	// notes according to the fixed ones.
	FloatingGap gap(leftToRight ? 0 : m_duration);

	// Determine gaps between non-floating notes
	// Variable leftToRight controls the iteration direction.
	for (int i = (leftToRight ? 0 : m_notes.size()-1); i >= 0 && i < m_notes.size(); i += (leftToRight ? 1 : -1)) {
		NoteLabel *child = m_notes[i];
		if (!child) continue;
		Note &n = m_notes[i]->note();

		if (child->isFloating() && i != (leftToRight ? m_notes.size()-1 : 0)) {
			// Add floating note to gap
			gap.addNote(child);

		} else {
			// Fixed note encountered, handle the gap (divide notes evenly into it)
			gap.end = leftToRight ? n.begin : n.end;

			if (!gap.notes.isEmpty()) {

				// Calculate note length and space between notes
				double len = NoteLabel::min_length, step = 0; // Minimum values
				if (gap.length() > gap.minLength()) { // Is there space?
					len = gap.length() / double(gap.notes.size()) * 0.9;
					step = (gap.length() - len * gap.notes.size()) / double(gap.notes.size() + 1);
				}

				// Calculate starting time of the first note in gap
				double pos = gap.begin + (leftToRight ? step : (-step - len));

				// Loop through all notes
				for (NoteLabels::iterator it2 = gap.notes.begin(); it2 != gap.notes.end(); ++it2) {
					Note &n2 = (*it2)->note();
					// Set new note begin/end
					n2.begin = pos;
					n2.end = pos + len;
					// Try to find optimal pitch
					// TODO: Use info also from other pitchvis
					if (m_pitch[0]) n2.note = m_pitch[0]->guessNote(pos, pos + len + step, 24);
					// Calculate starting time for the next note
					pos += (len + step) * (leftToRight ? 1 : -1);
					// Update NoteLabel geometry
					(*it2)->updateLabel();
				}
			}

			// Also move the fixed one (probably the one being moved by user) if there is no space
			double gapl = leftToRight ? (gap.end - gap.begin) : (gap.begin - gap.end);
			if (gapl < gap.minLength())
				n.move(gap.begin + (leftToRight ? gap.minLength() : (-gap.minLength() - n.length())));

			child->updateLabel();
			// Start a new gap
			gap = FloatingGap(leftToRight ? n.end : n.begin);
		}
	}
}

void NoteGraphWidget::updateMusicPos(qint64 time, bool smoothing)
{
	m_playbackPos = time;
	int x = s2px(m_playbackPos / 1000.0) - m_seekHandle.width() / 2;
	if (m_playbackTimer) killTimer(m_playbackTimer);
	m_seekHandle.move(x, 0);
	if (smoothing) m_playbackTimer = startTimer(20); // Hope for 50 fps
	else m_playbackTimer = 0;
	m_playbackInterval.restart();
}

void NoteGraphWidget::stopMusic()
{
	if (m_playbackTimer) killTimer(m_playbackTimer);
	m_playbackTimer = 0;
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
		double begin = px2s(m_seekHandle.curx());
		double end = px2s(m_seekHandle.curx() + selectedNote()->width());
		int n = selectedNote()->note().note;
		// TODO: Use info also from other pitchvis
		if (m_pitch[0]) n = m_pitch[0]->guessNote(begin, end, n);
		op << getNoteLabelId(selectedNote()) << begin << end << n;
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

			// Middle click or Alt + Left Click empty area = pan
			if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && event->modifiers() & Qt::AltModifier)) {
				m_mouseHotSpot = event->pos();

			// Left click empty area = dragbox selection
			} else if (event->button() == Qt::LeftButton) {
				m_mouseHotSpot = event->pos();
			}

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
			if (child->isSelected()) ; // No op
			// Ctrl and Shift allow selecting multiple notes for dragging
			else if (event->modifiers() & Qt::ShiftModifier) shiftSelect(child);
			else selectNote(child, !(event->modifiers() & Qt::ControlModifier));
			m_selectedAction = MOVE;
			child->startDragging(hotSpot);
		}

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
				NoteLabel *nl = m_selectedNotes[i];
				nl->startResizing(0);
				nl->startDragging(QPoint());
				const Note& n = nl->note();
				if (m_actionHappened) {
					// Operation for undo stack & saving
					Operation op("MOVE");
					op << getNoteLabelId(nl) << n.begin << n.end << n.note;
					doOperation(op, Operation::NO_EXEC);
					++movecount;
				}
			}
			// Combine to one undo operation
			if (movecount > 1) {
				Operation op("COMBINER"); op << movecount; doOperation(op);
			}

			// If we didn't move, select the note under cursor
			NoteLabel *child = qobject_cast<NoteLabel*>(childAt(event->pos()));
			if (child && !m_actionHappened && !event->modifiers() && event->button() == Qt::LeftButton)
				selectNote(child);
		}
		m_selectedAction = NONE;
	}
	m_actionHappened = false;
	m_mouseHotSpot = QPoint();
	m_seeking = false;
	setCursor(QCursor());
	updateNotes();
	update(); // Repaint to remove possible selection box
}

void NoteGraphWidget::wheelEvent(QWheelEvent *event)
{
	// Wheel = Zoom
	if (event->orientation() == Qt::Vertical) {
		float numDegrees = event->delta() / 8; // Qt resolution is 8th of a degree
		float numSteps = numDegrees / 15; // Usually mice have 15 degree steps
		zoom(numSteps, px2s(event->pos().x()));
		QToolTip::showText(event->globalPos(), "Zoom: " + QString::number(getZoomLevel()) + " %", this);
		event->accept();
		return;
	}
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
		// Unfloat all selected notes, otherwise the move would be b0rked by auto-pitch
		if (m_selectedAction != NONE && selectedNote()) {
			m_actionHappened = true; // We have movement, so resize/move can be accepted
			// Undo op is handled later by the MOVE constructed at drop
			for (int i = 0; i < m_selectedNotes.size(); ++i)
				m_selectedNotes[i]->setFloating(false);
		}
	}

	// Seeking
	if (m_seeking) {
		seek(event->x());

	// Box selection
	} else if (!m_mouseHotSpot.isNull() && (event->buttons() & Qt::LeftButton) && !(event->modifiers() & Qt::AltModifier)) {
		boxSelect(m_mouseHotSpot, event->pos());
		update(); // Paint selection box

	// Pan
	} else if (!m_mouseHotSpot.isNull()) {
		setCursor(QCursor(Qt::ClosedHandCursor));
		if (QScrollArea *scrollArea = getScrollArea()) {
			QPoint diff = event->pos() - m_mouseHotSpot;
			QScrollBar *scrollHor = scrollArea->horizontalScrollBar();
			scrollHor->setValue(scrollHor->value() - diff.x());
			QScrollBar *scrollVer = scrollArea->verticalScrollBar();
			scrollVer->setValue(scrollVer->value() - diff.y());
			m_mouseHotSpot = event->pos() - diff;
		}
	}

	MusicalScale ms;
	int note = round(px2n(event->y()));
	emit statusBarMessage(QString("Time: %1 s, note: %2 (%3)")
		.arg(px2s(event->x()))
		.arg(ms.getNoteStr(ms.getNoteFreq(note)))
		.arg(note));
	emit updateNoteInfo(selectedNote());
}

void NoteGraphWidget::keyPressEvent(QKeyEvent *event)
{
	int k = event->key(), m = event->modifiers();

	// NOTE: Putting shortcuts here makes them local to the NoteGraphWidget,
	// i.e. it must have focus for them to work. Concider using EditorApp.

	if (k == Qt::Key_Return) { // Edit lyric
		editLyric(selectedNote());
	} else if (k == Qt::Key_Left) { // Select note on the left
		selectNextSyllable(true, (m & Qt::ControlModifier));
	} else if (k == Qt::Key_Right) { // Select note on the right
		selectNextSyllable(false, (m & Qt::ControlModifier));
	} else if (k == Qt::Key_Up) { // Move note up
		move(selectedNote(), 1);
	} else if (k == Qt::Key_Down) { // Move note down
		move(selectedNote(), -1);
	} else if (k == Qt::Key_Escape) { // Unselect all note(s)
		selectNote(NULL);
	} else {
		QWidget::keyPressEvent(event);
	}
}

void NoteGraphWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/plain"))
		event->acceptProposedAction();
}

void NoteGraphWidget::dropEvent(QDropEvent *event)
{
	QString lyrics = event->mimeData()->text();
	if (m_notes.isEmpty()
		|| QMessageBox::question(this, tr("Replace lyrics"),
			tr("Do you wish to replace the existing lyrics?"),
			QMessageBox::Ok | QMessageBox::Cancel)
		== QMessageBox::Ok)
	{
		setLyrics(lyrics);
	}
	event->acceptProposedAction();
}


void NoteGraphWidget::showContextMenu(const QPoint &pos)
{
	QPoint globalPos = mapToGlobal(pos);
	NoteLabel *child = qobject_cast<NoteLabel*>(childAt(mapFromGlobal(globalPos)));
	if (child && !child->isSelected()) selectNote(child);
	QMenu menuContext(NULL);
	QMenu menuType(tr("Type"), NULL);

	QAction *actionNew = menuContext.addAction(tr("New note"));
	actionNew->setIcon(QIcon::fromTheme("insert-object", QIcon(":/icons/insert-object.png")));
	actionNew->setEnabled(!child); // Only available when no notes under cursor
	QAction *actionSplit = menuContext.addAction(tr("Split"));
	actionSplit->setEnabled(m_selectedNotes.size() == 1); // Only available when exactly one note selected
	QAction *actionLyric = menuContext.addAction(tr("Edit lyric"));
	actionLyric->setEnabled(m_selectedNotes.size() == 1); // Only available when exactly one note selected
	menuContext.addSeparator();

	QAction *actionFloating = menuContext.addAction(tr("Floating"));
	actionFloating->setCheckable(true);
	QAction *actionLineBreak = menuContext.addAction(tr("Line break"));
	actionLineBreak->setCheckable(true);
	menuContext.addAction(menuType.menuAction());
	QAction *actionNormal = menuType.addAction(tr("Normal"));
	actionNormal->setCheckable(true);
	QAction *actionGolden = menuType.addAction(tr("Golden"));
	actionGolden->setCheckable(true);
	QAction *actionFreestyle = menuType.addAction(tr("Freestyle"));
	actionFreestyle->setCheckable(true);
	NoteLabel *nl = selectedNote();
	if (!nl) {
		actionFloating->setEnabled(false);
		actionLineBreak->setEnabled(false);
		menuType.setEnabled(false);
	} else {
		actionFloating->setChecked(nl->isFloating());
		actionLineBreak->setChecked(nl->isLineBreak());
		actionNormal->setChecked(nl->note().type == Note::NORMAL);
		actionGolden->setChecked(nl->note().type == Note::GOLDEN);
		actionFreestyle->setChecked(nl->note().type == Note::FREESTYLE);
	}
	menuContext.addSeparator();

	QAction *actionResetZoom = menuContext.addAction(tr("Reset zoom"));
	actionResetZoom->setIcon(QIcon::fromTheme("zoom-original", QIcon(":/icons/zoom-original.png")));
	actionResetZoom->setEnabled(getZoomLevel() != 100);
	menuContext.addSeparator();

	QAction *actionCut = menuContext.addAction(tr("Cut"));
	actionCut->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/icons/edit-cut.png")));
	actionCut->setEnabled(!m_selectedNotes.isEmpty());
	QAction *actionCopy = menuContext.addAction(tr("Copy"));
	actionCopy->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/edit-copy.png")));
	actionCopy->setEnabled(!m_selectedNotes.isEmpty());
	QAction *actionPaste = menuContext.addAction(tr("Paste"));
	actionPaste->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/icons/edit-paste.png")));
	QAction *actionDelete = menuContext.addAction(tr("Delete"));
	actionDelete->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/icons/edit-delete.png")));
	actionDelete->setEnabled(!m_selectedNotes.isEmpty());
	menuContext.addSeparator();

	QAction *actionSelectAll = menuContext.addAction(tr("Select all"));
	actionSelectAll->setEnabled(!m_notes.isEmpty());
	actionSelectAll->setIcon(QIcon::fromTheme("edit-select-all", QIcon(":/icons/edit-select-all.png")));
	QAction *actionDeselect = menuContext.addAction(tr("Deselect"));
	actionDeselect->setEnabled(!m_selectedNotes.isEmpty());

	QAction *sel = menuContext.exec(globalPos);
	if (sel) {
		if (sel == actionNew) createNote(px2s(mapFromGlobal(globalPos).x()));
		else if (sel == actionSplit) split(nl);
		else if (sel == actionLyric) editLyric(nl);
		else if (sel == actionFloating) setFloating(nl, !nl->isFloating());
		else if (sel == actionLineBreak) setLineBreak(nl, !nl->isLineBreak());
		else if (sel == actionNormal) setType(nl, 0);
		else if (sel == actionGolden) setType(nl, 1);
		else if (sel == actionFreestyle) setType(nl, 2);
		else if (sel == actionResetZoom) zoom(getNaN());
		else if (sel == actionCut) cut();
		else if (sel == actionCopy) copy();
		else if (sel == actionPaste) paste();
		else if (sel == actionDelete) del(nl);
		else if (sel == actionSelectAll) selectAll();
		else if (sel == actionDeselect) selectNote(NULL);
	}
	menuType.clear();
	menuContext.clear();
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
		// Get to phrase beginning
		while (id >= 0 && !m_notes[id]->note().lineBreak) --id;
		if (id < 0) return "";
		// Now loop the phrase
		for (int i = id; i < m_notes.size(); ++i) {
			if (i != id && m_notes[i]->note().lineBreak) break;
			// Selected notes are highlighted with different color
			if (m_notes[i]->isSelected()) lyrics += "<span style=\"color: #a00;\">";
			lyrics += m_notes[i]->lyric();
			if (m_notes[i]->isSelected()) lyrics += "</span>";
			lyrics += " ";
		}
		lyrics = lyrics.left(lyrics.size() - 1); // Remove trailing space
	}
	return lyrics;
}

QString NoteGraphWidget::getPrevSentence() const
{
	QString lyrics;
	if (!m_notes.isEmpty() && selectedNote()) {
		// First find the previous start
		int id = getNoteLabelId(selectedNote());
		while (id >= 0 && !m_notes[id]->note().lineBreak) --id;
		if (id < 0) return "";
		--id;
		while (id >= 0 && !m_notes[id]->note().lineBreak) --id;
		if (id < 0) return "";

		// Now get the sentence
		for (int i = id; i < m_notes.size(); ++i) {
			if (i != id && m_notes[i]->note().lineBreak) break;
			lyrics += m_notes[i]->lyric() + " ";
		}
		lyrics = lyrics.left(lyrics.size() - 1); // Remove trailing space
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
	QImage image(8, NoteGraphWidget::Height, QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(128, 128, 128, 128));
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
	end = n->note().end;
	m_notesLength += n->note().length();
}

double FloatingGap::minLength() const
{
	return notes.size() * NoteLabel::min_length;
}
