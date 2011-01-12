#pragma once

#include "ui_editor.h"
#include "operation.hh"

class NoteLabel;
class NoteGraphWidget;

class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

public slots:
	void operationDone(const Operation& op);
	void updateNoteInfo(NoteLabel* note);

	// Automatic slots

	// File menu
	void on_actionNew_triggered();
	void on_actionOpen_triggered();
	void on_actionExit_triggered();

	// Edit menu
	void on_actionUndo_triggered();

	// Insert menu
	void on_actionMusicFile_triggered();
	void on_actionLyricsFromFile_triggered();
	void on_actionLyricsFromClipboard_triggered();

	// Help menu
	void on_actionWhatsThis_triggered();
	void on_actionAbout_triggered();

	// Note properties tab
	void on_cmbNoteType_currentIndexChanged(int);
	void on_chkFloating_stateChanged(int);

private:
	Ui::EditorApp ui;
	NoteGraphWidget* noteGraph;
	OperationStack opStack;
};
