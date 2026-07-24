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

/**
 * 34 is windows title bar height
 * 32 is macos title bar height
 * windows dialog heigth = content + title height(34)
 * windows dialog heigth = content height
 **/

//all is windows dialog size
#define PLS_TITLE_BAR_HEIGHT 34

class PLSToplevelWidget;
class PLSResizeTracker;

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
	enum CreateWinId { DontCreate, Create };
	void init(QWidget *widget, CreateWinId createWinId);

public:
	bool resizeEnabled() const;
	void setResizeEnabled(bool resizeEnabled);
	bool widthResizeEnabled() const;
	void setWidthResizeEnabled(bool widthResizeEnabled);
	bool heightResizeEnabled() const;
	void setHeightResizeEnabled(bool heightResizeEnabled);

	bool hasTitleBar() const;
#if defined(Q_OS_MACOS)
	PLSCustomMacWindow *customMacWindow() const;
	virtual QList<QWidget *> moveContentExcludeWidgetList();
#endif

	bool isAlwaysOnTop() const;
	static bool isAlwaysOnTop(const QWidget *widget);
	void disableWinSystemBorder() const;
	static void disableWinSystemBorder(const QWidget *widget);

	bool isAfterWin10() const;
	PLSResizeTracker *resizeTracker() const;

	bool isFirstPainting() const { return m_firstPainting; }
	void setFirstPainting(bool isFirstPainting) { m_firstPainting = isFirstPainting; }

public:
	virtual QWidget *self() const = 0;
	virtual int titleBarHeight() const;

	virtual void initSize(const QSize &size);

	virtual QByteArray saveGeometry() const = 0;
	virtual void restoreGeometry(const QByteArray &geometry) = 0;

	virtual void windowStateChanged(QWindowStateChangeEvent *event) {}
	virtual void winIdChanged(WId winId) {}

protected:
	virtual bool moveExcludeChild(QWidget *child) const;

	virtual void onRestoreGeometry();

	virtual void nativeResizeEvent(const QSize &size, const QSize &nativeSize);

	QSize calcSize(const QSize &size) const;
	bool stabilizeFixedSizeOnResize()
	{
		auto widget = self();
		if (!widget || m_adjustingFixedSize) {
			return false;
		}

		const bool fixedWidth = widget->minimumWidth() == widget->maximumWidth();
		const bool fixedHeight = widget->minimumHeight() == widget->maximumHeight();
		if (!fixedWidth && !fixedHeight) {
			return false;
		}

		QSize expectedSize = widget->size();
		if (fixedWidth && expectedSize.width() != widget->minimumWidth()) {
			expectedSize.setWidth(widget->minimumWidth());
		}
		if (fixedHeight && expectedSize.height() != widget->minimumHeight()) {
			expectedSize.setHeight(widget->minimumHeight());
		}
		if (expectedSize == widget->size()) {
			return false;
		}

		m_adjustingFixedSize = true;
		widget->resize(expectedSize);
		m_adjustingFixedSize = false;
		return true;
	}

#if defined(Q_OS_MACOS)
public:
	bool m_dragging = false;
#endif

private:
	QSize m_initSize;
	bool m_widthResizeEnabled = true;
	bool m_heightResizeEnabled = true;
	bool m_firstShow = true;
	bool m_firstPainting = true;
	bool m_adjustingFixedSize = false;
	int m_titleBarHeight = -1; // <0: auto, =0: no title bar, >0: manual
	PLSResizeTracker *m_resizeTracker{nullptr};
	friend class PLSToplevelWidgetAccess;
#if defined(Q_OS_MACOS)
	PLSCustomMacWindow *m_customMacWindow{nullptr};
#endif
};

template<typename QtType> class PLSToplevelView : public PLSWidgetCloseHookQt<QtType>, public PLSToplevelWidget {
	using QtBase = PLSWidgetCloseHookQt<QtType>;

protected:
	template<typename... Args> explicit PLSToplevelView(CreateWinId createWinId, Args &&...args) : QtBase(std::forward<Args>(args)...) { PLSToplevelWidget::init(this, createWinId); }
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
	void setMinimumSize(const QSize &size) { QtBase::setMinimumSize(calcSize(size)); }
	void setMinimumSize(int minw, int minh) { setMinimumSize({minw, minh}); }
	void setMaximumSize(const QSize &size) { QtBase::setMaximumSize(calcSize(size)); }
	void setMaximumSize(int maxw, int maxh) { setMaximumSize({maxw, maxh}); }

	QByteArray saveGeometry() const final { return pls::ui::toplevelView_saveGeometry(this); }
	void restoreGeometry(const QByteArray &geometry) final { pls::ui::toplevelView_restoreGeometry(this, geometry); }

	template<typename Widget> void setMoveExcludeChecker(PLSTransparentForMouseEvents<Widget> *widget)
	{
		widget->setMoveExcludeChecker([this](QWidget *child) { return moveExcludeChild(child); });
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
