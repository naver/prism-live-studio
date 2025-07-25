#pragma once

#include <QSlider>
#include <QInputEvent>
#include <QEvent>
#include <QtCore/QObject>
#include <QStyleOptionSlider>

class SliderIgnoreScroll : public QSlider {
	Q_OBJECT

public:
	SliderIgnoreScroll(QWidget *parent = nullptr);
	SliderIgnoreScroll(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
	virtual void wheelEvent(QWheelEvent *event) override;
	virtual void enterEvent(QEnterEvent *event) override;
	virtual void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	void SetSliderValue(const QPoint &position);
signals:
	void mouseReleaseSignal();

private:
	bool mousePress = false;
};

class SliderIgnoreClick : public SliderIgnoreScroll {
public:
	inline SliderIgnoreClick(Qt::Orientation orientation, QWidget *parent = nullptr)
		: SliderIgnoreScroll(orientation, parent)
	{
	}

protected:
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *event) override;

private:
	bool dragging = false;
};
