#include "PLSNCB2bBrowserDock.h"
#include "PLSBasic.h"
#include "PLSNCB2bBrowserDockContent.h"

PLSNCB2bBrowserDock::PLSNCB2bBrowserDock(const QString &title, const QString &selectedTitle, QWidget *parent) : OBSDock(title, parent)
{
	setAttribute(Qt::WA_NativeWindow);
	setObjectName("ncb2bDock");
	setAllowedAreas(Qt::AllDockWidgetAreas);
	content = pls_new<PLSNCB2bBrowserDockContent>(this);
	content->setWindowTitle(title);
	content->setCheckedTitle(selectedTitle);
	this->setWidget(content);
}

PLSNCB2bBrowserDock::~PLSNCB2bBrowserDock() {}

void PLSNCB2bBrowserDock::refreshUI(PLSErrorHandler::ErrCode errCode)
{
	content->refreshUI(errCode);
}

void PLSNCB2bBrowserDock::closeBrowser()
{
	setProperty("selectedTitle", content->getCheckedTitle());
	content->closeBrowser();
}

void PLSNCB2bBrowserDock::showEvent(QShowEvent *event)
{
	OBSDock::showEvent(event);
	if (auto basic = PLSBasic::instance(); basic) {
		content->refreshUI(basic->getBrowserSettingsErrorCode());
	}
}

void PLSNCB2bBrowserDock::resizeEvent(QResizeEvent *event)
{
	content->refreshScrollArea();
	OBSDock::resizeEvent(event);
}
