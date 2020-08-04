#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QClipboard>
#include <QMimeData>
#include <QWhatsThis>
#include <QUrl>
#include <QCloseEvent>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QMediaPlayer>
#include <iostream>
#include "config.hh"
#include "editorapp.hh"
#include "notelabel.hh"
#include "songparser.hh"
#include "songwriter.hh"
#include "textcodecselector.hh"
#include "gettingstarted.hh"
#include "busydialog.hh"

namespace {
	static const QString PROJECT_SAVE_FILE_EXTENSION = "songproject"; // FIXME: Nice extension here
	static const quint32 PROJECT_SAVE_FILE_MAGIC = 0x50455350;
	static const quint32 PROJECT_SAVE_FILE_VERSION = 101; // File format version 1.01
	static const QDataStream::Version PROJECT_SAVE_FILE_STREAM_VERSION = QDataStream::Qt_4_7;

	// Helper function scans widget's children and sets their status tips to their tooltips
	void handleTips(QWidget *widget) {
		QObjectList objs = widget->children();
		objs.push_back(widget); // Handle the parent too
		for (int i = 0; i < objs.size(); ++i) {
			QWidget *child = qobject_cast<QWidget*>(objs[i]);
			if (!child) continue;
			if (!child->toolTip().isEmpty() && child->statusTip().isEmpty())
				child->setStatusTip(child->toolTip());
		}
	}
}

EditorApp::EditorApp(QWidget *parent)
	: QMainWindow(parent), gettingStarted(), noteGraph(), player(), synth(), statusbarProgress(),
	projectFileName(), latestPath(QDir::homePath()), currentBufferPlayer()
{
	ui.setupUi(this);
	readSettings();

	ui.helpDock->setVisible(false);

	// Some icons to make menus etc prettier
	setWindowIcon(QIcon::fromTheme("composer", QIcon(":/icons/composer.png")));
	ui.actionNew->setIcon(QIcon::fromTheme("document-new", QIcon(":/icons/document-new.png")));
	ui.actionOpen->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/document-open.png")));
	ui.actionSave->setIcon(QIcon::fromTheme("document-save", QIcon(":/icons/document-save.png")));
	ui.actionSaveAs->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/icons/document-save-as.png")));
	ui.actionExit->setIcon(QIcon::fromTheme("application-exit", QIcon(":/icons/application-exit.png")));
	ui.actionUndo->setIcon(QIcon::fromTheme("edit-undo", QIcon(":/icons/edit-undo.png")));
	ui.actionRedo->setIcon(QIcon::fromTheme("edit-redo", QIcon(":/icons/edit-redo.png")));
	ui.actionCut->setIcon(QIcon::fromTheme("edit-cut", QIcon(":/icons/edit-cut.png")));
	ui.actionCopy->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/edit-copy.png")));
	ui.actionPaste->setIcon(QIcon::fromTheme("edit-paste", QIcon(":/icons/edit-paste.png")));
	ui.actionDelete->setIcon(QIcon::fromTheme("edit-delete", QIcon(":/icons/edit-delete.png")));
	ui.actionSelectAll->setIcon(QIcon::fromTheme("edit-select-all", QIcon(":/icons/edit-select-all.png")));
	ui.actionSelectAllAfter->setIcon(QIcon::fromTheme("edit-select-all", QIcon(":/icons/edit-select-all.png")));
	ui.menuPreferences->setIcon(QIcon::fromTheme("preferences-other", QIcon(":/icons/preferences-other.png")));
	ui.actionMusicFile->setIcon(QIcon::fromTheme("insert-object", QIcon(":/icons/insert-object.png")));
	ui.actionAdditionalMusicFile->setIcon(QIcon::fromTheme("insert-object", QIcon(":/icons/insert-object.png")));
	ui.actionLyricsFromFile->setIcon(QIcon::fromTheme("insert-text", QIcon(":/icons/insert-text.png")));
	ui.actionLyricsFromClipboard->setIcon(QIcon::fromTheme("insert-text", QIcon(":/icons/insert-text.png")));
	ui.actionLyricsFromLRCFile->setIcon(QIcon::fromTheme("insert-text", QIcon(":/icons/insert-text.png")));
	ui.actionZoomIn->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/icons/zoom-in.png")));
	ui.actionZoomOut->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/icons/zoom-out.png")));
	ui.actionResetZoom->setIcon(QIcon::fromTheme("zoom-original", QIcon(":/icons/zoom-original.png")));
	ui.actionHelp->setIcon(QIcon::fromTheme("help-contents", QIcon(":/icons/help-contents.png")));
	ui.actionWhatsThis->setIcon(QIcon::fromTheme("help-hint", QIcon(":/icons/help-hint.png")));
	ui.actionAbout->setIcon(QIcon::fromTheme("help-about", QIcon(":/icons/help-about.png")));
	ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/media-playback-start.png")));

	// Statusbar stuff
	statusbarButton = new QPushButton(NULL);
	ui.statusbar->addPermanentWidget(statusbarButton);
	statusbarButton->setText(tr("&Abort"));
	statusbarButton->hide();
	statusbarProgress = new QProgressBar(NULL);
	ui.statusbar->addPermanentWidget(statusbarProgress);
	statusbarProgress->hide();

	setWindowModified(false);
	updateMenuStates();

	// Audio stuff
	player = new QMediaPlayer(this);
	player->setNotifyInterval(100);
	bufferPlayers[0] = new BufferPlayer(this);
	bufferPlayers[1] = new BufferPlayer(this);

	// Audio info
	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	qDebug() << "QAudioDevice info";
	qDebug() << "Supported codecs:";
	foreach (const QString& codec, info.supportedCodecs())
		qDebug() << codec;
	qDebug() << "Supported sample rates:";
	foreach (const int& num, info.supportedSampleRates())
		qDebug() << num;
	qDebug() << "Supported sample sizes:";
	foreach (const int& num, info.supportedSampleSizes())
		qDebug() << num;
	qDebug() << "Supported sample types:";
	foreach (const int& num, info.supportedSampleTypes())
		qDebug() << num;
	qDebug() << "Supported byte orders:";
	foreach (const int& num, info.supportedByteOrders())
		qDebug() << num;
	qDebug() << "Supported channel counts:";
	foreach (const int& num, info.supportedChannelCounts())
		qDebug() << num;


	// Audio signals
	connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(audioTick(qint64)));
	connect(player, SIGNAL(stateChanged(QMediaPlayer::State)), this, SLOT(playerStateChanged(QMediaPlayer::State)));
	connect(player, SIGNAL(error(QMediaPlayer::Error)), this, SLOT(playerError(QMediaPlayer::Error)));
	connect(player, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));
	connect(player, SIGNAL(playbackRateChanged(qreal)), this, SLOT(playbackRateChanged(qreal)));

	// The piano keys
	piano = new Piano(ui.topFrame);
	QHBoxLayout *hl = new QHBoxLayout(ui.topFrame);
	hl->addWidget(piano);
	hl->addWidget(ui.noteGraphScroller);
	ui.topFrame->setLayout(hl);
	connect(ui.noteGraphScroller->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(updatePiano(int)));

	// NoteGraph setup down here so that the objects we setup signals are already created
	setupNoteGraph();
	updateNoteInfo(NULL);

	connect(ui.cmdMusicFile, SIGNAL(clicked()), this, SLOT(on_actionMusicFile_triggered()));
	connect(ui.cmdVideoFile, SIGNAL(clicked()), this, SLOT(on_actionVideoFile_triggered()));

	piano->updatePixmap(noteGraph);

	song.reset(new Song);

	// Set status tips to tool tips
	handleTips(ui.tabGeneral);
	handleTips(ui.tabSong);

	QSettings settings;
	if (settings.value("showhelp", true).toBool())
		on_actionGettingStarted_triggered();
}

