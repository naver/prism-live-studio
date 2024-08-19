#include "PLSToplevelView.h"

#include <qevent.h>
#include <libutils-api.h>

#include "libui.h"
#include "PLSTransparentForMouseEvents.h"
#include <QPainterPath>
#include <QPainter>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Comctl32.lib")
#endif
#include "liblog.h"
#include "PLSUIApp.h"

class PLSToplevelWidgetAccess {
public:
	static void setTitleBarHeight(PLSToplevelWidget *widget, int titleBarHeight) { widget->m_titleBarHeight = titleBarHeight; }
	static void show(PLSToplevelWidget *widget)
	{
		if (widget->m_firstShow) {
			widget->m_firstShow = false;
			widget->onRestoreGeometry();
		}
	}
	static void nativeResizeEvent(PLSToplevelWidget *widget, const QSize &size, const QSize &nativeSize) { widget->nativeResizeEvent(size, nativeSize); }
};

namespace pls {
namespace ui {
#ifdef Q_OS_WIN
const bool g_afterWin10 = pls_is_after_win10();
template<typename T> static int getDpiScale(T value, double dpi)
{
	return qRound(value * dpi / 96.0);
}
template<typename T> static int getDpiScaleOriginal(T value, double dpi)
{
	return qRound(value * 96.0 / dpi);
}
static QRect toQRect(const RECT &rc)
{
	return QRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}
static bool isMaximized(HWND handle)
{
	WINDOWPLACEMENT placement = {0};
	placement.length = sizeof(WINDOWPLACEMENT);
	if (GetWindowPlacement(handle, &placement)) {
		return placement.showCmd == SW_SHOWMAXIMIZED;
	}
	return false;
}
static RECT titlebarRect(HWND handle, PLSToplevelWidget *widget)
{
	int height = widget->titleBarHeight();
	if (height == 0) {
		return {0, 0, 0, 0};
	}

	UINT dpi = GetDpiForWindow(handle);

	if (height < 0) {
		SIZE titlebarSize = {0};
		HTHEME theme = OpenThemeData(handle, L"WINDOW");
		GetThemePartSize(theme, nullptr, WP_CAPTION, CS_ACTIVE, nullptr, TS_TRUE, &titlebarSize);
		CloseThemeData(theme);

		PLSToplevelWidgetAccess::setTitleBarHeight(widget, titlebarSize.cy);

		height = getDpiScale(titlebarSize.cy, dpi);
	} else {
		height = getDpiScale(height, dpi);
	}

	RECT rect;
	GetClientRect(handle, &rect);
	rect.bottom = rect.top + height;
	return rect;
}
static QColor getThemeColor()
{
	DWORD color = 0;
	BOOL opaque = FALSE;
	DwmGetColorizationColor(&color, &opaque);
	//0xAARRGGBB
	auto r = (BYTE)(color >> 16);
	auto g = (BYTE)(color >> 8);
	auto b = (BYTE)(color);
	auto a = (BYTE)(color >> 24);
	return QColor(r, g, b, a);
}
static bool wmNcCalcSize(qintptr *result, HWND hwnd, LPARAM lParam, bool fullscreen)
{
	UINT dpi = GetDpiForWindow(hwnd);

	int fx = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
	int fy = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
	int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

	auto params = (NCCALCSIZE_PARAMS *)lParam;
	RECT &rect0 = params->rgrc[0];

	if (g_afterWin10) {
		if (!fullscreen) {
			rect0.right -= fx + padding;
			rect0.left += fx + padding;
			rect0.bottom -= fy + padding;
			if (isMaximized(hwnd)) {
				rect0.top += fy + padding;
			} else if (g_afterWin10) {
				rect0.top += getDpiScale(1, dpi);
			}
		}
	} else if (isMaximized(hwnd)) {
		rect0.right -= fx + padding;
		rect0.left += fx + padding;
		rect0.bottom -= fy + padding;
		rect0.top += fy + padding;
	}

	*result = 0;
	return true;
}
static bool isMaximizedOrFullScreen(PLSToplevelWidget *widget)
{
	if (auto state = widget->self()->windowState(); state.testFlag(Qt::WindowMaximized) || state.testFlag(Qt::WindowFullScreen)) {
		return true;
	}
	return false;
}
static bool wmNcHitTest(qintptr *result, PLSToplevelWidget *widget, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	bool isMaxAndFull = isMaximizedOrFullScreen(widget);
	auto hit = DefWindowProc(hwnd, message, wParam, lParam);
	switch (hit) {
	case HTLEFT:
	case HTRIGHT:
	case HTNOWHERE:
	case HTTOPLEFT:
	case HTTOP:
	case HTTOPRIGHT:
	case HTBOTTOMRIGHT:
	case HTBOTTOM:
	case HTBOTTOMLEFT:
		if (isMaxAndFull) {
			*result = HTCLIENT;
		} else if (widget->resizeEnabled()) {
			*result = hit;
		} else if (widget->widthResizeEnabled()) {
			*result = (hit == HTLEFT || hit == HTRIGHT) ? hit : (widget->moveInContent() ? HTCAPTION : HTCLIENT);
		} else if (widget->heightResizeEnabled()) {
			*result = (hit == HTTOP || hit == HTBOTTOM) ? hit : (widget->moveInContent() ? HTCAPTION : HTCLIENT);
		}
		return true;
	default:
		break;
	}

	UINT dpi = GetDpiForWindow(hwnd);
	int fy = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
	int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
	int cy = fy + padding;

	POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	ScreenToClient(hwnd, &pt);
	if (!g_afterWin10) {
		int fx = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
		int cx = fx + padding;
		RECT rc;
		GetClientRect(hwnd, &rc);
		rc.left += cx;
		rc.top += cy;
		rc.right -= cx;
		rc.bottom -= cy;

		if (!isMaxAndFull) {
			if (widget->widthResizeEnabled() && widget->heightResizeEnabled()) {
				if (pt.x < rc.left && pt.y < rc.top) {
					*result = HTTOPLEFT;
					return true;
				} else if (pt.x < rc.left && pt.y > rc.bottom) {
					*result = HTBOTTOMLEFT;
					return true;
				} else if (pt.x > rc.right && pt.y < rc.top) {
					*result = HTTOPRIGHT;
					return true;
				} else if (pt.x > rc.right && pt.y > rc.bottom) {
					*result = HTBOTTOMRIGHT;
					return true;
				}
			}

			if (widget->widthResizeEnabled()) {
				if (pt.x < rc.left) {
					*result = HTLEFT;
					return true;
				} else if (pt.x > rc.right) {
					*result = HTRIGHT;
					return true;
				}
			}

			if (widget->heightResizeEnabled()) {
				if (pt.y < rc.top) {
					*result = HTTOP;
					return true;
				} else if (pt.y > rc.bottom) {
					*result = HTBOTTOM;
					return true;
				}
			}
		}

		if (widget->hasTitleBar() && (pt.y < titlebarRect(hwnd, widget).bottom)) {
			*result = HTCAPTION;
			return true;
		}
	} else if (pt.y < cy) {
		if (!isMaxAndFull && widget->heightResizeEnabled())
			*result = HTTOP;
		else if (widget->hasTitleBar() || widget->moveInContent())
			*result = HTCAPTION;
		else
			*result = HTCLIENT;
		return true;
	} else if (widget->hasTitleBar() && (pt.y < titlebarRect(hwnd, widget).bottom)) {
		*result = HTCAPTION;
		return true;
	}
	*result = widget->moveInContent() ? HTCAPTION : HTCLIENT;
	return true;
}
static bool wmCreate(HWND hwnd)
{
	RECT rect;
	GetWindowRect(hwnd, &rect);
	SetWindowPos(hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
	return false;
}
static bool wmActivate(qintptr *result, PLSToplevelWidget *widget, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT rect = titlebarRect(hwnd, widget);
	InvalidateRect(hwnd, &rect, FALSE);
	*result = DefWindowProc(hwnd, message, wParam, lParam);
	return true;
}
static void wmSize(PLSToplevelWidget *widget, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	auto width = LOWORD(lParam);
	auto height = HIWORD(lParam);
	UINT dpi = GetDpiForWindow(hwnd);
	PLSToplevelWidgetAccess::nativeResizeEvent(widget, QSize(getDpiScaleOriginal(width, dpi), getDpiScaleOriginal(height, dpi)), QSize(width, height));
}
#endif

static bool isWidgetFullscreen(QWidget *widget)
{
	return widget->windowState().testFlag(Qt::WindowFullScreen);
}

LIBUI_API void toplevelView_event(PLSToplevelWidget *widget, QEvent *event)
{
	switch (event->type()) {
	case QEvent::WindowStateChange:
		widget->windowStateChanged(static_cast<QWindowStateChangeEvent *>(event));
		break;
#ifdef Q_OS_WIN
	case QEvent::Show:
		PLSToplevelWidgetAccess::show(widget);
		break;
#else
	case QEvent::Show: {
		widget->customMacWindow()->initWinId(widget->self());
		widget->customMacWindow()->setWidthResizeEnabled(widget->self(), widget->widthResizeEnabled());
		widget->customMacWindow()->setHeightResizeEnabled(widget->self(), widget->heightResizeEnabled());
		PLSToplevelWidgetAccess::show(widget);
		break;
	}
	case QEvent::UpdateRequest:
	case QEvent::LayoutRequest:
		widget->customMacWindow()->updateTrafficButtonUI();
		break;
#endif
	default:
		break;
	}
}
LIBUI_API bool toplevelView_nativeEvent(PLSToplevelWidget *widget, const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
	PMSG msg = (PMSG)message;
	switch (msg->message) {
	case WM_NCCALCSIZE:
		if (msg->wParam)
			return wmNcCalcSize(result, msg->hwnd, msg->lParam, isWidgetFullscreen(widget->self()));
		break;
	case WM_NCHITTEST:
		return wmNcHitTest(result, widget, msg->hwnd, msg->message, msg->wParam, msg->lParam);
	case WM_CREATE:
		return wmCreate(msg->hwnd);
	case WM_ACTIVATE:
		return wmActivate(result, widget, msg->hwnd, msg->message, msg->wParam, msg->lParam);
	case WM_GETDPISCALEDSIZE:
		return true;
	case WM_SIZE:
		wmSize(widget, msg->hwnd, msg->wParam, msg->lParam);
		break;
	case WM_ACTIVATEAPP:
		if (auto app = PLSUiApp::instance(); app)
			app->setAppState(msg->wParam ? true : false);
		break;
	default:
		break;
	}
#endif
	return false;
}

LIBUI_API QByteArray toplevelView_saveGeometry(const PLSToplevelWidget *ctlwidget)
{
	return ctlwidget->self()->saveGeometry();
}

LIBUI_API void toplevelView_restoreGeometry(PLSToplevelWidget *tlwidget, const QByteArray &geometry)
{
	tlwidget->self()->restoreGeometry(geometry);
}

}
}

void PLSToplevelWidget::init(QWidget *widget)
{
#if defined(Q_OS_WIN)
	widget->setProperty("windows", true);
	widget->setProperty("afterWin10", pls::ui::g_afterWin10);
	auto hwnd = (HWND)widget->winId();
	auto menu = CreateMenu();
	SetMenu(hwnd, menu);
	if (pls::ui::g_afterWin10) {
		COLORREF color = RGB(17, 17, 17);
		DwmSetWindowAttribute(hwnd, 34, &color, sizeof(color));
	}
#elif defined(Q_OS_MACOS)
	widget->setProperty("windows", false);
	m_customMacWindow = new PLSCustomMacWindow();
#endif
}

#if defined(Q_OS_MACOS)
PLSCustomMacWindow *PLSToplevelWidget::customMacWindow() const
{
	return m_customMacWindow;
}
#endif

bool PLSToplevelWidget::resizeEnabled() const
{
	return m_widthResizeEnabled && m_heightResizeEnabled;
}
void PLSToplevelWidget::setResizeEnabled(bool resizeEnabled)
{
	m_widthResizeEnabled = resizeEnabled;
	m_heightResizeEnabled = resizeEnabled;
}

bool PLSToplevelWidget::widthResizeEnabled() const
{
	return m_widthResizeEnabled;
}
void PLSToplevelWidget::setWidthResizeEnabled(bool widthResizeEnabled)
{
	m_widthResizeEnabled = widthResizeEnabled;
}
bool PLSToplevelWidget::heightResizeEnabled() const
{
	return m_heightResizeEnabled;
}
void PLSToplevelWidget::setHeightResizeEnabled(bool heightResizeEnabled)
{
	m_heightResizeEnabled = heightResizeEnabled;
}

bool PLSToplevelWidget::moveInContent() const
{
	return m_moveInContent;
}
void PLSToplevelWidget::setMoveInContent(bool moveInContent)
{
	m_moveInContent = moveInContent;
}

bool PLSToplevelWidget::hasTitleBar() const
{
	return titleBarHeight() != 0;
}

bool PLSToplevelWidget::isAlwaysOnTop() const
{
	return isAlwaysOnTop(pls_ptr(this)->self());
}

bool PLSToplevelWidget::isAlwaysOnTop(const QWidget *widget)
{
#ifdef Q_OS_WIN
	DWORD exStyle = GetWindowLong((HWND)widget->winId(), GWL_EXSTYLE);
	return (exStyle & WS_EX_TOPMOST) != 0;
#else
	return false;
#endif
}

void PLSToplevelWidget::disableWinSystemBorder() const
{
	disableWinSystemBorder(pls_ptr(this)->self());
}

void PLSToplevelWidget::disableWinSystemBorder(const QWidget *widget)
{
#ifdef Q_OS_WIN
	auto hwnd = (HWND)widget->winId();
	if (pls::ui::g_afterWin10) {
		COLORREF color = RGB(17, 17, 17);
		DwmSetWindowAttribute(hwnd, 34, &color, sizeof(color));
	}
#else

#endif
}

int PLSToplevelWidget::titleBarHeight() const
{
	return m_titleBarHeight;
}
void PLSToplevelWidget::initSize(const QSize &size)
{
	m_initSize = size;
}

bool PLSToplevelWidget::moveInContentIncludeChild(QWidget *parentWidget, QWidget *childWidget) const
{
	return pls::ui::transparentForMouseEvents_moveInContentIncludeChild(parentWidget, childWidget);
}
bool PLSToplevelWidget::moveInContentExcludeChild(QWidget *parentWidget, QWidget *childWidget) const
{
	return pls::ui::transparentForMouseEvents_moveInContentExcludeChild(parentWidget, childWidget);
}

#if defined(Q_OS_MACOS)
QList<QWidget *> PLSToplevelWidget::moveContentExcludeWidgetList()
{
	return QList<QWidget *>();
}
#endif

static QWidget *getToplevel(QWidget *widget)
{
	for (auto parent = widget; parent; parent = parent->parentWidget())
		if (pls_is_toplevel_view(parent))
			return parent;
	return nullptr;
}

void PLSToplevelWidget::onRestoreGeometry()
{
	if (m_initSize.isEmpty())
		return;

	auto widget = self();
	auto parentWidgt = widget->parentWidget();
	auto parent = getToplevel(widget->parentWidget());
	auto screen = parent ? parent->screen() : widget->screen();
	QRect availableRect = screen->availableGeometry();
	QSize size = m_initSize.expandedTo(widget->minimumSize()).boundedTo(widget->maximumSize()).boundedTo(availableRect.size());
	QRect showRect = parent ? parent->geometry() : availableRect;
	QPoint pos(showRect.x() + (showRect.width() - size.width()) / 2, showRect.y() + (showRect.height() - size.height()) / 2);

#if defined(Q_OS_MACOS)
	bool hasParentTitleBar = false;
	bool hasTitleBar = customMacWindow()->hasTitleBar(widget);
	float titleBarHeight = customMacWindow()->getTitlebarHeight(widget);
	if (parent) {
		hasParentTitleBar = customMacWindow()->hasTitleBar(parent);
	}

	if (hasParentTitleBar && !hasTitleBar) {
		QPoint diffPosition = pos;
		diffPosition.setY(pos.y() - titleBarHeight * 0.5);
		pos = diffPosition;
	}

	if (!hasParentTitleBar && hasTitleBar) {
		QPoint diffPosition = pos;
		diffPosition.setY(pos.y() + titleBarHeight * 0.5f);
		pos = diffPosition;
	}

#endif

	pos.setX(qMax(availableRect.x(), pos.x()));
	pos.setY(qMax(availableRect.y(), pos.y()));
	if ((pos.x() + size.width()) > availableRect.right())
		pos.setX(availableRect.right() - size.width());
	if ((pos.y() + size.height()) > availableRect.bottom())
		pos.setY(availableRect.bottom() - size.height());

	widget->setGeometry(pos.x(), pos.y(), size.width(), size.height());
}

void PLSToplevelWidget::nativeResizeEvent(const QSize &size, const QSize &nativeSize) {}

QSize PLSToplevelWidget::calcSize(const QSize &size) const
{
#ifdef Q_OS_WIN
	if (pls_is_after_win10())
		return QSize(size.width() - 2, size.height() - 2);
#endif
	return size;
}
