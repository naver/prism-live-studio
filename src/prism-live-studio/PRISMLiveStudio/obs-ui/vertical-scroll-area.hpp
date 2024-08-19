#pragma once

#include <QScrollArea>
#include "PLSCommonScrollBar.h"
#ifdef __APPLE__
#include "PLSCustomMacWindow.h"
#endif

class QResizeEvent;

class VScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline VScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		this->setVerticalScrollBar(new PLSCommonScrollBar());
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#ifdef __APPLE__
		PLSCustomMacWindow::clipsToBounds(this);
#endif
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
