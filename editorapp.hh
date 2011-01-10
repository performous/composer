#pragma once

#include "ui_editor.h"

class NoteLabel;

class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

public slots:
	void updateNoteInfo(NoteLabel* note);

	// File menu
	void on_actionNew_triggered();
	void on_actionExit_triggered();

	// Insert menu
	void on_actionMusicFile_triggered();
	void on_actionLyricsFromFile_triggered();
	void on_actionLyricsFromClipboard_triggered();

	// Help menu
	void on_actionWhatsThis_triggered();
	void on_actionAbout_triggered();


private:
	Ui::EditorApp ui;
};
