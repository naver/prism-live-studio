#include "window-dock-browser.hpp"
#include <QCloseEvent>

void BrowserDock::closeEvent(QCloseEvent *event)
{
	OBSDock::closeEvent(event);

	if (!event->isAccepted()) {
		return;
	}

	closeBrowser();
}

void BrowserDock::closeBrowser()
{
	static int panel_version = -1;
	if (panel_version == -1) {
		panel_version = obs_browser_qcef_version();
	}

	if (panel_version >= 2 && !!cefWidget) {
		cefWidget->closeBrowser();
	}
}

void BrowserDock::showEvent(QShowEvent *event)
{
	OBSDock::showEvent(event);
	setWindowTitle(title);
}
