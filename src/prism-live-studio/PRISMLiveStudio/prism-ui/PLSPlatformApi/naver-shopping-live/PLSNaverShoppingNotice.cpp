#include "PLSNaverShoppingNotice.h"
#include "ui_PLSNaverShoppingNotice.h"
#include "utils-api.h"
#include "libui.h"
#include "liblog.h"
#include "pls-common-define.hpp"

constexpr auto logMoudule = "PLSNaverShoppingNotice";

PLSNaverShoppingNotice::PLSNaverShoppingNotice(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSNaverShoppingNotice>();
	pls_add_css(this, {"PLSNaverShoppingNotice"});
	setupUi(ui);
	initSize(720, 488);
	setResizeEnabled(false);

#ifdef Q_OS_MACOS
	setHasCaption(true);
	setMoveInContent(false);
	setHasHLine(false);
	setWindowTitle(tr("NaverShoppingLive.Service.Notice"));
#else
	setHasCaption(false);
	setMoveInContent(true);
	setHasHLine(true);
#endif // Q_OS_MACOS

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSNaverShoppingNotice::~PLSNaverShoppingNotice()
{
	pls_delete(ui);
}

void PLSNaverShoppingNotice::setURL(const QString &url)
{
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url(url)
								 .initBkgColor(QColor(17, 17, 17))
								 .css(common::TERM_WEBVIEW_CSS)
								 .showAtLoadEnded(true));
	ui->verticalLayout_2->addWidget(m_browserWidget);
}

void PLSNaverShoppingNotice::on_closeButton_clicked()
{
	PLS_UI_STEP(logMoudule, "Live Term Status: PLSNaverShoppingNotice close button", ACTION_CLICK);
	this->accept();
}