void EditorApp::setupNoteGraph()
{
	noteGraph = new NoteGraphWidget(NULL);
	ui.noteGraphScroller->setWidget(noteGraph); // Deletes previous widget

	// Splitter sizes cannot be set through designer :(
	//QList<int> ss; ss.push_back(700); ss.push_back(300); // Proportions, not pixels
	//ui.splitter->setSizes(ss);

	// Signals/slots
	connect(noteGraph, SIGNAL(operationDone(const Operation&)), this, SLOT(operationDone(const Operation&)));
	connect(noteGraph, SIGNAL(updateNoteInfo(NoteLabel*)), this, SLOT(updateNoteInfo(NoteLabel*)));
	connect(noteGraph, SIGNAL(statusBarMessage(QString)), this, SLOT(statusBarMessage(QString)));
	connect(statusbarButton, SIGNAL(clicked()), noteGraph, SLOT(abortPitch()));
	connect(ui.noteGraphScroller->horizontalScrollBar(), SIGNAL(valueChanged(int)), noteGraph, SLOT(updatePitch()));
	connect(ui.noteGraphScroller->verticalScrollBar(), SIGNAL(valueChanged(int)), noteGraph, SLOT(updatePitch()));
	connect(ui.actionCut, SIGNAL(triggered()), noteGraph, SLOT(cut()));
	connect(ui.actionCopy, SIGNAL(triggered()), noteGraph, SLOT(copy()));
	connect(ui.actionPaste, SIGNAL(triggered()), noteGraph, SLOT(paste()));
	connect(ui.cmdTimeSentence, SIGNAL(pressed()), noteGraph, SLOT(timeSentence()));
	connect(ui.cmdTimeNote,SIGNAL(pressed()),noteGraph, SLOT(timeSyllable()));
	connect(ui.cmdSkipSentence, SIGNAL(pressed()), noteGraph, SLOT(selectNextSentenceStart()));
	connect(ui.chkGrabSeekHandle, SIGNAL(toggled(bool)), noteGraph, SLOT(setSeekHandleWrapToViewport(bool)));
	noteGraph->setSeekHandleWrapToViewport(ui.chkGrabSeekHandle->isChecked());
	connect(noteGraph, SIGNAL(analyzeProgress(int, int)), this, SLOT(analyzeProgress(int, int)));
	connect(noteGraph, SIGNAL(seeked(qint64)), player, SLOT(setPosition(qint64)));
}

void EditorApp::operationDone(const Operation &op)
{
	//std::cout << "Push op: " << op.dump() << std::endl;
	setWindowModified(true);
	opStack.push(op);
	updateMenuStates();
	redoStack.clear();
}

void EditorApp::statusBarMessage(const QString& message)
{
	statusBar()->showMessage(message);
}

