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
