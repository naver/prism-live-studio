#pragma once

#include <QScrollArea>
#include "libui.h"

class QResizeEvent;

class HScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline HScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		pls_scroll_area_clips_to_bounds(this);
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
