#include <QtGui> 
#include <cstdlib>
#include "editorapp.hh"
#include "notelabel.hh"


EditorApp::EditorApp(QWidget *parent): QMainWindow(parent)
{
	ui.setupUi(this);
	// Splitter sizes cannot be set through designer :(
	QList<int> ss; ss.push_back(700); ss.push_back(300); // Proportions, not pixels
	ui.splitter->setSizes(ss);

	// Custom signals/slots
	connect(ui.noteGraph, SIGNAL(updateNoteInfo(NoteLabel*)), this, SLOT(updateNoteInfo(NoteLabel*)));
	updateNoteInfo(NULL);
}


void EditorApp::updateNoteInfo(NoteLabel *note)
{
	if (note) {
		ui.valNoteBegin->setText(QString::number(note->x()));
		ui.valNoteEnd->setText(QString::number(note->x() + note->width()));
		ui.valNoteDuration->setText(QString::number(note->width()));
		ui.valNote->setText(QString::number(note->y() / NoteGraphWidget::noteYStep));
		ui.chkFloating->setEnabled(true);
		ui.chkFloating->setChecked(note->isFloating());
	} else {
		ui.valNoteBegin->setText("-");
		ui.valNoteEnd->setText("-");
		ui.valNoteDuration->setText("-");
		ui.valNote->setText("-");
		ui.chkFloating->setEnabled(false);
	}
}

void EditorApp::on_actionNew_triggered()
{
	// TODO: Check if a save prompt is in order
	if (true) {
		QMessageBox::StandardButton b = QMessageBox::question(this, "Unsaved changes",
			"There are unsaved changes. Do you wish to save before creating a new project?",
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		switch(b) {
		case QMessageBox::Yes:
			// TODO: Save
		case QMessageBox::No:
			ui.noteGraph->clear(); break;
		default: break;
		}
	} else {
		ui.noteGraph->clear();
	}
}

void EditorApp::on_actionExit_triggered()
{
	// TODO: Check if a save prompt is in order
	if (false) {
		QMessageBox::StandardButton b = QMessageBox::question(this, "Unsaved changes",
			"There are unsaved changes. Do you wish to save before quitting?",
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		switch(b) {
		case QMessageBox::Yes:
			// TODO: Save
		case QMessageBox::No:
			close(); break;
		default: break;
		}
	} else {
		close();
	}
}

void EditorApp::on_actionMusicFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			"",
			tr("Music files (*.mp3 *.ogg)"));

	if (!fileName.isNull()) {
		ui.valMusicFile->setText(fileName);
		ui.tabWidget->setCurrentIndex(1);
		// TODO: Do something the file
	}
}

void EditorApp::on_actionLyricsFromFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			"",
			tr("Text files (*.txt)"));

	if (!fileName.isNull()) {
		QFile file(fileName);
		 if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			 return;

		 QTextStream in(&file);
		 QString text = "";
		 while (!in.atEnd()) {
			 text += in.readLine();
			 if (!in.atEnd()) text += " "; // TODO: Some kind of line separator here
		 }

		 if (text != "")
			 ui.noteGraph->setLyrics(text);
	}
}

void EditorApp::on_actionLyricsFromClipboard_triggered()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();

	if (mimeData->hasText() && !mimeData->text().isEmpty()) {
		QString text = mimeData->text();
		QMessageBox::StandardButton b = QMessageBox::question(this, "Replace lyrics",
			"Pasting lyrics from clipboard will replace the existing ones. Continue?",
			QMessageBox::Ok | QMessageBox::Cancel);
		if (b == QMessageBox::Ok) {
			ui.noteGraph->setLyrics(text);
		}
	} else {
		QMessageBox::warning(this, "No text to paste", "No suitable data on the clipboard.");
	}
}

void EditorApp::on_actionAbout_triggered()
{
	QMessageBox::about(this, "About",
		"Semi-automatic karaoke song editor.\n"
		);
}

