#include <QtGui>
#include <phonon/AudioOutput>
#include <iostream>
#include "config.hh"
#include "editorapp.hh"
#include "notelabel.hh"
#include "notegraphwidget.hh"
#include "songwriter.hh"

namespace {
	static const QString PROJECT_SAVE_FILE_EXTENSION = "songproject"; // FIXME: Nice extension here
	static const quint32 PROJECT_SAVE_FILE_MAGIC = 0x50455350;
	static const quint32 PROJECT_SAVE_FILE_VERSION = 100; // File format version 1.00
	static const QDataStream::Version PROJECT_SAVE_FILE_STREAM_VERSION = QDataStream::Qt_4_7;
}

EditorApp::EditorApp(QWidget *parent): QMainWindow(parent), projectFileName(), hasUnsavedChanges()
{
	showMaximized();
	ui.setupUi(this);

	noteGraph = new NoteGraphWidget(NULL);
	ui.noteGraphScroller->setWidget(noteGraph);

	// Splitter sizes cannot be set through designer :(
	QList<int> ss; ss.push_back(700); ss.push_back(300); // Proportions, not pixels
	ui.splitter->setSizes(ss);

	// Custom signals/slots
	connect(noteGraph, SIGNAL(operationDone(const Operation&)), this, SLOT(operationDone(const Operation&)));
	connect(noteGraph, SIGNAL(updateNoteInfo(NoteLabel*)), this, SLOT(updateNoteInfo(NoteLabel*)));
	// Duplicate/reused signals/slots
	connect(ui.cmdMusicFile, SIGNAL(clicked()), this, SLOT(on_actionMusicFile_triggered()));

	// We must set the initial lyrics here, because constructor doesn't have
	// signals yet ready, which leads to empty undo stack
	noteGraph->setLyrics(tr("Please add music file and lyrics text."));
	// We want the initial text to be completely visible on screen
	// FIXME: This should be handled with more robustness and elegance
	Operation moveop("MOVE");
	moveop << (int)noteGraph->noteLabels().size()-1 << 600 - noteGraph->noteLabels().back()->width() << noteGraph->noteLabels().back()->y();
	noteGraph->doOperation(moveop);
	noteGraph->doOperation(Operation("BLOCK")); // Lock the undo stack
	noteGraph->updateNotes();
	updateNoteInfo(NULL);

	song.reset(new Song);

	// Some icons to make menus etc prettier
	ui.actionNew->setIcon(QIcon::fromTheme("document-new"));
	ui.actionOpen->setIcon(QIcon::fromTheme("document-open"));
	ui.actionSave->setIcon(QIcon::fromTheme("document-save"));
	ui.actionSaveAs->setIcon(QIcon::fromTheme("document-save-as"));
	ui.actionExit->setIcon(QIcon::fromTheme("application-exit"));
	ui.actionUndo->setIcon(QIcon::fromTheme("edit-undo"));
	ui.actionRedo->setIcon(QIcon::fromTheme("edit-redo"));
	ui.actionPreferences->setIcon(QIcon::fromTheme("preferences-other"));
	ui.actionMusicFile->setIcon(QIcon::fromTheme("insert-object"));
	ui.actionLyricsFromFile->setIcon(QIcon::fromTheme("insert-text"));
	ui.actionLyricsFromClipboard->setIcon(QIcon::fromTheme("insert-text"));
	ui.actionAbout->setIcon(QIcon::fromTheme("help-about"));
	ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-start"));
	ui.cmdStop->setIcon(QIcon::fromTheme("media-playback-stop"));

	statusbarProgress = new QProgressBar(NULL);
	ui.statusbar->addPermanentWidget(statusbarProgress);
	connect(noteGraph, SIGNAL(analyzeProgress(int, int)), this, SLOT(analyzeProgress(int, int)));

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
	// Re-apply all operations in the stack
	// FIXME: This technique cannot work quickly enough, since analyzing would also be started from scratch
	for (OperationStack::const_iterator opit = opStack.begin(); opit != opStack.end(); ++opit) {
		//std::cout << "Doing op: " << opit->dump() << std::endl;
		// FIXME: This should check from the operation what class will implement it
		// and call the appropriate object. QObject meta info could be very useful.
		try {
			noteGraph->doOperation(*opit, Operation::NO_EMIT);
		} catch (std::exception& e) { std::cout << e.what() << std::endl; }
	}
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
	QFileInfo finfo(projectFileName);
	QString proName = finfo.fileName().isEmpty() ? tr("Untitled") : finfo.fileName();
	if (hasUnsavedChanges) proName += "*";
	setWindowTitle(QString(PACKAGE) + " - " + proName);
}

