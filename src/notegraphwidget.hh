#pragma once

#include "pitchvis.hh"
#include "notes.hh"
#include "operation.hh"
#include <QLabel>
#include <QList>
#include <QScopedPointer>
#include <QElapsedTimer>

class QScrollArea;
class NoteLabel;
typedef QList<NoteLabel*> NoteLabels;


class SeekHandle: public QLabel
{
	Q_OBJECT
public:
	SeekHandle(QWidget *parent = 0);
	int curx() const { return x() + width() / 2; }
	bool wrapToViewport;
protected:
	void mouseMoveEvent(QMouseEvent *event);
	void moveEvent(QMoveEvent *event);
};



class NoteLabelManager: public QLabel
{
	Q_OBJECT
public:
	static const QString MimeType;

	NoteLabelManager(QWidget *parent = 0);

	virtual void updateNotes(bool leftToRight = true) {}
	virtual void startNotePixmapUpdates() {}
	virtual void forcedNotePixmapUpdate() {}

	void reset();
	void clearNotes();
	void selectNote(NoteLabel *note, bool clearPrevious = true);
	void selectAll();
	void selectAllAfter();
	void shiftSelect(NoteLabel *note);
	void boxSelect(QPoint p1, QPoint p2);
	NoteLabel* selectedNote() const { return m_selectedNotes.isEmpty() ? NULL : m_selectedNotes.front(); }
	NoteLabels& selectedNotes() { return m_selectedNotes; }
	NoteLabels const& selectedNotes() const { return m_selectedNotes; }

	int getNoteLabelId(NoteLabel* note) const;
	int findIdForTime(double time) const;
	NoteLabels& noteLabels() { return m_notes; }

	void createNote(double time);
	void split(NoteLabel *note, float ratio = 0.5f);
	void del(NoteLabel *note);
	void move(NoteLabel *note, int value);
	void editLyric(NoteLabel *note);
	void setFloating(NoteLabel *note, bool state);
	void setLineBreak(NoteLabel *note, bool state);
	void setType(NoteLabel *note, int newtype);

	void doOperation(const Operation& op, int flags = Operation::NORMAL);

	virtual void zoom(float steps, double focalSecs = -1);
	int getZoomLevel() const;

	int s2px(double sec) const;
	double px2s(int px) const;
	int n2px(double note) const;
	double px2n(int px) const;

	double getSongLengthInSeconds() const;
	
signals:
	void updateNoteInfo(NoteLabel*);
	void operationDone(const Operation&);
	void statusBarMessage(QString);

public slots:
	void selectNextSyllable(bool backwards = false, bool addToSelection = false);
	void selectNextSentenceStart();
	void cut();
	void copy();
	void paste();

protected:
	QScrollArea* getScrollArea() const;
	void calcViewport(int &x1, int &y1, int &x2, int &y2) const;

	// Zoom settings
	static const double zoomStep;  ///< Mouse wheel steps * zoomStep => double/half zoom factor
	static const int zoomMin = -12;  ///< Number of steps to minimum zoom
	static const int zoomMax = 6;  ///< Number of steps to maximum zoom
	static const double ppsNormal;  ///< Pixels per second with default zoom
	double m_pixelsPerSecond;

	NoteLabels m_notes;
	NoteLabels m_selectedNotes;
	enum NoteAction { NONE, RESIZE, MOVE } m_selectedAction;
	int m_noteHalfHeight;
	double m_songLengthInSeconds;
};


const int MaxPitchVis = 2;

class NoteGraphWidget: public NoteLabelManager
{
	Q_OBJECT

public:
	static const QString BGColor;
	static const int Height;

	NoteGraphWidget(QWidget *parent = 0);

	void setLyrics(QString lyrics);
	void setLyrics(const VocalTrack &track);
	void analyzeMusic(QString filepath, int visId = 0);

	void updateNotes(bool leftToRight = true);
	void updateMusicPos(qint64 time, bool smoothing = true);
	void stopMusic();
	void seek(int x);
	void zoom(float steps, double focalSecs = -1);

	VocalTrack getVocalTrack() const;
	QString getCurrentSentence() const;
	QString getPrevSentence() const;
	QString dumpLyrics() const;

public slots:
	void showContextMenu(const QPoint &pos);
	void timeSyllable();
	void timeSentence();
	void setSeekHandleWrapToViewport(bool state) { m_seekHandle.wrapToViewport = state; }
	void updatePixmap(const QImage &image, const QPoint &position, int visId);
	void updatePitch();
	void abortPitch() { for (int i = 0; i < MaxPitchVis; ++i) if (m_pitch[i]) m_pitch[i]->cancel(); }
	void scrollToFirstNote();
	void startNotePixmapUpdates(); ///< Starts creating pixmaps for NoteLabels
	void forcedNotePixmapUpdate();
	void playbackRateChanged(qreal rate);

signals:
	void analyzeProgress(int, int);
	void seeked(qint64 time);
	void updatedNotes();
	
protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent * event);
	void keyPressEvent(QKeyEvent *event);
	void timerEvent(QTimerEvent *event);
	void paintEvent(QPaintEvent*);
	void resizeEvent(QResizeEvent *) { updatePitch(); }
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);

private:
	void finalizeNewLyrics();
	void timeCurrent();

	QPoint m_mouseHotSpot;
	bool m_seeking;
	bool m_actionHappened;
	QScopedPointer<PitchVis> m_pitch[MaxPitchVis];
	SeekHandle m_seekHandle;
	int m_nextNotePixmap;
	int m_notePixmapTimer;
	int m_analyzeTimer;
	int m_playbackTimer;
	QElapsedTimer m_playbackInterval;
	qint64 m_playbackPos;
	qreal m_playbackRate;
	QPixmap m_pixmap[MaxPitchVis];
	QPoint m_pixmapPos[MaxPitchVis];
};


struct FloatingGap
{
	FloatingGap(double startTime): begin(startTime), end(startTime), m_notesLength() {}

	void addNote(NoteLabel* n);
	bool isEmpty() const { return notes.empty(); }
	double length() const { return std::abs(end - begin); }
	double minLength() const;
	double notesLength() const { return m_notesLength; }

	double begin;
	double end;

	NoteLabels notes;

private:
	double m_notesLength;
};
