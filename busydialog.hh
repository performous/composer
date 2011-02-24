#pragma once
#include <QApplication>
#include <QDialog>
#include <QCloseEvent>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QElapsedTimer>

class BusyDialog: public QDialog {
public:
	BusyDialog(QWidget *parent = NULL, int eventsInterval = 10): QDialog(parent), timer(), interval(eventsInterval), count() {
		QProgressBar *progress = new QProgressBar(this);
		progress->setRange(0,0);
		setWindowTitle(tr("Working..."));
		QVBoxLayout *vb = new QVBoxLayout(this);
		vb->addWidget(progress);
		setLayout(vb);
		timer.start();
	}
	void operator()() {
		// Only show the dialog after certainamount of time
		if (isHidden() && timer.elapsed() > 3000) open();
		if (isVisible()) {
			if (count == 0) // Let's not process events all the time
				QApplication::processEvents();
			count = (count + 1) % interval;
		}
	}
protected:
	void closeEvent(QCloseEvent* event) { event->ignore(); }
private:
	QElapsedTimer timer;
	int interval;
	int count;
};
