#pragma once
#include <QApplication>
#include <QDialog>
#include <QCloseEvent>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QElapsedTimer>

class BusyDialog: public QDialog {
public:
	BusyDialog(QWidget *parent = NULL, int eventsInterval = 30): QDialog(parent), timer(), interval(eventsInterval), count() {
		QProgressBar *progress = new QProgressBar(this);
		progress->setRange(0,0);
		setWindowTitle(tr("Working..."));
		QVBoxLayout *vb = new QVBoxLayout(this);
		vb->addWidget(progress);
		setLayout(vb);
		timer.start();
	}
	void operator()() {
		if (count == 0) // Let's not process events all the time
			QApplication::processEvents();
		count = (count + 1) % interval;
		// Only show the dialog after certainamount of time
		if (isHidden() && timer.elapsed() > 1500) open();
	}
protected:
	void closeEvent(QCloseEvent* event) { event->ignore(); }
private:
	QElapsedTimer timer;
	int interval;
	int count;
};
