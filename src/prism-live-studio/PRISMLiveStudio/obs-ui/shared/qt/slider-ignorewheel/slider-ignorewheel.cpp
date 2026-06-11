#include "moc_slider-ignorewheel.cpp"
#include <QStyle>

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setProperty("enter", false);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation, QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setOrientation(orientation);
	setProperty("enter", false);
}

void SliderIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}

void SliderIgnoreClick::mousePressEvent(QMouseEvent *event)
{
	QStyleOptionSlider styleOption;
	initStyleOption(&styleOption);
	QRect handle = style()->subControlRect(QStyle::CC_Slider, &styleOption, QStyle::SC_SliderHandle, this);
	if (handle.contains(event->position().toPoint())) {
		SliderIgnoreScroll::mousePressEvent(event);
		dragging = true;
	} else {
		event->accept();
	}
}

void SliderIgnoreClick::mouseReleaseEvent(QMouseEvent *event)
{
	dragging = false;
	SliderIgnoreScroll::mouseReleaseEvent(event);
}

void SliderIgnoreClick::mouseMoveEvent(QMouseEvent *event)
{
	if (dragging) {
		SliderIgnoreScroll::mouseMoveEvent(event);
	} else {
		event->accept();
	}
}

void SliderIgnoreScroll::enterEvent(QEnterEvent *event)
{
	QSlider::enterEvent(event);
	if (isEnabled()) {
		setProperty("enter", true);
		style()->unpolish(this);
		style()->polish(this);
	}
}

void SliderIgnoreScroll::leaveEvent(QEvent *event)
{
	QSlider::leaveEvent(event);
	if (isEnabled()) {
		setProperty("enter", false);
		style()->unpolish(this);
		style()->polish(this);
	}
}

void SliderIgnoreScroll::mousePressEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		mousePress = true;
	}
	SetSliderValue(event->pos());
	QSlider::mousePressEvent(event);
}

void SliderIgnoreScroll::mouseMoveEvent(QMouseEvent *event)
{
	if (!mousePress) {
		return;
	}
	SetSliderValue(event->pos());
	QSlider::mouseMoveEvent(event);
}

void SliderIgnoreScroll::mouseReleaseEvent(QMouseEvent *event)
{
	mousePress = false;
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
		value = (int)round(pos * (double)(maximum() - minimum()) + minimum());

	} else {
		double pos = position.y() / (double)height();
		if (pos < 0) {
			pos = 0;
		}
		value = static_cast<int>((1 - pos) * (maximum() - minimum()) + minimum());
	}

	if (value > maximum()) {
		value = maximum();
	}
	if (value < minimum()) {
		value = minimum();
	}
	setValue(value);
}
