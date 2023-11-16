#pragma once

#include "obs.hpp"
#include <QSlider>
#include <QInputEvent>
#include <QEvent>
#include <QtCore/QObject>
#include <QAccessibleWidget>

class SliderIgnoreScroll : public QSlider {
	Q_OBJECT

public:
	SliderIgnoreScroll(QWidget *parent = nullptr);
	SliderIgnoreScroll(Qt::Orientation orientation,
			   QWidget *parent = nullptr);

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

class VolumeSlider : public SliderIgnoreScroll {
	Q_OBJECT

public:
	obs_fader_t *fad;

	VolumeSlider(obs_fader_t *fader, QWidget *parent = nullptr);
	VolumeSlider(obs_fader_t *fader, Qt::Orientation orientation,
		     QWidget *parent = nullptr);
};

class VolumeAccessibleInterface : public QAccessibleWidget {

public:
	VolumeAccessibleInterface(QWidget *w);

	QVariant currentValue() const;
	void setCurrentValue(const QVariant &value);

	QVariant maximumValue() const;
	QVariant minimumValue() const;

	QVariant minimumStepSize() const;

private:
	VolumeSlider *slider() const;

protected:
	virtual QAccessible::Role role() const override;
	virtual QString text(QAccessible::Text t) const override;
};
