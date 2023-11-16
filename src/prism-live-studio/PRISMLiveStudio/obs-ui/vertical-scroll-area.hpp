#pragma once

#include <QScrollArea>
#include "PLSCommonScrollBar.h"

class QResizeEvent;

class VScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline VScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		this->setVerticalScrollBar(new PLSCommonScrollBar());
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
