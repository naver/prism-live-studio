#include "moc_qt-display.cpp"
#include "display-helpers.hpp"
#include <QWindow>
#include <QScreen>
#include <QResizeEvent>
#include <QShowEvent>
#include "libui.h"

#include <qt-wrappers.hpp>
#include <obs-config.h>
#include "qt-display-failed-view.hpp"
#include "PLSDock.h"
#include "libutils-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "source-tree.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#endif

#ifdef ENABLE_WAYLAND
#include <qpa/qplatformnativeinterface.h>
#endif

using namespace common;

class SurfaceEventFilter : public QObject {
	OBSQTDisplay *display;

public:
	SurfaceEventFilter(OBSQTDisplay *src) : QObject(src), display(src) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override
	{
		bool result = QObject::eventFilter(obj, event);
		QPlatformSurfaceEvent *surfaceEvent;

		switch (event->type()) {
		case QEvent::PlatformSurface:
			surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);

			switch (surfaceEvent->surfaceEventType()) {
			case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
				display->DestroyDisplay();
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		return result;
	}
};

static inline long long color_to_int(const QColor &color)
{
	auto shift = [&](unsigned val, int shift) {
		return ((val & 0xff) << shift);
	};

	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}

static inline QColor rgba_to_color(uint32_t rgba)
{
	return QColor::fromRgb(rgba & 0xFF, (rgba >> 8) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 24) & 0xFF);
}

static bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
#elif __APPLE__
	gswindow.view = (id)window->winId();
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		break;
#ifdef ENABLE_WAYLAND
	case OBS_NIX_PLATFORM_WAYLAND: {
		QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
		gswindow.display = native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
		break;
	}
#endif
	default:
		success = false;
		break;
	}
#endif
	return success;
}

static bool overlayRequestedToShow(const QWidget *widget)
{
	return widget && !widget->isHidden();
}

OBSQTDisplay::OBSQTDisplay(QWidget *parent, Qt::WindowFlags flags) : QWidget(parent, flags)
{
#ifndef USE_WIN32_HWND
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
#endif
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	pls_add_css(this, {"PLSQTDisplay"});

	auto windowVisible = [this](bool visible) {
		if (destroying) {
			return;
		}

		if (!visible) {
#if !defined(_WIN32) && !defined(__APPLE__)
			display = nullptr;
#endif
			return;
		}

		if (!display) {
			CreateDisplay();
		} else {
			syncObsDisplaySurfacePixels();
		}
	};

	auto screenChanged = [this](QScreen *screen) {
		if (destroying) {
			return;
		}

		CreateDisplay();
		disconnect(screenConnection);
		if (!screen) {
			return;
		}

		screenConnection = connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this]() {
			if (destroying) {
				return;
			}

			syncObsDisplaySurfacePixels();
			emit DisplayResized();
		});

		syncObsDisplaySurfacePixels();
		emit DisplayResized();
	};

	connect(windowHandle(), &QWindow::visibleChanged, this, windowVisible);
	connect(windowHandle(), &QWindow::screenChanged, this, screenChanged);

	windowHandle()->installEventFilter(new SurfaceEventFilter(this));

	textOverlay = pls_new<QWidget>(this);
#ifdef USE_WIN32_HWND
	textOverlay->setAttribute(Qt::WA_NativeWindow);
#endif
	textOverlay->hide();

	displayText = pls_new<QLabel>(textOverlay);
	displayText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	displayText->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	displayText->setObjectName("displayText");
	displayText->setWordWrap(true);
	displayText->setIndent(-1);
	displayText->hide();
	QSizePolicy sizePolicy = displayText->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	displayText->setSizePolicy(sizePolicy);

	failedView = pls_new<OBSQTDisplayFailedView>(textOverlay);
	failedView->hide();
	sizePolicy = failedView->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	failedView->setSizePolicy(sizePolicy);

	textOverlay->setGeometry(rect());
	displayText->setGeometry(textOverlay->rect());
	failedView->setGeometry(textOverlay->rect());
}

OBSQTDisplay::~OBSQTDisplay()
{
	destroying = true;
	disconnect(screenConnection);
	display = nullptr; // obs::display should be freed before destroying window

#ifdef USE_WIN32_HWND
	HWND hWnd = (HWND)hWndDisplay;
	if (IsWindow(hWnd)) {
		SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
		DestroyWindow(hWnd);
	}
#endif
}

