#include <iostream>
#include <QString>
#include <QInputDialog>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include "notegraphwidget.hh"
#include "notelabel.hh"
#include "operation.hh"


/*static*/ const QString NoteLabelManager::MimeType = "application/x-notelabels";

NoteLabelManager::NoteLabelManager(QWidget *parent)
	: QLabel(parent), m_selectedAction(NONE), m_pixelsPerSecond(ppsNormal)
{
	// Determine NoteLabel height
	NoteLabel templabel(Note(" "), NULL);
	m_noteHalfHeight = templabel.height()/2;
	templabel.close();
}

void NoteLabelManager::clearNotes()
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

void NoteLabelManager::selectNote(NoteLabel* note, bool clearPrevious)
{
	// Clear all previous selections?
	if (!note || clearPrevious) { // NULL means allways clear all
		for (int i = 0; i < m_selectedNotes.size(); ++i)
			m_selectedNotes[i]->setSelected(false);
		m_selectedNotes.clear();
	}

	// Add at the beginning of the chain
	if (note && !note->isSelected()) {
		m_selectedNotes.push_front(note);
		note->setSelected(true);
	} else if (!note) m_selectedAction = NONE;

	m_cutNotes.clear(); // Clear cut buffer when selection changes

	// Signal UI about the change
	emit updateNoteInfo(selectedNote());
}

void NoteLabelManager::selectAll()
{
	selectNote(NULL); // Clear previous
	for (int i = m_notes.size()-1; i >= 0; --i) // Traverse in reverse order to get the first note first
		selectNote(m_notes[i], false);
}

void NoteLabelManager::shiftSelect(NoteLabel* note)
{
	if (!note || note == selectedNote()) return;
	if (!selectedNote()) { selectNote(note); return; }

	// Select all notes between the last selection and this
	int n = getNoteLabelId(selectedNote()), m = getNoteLabelId(note);
	selectNote(NULL); // Unselect everything
	if (m < n) std::swap(n, m);
	for (int i = n; i <= m; ++i)
		selectNote(m_notes[i], false);
}

void NoteLabelManager::boxSelect(QPoint p1, QPoint p2)
{
	// Make sure the points are in right order (p1 = upper left, p2 = lower right)
	if (p1.x() > p2.x()) std::swap(p1.rx(), p2.rx());
	if (p1.y() > p2.y()) std::swap(p1.ry(), p2.ry());
	// Deselect all
	selectNote(NULL);
	// Loop through notes, select the ones inside rectangle
	for (int i = 0; i < m_notes.size(); ++i) {
		NoteLabel *nl = m_notes[i];
		if (nl->x() > p2.x()) break;
		if (nl->x() + nl->width() > p1.x()
			&& nl->y() + nl->height() > p1.y()
			&& nl->y() < p2.y())
				selectNote(nl, false);
	}
}

int NoteLabelManager::getNoteLabelId(NoteLabel* note) const
{
	for (int i = 0; i < m_notes.size(); ++i)
		if (m_notes[i] == note) return i;
	return -1;
}

int NoteLabelManager::findIdForTime(double time) const
{
	for (int i = 0; i < m_notes.size(); ++i) {
		if (m_notes[i]->note().begin >= time) return i;
	}
	return m_notes.size();
}

void NoteLabelManager::selectNextSyllable(bool backwards, bool addToSelection)
{
	int i = getNoteLabelId(selectedNote());
	if (!backwards && i < m_notes.size()-1)
		selectNote(m_notes[i+1], !addToSelection);
	else if (backwards && i > 0)
		selectNote(m_notes[i-1], !addToSelection);
}

void NoteLabelManager::selectNextSentenceStart()
{
	// Start looking for the sentance start from the next NoteLabel
	for (int i = getNoteLabelId(selectedNote()) + 1; i < m_notes.size(); ++i) {
		if (m_notes[i]->note().lineBreak) {
			selectNote(m_notes[i]);
			return;
		}
	}
}



