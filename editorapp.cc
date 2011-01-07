#include <QtGui> 
#include <cstdlib>
#include "editorapp.hh"


EditorApp::EditorApp(QWidget *parent): QMainWindow(parent)
{
	ui.setupUi(this);
	// FIXME: Should not set any lyrics here
	ui.noteGraph->setLyrics("Young man, there's no need to feel down. "
							"I said, young man, pick yourself off the ground.");
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

void EditorApp::on_actionExit_triggered()
{
	exit(0); // Hack
}
