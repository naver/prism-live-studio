#include "PLSTrackers.h"
#include <qevent.h>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

PLSResizeTracker::PLSResizeTracker(QObject *parent) : QObject(parent) {}

void PLSResizeTracker::addWidget(QWidget *widget)
{
#if defined(Q_OS_WIN)
	if (!m_widgets.contains(widget)) {
		m_widgets.insert(widget);
		widget->installEventFilter(this);
	}
#endif
}
void PLSResizeTracker::removeWidget(QWidget *widget)
{
#if defined(Q_OS_WIN)
	if (m_widgets.contains(widget)) {
		m_widgets.remove(widget);
		widget->removeEventFilter(this);
	}
#endif
}

#if defined(Q_OS_WIN)
static bool isLeftButtonDown()
{
	return GetAsyncKeyState(VK_LBUTTON) < 0;
}
static void endTimer(PLSResizeTracker *tracker, int &timerId)
{
	if (timerId > 0) {
		tracker->killTimer(timerId);
		timerId = 0;
	}
}
static void beginTimer(PLSResizeTracker *tracker, int &timerId, int interval)
{
	endTimer(tracker, timerId);
	timerId = tracker->startTimer(interval);
}
static void checkResize(PLSResizeTracker *tracker, bool &resizing, int &resizeEndCheckTimer)
{
	if (resizing) {
		beginTimer(tracker, resizeEndCheckTimer, 500);
	} else if (isLeftButtonDown()) {
		resizing = true;
		beginTimer(tracker, resizeEndCheckTimer, 500);
		tracker->beginResize();
	}
}
#endif

void PLSResizeTracker::enableTracking()
{
	m_trackingEnabled = true;
}
void PLSResizeTracker::disableTracking(bool autoEnabled)
{
	m_trackingEnabled = false;
#if defined(Q_OS_WIN)
	if (autoEnabled) {
		beginTimer(this, m_autoTrackingEnabledTimerId, 400);
	} else {
		endTimer(this, m_autoTrackingEnabledTimerId);
	}
#endif
}

void PLSResizeTracker::checkEndResize()
{
#if defined(Q_OS_WIN)
	if (m_resizing && !isLeftButtonDown()) {
		endTimer(this, m_resizeEndCheckTimerId);
		m_resizing = false;
		endResize();
	}
#endif
}

bool PLSResizeTracker::event(QEvent *event)
{
	if (event->type() != QEvent::Timer)
		return QObject::event(event);

#if defined(Q_OS_WIN)
	using namespace std::literals::chrono_literals;
	if (auto timerId = static_cast<QTimerEvent *>(event)->timerId(); timerId == m_resizeEndCheckTimerId) {
		endTimer(this, m_resizeEndCheckTimerId);
		if (m_resizing) {
			m_resizing = false;
			endResize();
		}
	} else if (timerId == m_autoTrackingEnabledTimerId) {
		endTimer(this, m_autoTrackingEnabledTimerId);
		if (!m_trackingEnabled) {
			enableTracking();
		}
	}
#endif

	return QObject::event(event);
}

bool PLSResizeTracker::eventFilter(QObject *watched, QEvent *event)
{
#if defined(Q_OS_WIN)
	if (m_trackingEnabled && watched->isWidgetType() && event->type() == QEvent::Resize && m_widgets.contains(static_cast<QWidget *>(watched)))
		checkResize(this, m_resizing, m_resizeEndCheckTimerId);
#endif

	return QObject::eventFilter(watched, event);
}
