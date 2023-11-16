#ifndef PLSMEDIASLIDER_H
#define PLSMEDIASLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include "double-slider.hpp"

class PLSMediaSlider : public SliderIgnoreScroll {
	Q_OBJECT

public:
	explicit inline PLSMediaSlider(QWidget *parent = nullptr) : SliderIgnoreScroll(parent) { setMouseTracking(true); };

	void setMediaSliderEnabled(bool isDisabled);
	bool isMediaSliderEnabled() const;
signals:
	void mediaSliderHovered(int value);
	void mediaSliderReleased(int value);
	void mediaSliderClicked(int value);
	void mediaSliderMoved(int value);
	void mediaSliderFocusOut();

protected:
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *event);
#else
	void enterEvent(QEvent *event);
#endif
	void leaveEvent(QEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;

private:
	bool mousePress = false;
	int prePos = 0;
	bool enabled = false;
	bool mouseEnter = false;
};

#endif // PLSMEDIASLIDER_H
