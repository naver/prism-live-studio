#pragma once

#include <QMouseEvent>
#include "slider-ignorewheel.hpp"

class AbsoluteSlider : public SliderIgnoreScroll {
	Q_OBJECT

public:
	AbsoluteSlider(QWidget *parent = nullptr);
	AbsoluteSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
	void absoluteSliderHovered(int value);
	void doubleClicked();

protected:
	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;
	virtual bool eventFilter(QObject *obj, QEvent *event) override;

	int posToRangeValue(QMouseEvent *event);

	void mouseDoubleClickEvent(QMouseEvent *event)
	{
		emit doubleClicked();
		event->accept();
	}


private:
	bool dragging = false;
};
