#include <QProgressBar>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QClipboard>
#include <QMimeData>
#include <QWhatsThis>
#include <QUrl>
#include <QCloseEvent>
#include <QShortcut>
#include <QSettings>
#include <phonon/AudioOutput>
#include <iostream>
#include "config.hh"
#include "editorapp.hh"
#include "notelabel.hh"
#include "notegraphwidget.hh"
#include "songwriter.hh"
#include "textcodecselector.hh"
#include "gettingstarted.hh"

namespace {
	static const QString PROJECT_SAVE_FILE_EXTENSION = "songproject"; // FIXME: Nice extension here
	static const quint32 PROJECT_SAVE_FILE_MAGIC = 0x50455350;
	static const quint32 PROJECT_SAVE_FILE_VERSION = 100; // File format version 1.00
	static const QDataStream::Version PROJECT_SAVE_FILE_STREAM_VERSION = QDataStream::Qt_4_7;
}

EditorApp::EditorApp(QWidget *parent)
	: QMainWindow(parent), gettingStarted(), noteGraph(), player(), audioOutput(), synth(), statusbarProgress(),
	projectFileName(), hasUnsavedChanges(), latestPath(QDir::homePath())
{
	ui.setupUi(this);
	readSettings();

	// Some icons to make menus etc prettier
	ui.actionNew->setIcon(QIcon::fromTheme("document-new", QIcon(":/icons/document-new.png")));
	ui.actionOpen->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/document-open.png")));
	ui.actionSave->setIcon(QIcon::fromTheme("document-save", QIcon(":/icons/document-save.png")));
	ui.actionSaveAs->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/icons/document-save-as.png")));
	ui.actionExit->setIcon(QIcon::fromTheme("application-exit", QIcon(":/icons/application-exit.png")));
	ui.actionUndo->setIcon(QIcon::fromTheme("edit-undo", QIcon(":/icons/edit-undo.png")));
	ui.actionRedo->setIcon(QIcon::fromTheme("edit-redo", QIcon(":/icons/edit-redo.png")));
	ui.menuPreferences->setIcon(QIcon::fromTheme("preferences-other", QIcon(":/icons/preferences-other.png")));
	ui.actionMusicFile->setIcon(QIcon::fromTheme("insert-object", QIcon(":/icons/insert-object.png")));
	ui.actionLyricsFromFile->setIcon(QIcon::fromTheme("insert-text", QIcon(":/icons/insert-text.png")));
	ui.actionLyricsFromClipboard->setIcon(QIcon::fromTheme("insert-text", QIcon(":/icons/insert-text.png")));
	ui.actionWhatsThis->setIcon(QIcon::fromTheme("help-hint", QIcon(":/icons/help-hint.png")));
	ui.actionAbout->setIcon(QIcon::fromTheme("help-about", QIcon(":/icons/help-about.png")));
	ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/media-playback-start.png")));

	statusbarProgress = new QProgressBar(NULL);
	ui.statusbar->addPermanentWidget(statusbarProgress);
	statusbarProgress->hide();

	hasUnsavedChanges = false;
	updateMenuStates();

	// Audio stuff
	player = new Phonon::MediaObject(this);
	audioOutput = new Phonon::AudioOutput(this);
	player->setTickInterval(100);
	Phonon::createPath(player, audioOutput);
	// Audio signals
	connect(player, SIGNAL(tick(qint64)), this, SLOT(audioTick(qint64)));
	connect(player, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(playerStateChanged(Phonon::State,Phonon::State)));
	connect(player, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));

	// NoteGraph setup down here so that the objects we setup signals are already created
	setupNoteGraph();
	updateNoteInfo(NULL);
	new QShortcut(QKeySequence("Ctrl+A"), this, SLOT(selectAllNotes()));

	song.reset(new Song);

	QSettings settings;
	if (settings.value("showhelp", true).toBool())
		on_actionGettingStarted_triggered();
}

