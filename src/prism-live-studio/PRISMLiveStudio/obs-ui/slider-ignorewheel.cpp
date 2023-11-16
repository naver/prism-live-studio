#include "slider-ignorewheel.hpp"
#include <QStyle>

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setProperty("enter", false);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation,
				       QWidget *parent)
	: QSlider(parent)
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
		value = (int)round(pos * (double)(maximum() - minimum()) +
				   minimum());

	} else {
		double pos = position.y() / (double)height();
		if (pos < 0) {
			pos = 0;
		}
		value = static_cast<int>((1 - pos) * (maximum() - minimum()) +
					 minimum());
	}

	if (value > maximum()) {
		value = maximum();
	}
	if (value < minimum()) {
		value = minimum();
	}
	setValue(value);
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, QWidget *parent)
	: SliderIgnoreScroll(parent)
{
	fad = fader;
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, Qt::Orientation orientation,
			   QWidget *parent)
	: SliderIgnoreScroll(orientation, parent)
{
	fad = fader;
}

VolumeAccessibleInterface::VolumeAccessibleInterface(QWidget *w)
	: QAccessibleWidget(w)
{
}

VolumeSlider *VolumeAccessibleInterface::slider() const
{
	return qobject_cast<VolumeSlider *>(object());
}

QString VolumeAccessibleInterface::text(QAccessible::Text t) const
{
	if (slider()->isVisible()) {
		switch (t) {
		case QAccessible::Text::Value:
			return currentValue().toString();
		default:
			break;
		}
	}
	return QAccessibleWidget::text(t);
}

QVariant VolumeAccessibleInterface::currentValue() const
{
	QString text;
	float db = obs_fader_get_db(slider()->fad);

	if (db < -96.0f)
		text = "-inf dB";
	else
		text = QString::number(db, 'f', 1).append(" dB");

	return text;
}

void VolumeAccessibleInterface::setCurrentValue(const QVariant &value)
{
	slider()->setValue(value.toInt());
}

QVariant VolumeAccessibleInterface::maximumValue() const
{
	return slider()->maximum();
}

QVariant VolumeAccessibleInterface::minimumValue() const
{
	return slider()->minimum();
}

QVariant VolumeAccessibleInterface::minimumStepSize() const
{
	return slider()->singleStep();
}

QAccessible::Role VolumeAccessibleInterface::role() const
{
	return QAccessible::Role::Slider;
}
