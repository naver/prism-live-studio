#pragma once

#include <QWidget>
#include <QLayout>
#include <QScrollArea>
#include <QBasicTimer>
#include <QPointer>
#include "volume-control.hpp"

enum class Direction { Left, Right, Top, Bottom, Unknown };

class PLSMixerContent : public QWidget {
	Q_OBJECT
public:
	explicit PLSMixerContent(Qt::Orientation _orientation, QWidget *parent = nullptr);
	~PLSMixerContent() = default;

	void AddWidget(QWidget *widget);
	void DisplayItemBorder(VolControl *current, const char *borderType);
	void ClearItemBorder();

protected:
	Qt::Orientation orientation;
	QBoxLayout *main_layout = nullptr;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void timerEvent(QTimerEvent *event) override;
	void changeEvent(QEvent *event) override;

signals:
	void mixerReorderd();

private:
	void SetLinePos(const int &startX, const int &startY, const int &endX, const int &endY);
	void OnDragMoveEvent(QDragMoveEvent *event);
	void AutoScroll(QDragMoveEvent *event);

	QPoint lineStart;
	QPoint lineEnd;
	bool isDrag{false};
	int autoScrollStepValue = 0;
	int destIndex = 0;
	QBasicTimer delayedAutoScroll;
	QPointer<QScrollArea> scrollarea;
	QPoint startDragPoint;
	QPointer<VolControl> lastClickedVol;
	QPointer<VolControl> lastDisplayedVol;
};

class HMixerContent : public PLSMixerContent {
	Q_OBJECT
public:
	explicit HMixerContent(QWidget *parent = nullptr) : PLSMixerContent(Qt::Horizontal, parent) {}
	~HMixerContent() = default;
};

class VMixerContent : public PLSMixerContent {
	Q_OBJECT
public:
	explicit VMixerContent(QWidget *parent = nullptr) : PLSMixerContent(Qt::Vertical, parent) {}
	~VMixerContent() = default;
};