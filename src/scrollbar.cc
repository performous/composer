#include "scrollbar.hh"
#include <QPainter>
#include <QResizeEvent>

ScrollBar::ScrollBar(Qt::Orientation orientation, QWidget * parent)
: QScrollBar(orientation, parent) {
	m_paintFunction = [](QImage&){};
}

ScrollBar::ScrollBar(PaintFunction const& f, Qt::Orientation orientation, QWidget * parent)
: QScrollBar(orientation, parent) {
	m_paintFunction = f;
}

void ScrollBar::update() {
	paint();
	QScrollBar::update();
}

void ScrollBar::paint() {
	m_paintFunction(m_background);
}

void ScrollBar::resizeEvent(QResizeEvent * event) {
	QScrollBar::resizeEvent(event);
	
	auto image = QImage(event->size(), QImage::Format_ARGB32);
	
	image.swap(m_background);
	
	paint();
}

void ScrollBar::paintEvent(QPaintEvent* event) {
	//QScrollBar::paintEvent(event);

	const auto range = std::max(1, maximum() - minimum());
	
	QPainter painter(this);

	painter.drawImage(0, 0, m_background);
	painter.setPen(QPen(Qt::gray,1));

	if(orientation() == Qt::Horizontal) {
		const auto viewWidth = width();
		const auto w = viewWidth * viewWidth / range;
		const auto x = value() * (width() - w) / range;
		
		painter.drawRect(x, y(), w - 1, height() - 1);
	} else {
		const auto viewHeight = height();
		const auto h = viewHeight * viewHeight / range;
		const auto y = value() * (height() - h) / range;
		
		painter.drawRect(x(), y, width() - 1, h - 1);
	}
}
