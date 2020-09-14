#pragma once

#include <QScrollBar>
#include <QImage>
#include <functional>

class ScrollBar : public QScrollBar
{
public:
	using PaintFunction = std::function<void(QImage&)>;
	
	ScrollBar(Qt::Orientation orientation, QWidget * parent = nullptr);
	ScrollBar(PaintFunction const&, Qt::Orientation orientation, QWidget * parent = nullptr);
	
	void update();
	
protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent * event) override;
	
private:
	void paint();
	
private:
	PaintFunction m_paintFunction;
	QImage m_background;
};
