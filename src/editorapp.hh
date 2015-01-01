#pragma once

#include "ui_editor.h"
#include "ui_aboutdialog.h"
#include "operation.hh"
#include "song.hh"
#include "synth.hh"
#include "notegraphwidget.hh"
#include <QMediaPlayer>

class QProgressBar;
class QPushButton;
class QCloseEvent;
class NoteLabel;
class NoteGraphWidget;
class GettingStartedDialog;


class AboutDialog: public QDialog, private Ui::AboutDialog
{
	Q_OBJECT
public:
	AboutDialog(QWidget* parent = 0);
};


class Piano: public QLabel
{
	Q_OBJECT
public:
	Piano(QWidget *parent = 0);
public slots:
	void updatePixmap(NoteGraphWidget *ngw);
protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
private:
	BufferPlayer *m_player;
};


class EditorApp: public QMainWindow
{
	Q_OBJECT

public:
	EditorApp(QWidget *parent = 0);

	void openFile(QString fileName);
	void updateSongMeta(bool readFromSongToUI = false);
	void updateMenuStates();
	void updateTitle();
	void highlightLabel(QString id);
	void showExportMenu() { ui.menuExport->exec(pos() + QPoint(0, ui.menubar->height())); }

private:
	void setupNoteGraph();
	void setMusic(QString filepath, bool primary = true);
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
	void playerStateChanged(QMediaPlayer::State state);
	void playerError(QMediaPlayer::Error error);
	void playBuffer(const QByteArray& buffer);
	void statusBarMessage(const QString& message);
	void updatePiano(int y);
	void clearLabelHighlights();

	// Automatic slots

	void on_cmdPlay_clicked();
	void on_chkSynth_clicked(bool checked);

	// File menu
	void on_actionNew_triggered();
	void on_actionOpen_triggered();
	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();
	void on_actionSingStarXML_triggered();
	void on_actionUltraStarTXT_triggered();
	void on_actionFoFMIDI_triggered();
	void on_actionLRC_triggered();
	void on_actionLyricsToFile_triggered();
	void on_actionLyricsToClipboard_triggered();
	void on_actionExit_triggered();

	// Edit menu
	void on_actionUndo_triggered();
	void on_actionRedo_triggered();
	void on_actionDelete_triggered();
	void on_actionSelectAll_triggered();
	void on_actionSelectAllAfter_triggered();
	void on_actionAntiAliasing_toggled(bool checked);

	// Insert menu
	void on_actionMusicFile_triggered();
	void on_actionAdditionalMusicFile_triggered();
	void on_actionLyricsFromFile_triggered();
	void on_actionLyricsFromClipboard_triggered();
	void on_actionLyricsFromLRCFile_triggered();

	// View menu
	void on_actionZoomIn_triggered();
	void on_actionZoomOut_triggered();
	void on_actionResetZoom_triggered();

	// Help menu
	void on_actionGettingStarted_triggered();
	void on_actionWhatsThis_triggered();
	void on_actionAbout_triggered();
	void on_actionAboutQt_triggered();

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


private slots:
	void on_actionSoramimi_TXT_triggered();

	void on_SliderPlaybackRate_valueChanged(int value);

private:
	Ui::EditorApp ui;
	GettingStartedDialog *gettingStarted;
	NoteGraphWidget *noteGraph;
	OperationStack opStack;
	OperationStack redoStack;
	QScopedPointer<Song> song;
	QMediaPlayer *player;
	BufferPlayer *bufferPlayers[2];
	QScopedPointer<Synth> synth;
	Piano *piano;
	QProgressBar *statusbarProgress;
	QPushButton *statusbarButton;
	QString projectFileName;
	QString latestPath;
	int currentBufferPlayer;
};
