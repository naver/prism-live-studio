#pragma once

#include "qt-display.hpp"

class PLSVirtualBackgroundDisplay : public PLSQTDisplay {
	Q_OBJECT

public:
	explicit PLSVirtualBackgroundDisplay(QWidget *parent = nullptr, Qt::WindowFlags flags = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSVirtualBackgroundDisplay();

signals:
	void beginVBkgDrag();
	void dragVBkgMoving(int cx, int cy);
	void mousePressed();
	void mouseReleased();

protected:
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *event) override;

private:
	bool mousePress{false};
	QPoint mousePressPos;
};
