#pragma once

#include <QScrollArea>

class QResizeEvent;

class HScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline HScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