void EditorApp::doOpStack()
{
	BusyDialog busy(this, 20);
	noteGraph->clearNotes();
	QString newMusic = "";
	OperationStack::iterator opit = opStack.begin();

	// Re-apply all operations in the stack
	while (opit != opStack.end()) {
		//std::cout << "Doing op: " << opit->dump() << std::endl;
		busy();
		bool erased = false;
		try {
			if (opit->op() == "META") {
				// META ops are handled differently:
				// They are run once and then removed from the stack.
				// They are written to disk when saving though.
				QString metakey = opit->s(1), metavalue = opit->s(2);
				opit = opStack.erase(opit);
				erased = true;
				if (metakey == "MUSICFILE") {
					newMusic = metavalue;
				} else if (metakey == "VIDEOFILE") {
					song->video = metavalue;
				} else if (metakey == "BPM") {
					std::cout << "BPM: " << metavalue.toDouble() << " from '" << metavalue.toStdString() << "'" << std::endl;
					song->bpm = metavalue.toDouble();
				} else if (metakey == "TITLE") {
					song->title = metavalue;
				} else if (metakey == "ARTIST") {
					song->artist = metavalue;
				} else if (metakey == "GENRE") {
					song->genre = metavalue;
				} else if (metakey == "DATE") {
					song->year = metavalue;
				} else throw std::runtime_error("Unknown META key " + metakey.toStdString());

				updateSongMeta(true);

			} else // Regular note operations
				noteGraph->doOperation(*opit, Operation::NO_EMIT | Operation::NO_UPDATE);
		} catch (std::exception& e) { std::cout << e.what() << std::endl; }

		if (!erased) {
			++opit;
			erased = false;
		}
	}

	noteGraph->updateNotes();
	noteGraph->startNotePixmapUpdates();
	if (!newMusic.isEmpty()) setMusic(newMusic);
	
	setVideo(song->video);
	setBPM(song->bpm);
	
	updateMenuStates();
}

void EditorApp::updateMenuStates()
{
	// File menu
	ui.actionSave->setEnabled(isWindowModified());
	// Edit menu
	ui.actionUndo->setEnabled(!opStack.isEmpty() && opStack.top().op() != "BLOCK");
	ui.actionRedo->setEnabled(!redoStack.isEmpty());
	bool hasSelectedNotes = (noteGraph && noteGraph->selectedNote());
	ui.actionCut->setEnabled(hasSelectedNotes);
	ui.actionCopy->setEnabled(hasSelectedNotes);
	ui.actionDelete->setEnabled(hasSelectedNotes);
	bool hasNotes = (noteGraph && !noteGraph->noteLabels().isEmpty());
	ui.actionSelectAll->setEnabled(hasNotes);
	ui.actionSelectAllAfter->setEnabled(hasSelectedNotes);
	bool zoom = (noteGraph && noteGraph->getZoomLevel() != 100);
	ui.actionResetZoom->setEnabled(zoom);
	// Window title
	updateTitle();
}

void EditorApp::updateTitle()
{
	// Project name
	QFileInfo finfo(projectFileName);
	QString proName = finfo.fileName().isEmpty() ? tr("Untitled") : finfo.fileName();
	// Zoom level
	QString zoom = "";
	if (noteGraph && noteGraph->getZoomLevel() != 100)
		zoom = " - " + QString::number(noteGraph->getZoomLevel()) + " %";
	// Set the title
	setWindowTitle(QString(PACKAGE) + " - " + proName + "[*]" + zoom);
}

void EditorApp::updateNoteInfo(NoteLabel *note)
{
	if (!note || !noteGraph || noteGraph->selectedNotes().size() > 1) {
		ui.cmdSplit->setEnabled(false);
		ui.cmdInsert->setEnabled(false);
		ui.lblCurrentSentence->setText(tr("Current phrase:") + " -");
		// The next ones are available also for multi-note selections, so let's not disable them
		if (!note || !noteGraph) {
			ui.cmbNoteType->setEnabled(false);
			ui.chkFloating->setEnabled(false);
			ui.chkLineBreak->setEnabled(false);
		}
	}
	if (note && noteGraph) {
		if (noteGraph->selectedNotes().size() == 1) {
			// These are only available for single note selections
			ui.cmdSplit->setEnabled(true);
			ui.cmdInsert->setEnabled(true);
			ui.lblCurrentSentence->setText(tr("Current phrase: ")
				+ "<small>" + noteGraph->getPrevSentence() + "</small> <b>" + noteGraph->getCurrentSentence() + "</b>");
		}
		// The next ones are available also for multi-note selections
		ui.cmbNoteType->setEnabled(true);
		ui.cmbNoteType->setCurrentIndex(note->note().getTypeInt());
		ui.chkFloating->setEnabled(true);
		ui.chkFloating->setChecked(note->isFloating());
		ui.chkLineBreak->setEnabled(true);
		ui.chkLineBreak->setChecked(note->note().lineBreak);
	}
	updateMenuStates();
	// Update piano
	if (piano && noteGraph) {
		piano->updatePixmap(noteGraph);
	}
}

void EditorApp::analyzeProgress(int value, int maximum)
{
	if (statusbarProgress) {
		if (value == maximum) {
			statusbarProgress->hide();
			statusbarButton->hide();
		} else {
			statusbarProgress->setMaximum(maximum);
			statusbarProgress->setValue(value);
			statusbarProgress->show();
			statusbarButton->show();
		}
	}
}



// File menu


void EditorApp::on_actionNew_triggered()
{
	if (promptSaving()) {
		player->setMedia(NULL);
		song.reset(new Song);
		setupNoteGraph();
		projectFileName = "";
		opStack.clear();
		redoStack.clear();
		updateNoteInfo(NULL);
		statusbarProgress->hide();
		statusbarButton->hide();
		ui.txtTitle->clear(); ui.txtArtist->clear(); ui.txtGenre->clear(); ui.txtYear->clear();
		ui.valMusicFile->clear();
		setWindowModified(false);
	}
	updateMenuStates();
}

