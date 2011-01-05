#include <QtGui> 
#include <cstdlib>
#include "editorapp.hh"


EditorApp::EditorApp(QWidget *parent): QMainWindow(parent)
{
	ui.setupUi(this);

	// Signals/slots
	//connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(about()));
	//connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(quit()));
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
