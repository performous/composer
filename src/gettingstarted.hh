#include <QSettings>
#include "ui_gettingstarted.h"
#include "editorapp.hh"
#include <iostream>

class GettingStartedDialog: public QDialog, private Ui::GettingStarted
{
	Q_OBJECT
public:
	GettingStartedDialog(QWidget* parent)
		: QDialog(parent), m_editorApp(qobject_cast<EditorApp*>(parent))
	{
		if (!m_editorApp) throw std::runtime_error("Couldn't open help dialog.");
		setupUi(this);
		QSettings settings;
		chkShowOnStartup->setChecked(settings.value("showhelp", true).toBool());
	}

public slots:
	void on_cmdClose_clicked(bool) { close(); }

	void on_chkShowOnStartup_stateChanged(int state) {
		QSettings settings;
		settings.setValue("showhelp", state != Qt::Unchecked);
	}

	// Command link buttons

	void on_cmdMusicFile_clicked(bool) {
		m_editorApp->on_actionMusicFile_triggered();
	}

	void on_cmdLyricsFromFile_clicked(bool) {
		m_editorApp->on_actionLyricsFromFile_triggered();
	}

	void on_cmdLyricsFromClipboard_clicked(bool) {
		m_editorApp->on_actionLyricsFromClipboard_triggered();
	}

	void on_cmdTimeLyrics_clicked(bool) {
		m_editorApp->highlightLabel("TIMING");
	}

	void on_cmdFineTuneLyrics_clicked(bool) {
		m_editorApp->highlightLabel("TUNING");
	}

	void on_cmdMetadata_clicked(bool) {
		m_editorApp->highlightLabel("SONG");
	}

	void on_cmdExport_clicked(bool) {
		m_editorApp->showExportMenu();
	}
	void on_cmdLRC_clicked(bool) {
		m_editorApp->on_actionLyricsFromLRCFile_triggered();
	}

protected:
	void closeEvent(QCloseEvent*) {
		QSettings settings;
		settings.setValue("showhelp", chkShowOnStartup->isChecked());
	}

private:
	EditorApp *m_editorApp;
};
