#pragma once

#include <phonon/MediaObject>
#include "ui_editor.h"
#include "ui_aboutdialog.h"
#include "operation.hh"
#include "song.hh"
#include "synth.hh"

class QProgressBar;
class QCloseEvent;
class NoteLabel;
class NoteGraphWidget;
class GettingStartedDialog;
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
	void updateTitle();
	void showTab(int tab) { ui.tabWidget->setCurrentIndex(tab); }
	void showExportMenu() { ui.menuExport->exec(pos() + QPoint(0, ui.menubar->height())); }

private:
	void setupNoteGraph();
	void setMusic(QString filepath);
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
	void on_chkSynth_clicked(bool checked);
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
	void on_actionDelete_triggered();
	void on_actionSelectAll_triggered();
	void on_actionAntiAliasing_toggled(bool checked);

	// Insert menu
	void on_actionMusicFile_triggered();
	void on_actionLyricsFromFile_triggered();
	void on_actionLyricsFromClipboard_triggered();

	// View menu
	void on_actionZoomIn_triggered();
	void on_actionZoomOut_triggered();
	void on_actionResetZoom_triggered();

	// Help menu
	void on_actionGettingStarted_triggered();
	void on_actionWhatsThis_triggered();
	void on_actionAbout_triggered();

	// Note properties tab
	void on_txtTitle_editingFinished();
	void on_txtArtist_editingFinished();
	void on_txtGenre_editingFinished();
	void on_txtYear_editingFinished();
	void on_cmdSplit_clicked();
	void on_cmdInsert_clicked();
	void on_cmbNoteType_activated(int state);
	void on_chkFloating_clicked(bool checked);
	void on_chkLineBreak_clicked(bool checked);

protected:
	void closeEvent(QCloseEvent *event);

private:
	Ui::EditorApp ui;
	GettingStartedDialog *gettingStarted;
	NoteGraphWidget *noteGraph;
	OperationStack opStack;
	OperationStack redoStack;
	QScopedPointer<Song> song;
	Phonon::MediaObject *player;
	Phonon::AudioOutput *audioOutput;
	QScopedPointer<Synth> synth;
	QProgressBar *statusbarProgress;
	QString projectFileName;
	bool hasUnsavedChanges;
	QString latestPath;
};
