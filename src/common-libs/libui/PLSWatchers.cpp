#include "PLSWatchers.h"

#include <qcoreevent.h>

PLSShowWatcher::PLSShowWatcher(QWidget *watched) : QObject(watched)
{
	watched->installEventFilter(this);
}

bool PLSShowWatcher::eventFilter(QObject *watched, QEvent *event)
{
	if (watched->isWidgetType() && event->type() == QEvent::Show) {
		emit signalShow(static_cast<QWidget *>(watched));
	}
	return false;
}

PLSHideWatcher::PLSHideWatcher(QWidget *watched) : QObject(watched)
{
	watched->installEventFilter(this);
}

bool PLSHideWatcher::eventFilter(QObject *watched, QEvent *event)
{
	if (watched->isWidgetType() && event->type() == QEvent::Hide) {
		emit signalHide(static_cast<QWidget *>(watched));
	}
	return false;
}
