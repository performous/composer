#pragma once

#include <phonon/MediaObject>
#include "ui_editor.h"
#include "ui_aboutdialog.h"
#include "operation.hh"
#include "song.hh"

class QProgressBar;
class QCloseEvent;
class NoteLabel;
class NoteGraphWidget;
namespace Phonon {
	class MediaObject;
	class AudioOutput;
}


class AboutDialog: public QDialog, private Ui::AboutDialog
{
	Q_OBJECT
public:
	AboutDialog(QWidget* parent = 0);
};


class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

	void updateSongMeta(bool readFromSongToUI = false);
	void updateMenuStates();

private:
	bool promptSaving();
	void saveProject(QString fileName);
	void exportSong(QString format, QString dialogTitle);
	void doOpStack();
	void playButton();
	void readSettings();
	void writeSettings();

public slots:
	void operationDone(const Operation &op);
	void updateNoteInfo(NoteLabel *note);
	void analyzeProgress(int value, int maximum);
	void metaDataChanged();
	void audioTick(qint64 time);
	void playerStateChanged(Phonon::State newstate, Phonon::State olstate);

	// Automatic slots

	void on_cmdPlay_clicked();
	void on_cmdStop_clicked();
	void on_cmdRefreshLyrics_clicked();

	// File menu
	void on_actionNew_triggered();
	void on_actionOpen_triggered();
	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();
	void on_actionSingStarXML_triggered();
	void on_actionUltraStarTXT_triggered();
	void on_actionFoFMIDI_triggered();
	void on_actionLyricsToFile_triggered();
	void on_actionLyricsToClipboard_triggered();
	void on_actionExit_triggered();

	// Edit menu
	void on_actionUndo_triggered();
	void on_actionRedo_triggered();
	void on_actionAntiAliasing_toggled(bool checked);

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
	void on_cmbNoteType_currentIndexChanged(int state);
	void on_chkFloating_stateChanged(int state);
	void on_chkLineBreak_stateChanged(int state);

protected:
	void closeEvent(QCloseEvent *event);

private:
	Ui::EditorApp ui;
	NoteGraphWidget *noteGraph;
	OperationStack opStack;
	OperationStack redoStack;
	QScopedPointer<Song> song;
	Phonon::MediaObject *player;
	Phonon::AudioOutput *audioOutput;
	QProgressBar *statusbarProgress;
	QString projectFileName;
	bool hasUnsavedChanges;
};