void NoteLabelManager::createNote(double time)
{
	// Spawn an input dialog
	bool ok;
	QString text = QInputDialog::getText(this, tr("New note"),
										tr("Enter one or more lyrics:"), QLineEdit::Normal,
										"", &ok);
	if (ok) {
		// TODO: Should create more than one note if there is more words
		// Find the correct place for this note
		int id = findIdForTime(time);
		int nlvl = (id > 0) ? m_notes[id-1]->note().note : 24;
		// Create Operation
		Operation op("NEW");
		op << id << text
			<< time // begin
			<< time+1 // dummy end
			<< nlvl // note
			<< true // floating
			<< false // linebreak
			<< 0; // type
		doOperation(op); // Execute operation
	}
}

void NoteLabelManager::split(NoteLabel *note, float ratio)
{
	if (!note) return;

	if (selectedNote() == note)
		selectNote(NULL);

	// Cut the text
	int cutpos = int(std::ceil(note->lyric().length() * ratio));
	QString firstst = note->lyric().left(cutpos);
	QString secondst = note->lyric().right(note->lyric().length() - cutpos);

	const Note& n = note->note();

	// Create operations for adding the new labels and deleting the old one
	int id = getNoteLabelId(note);
	Operation new1("NEW"), new2("NEW");
	new1 << id << firstst << n.begin << n.begin + n.length() * ratio << n.note << note->isFloating() << n.lineBreak << n.getTypeInt();
	new2 << id+1 << secondst << n.begin + n.length() * ratio << n.end << n.note << note->isFloating() << false << 0;
	doOperation(new1, Operation::NO_UPDATE);
	doOperation(new2, Operation::NO_UPDATE);
	doOperation(Operation("DEL", id+2), Operation::NO_UPDATE);
	doOperation(Operation("COMBINER", 3)); // This will combine the previous ones to one undo action
}

void NoteLabelManager::del(NoteLabel *note)
{
	if (!note) return;

	// If delete is directed to a selected note, all selected notes will be deleted
	if (note->isSelected()) {
		int i = 0; // We need this after the loop
		for (; i < m_selectedNotes.size(); ++i) {
			Operation op("DEL");
			op << getNoteLabelId(m_selectedNotes[i]);
			doOperation(op);
		}
		// Combine to one undo operation
		if (i > 1) {
			doOperation(Operation("COMBINER", i));
		}
		// Clear all
		m_selectedNotes.clear();

	} else {
		// Here we have non-selected note up for deletion
		doOperation(Operation("DEL", getNoteLabelId(note)));
	}
}

void NoteLabelManager::move(NoteLabel *note, int value)
{
	if (!note) return;

	int i = 0; // We need this after the loop
	for (; i < m_selectedNotes.size(); ++i) {
		NoteLabel *n = m_selectedNotes[i];
		Operation op("MOVE");
		op << getNoteLabelId(n)
		  << px2s(n->x()) << px2s(n->x() + n->width())
		  << int(round(px2n(n->y() + m_noteHalfHeight))) + value;
		doOperation(op);
	}

	// Combine to one undo operation
	if (i > 1) {
		doOperation(Operation("COMBINER", i));
	}
}

void NoteLabelManager::setType(NoteLabel *note, int index)
{
	if (!note) return;
	// Easy case: only one note
	if (m_selectedNotes.size() == 1 || !note->isSelected()) {
		if (note->note().getTypeInt() == index) return;
		Operation op("TYPE");
		op << getNoteLabelId(note) << index;
		doOperation(op);
	}

	// Multiple notes selected: apply to all
	int i = 0;
	for (; i < m_selectedNotes.size(); ++i) {
		Operation op("TYPE");
		op << getNoteLabelId(m_selectedNotes[i]) << index;
		doOperation(op);
	}
	doOperation(Operation("COMBINER", i));
}

