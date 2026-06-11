#ifndef PLSWATCHERS_H
#define PLSWATCHERS_H

#include "libui-globals.h"
#include <qwidget.h>

class LIBUI_API PLSShowWatcher : public QObject {
	Q_OBJECT

public:
	explicit PLSShowWatcher(QWidget *watched);
	~PLSShowWatcher() override = default;

signals:
	void signalShow(QWidget *wathced);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
};

class LIBUI_API PLSHideWatcher : public QObject {
	Q_OBJECT

public:
	explicit PLSHideWatcher(QWidget *watched);
	~PLSHideWatcher() override = default;

signals:
	void signalHide(QWidget *wathced);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // PLSWATCHERS_H