void EditorApp::on_actionOpen_triggered()
{
	if (!promptSaving()) return;

	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			latestPath,
			tr("All supported formats") + "(*." + PROJECT_SAVE_FILE_EXTENSION + " *.xml *.mid *.ini *.txt);;" +
			tr("Project files") +" (*." + PROJECT_SAVE_FILE_EXTENSION + ") ;;" +
			tr("SingStar XML") + " (*.xml);;" +
			tr("Frets on Fire MIDI") + " (*.mid *.ini);;" +
			tr("UltraStar TXT") + " (*.txt);;" +
			tr("LRC") + " (*.lrc);;" +
			tr("All files") + " (*)");

	if (!fileName.isNull()) openFile(fileName);
}

void EditorApp::openFile(QString fileName)
{
	if (!fileName.isNull()) {
		QFileInfo finfo(fileName);
		latestPath = finfo.path();
		try {
			if (finfo.suffix() == PROJECT_SAVE_FILE_EXTENSION) {

				// Project file loading
				QFile f(fileName);
				if (f.open(QFile::ReadOnly)) {
					opStack.clear();
					QDataStream in(&f);
					quint32 magic; in >> magic;
					if (magic == PROJECT_SAVE_FILE_MAGIC) {
						quint32 version; in >> version;
						if (version == PROJECT_SAVE_FILE_VERSION) {
							in.setVersion(PROJECT_SAVE_FILE_STREAM_VERSION);
							while (!in.atEnd()) {
								Operation op;
								in >> op;
								//std::cout << "Loaded op: " << op.dump() << std::endl;
								opStack.push(op);
							}
							doOpStack();
							projectFileName = fileName;
							setWindowModified(false);
							updateNoteInfo(NULL); // Title bar
							if (noteGraph) noteGraph->scrollToFirstNote();
						} else
							QMessageBox::critical(this, tr("Error opening file!"), tr("Unsupported project file version in %1").arg(fileName));
					} else
						QMessageBox::critical(this, tr("Error opening file!"), tr("File %1 doesn't look like a project file.").arg(fileName));
				} else
					QMessageBox::critical(this, tr("Error opening file!"), tr("Couldn't open file %1 for reading.").arg(fileName));

			} else {

				// Song import
				QString musicfile = song->music["EDITOR"]; // Preserve the music file
				song.reset(new Song(QString(finfo.path()+"/"), finfo.fileName()));
				song->music["EDITOR"] = musicfile;
				noteGraph->setLyrics(song->getVocalTrack());
				updateSongMeta(true);
			}
		} catch (const std::exception& e) {
			QMessageBox::critical(this, tr("Error loading file!"), e.what());
		}
	}
}

void EditorApp::on_actionSave_triggered()
{
	if (projectFileName.isEmpty()) on_actionSaveAs_triggered();
	else saveProject(projectFileName);
}

void EditorApp::on_actionSaveAs_triggered()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save Project"),
			latestPath,
			tr("Project files ") + "(*." + PROJECT_SAVE_FILE_EXTENSION + ");;" +
			tr("All files") + " (*)");
	if (!fileName.isNull()) {
		// Add the correct suffix if it is missing
		QFileInfo finfo(fileName);
		latestPath = finfo.path();
		if (finfo.suffix() != PROJECT_SAVE_FILE_EXTENSION)
			fileName += "." + PROJECT_SAVE_FILE_EXTENSION;
		saveProject(fileName);
	}
}