void NoteLabelManager::setFloating(NoteLabel *note, bool state)
{
	if (!note) return;
	// Easy case: only one note
	if (m_selectedNotes.size() == 1 || !note->isSelected()) {
		if (note->isFloating() == state) return;
		doOperation(Operation("FLOATING", getNoteLabelId(note), state));
	}

	// Multiple notes selected: apply to all
	int i = 0;
	for (; i < m_selectedNotes.size(); ++i) {
		doOperation(Operation("FLOATING", getNoteLabelId(m_selectedNotes[i]), state));
	}
	doOperation(Operation("COMBINER", i));
}

void NoteLabelManager::setLineBreak(NoteLabel *note, bool state)
{
	if (!note) return;
	// Easy case: only one note
	if (m_selectedNotes.size() == 1 || !note->isSelected()) {
		if (note->isLineBreak() == state) return;
		doOperation(Operation("LINEBREAK", getNoteLabelId(note), state));
	}

	// Multiple notes selected: apply to all
	int i = 0;
	for (; i < m_selectedNotes.size(); ++i) {
		doOperation(Operation("LINEBREAK", getNoteLabelId(m_selectedNotes[i]), state));
	}
	doOperation(Operation("COMBINER", i));
}

void NoteLabelManager::editLyric(NoteLabel *note) {
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


void NoteLabelManager::doOperation(const Operation& op, Operation::OperationFlags flags)
{
	if (!(flags & Operation::NO_EXEC)) {
		try {
			QString action = op.op();
			if (action == "BLOCK" || action == "COMBINER") {
				; // No op
			} else if (action == "CLEAR") {
				clearNotes();
			} else if (action == "NEW") {
				Note newnote(op.s(2)); // lyric
				newnote.begin = op.d(3); // begin
				newnote.end = op.d(4); // end
				newnote.note = op.d(5); // note
				newnote.lineBreak = op.b(7); // lineBreak
				newnote.type = Note::types[op.i(8)]; // note type
				NoteLabel *newLabel = new NoteLabel(
					newnote, // Note(lyric)
					this, // parent
					op.b(6) // floating
					);
				int id = op.i(1);
				if (id < 0) id = findIdForTime(op.d(3)); // -1 = auto-choose
				if (m_notes.isEmpty() || id > m_notes.size()) m_notes.push_back(newLabel);
				else m_notes.insert(id, newLabel);
			} else {
				NoteLabel *n = m_notes.at(op.i(1));
				if (n) {
					if (action == "DEL") {
						n->close();
						m_notes.removeAt(op.i(1));
					} else if (action == "MOVE") {
						n->setGeometry(s2px(op.d(2)), n2px(op.i(4)) - m_noteHalfHeight, s2px(op.d(3) - op.d(2)), 2 * m_noteHalfHeight);
						n->setFloating(false);
					} else if (action == "FLOATING") {
						n->setFloating(op.b(2));
					} else if (action == "LINEBREAK") {
						n->setLineBreak(op.b(2));
					} else if (action == "LYRIC") {
						n->setLyric(op.s(2));
					} else if (action == "TYPE") {
						n->setType(op.i(2));
					} else {
						std::cerr << "Error: Unkown operation type " << action.toStdString() << std::endl;
					}
					n->createPixmap(n->size());
				}
			}
		} catch (std::runtime_error&) {
			std::cerr << "Error! Invalid operation: " << op.dump() << std::endl;
		}

		if (!(flags & Operation::NO_UPDATE))
			updateNotes();
	}
	if (!(flags & Operation::NO_EMIT)) {
		emit operationDone(op);
		emit updateNoteInfo(selectedNote());
	}
}

void NoteLabelManager::zoom(float steps) {
	// Get scrollArea position
	QScrollArea *scrollArea = NULL;
	double scrollSecs = -1;
	if (parentWidget()) {
		scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parent());
		if (scrollArea) scrollSecs = px2s(scrollArea->horizontalScrollBar()->value() + scrollArea->width()/2);
	}

	// Update m_pixelsPerSecond
	{
		double pps = m_pixelsPerSecond;
		// Update zoom factor, NaN means reset
		if (steps != steps) pps = ppsNormal;
		else {
			// A little trickier exponential adjustment to avoid accumulating rounding errors
			double current = std::log(pps / ppsNormal) / std::log(2.0) / zoomStep;  // Find the steps for current level
			int level = clamp(int(round(current + steps)), zoomMin, zoomMax);  // New level
			pps = ppsNormal * std::pow(2.0, level * zoomStep);  // Calculate new zoom
		}
		if (pps == m_pixelsPerSecond) return;  // Nothing changed
		m_pixelsPerSecond = pps;
	}
	
	// Update scroll bar position
	if (scrollArea && scrollSecs >= 0) {
		QScrollBar *scrollVer = scrollArea->verticalScrollBar();
		int y = 0;
		if (scrollVer) y = scrollVer->value();
		scrollArea->ensureVisible(s2px(scrollSecs), y, scrollArea->width()/2, 0);
	}

	// Update notes
	for (int i = 0; i < m_notes.size(); ++i) {
		const Note &n = m_notes[i]->note();
		m_notes[i]->setGeometry(s2px(n.begin), m_notes[i]->y(), s2px(n.length()), m_notes[i]->height());
	}

	// Update pitch visualization
	update();

	// Update window title
	emit updateNoteInfo(selectedNote());
}