void EditorApp::setupNoteGraph()
{
	noteGraph = new NoteGraphWidget(NULL);
	ui.noteGraphScroller->setWidget(noteGraph);

	// Splitter sizes cannot be set through designer :(
	QList<int> ss; ss.push_back(700); ss.push_back(300); // Proportions, not pixels
	ui.splitter->setSizes(ss);

	// Signals/slots
	connect(noteGraph, SIGNAL(operationDone(const Operation&)), this, SLOT(operationDone(const Operation&)));
	connect(noteGraph, SIGNAL(updateNoteInfo(NoteLabel*)), this, SLOT(updateNoteInfo(NoteLabel*)));
	connect(ui.noteGraphScroller->horizontalScrollBar(), SIGNAL(valueChanged(int)), noteGraph, SLOT(updatePitch()));
	connect(ui.noteGraphScroller->verticalScrollBar(), SIGNAL(valueChanged(int)), noteGraph, SLOT(updatePitch()));
	connect(ui.cmdTimeSentence, SIGNAL(pressed()), noteGraph, SLOT(timeSentence()));
	connect(ui.cmdSkipSentence, SIGNAL(pressed()), noteGraph, SLOT(selectNextSentenceStart()));
	connect(ui.chkGrabSeekHandle, SIGNAL(toggled(bool)), noteGraph, SLOT(setSeekHandleWrapToViewport(bool)));
	connect(ui.cmdMusicFile, SIGNAL(clicked()), this, SLOT(on_actionMusicFile_triggered()));
	noteGraph->setSeekHandleWrapToViewport(ui.chkGrabSeekHandle->isChecked());
	connect(noteGraph, SIGNAL(analyzeProgress(int, int)), this, SLOT(analyzeProgress(int, int)));
	connect(noteGraph, SIGNAL(seeked(qint64)), player, SLOT(seek(qint64)));
}

void EditorApp::operationDone(const Operation &op)
{
	//std::cout << "Push op: " << op.dump() << std::endl;
	hasUnsavedChanges = true;
	opStack.push(op);
	updateMenuStates();
	redoStack.clear();
}