bool EditorApp::promptSaving()
{
	updateSongMeta(); // Make sure the stuff in textboxes is updated
	if (isWindowModified()) {
		QMessageBox::StandardButton b = QMessageBox::question(this, tr("Unsaved changes"),
			tr("There are unsaved changes, which would be lost. Save now?"),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		switch(b) {
		case QMessageBox::Yes:
			on_actionSave_triggered();
		case QMessageBox::No:
			return true;
		default:
			return false;
		}
	}
	return true;
}

namespace {

	QString toQString(double d) {
		const auto a = floor(d);
		const auto b = (d - a) * 1000;	
		
		return QString(std::to_string(static_cast<int>(a)).c_str()) + "." + QString(std::to_string(static_cast<int>(b)).c_str());
	}
}

void EditorApp::saveProject(QString fileName)
{
	QFile f(fileName);
	if (f.open(QFile::WriteOnly)) {
		QDataStream out(&f);
		out.setVersion(PROJECT_SAVE_FILE_STREAM_VERSION);
		out << PROJECT_SAVE_FILE_MAGIC << PROJECT_SAVE_FILE_VERSION;

		// Notes
		foreach (Operation op, opStack)
			out << op;

		// Song metadata
		out << Operation("META", "TITLE", song->title)
			<< Operation("META", "ARTIST", song->artist)
			<< Operation("META", "GENRE", song->genre)
			<< Operation("META", "DATE", song->year)
			<< Operation("META", "MUSICFILE", song->music["EDITOR"])
			<< Operation("META", "VIDEOFILE", song->video)
			<< Operation("META", "BPM", toQString(song->bpm));

		projectFileName = fileName;
		setWindowModified(false);
	} else
		QMessageBox::critical(this, tr("Error saving file!"), tr("Couldn't open file %1 for saving.").arg(fileName));
	updateMenuStates();
}

void EditorApp::exportSong(QString format, QString dialogTitle)
{
	QString path = QFileDialog::getExistingDirectory(this, dialogTitle, latestPath);
	if (!path.isNull()) {
		latestPath = path;
		// Sync notes
		if (noteGraph) song->insertVocalTrack(TrackName::LEAD_VOCAL, noteGraph->getVocalTrack());
		// Pick exporter
		try {
			if (format == "XML") SingStarXMLWriter(*song.data(), path);
			else if (format == "TXT") UltraStarTXTWriter(*song.data(), path);
			else if (format == "INI") FoFMIDIWriter(*song.data(), path);
			else if (format == "LRC") LRCWriter(*song.data(), path, false);
			else if (format == "ENHANCED LRC") LRCWriter(*song.data(), path, true);
			else if (format == "SMM") SMMWriter(*song.data(), path);
		} catch (const std::exception& e) {
			QMessageBox::critical(this, tr("Error exporting song!"), e.what());
		}
	}
}

void EditorApp::on_actionSingStarXML_triggered() { exportSong("XML", tr("Export SingStar XML")); }

void EditorApp::on_actionUltraStarTXT_triggered() { exportSong("TXT", tr("Export UltraStar TXT")); }

void EditorApp::on_actionFoFMIDI_triggered() { exportSong("INI", tr("Export Frets on Fire MIDI")); }

void EditorApp::on_actionLRC_triggered() { exportSong("LRC", tr("Export LRC")); }

void EditorApp::on_actionEnhanced_LRC_triggered() { exportSong("ENHANCED LRC", tr("Export Enhanced LRC")); }

void EditorApp::on_actionSoramimiTXT_triggered() { exportSong("SMM", tr("Export Soramimi TXT")); }


void EditorApp::on_actionLyricsToFile_triggered()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export to plain text lyrics"), latestPath);
	if (!fileName.isNull()) {
		QFile f(fileName);
		QFileInfo finfo(f); latestPath = finfo.path();
		if (f.open(QFile::WriteOnly | QFile::Truncate)) {
			QTextStream out(&f);
			out << noteGraph->dumpLyrics();
		} else
			QMessageBox::critical(this, tr("Error saving file!"), tr("Couldn't open file %1 for writing.").arg(fileName));
	}
}

void EditorApp::on_actionLyricsToClipboard_triggered()
{
	QApplication::clipboard()->setText(noteGraph->dumpLyrics());
}

void EditorApp::on_actionExit_triggered()
{
	close();
}



// Edit menu


void EditorApp::on_actionUndo_triggered()
{
	if (opStack.isEmpty())
		return;
	if (opStack.top().op() == "BLOCK") {
		updateMenuStates();
		return;
	} else if (opStack.top().op() == "COMBINER") {
		// Special handling to add the ops in the right order
		try {
			int count = opStack.top().i(1);
			int start = opStack.size() - count - 1;
			for (int i = start; i < start + count; ++i) {
				redoStack.push(opStack.at(i));
			}
			opStack.remove(start, count);
		} catch (std::runtime_error&) {
			QMessageBox::critical(this, tr("Error!"), tr("Corrupted undo stack."));
		}
	}
	redoStack.push(opStack.top());
	opStack.pop();
	doOpStack();
}

void EditorApp::on_actionRedo_triggered()
{
	if (redoStack.isEmpty())
		return;
	else if (redoStack.top().op() == "COMBINER") {
		// Special handling to add the ops in the right order
		try {
			int count = redoStack.top().i(1);
			int start = redoStack.size() - count - 1;
			for (int i = start; i < start + count; ++i) {
				opStack.push(redoStack.at(i));
			}
			redoStack.remove(start, count);
		} catch (std::runtime_error&) {
			QMessageBox::critical(this, tr("Error!"), tr("Corrupted redo stack."));
		}
	}
	opStack.push(redoStack.top());
	redoStack.pop();
	doOpStack();
}

void EditorApp::on_actionDelete_triggered()
{
	if (noteGraph) noteGraph->del(noteGraph->selectedNote());
}

void EditorApp::on_actionSelectAll_triggered()
{
	if (noteGraph) noteGraph->selectAll();
}

void EditorApp::on_actionSelectAllAfter_triggered()
{
	if (noteGraph) noteGraph->selectAllAfter();
}

void EditorApp::on_actionAntiAliasing_toggled(bool checked)
{
	QSettings settings; // Default QSettings parameters given in main()
	settings.setValue("anti-aliasing", checked);
}



// Insert menu


void EditorApp::setMusic(QString filepath, bool primary)
{
	ui.valMusicFile->setText(filepath);
	song->music[primary ? "EDITOR" : "ADDITIONAL"] = filepath;
	setWindowModified(true);
	updateMenuStates();
	if (primary) {
		// Metadata is updated when it becomes available (signal)
		player->setMedia(QMediaContent(QUrl::fromLocalFile(filepath)));
		noteGraph->updateMusicPos(0, false);
		// Fire up analyzer
		noteGraph->analyzeMusic(filepath);
	} else noteGraph->analyzeMusic(filepath, 1);
}

void EditorApp::setVideo(QString filepath)
{
	ui.valVideoFile->setText(filepath);
	song->video = filepath;
	setWindowModified(true);
	updateMenuStates();
}

void EditorApp::setBPM(double bpm)
{
	ui.txtBPM->setText(std::to_string(bpm).c_str());
	song->bpm = bpm;
	setWindowModified(true);
	updateMenuStates();
}


