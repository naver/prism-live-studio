#pragma once

#include "window-dock.hpp"
#include <QScopedPointer>
#include "libui.h"

#include <browser-panel.hpp>
extern QCef *cef;
extern QCefCookieManager *panel_cookies;

class BrowserDock : public OBSDock {
public:
	inline BrowserDock() : OBSDock()
	{
		pls_add_css(this, {"BrowserDock"});
		setAttribute(Qt::WA_NativeWindow);
	}

	QScopedPointer<QCefWidget> cefWidget;

	inline void SetWidget(QCefWidget *widget_)
	{
		setWidget(widget_);
		cefWidget.reset(widget_);
	}

	void closeEvent(QCloseEvent *event) override;

	void closeBrowser();
};