void EditorApp::doOpStack()
{
	noteGraph->clearNotes();
	QString newMusic = "";
	// Re-apply all operations in the stack
	for (OperationStack::const_iterator opit = opStack.begin(); opit != opStack.end(); ++opit) {
		//std::cout << "Doing op: " << opit->dump() << std::endl;
		try {
			if (opit->op() == "META") {
				// META ops are handled differently:
				// They are run once and then removed from the stack.
				// They are written to disk when saving though.
				QString metakey = opit->s(1), metavalue = opit->s(2);
				if (metakey == "MUSICFILE") {
					newMusic = metavalue;
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
				noteGraph->doOperation(*opit, Operation::NO_EMIT);
		} catch (std::exception& e) { std::cout << e.what() << std::endl; }
	}

	if (!newMusic.isEmpty()) setMusic(newMusic);
	updateMenuStates();
}

void EditorApp::updateMenuStates()
{
	// Save menu
	if (hasUnsavedChanges)
		ui.actionSave->setEnabled(true);
	else ui.actionSave->setEnabled(false);
	// Undo menu
	if (opStack.isEmpty() || opStack.top().op() == "BLOCK")
		ui.actionUndo->setEnabled(false);
	else ui.actionUndo->setEnabled(true);
	// Redo menu
	if (redoStack.isEmpty())
		ui.actionRedo->setEnabled(false);
	else ui.actionRedo->setEnabled(true);
	// Window title
	updateTitle();
}

void EditorApp::updateTitle()
{
	// Project name
	QFileInfo finfo(projectFileName);
	QString proName = finfo.fileName().isEmpty() ? tr("Untitled") : finfo.fileName();
	if (hasUnsavedChanges) proName += "*";
	// Zoom level
	QString zoom = "";
	if (noteGraph && noteGraph->getZoomLevel() != "100 %")
		zoom = " - " + noteGraph->getZoomLevel();
	// Set the title
	setWindowTitle(QString(PACKAGE) + " - " + proName + zoom);
}

void EditorApp::updateNoteInfo(NoteLabel *note)
{
	// For now we disable all controls when multiple notes are selected
	if (note && noteGraph && noteGraph->selectedNotes().size() == 1) {
		MusicalScale ms;
		ui.valNoteBegin->setText(QString::number(note->note().begin, 'f', 2) + tr(" s"));
		ui.valNoteEnd->setText(QString::number(note->note().end, 'f', 2) + tr(" s"));
		ui.valNoteDuration->setText(QString::number(note->note().length(), 'f', 2) + tr(" s"));
		ui.valNote->setText(ms.getNoteStr(ms.getNoteFreq(note->note().note))
			+ " (" + QString::number(note->note().note) + ")");
		ui.cmdSplit->setEnabled(true);
		ui.cmbNoteType->setEnabled(true);
		ui.cmbNoteType->setCurrentIndex(note->note().getTypeInt());
		ui.chkFloating->setEnabled(true);
		ui.chkFloating->setChecked(note->isFloating());
		ui.chkLineBreak->setEnabled(true);
		ui.chkLineBreak->setChecked(note->note().lineBreak);
		ui.lblCurrentSentence->setText(tr("Current phrase:") + " <b>" + noteGraph->getCurrentSentence() + "</b>");
		ui.lblPrevSentence->setText(tr("Previous phrase:") + " " + noteGraph->getPrevSentence());
	} else {
		ui.valNoteBegin->setText("-");
		ui.valNoteEnd->setText("-");
		ui.valNoteDuration->setText("-");
		ui.valNote->setText("-");
		ui.cmdSplit->setEnabled(false);
		ui.cmbNoteType->setEnabled(false);
		ui.chkFloating->setEnabled(false);
		ui.chkLineBreak->setEnabled(false);
		ui.lblCurrentSentence->setText(tr("Current phrase:") + " -");
		ui.lblPrevSentence->setText(tr("Previous phrase:") + " -");
	}
	updateTitle();
}

void EditorApp::analyzeProgress(int value, int maximum)
{
	if (statusbarProgress) {
		if (value == maximum) {
			statusbarProgress->hide();
		} else {
			statusbarProgress->setMaximum(maximum);
			statusbarProgress->setValue(value);
			statusbarProgress->show();
		}
	}
}

void EditorApp::selectAllNotes()
{
	if (noteGraph) noteGraph->selectAll();
}


// File menu


void EditorApp::on_actionNew_triggered()
{
	if (promptSaving()) {
		player->clear();
		song.reset(new Song);
		setupNoteGraph();
		projectFileName = "";
		opStack.clear();
		redoStack.clear();
		updateNoteInfo(NULL);
		statusbarProgress->hide();
		ui.txtTitle->clear(); ui.txtArtist->clear(); ui.txtGenre->clear(); ui.txtYear->clear();
		ui.valMusicFile->clear();
		hasUnsavedChanges = false;
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
			tr("All files") + " (*)");

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
								std::cout << "Loaded op: " << op.dump() << std::endl;
								opStack.push(op);
							}
							doOpStack();
							projectFileName = fileName;
							updateNoteInfo(NULL); // Title bar
						} else
							QMessageBox::critical(this, tr("Error opening file!"), tr("File %1 has unsupported format version.").arg(fileName));
					} else
						QMessageBox::critical(this, tr("Error opening file!"), tr("File %1 doesn't look like a project file.").arg(fileName));
				} else
					QMessageBox::critical(this, tr("Error opening file!"), tr("Couldn't open file %1 for reading.").arg(fileName));

			} else {

				// Song import
				song.reset(new Song(QString(finfo.path()+"/"), finfo.fileName()));
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
	if (hasUnsavedChanges) {
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
			<< Operation("META", "MUSICFILE", ui.valMusicFile->text());

		projectFileName = fileName;
		hasUnsavedChanges = false;
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
		} catch (const std::exception& e) {
			QMessageBox::critical(this, tr("Error exporting song!"), e.what());
		}
	}
}

void EditorApp::on_actionSingStarXML_triggered() { exportSong("XML", tr("Export SingStar XML")); }

void EditorApp::on_actionUltraStarTXT_triggered() { exportSong("TXT", tr("Export UltraStar TXT")); }

void EditorApp::on_actionFoFMIDI_triggered() { exportSong("INI", tr("Export Frets on Fire MIDI")); }

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
		int count = opStack.top().i(1);
		int start = opStack.size() - count - 1;
		for (int i = start; i < start + count; ++i) {
			redoStack.push(opStack.at(i));
		}
		opStack.remove(start, count);
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
		int count = redoStack.top().i(1);
		int start = redoStack.size() - count - 1;
		for (int i = start; i < start + count; ++i) {
			opStack.push(redoStack.at(i));
		}
		redoStack.remove(start, count);
	}
	opStack.push(redoStack.top());
	redoStack.pop();
	doOpStack();
}


void EditorApp::on_actionAntiAliasing_toggled(bool checked)
{
	QSettings settings; // Default QSettings parameters given in main()
	settings.setValue("anti-aliasing", checked);
}



