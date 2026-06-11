#ifndef PLSWIDGETCLOSEHOOK_H
#define PLSWIDGETCLOSEHOOK_H

#include <functional>
#include <utility>

#include <qwidget.h>
#include <qevent.h>

#include "libui-globals.h"

class LIBUI_API PLSWidgetCloseHook {
public:
	using CanCloseChecker = std::function<bool()>;
	using CloseListener = std::function<void()>;

protected:
	PLSWidgetCloseHook() = default;
	virtual ~PLSWidgetCloseHook() = default;

public:
	void addCanCloseChecker(const CanCloseChecker &canCloseChecker);
	bool canCloseCheck() const;

	void addCloseListener(const CloseListener &closeListener);
	void notifyClose() const;

public:
	virtual QWidget *self() = 0;

private:
	QList<CanCloseChecker> m_canCloseCheckers;
	QList<CloseListener> m_closeListeners;
};

template<typename QtType> class PLSWidgetCloseHookQt : public QtType, public PLSWidgetCloseHook {
	Q_DISABLE_COPY(PLSWidgetCloseHookQt)

protected:
	template<typename... Args> explicit PLSWidgetCloseHookQt(Args &&...args) : QtType(std::forward<Args>(args)...) {}
	virtual ~PLSWidgetCloseHookQt() override = default;

public:
	QWidget *self() override { return this; }

protected:
	void closeEvent(QCloseEvent *event) override
	{
		if (!event->isAccepted())
			return;

		if (!PLSWidgetCloseHook::canCloseCheck()) {
			event->ignore();
			return;
		}

		PLSWidgetCloseHook::notifyClose();
		QtType::closeEvent(event);
	}
};

#endif // !PLSWIDGETCLOSEHOOK_H
