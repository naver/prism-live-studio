#ifndef PLSCUSTOMTITLEBAR_WINDOWS_H
#define PLSCUSTOMTITLEBAR_WINDOWS_H

#include <functional>
#include <utility>

#include <qwidget.h>
#include <libutils-api.h>

#include "PLSWidgetCloseHook.h"
#include "PLSTransparentForMouseEvents.h"

#if defined(Q_OS_MACOS)
#include "PLSCustomMacWindow.h"
#endif

class PLSToplevelWidget;

namespace pls {
namespace ui {

LIBUI_API void toplevelView_event(PLSToplevelWidget *widget, QEvent *event);
LIBUI_API bool toplevelView_nativeEvent(PLSToplevelWidget *widget, const QByteArray &eventType, void *message, qintptr *result);

LIBUI_API QByteArray toplevelView_saveGeometry(const PLSToplevelWidget *widget);
LIBUI_API void toplevelView_restoreGeometry(PLSToplevelWidget *widget, const QByteArray &geometry);

}
}

class PLSToplevelWidgetAccess;

class LIBUI_API PLSToplevelWidget {
protected:
	PLSToplevelWidget() = default;
#if defined(Q_OS_WIN)
	virtual ~PLSToplevelWidget() = default;
#elif defined(Q_OS_MACOS)
	virtual ~PLSToplevelWidget() { delete m_customMacWindow; };
#endif

protected:
	void init(QWidget *widget);

public:
	bool resizeEnabled() const;
	void setResizeEnabled(bool resizeEnabled);
	bool widthResizeEnabled() const;
	void setWidthResizeEnabled(bool widthResizeEnabled);
	bool heightResizeEnabled() const;
	void setHeightResizeEnabled(bool heightResizeEnabled);

	bool moveInContent() const;
	void setMoveInContent(bool moveInContent);

	bool hasTitleBar() const;
#if defined(Q_OS_MACOS)
	PLSCustomMacWindow *customMacWindow() const;
	virtual QList<QWidget *> moveContentExcludeWidgetList();
#endif

	bool isAlwaysOnTop() const;
	static bool isAlwaysOnTop(const QWidget *widget);

public:
	virtual QWidget *self() const = 0;
	virtual int titleBarHeight() const;

	virtual void initSize(const QSize &size);

	virtual QByteArray saveGeometry() const = 0;
	virtual void restoreGeometry(const QByteArray &geometry) = 0;

	virtual void windowStateChanged(QWindowStateChangeEvent *event) {}

protected:
	virtual bool moveInContentIncludeChild(QWidget *parentWidget, QWidget *childWidget) const;
	virtual bool moveInContentExcludeChild(QWidget *parentWidget, QWidget *childWidget) const;

	virtual void onRestoreGeometry();

	QSize calcSize(const QSize &size) const;

#if defined(Q_OS_MACOS)
public:
	bool m_dragging = false;
#endif

private:
	QSize m_initSize;
	bool m_widthResizeEnabled = true;
	bool m_heightResizeEnabled = true;
	bool m_moveInContent = false;
	bool m_firstShow = true;
	int m_titleBarHeight = -1; // <0: auto, =0: no title bar, >0: manual
	friend class PLSToplevelWidgetAccess;
#if defined(Q_OS_MACOS)
	PLSCustomMacWindow *m_customMacWindow{nullptr};
#endif
};

template<typename QtType> class PLSToplevelView : public PLSWidgetCloseHookQt<QtType>, public PLSToplevelWidget {
	using QtBase = PLSWidgetCloseHookQt<QtType>;

protected:
	template<typename... Args> explicit PLSToplevelView(Args &&...args) : QtBase(std::forward<Args>(args)...) { PLSToplevelWidget::init(this); }
	virtual ~PLSToplevelView() override = default;

public:
	QWidget *self() const final { return pls_ptr(this); }

	void initSize(int width, int height) { initSize({width, height}); }
	void initSize(const QSize &size) final
	{
		auto sz = calcSize(size);
		PLSToplevelWidget::initSize(sz);
		QtBase::resize(sz);
	}

	void setFixedSize(int width, int height) { setFixedSize({width, height}); }
	void setFixedSize(const QSize &size)
	{
		auto sz = calcSize(size);
		QtBase::setFixedSize(sz);
		initSize(sz);
	}

	QByteArray saveGeometry() const final { return pls::ui::toplevelView_saveGeometry(this); }
	void restoreGeometry(const QByteArray &geometry) final { pls::ui::toplevelView_restoreGeometry(this, geometry); }

	template<typename Widget> void setCustomChecker(PLSTransparentForMouseEvents<Widget> *widget)
	{
		widget->setCustomChecker([this](QWidget *parentWidget, QWidget *childWidget) {
			if (moveInContentExcludeChild(parentWidget, childWidget)) {
				return pls::ui::CheckResult::Exclude;
			} else if (moveInContentIncludeChild(parentWidget, childWidget)) {
				return pls::ui::CheckResult::Include;
			}
			return pls::ui::CheckResult::Unknown;
		});
	}

protected:
	bool event(QEvent *event) override
	{
		auto result = QtBase::event(event);
		pls::ui::toplevelView_event(this, event);
		return result;
	}
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override
	{
		if (!pls::ui::toplevelView_nativeEvent(this, eventType, message, result)) {
			return QtBase::nativeEvent(eventType, message, result);
		}
		return true;
	}
};

#endif // !PLSCUSTOMTITLEBAR_WINDOWS_H