void EditorApp::on_actionMusicFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			latestPath,
			tr("Music files") + " (*.mp3 *.ogg *.wav *.wma *.flac)");

	if (!fileName.isNull()) {
		QFileInfo finfo(fileName); latestPath = finfo.path();
		setMusic(fileName);
	}
}

void EditorApp::on_actionVideoFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			latestPath,
			tr("Video files") + " (*.avi *.mpg *.mpeg *.flv *.mov *.mp4 *.mkv *.m4v)");

	if (!fileName.isNull()) {
		QFileInfo finfo(fileName); latestPath = finfo.path();
		setVideo(fileName);
	}
}

void EditorApp::on_actionAdditionalMusicFile_triggered()
{
	// FIXME: Duplication with on_actionMusicFile_triggered()
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			latestPath,
			tr("Music files") + " (*.mp3 *.ogg *.wav *.wma *.flac)");

	if (!fileName.isNull()) {
		QFileInfo finfo(fileName); latestPath = finfo.path();
		setMusic(fileName, false);
	}
}

void EditorApp::on_actionLyricsFromFile_triggered()
{
	if ((noteGraph && noteGraph->noteLabels().empty())
		|| QMessageBox::question(this, tr("Replace lyrics"),
			tr("Loading lyrics from a file will replace the existing ones. Continue?"),
			QMessageBox::Ok | QMessageBox::Cancel)
		== QMessageBox::Ok)
	{
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
				latestPath,
				tr("Text files (*.txt)"));

		if (!fileName.isNull()) {
			QFile file(fileName);
			QFileInfo finfo(file); latestPath = finfo.path();
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
				return;

			QString text = TextCodecSelector::readAllAndHandleEncoding(file, this);
			if (text != "") {
				if (SongParser::looksLikeSongFile(text)
					&& QMessageBox::question(this, tr("Song file detected"),
						tr("The file you are opening doesn't look like plain text lyrics, but rather an actual song file. Would you like to reload it as such?"),
						QMessageBox::Yes | QMessageBox::No)
					== QMessageBox::Yes)
				{
					openFile(fileName);
				} else noteGraph->setLyrics(text);
			}
		}
	}
}

void EditorApp::on_actionLyricsFromClipboard_triggered()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();

	if (mimeData->hasText() && !mimeData->text().isEmpty()) {
		QString text = mimeData->text();
		if ((noteGraph && noteGraph->noteLabels().empty())
			|| QMessageBox::question(this, tr("Replace lyrics"),
				tr("Pasting lyrics from clipboard will replace the existing ones. Continue?"),
				QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Ok)
		{
			noteGraph->setLyrics(text);
		}
	} else {
		QMessageBox::warning(this, tr("No text to paste"), tr("No suitable data on the clipboard."));
	}
}

void EditorApp::on_actionLyricsFromLRCFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		latestPath,
		tr("LRC/Soramimi timecodes") + " (*.lrc *.txt)");
	openFile(fileName);
}


// View menu


void EditorApp::on_actionZoomIn_triggered()
{
	if (noteGraph) noteGraph->zoom(1.0);
}

void EditorApp::on_actionZoomOut_triggered()
{
	if (noteGraph) noteGraph->zoom(-1.0);
}

void EditorApp::on_actionResetZoom_triggered()
{
	if (noteGraph) noteGraph->zoom(getNaN());
}



// Help menu


void EditorApp::on_actionGettingStarted_triggered()
{
	if (!gettingStarted) gettingStarted = new GettingStartedDialog(this);
	gettingStarted->show();
}

void EditorApp::on_actionWhatsThis_triggered()
{
	QWhatsThis::enterWhatsThisMode();
}

void EditorApp::on_actionAboutQt_triggered()
{
	QApplication::aboutQt();
}

void EditorApp::on_actionAbout_triggered()
{
	AboutDialog aboutDialog(this);
	aboutDialog.exec();
}



// Misc stuff


void EditorApp::updateSongMeta(bool readFromSongToUI)
{
	if (!song) return;

	if (!readFromSongToUI) {
		if (ui.txtTitle->text() != song->title) {
			song->title = ui.txtTitle->text();
		}
		if (ui.txtArtist->text() != song->artist) {
			song->artist = ui.txtArtist->text();
		}
		if (ui.txtGenre->text() != song->genre) {
			song->genre = ui.txtGenre->text();
		}
		if (ui.txtYear->text() != song->year) {
			song->year = ui.txtYear->text();
		}
		const auto bpm = ui.txtBPM->text().toDouble(); 
		if (bpm != song->bpm)
			song->bpm = bpm;
	} else {
		if (!song->title.isEmpty()) ui.txtTitle->setText(song->title);
		if (!song->artist.isEmpty()) ui.txtArtist->setText(song->artist);
		if (!song->genre.isEmpty()) ui.txtGenre->setText(song->genre);
		if (!song->year.isEmpty()) ui.txtYear->setText(song->year);
		
		ui.txtBPM->setText(toQString(song->bpm));
	}
}

void EditorApp::metaDataChanged()
{
	if (player) {
		//foreach (const QString& str, player->availableMetaData())
		//	qDebug() << str;
		if (!player->metaData("Title").toString().isEmpty())
			song->title = player->metaData("Title").toString();
		if (!player->metaData("AlbumArtist").toString().isEmpty())
			song->artist = player->metaData("AlbumArtist").toString();
		if (!player->metaData("Genre").toString().isEmpty())
			song->genre = player->metaData("Genre").toString();
		if (!player->metaData("Year").toString().isEmpty())
			song->year = player->metaData("Year").toString();
		if (!player->metaData("BPM").toString().isEmpty())
			song->bpm = player->metaData("BPM").toDouble();
		updateSongMeta(true);
	}
}

