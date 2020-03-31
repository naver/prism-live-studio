#include "PLSCompleter.hpp"

#include <Windows.h>
#include <QVBoxLayout>
#include <QEvent>
#include <QApplication>

#include "frontend-api.h"

const int ItemHeight = 40;

float getDevicePixelRatio(QWidget *widget);

namespace {
PLSCompleter *g_activeCompleter = nullptr;

class HookNativeEvent {
public:
	HookNativeEvent() { m_mouseHook = SetWindowsHookExW(WH_MOUSE, &mouseHookProc, GetModuleHandleW(nullptr), GetCurrentThreadId()); }
	~HookNativeEvent()
	{
		if (m_mouseHook) {
			UnhookWindowsHookEx(m_mouseHook);
			m_mouseHook = nullptr;
		}
	}

protected:
	static LRESULT CALLBACK mouseHookProc(_In_ int code, _In_ WPARAM wParam, _In_ LPARAM lParam)
	{
		if ((code < 0) || !g_activeCompleter) {
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

		if ((wParam == WM_LBUTTONDOWN) || (wParam == WM_LBUTTONUP) || (wParam == WM_LBUTTONDBLCLK) || (wParam == WM_RBUTTONDOWN) || (wParam == WM_RBUTTONUP) || (wParam == WM_RBUTTONDBLCLK) ||
		    (wParam == WM_MBUTTONDOWN) || (wParam == WM_MBUTTONUP) || (wParam == WM_MBUTTONDBLCLK) || (wParam == WM_NCLBUTTONDOWN) || (wParam == WM_NCLBUTTONUP) ||
		    (wParam == WM_NCLBUTTONDBLCLK) || (wParam == WM_NCRBUTTONDOWN) || (wParam == WM_NCRBUTTONUP) || (wParam == WM_NCRBUTTONDBLCLK) || (wParam == WM_NCMBUTTONDOWN) ||
		    (wParam == WM_NCMBUTTONUP) || (wParam == WM_NCMBUTTONDBLCLK)) {
			LPMOUSEHOOKSTRUCT mhs = (LPMOUSEHOOKSTRUCT)lParam;
			if (!isInCompleter(mhs->pt)) {
				g_activeCompleter->hide();
			}
		}

		return CallNextHookEx(nullptr, code, wParam, lParam);
	}
	static bool isInCompleter(const POINT &pt)
	{
		RECT rc;
		GetWindowRect((HWND)g_activeCompleter->winId(), &rc);
		if (PtInRect(&rc, pt)) {
			return true;
		}
		return false;
	}

	HHOOK m_keyboardHook;
	HHOOK m_mouseHook;
};

void installNativeEventFilter()
{
	static std::unique_ptr<HookNativeEvent> hookNativeEvent;
	if (!hookNativeEvent) {
		hookNativeEvent.reset(new HookNativeEvent);
	}
}
}

PLSCompleter::PLSCompleter(QLineEdit *lineEdit, const QStringList &completions) : QFrame(lineEdit, Qt::Tool | Qt::FramelessWindowHint)
{
	installNativeEventFilter();
	setMouseTracking(true);

	this->lineEdit = lineEdit;

	QVBoxLayout *layout1 = new QVBoxLayout(this);
	layout1->setSpacing(0);
	layout1->setMargin(0);
	this->scrollArea = new QScrollArea(this);
	this->scrollArea->setWidgetResizable(true);
	this->scrollArea->setFrameShape(QFrame::NoFrame);
	layout1->addWidget(scrollArea);

	QWidget *widget = new QWidget();
	this->scrollArea->setWidget(widget);

	QVBoxLayout *layout2 = new QVBoxLayout(widget);
	layout2->setSpacing(0);
	layout2->setMargin(0);

	for (auto &completion : completions) {
		QLabel *label = new QLabel(completion, widget);
		label->setMouseTracking(true);
		label->installEventFilter(this);
		layout2->addWidget(label);
		labels.append(label);
	}

	connect(qApp, &QApplication::applicationStateChanged, this, &PLSCompleter::hide);

	connect(lineEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
		if (!this->lineEdit->hasFocus()) {
			return;
		}

		int showCount = 0;
		for (auto &label : labels) {
			if (label->text().contains(text)) {
				label->show();
				++showCount;
			} else {
				label->hide();
			}
		}

		if (showCount <= 0) {
			hide();
			return;
		}

		float pixelRatio = getDevicePixelRatio(this);
		setFixedSize(this->lineEdit->width(), (showCount > 5 ? 5 : showCount) * ItemHeight + 5);
		scrollArea->widget()->adjustSize();
		ensurePolished();
		move(this->lineEdit->mapToGlobal(QPoint(0, int(this->lineEdit->height()))));
		show();
	});

	connect(this, &PLSCompleter::activated, lineEdit, &QLineEdit::setText);
}

PLSCompleter::~PLSCompleter()
{
	if (g_activeCompleter == this) {
		g_activeCompleter = nullptr;
	}
}

bool PLSCompleter::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::Show:
		g_activeCompleter = this;
		break;
	case QEvent::Hide:
		if (g_activeCompleter == this) {
			g_activeCompleter = nullptr;
		}
		break;
	}
	return QFrame::event(event);
}

bool PLSCompleter::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::Enter:
		if (QLabel *label = dynamic_cast<QLabel *>(watched); label) {
			pls_flush_style(label, "state", "hover");
		}
		break;
	case QEvent::Leave:
		if (QLabel *label = dynamic_cast<QLabel *>(watched); label) {
			pls_flush_style(label, "state", "");
		}
		break;
	case QEvent::MouseButtonRelease:
		if (QLabel *label = dynamic_cast<QLabel *>(watched); label) {
			QSignalBlocker blocker(lineEdit);
			emit activated(label->text());
			hide();
		}
		break;
	}
	return QFrame::eventFilter(watched, event);
}
