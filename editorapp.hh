#pragma once

#include "ui_editor.h"
#include "operation.hh"
#include "song.hh"

class NoteLabel;
class NoteGraphWidget;

class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

	void updateSongMeta(bool readFromSongToUI = false);
	void saveProject(QString fileName);
	void doOpStack();
	void updateMenuStates();

public slots:
	void operationDone(const Operation &op);
	void updateNoteInfo(NoteLabel *note);

	// Automatic slots

	// File menu
	void on_actionNew_triggered();
	void on_actionOpen_triggered();
	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();
	void on_actionSingStarXML_triggered();
	void on_actionUltraStarTXT_triggered();
	void on_actionFoFMIDI_triggered();
	void on_actionExit_triggered();

	// Edit menu
	void on_actionUndo_triggered();
	void on_actionRedo_triggered();

	// Insert menu
	void on_actionMusicFile_triggered();
	void on_actionLyricsFromFile_triggered();
	void on_actionLyricsFromClipboard_triggered();

	// Help menu
	void on_actionWhatsThis_triggered();
	void on_actionAbout_triggered();

	// Note properties tab
	void on_txtTitle_editingFinished();
	void on_txtArtist_editingFinished();
	void on_txtGenre_editingFinished();
	void on_txtYear_editingFinished();
	void on_cmbNoteType_currentIndexChanged(int);
	void on_chkFloating_stateChanged(int);

private:
	Ui::EditorApp ui;
	NoteGraphWidget* noteGraph;
	OperationStack opStack;
	OperationStack redoStack;
	QScopedPointer<Song> song;
	QString projectFileName;
};