QColor OBSQTDisplay::GetDisplayBackgroundColor() const
{
	return rgba_to_color(backgroundColor);
}

void OBSQTDisplay::SetDisplayBackgroundColor(const QColor &color)
{
	uint32_t newBackgroundColor = (uint32_t)color_to_int(color);

	if (newBackgroundColor != backgroundColor) {
		backgroundColor = newBackgroundColor;
		UpdateDisplayBackgroundColor();
	}
}

void OBSQTDisplay::UpdateDisplayBackgroundColor()
{
	obs_display_set_background_color(display, backgroundColor);
}

void OBSQTDisplay::CreateDisplay(bool force)
{
	if (display)
		return;

	if (destroying)
		return;

#ifndef USE_WIN32_HWND
	if (!windowHandle()->isExposed() && !force)
		return;
#endif

	QSize size = GetPixelSize(this);
	if (size.width() <= 0 || size.height() <= 0)
		return;

	gs_init_data info = {};
	info.cx = size.width();
	info.cy = size.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;

	if (!QTToGSWindow(windowHandle(), info.window))
		return;

#ifdef USE_WIN32_HWND
	if (CreatePreviewWindow()) {
		MoveWindow((HWND)hWndDisplay, 0, 0, size.width(), size.height(), FALSE);

		const bool hideDisplay = overlayRequestedToShow(displayText) || overlayRequestedToShow(failedView);
		ShowWindow((HWND)hWndDisplay, hideDisplay ? SW_HIDE : SW_SHOW);

		info.window.hwnd = (HWND)hWndDisplay;
	}
#endif

	display = obs_display_create(&info, backgroundColor);
#ifdef USE_WIN32_HWND
	if (!display && IsWindow((HWND)hWndDisplay)) {
		SetWindowLongPtr((HWND)hWndDisplay, GWLP_USERDATA, 0);
		DestroyWindow((HWND)hWndDisplay);
		hWndDisplay = 0;
	}
#endif

	emit DisplayCreated(this);
}

void OBSQTDisplay::paintEvent(QPaintEvent *event)
{
	CreateDisplay();

	QWidget::paintEvent(event);
}

void OBSQTDisplay::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);

	OnMove();
}

bool OBSQTDisplay::nativeEvent(const QByteArray &, void *message, qintptr *)
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_DISPLAYCHANGE:
		OnDisplayChange();
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return false;
}

void OBSQTDisplay::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	if (textOverlay) {
		textOverlay->setGeometry(rect());
		displayText->setGeometry(textOverlay->rect());
		failedView->setGeometry(textOverlay->rect());
	}

	CreateDisplay();
	ResizeDisplay();
}

QSize OBSQTDisplay::GetWidgetSize()
{
	return GetPixelSize(this);
}

QPaintEngine *OBSQTDisplay::paintEngine() const
{
	return nullptr;
}

void OBSQTDisplay::syncObsDisplaySurfacePixels(int pixelWidth, int pixelHeight)
{
	if (!display) {
		return;
	}

	int w = 0;
	int h = 0;
	if (pixelWidth > 0 && pixelHeight > 0) {
		w = pixelWidth;
		h = pixelHeight;
	} else {
		QSize size = GetPixelSize(this);
		w = size.width();
		h = size.height();
	}

	obs_display_resize(display, w, h);

#ifdef USE_WIN32_HWND
	if (IsWindow((HWND)hWndDisplay))
		MoveWindow((HWND)hWndDisplay, 0, 0, w, h, FALSE);
#endif
}

void OBSQTDisplay::ResizeDisplay()
{
	if (!display) {
		return;
	}

	syncObsDisplaySurfacePixels();
	emit DisplayResized();
}

void OBSQTDisplay::OnMove()
{
	if (display)
		obs_display_update_color_space(display);
}

void OBSQTDisplay::OnDisplayChange()
{
	if (display)
		obs_display_update_color_space(display);
}

void OBSQTDisplay::showGuideText(const QString &guideText)
{
	displayText->setText(guideText);
	showTextLabel(displayText, true);
}

void OBSQTDisplay::hideGuideText()
{
	showTextLabel(displayText, false);
}

