#include "PLSCommonScrollBar.h"

PLSCommonScrollBar::PLSCommonScrollBar(QWidget *parent) : QScrollBar(parent) {}

void PLSCommonScrollBar::showEvent(QShowEvent *event)
{
	QScrollBar::showEvent(event);
	emit isShowScrollBar(true);
}

void PLSCommonScrollBar::hideEvent(QHideEvent *event)
{
	QScrollBar::hideEvent(event);
	emit isShowScrollBar(false);
}