void EditorApp::playButton()
{
	if (player && player->state() == QMediaPlayer::PlayingState) {
		ui.cmdPlay->setText(tr("Pause (P)"));
		ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-pause", QIcon(":/icons/media-playback-pause.png")));
		ui.cmdPlay->setShortcut(QKeySequence("P"));
		if (ui.chkSynth->isChecked()) on_chkSynth_clicked(true);
	} else {
		ui.cmdPlay->setText(tr("Play (P)"));
		ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/media-playback-start.png")));
		ui.cmdPlay->setShortcut(QKeySequence("P"));
		on_chkSynth_clicked(false);
	}
}

void EditorApp::on_chkSynth_clicked(bool checked)
{
	if (checked && player && player->state() == QMediaPlayer::PlayingState) {
		synth.reset(new Synth);
		connect(synth.data(), SIGNAL(playBuffer(QByteArray)), this, SLOT(playBuffer(QByteArray)));
		if (player) player->setVolume(66);
	} else if (!checked) {
		synth.reset();
		if (player) player->setVolume(100);
	}
}

void EditorApp::on_cmdPlay_clicked()
{
	if (player) {
		// Can't use currentMedia().isNull() because it will always return false after first set
		if (player->currentMedia().canonicalUrl().isEmpty()) {
			on_actionMusicFile_triggered();
		} else {
			if (player->state() == QMediaPlayer::PlayingState) player->pause();
			else player->play();
		}
	}
}

void EditorApp::audioTick(qint64 time)
{
	if (noteGraph && player)
		noteGraph->updateMusicPos(time, (player->state() == QMediaPlayer::PlayingState ? true : false));

	if (noteGraph && synth) {
		// Here we create some notes for the synthesizer to use.
		// We don't simply pass the whole list because then there
		// would be two threads working on the same list.
		SynthNotes notes;
		const NoteLabels &nls = noteGraph->noteLabels();
		int numberOfNotesToPass = 12;
		for (NoteLabels::const_iterator it = nls.begin(); it != nls.end() && numberOfNotesToPass > 0; ++it) {
			if ((*it)->note().begin >= time / 1000.0) {
				notes.push_back(SynthNote((*it)->note()));
				--numberOfNotesToPass;
			}
		}
		synth->tick(time, player ? player->playbackRate() : 1.0, notes);
	}
}

void EditorApp::playerStateChanged(QMediaPlayer::State state)
{
	playButton();
	if (state != QMediaPlayer::PlayingState) {
		noteGraph->stopMusic();
	} else if (!noteGraph->selectedNote() && !noteGraph->noteLabels().isEmpty()) {
		noteGraph->selectNote(noteGraph->noteLabels().front());
	}
}

void EditorApp::playerError(QMediaPlayer::Error)
{
	QString errst(tr("Error playing audio!"));
	if (player) errst += " " + player->errorString();
	QMessageBox::critical(this, tr("Playback error"), errst);
}

void EditorApp::playbackRateChanged(qreal rate)
{
	if (noteGraph) noteGraph->playbackRateChanged(rate);
}

void EditorApp::playBuffer(const QByteArray& buffer)
{
	if (bufferPlayers[currentBufferPlayer]->play(buffer))
		currentBufferPlayer = (currentBufferPlayer+1) % 2;
}


void EditorApp::on_txtTitle_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtArtist_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtGenre_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtYear_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtBPM_editingFinished() { updateSongMeta(); }

void EditorApp::on_cmdSplit_clicked()
{
	if (noteGraph) noteGraph->split(noteGraph->selectedNote());
}

void EditorApp::on_cmdInsert_clicked()
{
	if (noteGraph) noteGraph->createNote(noteGraph->selectedNote()->note().end);
}

void EditorApp::on_cmbNoteType_activated(int index)
{
	if (noteGraph) noteGraph->setType(noteGraph->selectedNote(), index);
}

void EditorApp::on_chkFloating_clicked(bool checked)
{
	if (noteGraph) noteGraph->setFloating(noteGraph->selectedNote(), checked);
}

void EditorApp::on_chkLineBreak_clicked(bool checked)
{
	if (noteGraph) noteGraph->setLineBreak(noteGraph->selectedNote(), checked);
}

void EditorApp::highlightLabel(QString id)
{
	const QString style = "background-color: #f00; font-weight: bold";
	clearLabelHighlights();

	// Set correct tab
	if (id == "SONG") ui.tabWidget->setCurrentIndex(1);
	else ui.tabWidget->setCurrentIndex(0);

	// Color the labels
	if (id == "TIMING") {
		ui.lblPlayback->setStyleSheet(style);
		ui.lblTiming->setStyleSheet(style);
	} else if (id == "TUNING") {
		ui.lblTools->setStyleSheet(style);
		ui.lblNoteProperties->setStyleSheet(style);
	}

	// Clear highlights after a while
	QTimer::singleShot(4000, this, SLOT(clearLabelHighlights()));
}

void EditorApp::clearLabelHighlights()
{
	ui.lblPlayback->setStyleSheet("");
	ui.lblTiming->setStyleSheet("");
	ui.lblTools->setStyleSheet("");
	ui.lblNoteProperties->setStyleSheet("");
}

void EditorApp::closeEvent(QCloseEvent *event)
{
	if (promptSaving()) event->accept();
	else event->ignore();
	writeSettings();
}