// Insert menu


void EditorApp::setMusic(QString filepath)
{
	ui.valMusicFile->setText(filepath);
	// Metadata is updated when it becomes available (signal)
	player->setCurrentSource(Phonon::MediaSource(QUrl::fromLocalFile(filepath)));
	noteGraph->updateMusicPos(0, false);
	// Fire up analyzer
	noteGraph->analyzeMusic(filepath);
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

void EditorApp::on_actionLyricsFromFile_triggered()
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
		if (text != "") noteGraph->setLyrics(text);
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
	} else {
		if (!song->title.isEmpty()) ui.txtTitle->setText(song->title);
		if (!song->artist.isEmpty()) ui.txtArtist->setText(song->artist);
		if (!song->genre.isEmpty()) ui.txtGenre->setText(song->genre);
		if (!song->year.isEmpty()) ui.txtYear->setText(song->year);
	}
}

void EditorApp::metaDataChanged()
{
	if (player) {
		if (!player->metaData(Phonon::TitleMetaData).isEmpty())
			song->title = player->metaData(Phonon::TitleMetaData).join(", ");
		if (!player->metaData(Phonon::ArtistMetaData).isEmpty())
			song->artist = player->metaData(Phonon::ArtistMetaData).join(", ");
		if (!player->metaData(Phonon::GenreMetaData).isEmpty())
			song->genre = player->metaData(Phonon::GenreMetaData).join(", ");
		if (!player->metaData("DATE").isEmpty())
			song->year = player->metaData("DATE").join(", ");
		updateSongMeta(true);
	}
}

void EditorApp::playButton()
{
	if (player && player->state() == Phonon::PlayingState) {
		ui.cmdPlay->setText(tr("Pause"));
		ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-pause", QIcon(":/icons/media-playback-pause.png")));
		if (ui.chkSynth->isChecked()) {
			synth.reset(new Synth(noteGraph->noteLabels()));
			synth->start(player->currentTime());
		}
	} else {
		ui.cmdPlay->setText(tr("Play"));
		ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/media-playback-start.png")));
		if (synth) synth->stop();
		synth.reset();
	}
}

void EditorApp::on_cmdRefreshLyrics_clicked()
{
	QString text = noteGraph->dumpLyrics();
	ui.textEditLyrics->setText(text);
}

void EditorApp::on_cmdPlay_clicked()
{
	if (player) {
		if (player->currentSource().type() == Phonon::MediaSource::Empty
			|| player->currentSource().type() == Phonon::MediaSource::Invalid) {
			on_actionMusicFile_triggered();
		} else {
			if (player && player->state() == Phonon::PlayingState) player->pause();
			else player->play();
		}
	}
}

void EditorApp::audioTick(qint64 time)
{
	if (noteGraph && player)
		noteGraph->updateMusicPos(time, (player->state() == Phonon::PlayingState ? true : false));
}

void EditorApp::playerStateChanged(Phonon::State newstate, Phonon::State oldstate)
{
	(void)oldstate;
	playButton();
	if (newstate != Phonon::PlayingState) {
		noteGraph->stopMusic();
		if (newstate == Phonon::ErrorState) {
			QString errst(tr("Error playing audio!"));
			if (player) errst += " " + player->errorString();
			QMessageBox::critical(this, tr("Playback error"), errst);
		}
	}
}

void EditorApp::on_txtTitle_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtArtist_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtGenre_editingFinished() { updateSongMeta(); }
void EditorApp::on_txtYear_editingFinished() { updateSongMeta(); }

void EditorApp::on_cmbNoteType_currentIndexChanged(int index)
{
	if (noteGraph) noteGraph->setType(noteGraph->selectedNote(), index);
}

void EditorApp::on_cmdSplit_clicked()
{
	if (noteGraph) noteGraph->split(noteGraph->selectedNote());
}

void EditorApp::on_chkFloating_stateChanged(int state)
{
	bool floating = (state != 0);
	if (noteGraph) noteGraph->setFloating(noteGraph->selectedNote(), floating);
}

void EditorApp::on_chkLineBreak_stateChanged(int state)
{
	bool linebreak = (state != 0);
	if (noteGraph) noteGraph->setLineBreak(noteGraph->selectedNote(), linebreak);
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
	// Poplate License text
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
