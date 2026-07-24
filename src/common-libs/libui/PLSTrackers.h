#ifndef PLSTRACKERS_H
#define PLSTRACKERS_H

#include "libui-globals.h"
#include <chrono>
#include <qset.h>
#include <qwidget.h>

// only for windows system
class LIBUI_API PLSResizeTracker : public QObject {
	Q_OBJECT

public:
	explicit PLSResizeTracker(QObject *parent = nullptr);
	~PLSResizeTracker() override = default;

public:
	void addWidget(QWidget *widget);
	void removeWidget(QWidget *widget);

	void enableTracking();
	void disableTracking(bool autoEnabled = false);

	void checkEndResize();

signals:
	void beginResize();
	void endResize();

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QSet<QWidget *> m_widgets;
	int m_resizeEndCheckTimerId = 0;
	int m_autoTrackingEnabledTimerId = 0;
	bool m_resizing = false;
	bool m_trackingEnabled = true;
};

#endif // PLSTRACKERS_H