void EditorApp::readSettings()
{
	QSettings settings; // Default QSettings parameters given in main()
	// Read values
	QPoint pos = settings.value("pos", QPoint()).toPoint();
	QSize size = settings.value("size", QSize(800, 600)).toSize();
	bool maximized = settings.value("maximized", false).toBool();
	bool aa = settings.value("anti-aliasing", true).toBool();
	latestPath = settings.value("latestpath", QDir::homePath()).toString();
	// Apply them
	if (!pos.isNull()) move(pos);
	resize(size);
	if (maximized) showMaximized();
	ui.actionAntiAliasing->setChecked(aa);
 }

void EditorApp::writeSettings()
 {
	QSettings settings; // Default QSettings parameters given in main()
	settings.setValue("pos", pos());
	settings.setValue("size", size());
	settings.setValue("maximized", isMaximized());
	settings.setValue("anti-aliasing", ui.actionAntiAliasing->isChecked());
	settings.setValue("latestpath", latestPath);
 }



AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent)
{
	setupUi(this);
	setWindowTitle(tr("About %1").arg(PACKAGE));
	// Setup About tab
	lblName->setText(PACKAGE);
	lblVersion->setText(QString(tr("Version: ")) + VERSION);

	// Populate Authors tab
	{
		QFile f(":/docs/Authors.txt");
		f.open(QIODevice::ReadOnly);
		QTextStream in(&f);
		in.setCodec("UTF-8");
		txtAuthors->setPlainText(in.readAll());
	}
	// Populate License text
	{
		QFile f(":/docs/License.txt");
		f.open(QIODevice::ReadOnly);
		QTextStream in(&f);
		in.setCodec("UTF-8");
		txtLicense->setPlainText(in.readAll());
	}

	connect(cmdClose, SIGNAL(clicked()), this, SLOT(accept()));
	setModal(true);
}



namespace {
	bool selectionMatches(int n, NoteGraphWidget *ngw) {
		if (!ngw) return false;
		const NoteLabels& nls = ngw->selectedNotes();
		for (int i = 0; i < nls.size(); ++i)
			if (nls[i]->note().note == n) return true;
		return false;
	}
}

void EditorApp::updatePiano(int y)
{
	if (!piano) return;
	piano->move(piano->x(), ui.noteGraphScroller->y() - y);
}

Piano::Piano(QWidget *parent): QLabel(parent), m_player(new BufferPlayer(this))
{
	setMouseTracking(true);
}

void Piano::updatePixmap(NoteGraphWidget *ngw)
{
	const int notes = 12 * 4; // Four octaves
	const QColor borderColor = QColor("#c0c0c0");
	const QColor selectionColor = QColor("#a00");
	const QColor mouseColor = QColor("#090");
	int noteHeight = 16;
	int mousen = round((NoteGraphWidget::Height - mapFromGlobal(QCursor::pos()).y()) / 16.0);

	QImage image(50, notes * noteHeight, QImage::Format_ARGB32_Premultiplied);
	image.fill(qRgba(0, 0, 0, 0));
	setFixedSize(image.width(), image.height());
	{
		QPainter painter(&image);
		MusicalScale scale;
		QPen pen; pen.setWidth(2);
		int y;
		int w = image.width();
		// Render only the white keys first
		for (int i = -1; i < notes; ++i) {
			if (scale.isSharp(i)) continue;
			int y2 = image.height() - i * noteHeight;  // Note center y
			y2 -= (scale.isSharp(i + 1) ? 1.0 : 0.5) * noteHeight;  // Key top y
			// Pick border color according to selection status
			if (i == mousen) pen.setColor(mouseColor);
			else if (selectionMatches(i, ngw)) pen.setColor(selectionColor);
			else pen.setColor(borderColor);
			painter.setPen(pen);
			// Skip the first key because y hasn't been calculated yet
			if (i > -1) {
				painter.fillRect(0, y2, w, y - y2, QColor("#ffffff"));
				painter.drawRect(0, y2 + 2, w-1, y - y2 - 2);
			}
			y = y2;  // The next key bottom y
		}
		// Now render the black keys
		w *= 0.6;
		for (int i = 0; i < notes; ++i) {
			if (!scale.isSharp(i)) continue;
			y = image.height() - i*noteHeight - noteHeight / 2;
			// Pick border color according to selection status
			if (i == mousen) pen.setColor(mouseColor);
			else if (selectionMatches(i, ngw)) pen.setColor(selectionColor);
			else pen.setColor(borderColor);
			painter.setPen(pen);
			painter.fillRect(0, y, w, noteHeight, QColor("#000000"));
			painter.drawRect(0, y, w, noteHeight);
		}
	}
	setPixmap(QPixmap::fromImage(image));
}

void Piano::mousePressEvent(QMouseEvent *event)
{
	if (!m_player) return;
	QByteArray ba;
	int n = round((NoteGraphWidget::Height - event->pos().y()) / 16.0);
	Synth::createBuffer(ba, n % 12, 0.4);
	m_player->play(ba);
}

void Piano::mouseMoveEvent(QMouseEvent *event)
{
	updatePixmap(NULL);
}

void EditorApp::on_sliderPlaybackRate_valueChanged(int value)
{
	qreal playbackRate = value / 100.0; //qreal is actually a double
	player->setPlaybackRate(playbackRate);
	ui.labelPlayBackRate->setText(tr("Playback rate: ") + QString::number(playbackRate));
}