QString NoteLabelManager::getZoomLevel() { return QString::number(int(m_pixelsPerSecond / ppsNormal * 100)) + " %"; }

int NoteLabelManager::s2px(double sec) const { return sec * m_pixelsPerSecond; }
double NoteLabelManager::px2s(int px) const { return px / m_pixelsPerSecond; }
int NoteLabelManager::n2px(double note) const { return height() - 16.0 * note; }
double NoteLabelManager::px2n(int px) const { return (height() - px) / 16.0; }




void NoteLabelManager::cut()
{
	if (m_selectedNotes.isEmpty()) return;
	m_cutNotes.clear();
	// Save selected notes for future deletion
	for (int i = 0; i < m_selectedNotes.size(); ++i)
		m_cutNotes.push_back(m_selectedNotes[i]);
	// Copy
	copy();
}

void NoteLabelManager::copy()
{
	if (m_selectedNotes.isEmpty()) return;

	// Create operations of the notes and serialize to byte array
	QByteArray buf;
	QDataStream stream(&buf, QIODevice::WriteOnly);
	for (int i = 0; i < m_selectedNotes.size(); ++i)
		stream << (Operation)(*m_selectedNotes[i]);

	QClipboard *clipboard = QApplication::clipboard();
	if (!clipboard) return;

	// Put the data to clipboard
	QMimeData *mimeData = new QMimeData;
	mimeData->setData(MimeType, buf);
	clipboard->setMimeData(mimeData);
}

void NoteLabelManager::paste()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();

	if (mimeData->hasFormat(MimeType) && !mimeData->data(MimeType).isEmpty()) {
		// Get data
		QByteArray buf = mimeData->data(MimeType);
		QDataStream stream(&buf, QIODevice::ReadOnly);

		// FIXME
		QMessageBox::critical(this, tr("Oh noes!"), tr("Here be bugs - pasting is so far only partially implemented. :-("));

		// Delete all notes that were cut
		// TODO: We need to clear cut buffer is clipboard contents has changed (by other instance/application)
		if (!m_cutNotes.isEmpty()) {
			int i = 0;
			for (; i < m_cutNotes.size(); ++i)
				doOperation(Operation("DEL", getNoteLabelId(m_cutNotes[i])));
			doOperation(Operation("COMBINER", i)); // Combine into one undo-op
			m_cutNotes.clear();
			selectNote(NULL); // Clear selection as we might just have deleted some of it
		}

		// Read and execute all NoteLabel Operations from the clipboard
		while (!stream.atEnd()) {
			Operation op;
			stream >> op;
			std::cout << "Pasted op: " << op.dump() << std::endl;
			// TODO: Adjust position according to mouse
			// TODO: Could we select the new notes?
			doOperation(op);
		}
	}
}
