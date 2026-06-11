#pragma once

#include <QLabel>
#include <QMouseEvent>

class ClickableLabel : public QLabel {
	Q_OBJECT

public:
	inline ClickableLabel(QWidget *parent = 0) : QLabel(parent) {}

signals:
	void clicked();
	void rightclicked();

protected:
	void mousePressEvent(QMouseEvent *event)
	{
		if (event->button() == Qt::RightButton) {
			emit rightclicked();
		}
		emit clicked();
		event->accept();
	}
};
