#include "PLSVirtualBackgroundDisplay.h"

#include "log.h"

PLSVirtualBackgroundDisplay::PLSVirtualBackgroundDisplay(QWidget *parent, Qt::WindowFlags flags, PLSDpiHelper dpiHelper) : PLSQTDisplay(parent, flags, dpiHelper) {}

PLSVirtualBackgroundDisplay::~PLSVirtualBackgroundDisplay() {}

void PLSVirtualBackgroundDisplay::mousePressEvent(QMouseEvent *event)
{
	PLSQTDisplay::mousePressEvent(event);

	if (event->button() != Qt::LeftButton) {
		return;
	}

	mousePress = true;
	mousePressPos = event->globalPos();
	emit beginVBkgDrag();
	emit mousePressed();
}

void PLSVirtualBackgroundDisplay::mouseReleaseEvent(QMouseEvent *event)
{
	PLSQTDisplay::mouseReleaseEvent(event);

	if (mousePress) {
		mousePress = false;
		QPoint globalPos = event->globalPos();
		emit dragVBkgMoving(globalPos.x() - mousePressPos.x(), globalPos.y() - mousePressPos.y());
		emit mouseReleased();
	}
}

void PLSVirtualBackgroundDisplay::mouseMoveEvent(QMouseEvent *event)
{
	PLSQTDisplay::mouseMoveEvent(event);

	if (mousePress) {
		QPoint globalPos = event->globalPos();
		emit dragVBkgMoving(globalPos.x() - mousePressPos.x(), globalPos.y() - mousePressPos.y());
	}
}