void OBSQTDisplay::setFailedText(const QString &text, bool needLoading)
{
	if (text.isEmpty() && !needLoading) {
		hideFailedText();
		return;
	}

	failedView->setContent(text, needLoading);
	showTextLabel(failedView, true);
	failedView->refreshContentLayout();
}

void OBSQTDisplay::hideFailedText()
{
	failedView->hideContent();
	showTextLabel(failedView, false);
}

void OBSQTDisplay::showTextLabel(QWidget *label, bool isShow)
{
	// firstly hide other labels, then show the target label

	if (isShow) {
		if (label != displayText)
			displayText->hide();
		if (label != failedView)
			failedView->hide();

		if (textOverlay) {
			textOverlay->setGeometry(rect());
			label->setGeometry(textOverlay->rect());
			textOverlay->show();
			textOverlay->raise();
		}

#ifdef USE_WIN32_HWND
		ShowPreviewWindow(false);
#endif
		label->show();
		label->raise();

	} else {
		label->hide();

		const bool hasVisibleText = overlayRequestedToShow(displayText) || overlayRequestedToShow(failedView);

		if (!hasVisibleText && textOverlay)
			textOverlay->hide();

#ifdef USE_WIN32_HWND
		ShowPreviewWindow(!hasVisibleText);
#endif
	}
}

#ifdef USE_WIN32_HWND
bool OBSQTDisplay::CreatePreviewWindow()
{
	if (IsWindow((HWND)hWndDisplay))
		return true;

	hWndDisplay = 0;

	if (!CustomRegisterClass())
		return false;

	// refer to https://github.com/streamlabs/obs-studio-node/blob/staging/obs-studio-server/source/nodeobs_display.cpp
	HWND newWindow = CreateWindowEx(WS_EX_LAYERED, DISPLAY_CLASS_NAME, L"", WS_POPUP, 0, 0, 0, 0, NULL, NULL,
					GetModuleHandle(NULL), this);
	if (!IsWindow(newWindow)) {
		assert(false);
		return false;
	}

	SetWindowLongPtr(newWindow, GWLP_USERDATA, (LONG_PTR)this);
	connect(this, &QWidget::destroyed, this,
		[this, newWindow]() { SetWindowLongPtr(newWindow, GWLP_USERDATA, 0); });

	SetLayeredWindowAttributes(newWindow, 0, 255, LWA_ALPHA);
	SetParent(newWindow, (HWND)this->winId());

	LONG_PTR style = GetWindowLongPtr(newWindow, GWL_STYLE);
	style &= ~WS_POPUP;
	style |= WS_CHILD;
	SetWindowLongPtr(newWindow, GWL_STYLE, style);

	LONG_PTR exStyle = GetWindowLongPtr(newWindow, GWL_EXSTYLE);
	exStyle |= WS_EX_TRANSPARENT;
	SetWindowLongPtr(newWindow, GWL_EXSTYLE, exStyle);
	SetWindowPos(newWindow, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	hWndDisplay = (uint64_t)newWindow;
	return true;
}

LRESULT __stdcall DisplayMessageProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch (nMsg) {
	case WM_DESTROY: {
		//PostQuitMessage(0); // Here we can't exit message loop because that is managed by Qt
		if (auto self = (OBSQTDisplay *)GetWindowLongPtr(hWnd, GWLP_USERDATA); self != nullptr)
			self->OnWindowDestroy((uint64_t)hWnd);
		break;
	}

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}

	default:
		break;
	}

	return DefWindowProc(hWnd, nMsg, wParam, lParam);
}

bool OBSQTDisplay::CustomRegisterClass()
{
	WNDCLASS wc = {0};
	wc.style = CS_NOCLOSE | CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = DisplayMessageProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = DISPLAY_CLASS_NAME;

	ATOM ret = RegisterClass(&wc);
	if (0 == ret && ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
		assert(false);
		return false;
	}

	return true;
}

void OBSQTDisplay::OnWindowDestroy(uint64_t hWnd)
{
	if (hWnd == hWndDisplay) {
		DestroyDisplay();
		hWndDisplay = 0;
	}
}

void OBSQTDisplay::ShowPreviewWindow(bool show)
{
	HWND hWnd = (HWND)hWndDisplay;
	if (IsWindow(hWnd))
		ShowWindow(hWnd, show ? SW_SHOW : SW_HIDE);
}

#endif // USE_WIN32_HWND