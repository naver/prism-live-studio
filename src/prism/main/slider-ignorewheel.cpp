#include "slider-ignorewheel.hpp"
#include "pls-common-define.hpp"
#include <QStyle>

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setProperty(STATUS_ENTER, false);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation, QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setOrientation(orientation);
	setProperty(STATUS_ENTER, false);
}

void SliderIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}

void SliderIgnoreScroll::enterEvent(QEvent *event)
{
	QSlider::enterEvent(event);
	if (isEnabled()) {
		setProperty(STATUS_ENTER, true);
		style()->unpolish(this);
		style()->polish(this);
	}
}

void SliderIgnoreScroll::leaveEvent(QEvent *event)
{
	QSlider::enterEvent(event);
	setProperty(STATUS_ENTER, false);
	style()->unpolish(this);
	style()->polish(this);
}

void SliderIgnoreScroll::mousePressEvent(QMouseEvent *event)
{
	SetSliderValue(event->pos());
}

void SliderIgnoreScroll::mouseMoveEvent(QMouseEvent *event)
{
	SetSliderValue(event->pos());
	QSlider::mouseMoveEvent(event);
}

void SliderIgnoreScroll::mouseReleaseEvent(QMouseEvent *event)
{
	emit mouseReleaseSignal();
	QSlider::mouseReleaseEvent(event);
}

void SliderIgnoreScroll::SetSliderValue(const QPoint &position)
{
	int value = 0;
	if (this->orientation() == Qt::Horizontal) {
		double pos = position.x() / (double)width();
		if (pos < 0) {
			pos = 0;
		}
		value = round(pos * (maximum() - minimum()) + minimum());

	} else {
		double pos = position.y() / (double)height();
		if (pos < 0) {
			pos = 0;
		}
		value = (1 - pos) * (maximum() - minimum()) + minimum();
	}

	if (value > maximum()) {
		value = maximum();
	}
	if (value < minimum()) {
		value = minimum();
	}
	setValue(value);
}
