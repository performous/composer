#pragma once

#include "ui_editor.h"

class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

public slots:
	// File
	void on_actionNew_triggered();
	void on_actionExit_triggered();

	// Insert
	void on_actionMusicFile_triggered();
	void on_actionLyricsFromFile_triggered();
	void on_actionLyricsFromClipboard_triggered();

	// Help
	void on_actionAbout_triggered();


private:
	Ui::EditorApp ui;
};
