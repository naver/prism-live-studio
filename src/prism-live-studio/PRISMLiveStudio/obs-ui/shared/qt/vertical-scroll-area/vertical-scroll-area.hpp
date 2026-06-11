#pragma once

#include <QScrollArea>
#include "PLSCommonScrollBar.h"
#include "libui.h"

class QResizeEvent;

class VScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline VScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		this->setVerticalScrollBar(new PLSCommonScrollBar());
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		pls_scroll_area_clips_to_bounds(this);
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