void EditorApp::updateNoteInfo(NoteLabel *note)
{
	if (note) {
		MusicalScale ms;
		ui.valNoteBegin->setText(QString::number(note->note().begin) + tr(" s"));
		ui.valNoteEnd->setText(QString::number(note->note().end) + tr(" s"));
		ui.valNoteDuration->setText(QString::number(note->note().length()) + tr(" s"));
		ui.valNote->setText(QString::fromStdString(ms.getNoteStr(ms.getNoteFreq(note->note().note)))
			+ " (" + QString::number(note->note().note) + ")");
		ui.cmbNoteType->setEnabled(true);
		ui.cmbNoteType->setCurrentIndex(note->note().getTypeInt());
		ui.chkFloating->setEnabled(true);
		ui.chkFloating->setChecked(note->isFloating());
	} else {
		ui.valNoteBegin->setText("-");
		ui.valNoteEnd->setText("-");
		ui.valNoteDuration->setText("-");
		ui.valNote->setText("-");
		ui.cmbNoteType->setEnabled(false);
		ui.chkFloating->setEnabled(false);
	}
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

void EditorApp::on_actionNew_triggered()
{
	if (promptSaving()) {
		noteGraph->clearNotes();
		projectFileName = "";
	}
	updateMenuStates();
}

void EditorApp::on_actionOpen_triggered()
{
	if (!promptSaving()) return;

	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			QDir::homePath(),
			tr("All supported formats") + "(*." + PROJECT_SAVE_FILE_EXTENSION + " *.xml *.mid *.ini *.txt);;" +
			tr("Project files") +" (*." + PROJECT_SAVE_FILE_EXTENSION + ") ;;" +
			tr("SingStar XML") + " (*.xml);;" +
			tr("Frets on Fire MIDI") + " (*.mid *.ini);;" +
			tr("UltraStar TXT") + " (*.txt);;" +
			tr("All files") + " (*)");

	if (!fileName.isNull()) {
		QFileInfo finfo(fileName);
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
				song.reset(new Song(QString(finfo.path()+"/").toStdString(), finfo.fileName().toStdString()));
				noteGraph->setLyrics(song->getVocalTrack());
				updateSongMeta(true);
				// Combine the import into one undo action
				Operation combiner("COMBINER"); combiner << opStack.size();
				operationDone(combiner);
				//noteGraph->doOperation(Operation("BLOCK")); // Lock the undo stack
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
			QDir::homePath(),
			tr("Project files ") + "(*." + PROJECT_SAVE_FILE_EXTENSION + ");;" +
			tr("All files") + " (*)");
	if (!fileName.isNull()) {
		// Add the correct suffix if it is missing
		QFileInfo finfo(fileName);
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
		foreach (Operation op, opStack)
			out << op;
		projectFileName = fileName;
		hasUnsavedChanges = false;
	} else
		QMessageBox::critical(this, tr("Error saving file!"), tr("Couldn't open file %1 for saving.").arg(fileName));
	updateMenuStates();
}

void EditorApp::on_actionSingStarXML_triggered()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Export SingStar XML"), QDir::homePath());
	if (!path.isNull()) {
		try { SingStarXMLWriter(*song.data(), path); }
		catch (const std::exception& e) {
			QMessageBox::critical(this, tr("Error exporting song!"), e.what());
		}
	}
}

void EditorApp::on_actionUltraStarTXT_triggered()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Export UltraStar TXT"), QDir::homePath());
	if (!path.isNull()) {
		try { UltraStarTXTWriter(*song.data(), path); }
		catch (const std::exception& e) {
			QMessageBox::critical(this, tr("Error exporting song!"), e.what());
		}
	}
}

void EditorApp::on_actionFoFMIDI_triggered()
{
		QString path = QFileDialog::getExistingDirectory(this, tr("Export FoF MIDI"), QDir::homePath());
	if (!path.isNull()) {
		try { FoFMIDIWriter(*song.data(), path); }
		catch (const std::exception& e) {
			QMessageBox::critical(this, tr("Error exporting song!"), e.what());
		}
	}
}

void EditorApp::on_actionExit_triggered()
{
	if (promptSaving()) close();
}

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

void EditorApp::on_actionMusicFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			"",
			tr("Music files (*.mp3 *.ogg)"));

	if (!fileName.isNull()) {
		ui.valMusicFile->setText(fileName);
		// Metadata is updated when it becomes available (signal)
		player->setCurrentSource(Phonon::MediaSource(QUrl::fromLocalFile(fileName)));
		noteGraph->updateMusicPos(0, false);
		// Fire up analyzer
		noteGraph->analyzeMusic(fileName);
	}
}

