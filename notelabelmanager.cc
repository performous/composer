#include <iostream>
#include <QString>
#include <QInputDialog>
#include <QLineEdit>
#include "notegraphwidget.hh"
#include "notelabel.hh"
#include "operation.hh"

NoteLabelManager::NoteLabelManager(QWidget *parent)
	: QLabel(parent), m_selectedAction(NONE), m_pixelsPerSecond(200.0)
{
	// Determine NoteLabel height
	NoteLabel templabel(Note(" "), NULL);
	m_noteHalfHeight = templabel.height()/2;
	templabel.close();
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

	// Signal UI about the change
	emit updateNoteInfo(selectedNote());
}

int NoteLabelManager::getNoteLabelId(NoteLabel* note) const
{
	for (int i = 0; i < m_notes.size(); ++i)
		if (m_notes[i] == note) return i;
	return -1;
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



void NoteLabelManager::split(NoteLabel *note, float ratio)
{
	if (!note) return;

	if (selectedNote() == note)
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
			Operation op("COMBINER"); op << i; doOperation(op);
		}
		// Clear all
		m_selectedNotes.clear();

	} else {
		// Here we have non-selected note up for deletion
		Operation op("DEL");
		op << getNoteLabelId(note);
		doOperation(op);
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
		Operation op("COMBINER"); op << i; doOperation(op);
	}
}

void NoteLabelManager::setType(NoteLabel *note, int index)
{
	if (note && note->note().getTypeInt() != index) {
		Operation op("TYPE");
		op << getNoteLabelId(note) << index;
		doOperation(op);
	}
}

void NoteLabelManager::setFloating(NoteLabel *note, bool state)
{
	if (note && note->isFloating() != state) {
		Operation op("FLOATING");
		op << getNoteLabelId(note) << state;
		doOperation(op);
	}
}

void NoteLabelManager::setLineBreak(NoteLabel *note, bool state)
{
	if (note && note->isLineBreak() != state) {
		Operation op("LINEBREAK");
		op << getNoteLabelId(note) << state;
		doOperation(op);
	}
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
				} else if (action == "MOVE") {
					n->setGeometry(s2px(op.i(2)), n2px(op.i(4)) - m_noteHalfHeight, s2px(op.i(3) - op.i(2)), 2 * m_noteHalfHeight);
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
		updateNotes();
	}
	if (!(flags & Operation::NO_EMIT)) {
		emit operationDone(op);
		emit updateNoteInfo(selectedNote());
	}
}

int NoteLabelManager::s2px(double sec) const { return sec * m_pixelsPerSecond; }
double NoteLabelManager::px2s(int px) const { return px / m_pixelsPerSecond; }
int NoteLabelManager::n2px(double note) const { return height() - 16.0 * note; }
double NoteLabelManager::px2n(int px) const { return (height() - px) / 16.0; }
