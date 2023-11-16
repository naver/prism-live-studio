#include "PLSMediaSlider.h"
#include "pls-common-define.hpp"
#include "libui.h"
#include <QStyle>
using namespace common;

void PLSMediaSlider::mouseMoveEvent(QMouseEvent *event)
{
	if (!(event->buttons() & Qt::LeftButton) && mousePress) {
		mousePress = false;
	}

	int val = minimum() + ((maximum() - minimum()) * event->x()) / width();

	if (val > maximum())
		val = maximum();
	else if (val < minimum())
		val = minimum();

	if (mousePress && prePos != val) {
		emit mediaSliderMoved(val);
		prePos = val;
	} else
		emit mediaSliderHovered(val);

	event->accept();
	QSlider::mouseMoveEvent(event);
}

void PLSMediaSlider::mousePressEvent(QMouseEvent *event)
{
	if (!isMediaSliderEnabled())
		return;

	if (event->button() == Qt::LeftButton) {
		int val = minimum() + ((maximum() - minimum()) * event->x()) / width();

		if (val > maximum())
			val = maximum();
		else if (val < minimum())
			val = minimum();

		prePos = val;
		emit mediaSliderClicked(val);
		event->accept();
	}
	mousePress = true;
}

void PLSMediaSlider::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		int val = minimum() + ((maximum() - minimum()) * event->x()) / width();

		if (val > maximum())
			val = maximum();
		else if (val < minimum())
			val = minimum();

		emit mediaSliderReleased(val);
		event->accept();
	}

	mousePress = false;
	QSlider::mouseReleaseEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PLSMediaSlider::enterEvent(QEnterEvent *event)
#else
void PLSMediaSlider::enterEvent(QEvent *event)
#endif
{
	QSlider::enterEvent(event);
	if (isEnabled()) {
		setProperty(STATUS_ENTER, true);
		style()->unpolish(this);
		style()->polish(this);
	}
	mouseEnter = true;
}

void PLSMediaSlider::leaveEvent(QEvent *event)
{
	QSlider::leaveEvent(event);
	setProperty(STATUS_ENTER, false);
	style()->unpolish(this);
	style()->polish(this);
	mouseEnter = false;
}

void PLSMediaSlider::wheelEvent(QWheelEvent *event)
{
	QWidget::wheelEvent(event);
}

void PLSMediaSlider::keyPressEvent(QKeyEvent *event)
{
	if (hasFocus())
		event->ignore();
	else
		QSlider::keyPressEvent(event);
}

void PLSMediaSlider::setMediaSliderEnabled(bool isEnabled)
{
	enabled = isEnabled;
	if (mouseEnter && isEnabled) {
		setProperty(STATUS_ENTER, true);
		style()->unpolish(this);
		style()->polish(this);
	}
}

bool PLSMediaSlider::isMediaSliderEnabled() const
{
	return enabled;
}

void PLSMediaSlider::focusOutEvent(QFocusEvent *event)
{
	emit mediaSliderFocusOut();
	QSlider::focusOutEvent(event);
}