void EditorApp::on_actionLyricsFromFile_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
			"",
			tr("Text files (*.txt)"));

	if (!fileName.isNull()) {
		QFile file(fileName);
		 if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			 return;

		 QTextStream in(&file);
		 QString text = "";
		 while (!in.atEnd()) {
			 text += in.readLine();
			 if (!in.atEnd()) text += " "; // TODO: Some kind of line separator here
		 }

		 if (text != "")
			 noteGraph->setLyrics(text);
	}
}

void EditorApp::on_actionLyricsFromClipboard_triggered()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();

	if (mimeData->hasText() && !mimeData->text().isEmpty()) {
		QString text = mimeData->text();
		QMessageBox::StandardButton b = QMessageBox::question(this, tr("Replace lyrics"),
			tr("Pasting lyrics from clipboard will replace the existing ones. Continue?"),
			QMessageBox::Ok | QMessageBox::Cancel);
		if (b == QMessageBox::Ok) {
			noteGraph->setLyrics(text);
		}
	} else {
		QMessageBox::warning(this, tr("No text to paste"), tr("No suitable data on the clipboard."));
	}
}

void EditorApp::on_actionWhatsThis_triggered()
{
	QWhatsThis::enterWhatsThisMode();
}

void EditorApp::on_actionAbout_triggered()
{
	QMessageBox::about(this, tr("About"),
		tr("Semi-automatic karaoke song editor.\n")
		);
}

void EditorApp::updateSongMeta(bool readFromSongToUI)
{
	if (!song) return;
	// TODO: Undo
	if (!readFromSongToUI) {
		if (ui.txtTitle->text().toStdString() != song->title) {
			song->title = ui.txtTitle->text().toStdString();
		}
		if (ui.txtArtist->text().toStdString() != song->artist) {
			song->artist = ui.txtArtist->text().toStdString();
		}
		if (ui.txtGenre->text().toStdString() != song->genre) {
			song->genre = ui.txtGenre->text().toStdString();
		}
		if (ui.txtYear->text().toStdString() != song->year) {
			song->year = ui.txtYear->text().toStdString();
		}
	} else {
		if (!song->title.empty()) ui.txtTitle->setText(QString::fromStdString(song->title));
		if (!song->artist.empty()) ui.txtArtist->setText(QString::fromStdString(song->artist));
		if (!song->genre.empty()) ui.txtGenre->setText(QString::fromStdString(song->genre));
		if (!song->year.empty()) ui.txtYear->setText(QString::fromStdString(song->year));
	}
}

void EditorApp::metaDataChanged()
{
	if (player) {
		if (!player->metaData(Phonon::TitleMetaData).isEmpty())
			song->title = player->metaData(Phonon::TitleMetaData).join(", ").toStdString();
		if (!player->metaData(Phonon::ArtistMetaData).isEmpty())
			song->artist = player->metaData(Phonon::ArtistMetaData).join(", ").toStdString();
		if (!player->metaData(Phonon::GenreMetaData).isEmpty())
			song->genre = player->metaData(Phonon::GenreMetaData).join(", ").toStdString();
		if (!player->metaData("DATE").isEmpty())
			song->year = player->metaData("DATE").join(", ").toStdString();
		updateSongMeta(true);
	}
}

void EditorApp::playButton()
{
	if (player && player->state() == Phonon::PlayingState) {
		ui.cmdPlay->setText(tr("Pause"));
		ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-pause"));
	} else {
		ui.cmdPlay->setText(tr("Play"));
		ui.cmdPlay->setIcon(QIcon::fromTheme("media-playback-start"));
	}
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

void EditorApp::on_cmdStop_clicked()
{
	if (player) player->stop();
	if (noteGraph) noteGraph->updateMusicPos(0, false);
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
	if (noteGraph->selectedNote() && noteGraph->selectedNote()->note().getTypeInt() != index) {
		noteGraph->selectedNote()->setType(index);
		Operation op("TYPE");
		op << noteGraph->getNoteLabelId(noteGraph->selectedNote()) << index;
		operationDone(op);
	}
}

void EditorApp::on_chkFloating_stateChanged(int state)
{
	bool floating = (state != 0);
	if (noteGraph->selectedNote() && noteGraph->selectedNote()->isFloating() != floating) {
		noteGraph->selectedNote()->setFloating(floating);
		Operation op("FLOATING");
		op << noteGraph->getNoteLabelId(noteGraph->selectedNote()) << (floating);
		operationDone(op);
	}
}
