#pragma once

#include "ui_editor.h"

class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

public slots:
	void on_actionLyricsFromClipboard_triggered();
	void on_actionAbout_triggered();
	void on_actionExit_triggered();

private:
	Ui::EditorApp ui;
};
