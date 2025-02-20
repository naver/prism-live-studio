#include "PLSEndTipView.h"
#include <PLSBrowserPanel.h>
#include <QTimer>
#include "log/log.h"

#include "PLSBasic.h"
#include "PLSSyncServerManager.hpp"
#include "libbrowser.h"
#include "ui_PLSEndTipView.h"
#include "window-basic-main.hpp"

extern PLSQCef *plsCef;
extern QCefCookieManager *panel_cookies;

PLSEndTipView::PLSEndTipView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSEndTipView>();

	pls_add_css(this, {"PLSEndTipView"});
	setupUi(ui);
	setResizeEnabled(false);

	PLS_INFO(END_MODULE, "PLSEndTipView Show");
	setWindowTitle(tr("TransitionInProgress.Title"));
	this->setAttribute(Qt::WA_AlwaysShowToolTips, true);
	this->setAttribute(Qt::WA_NativeWindow);

	ui->topTitle->setText(tr("nshopping.end.alert.title"));

	timer = pls_new<QTimer>(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
	timer->start(100);

	connect(ui->pushButton_ok, &QPushButton::clicked, [this]() {
		PLS_UI_STEP(END_MODULE, "PLSEndTipView ok button clicked", ACTION_CLICK);
		this->accept();
	});
}

PLSEndTipView::~PLSEndTipView()
{
	PLS_INFO(END_MODULE, "PLSEndTipView destroing");
	if (timer) {
		timer->stop();
	}
	pls_delete(ui, nullptr);
}

void PLSEndTipView::accept()
{
	PLSDialogView::accept();
}

void PLSEndTipView::done(int c)
{
	if (m_cefWidget) {
		m_cefWidget->closeBrowser();
	}
	PLSDialogView::done(c);
}

void PLSEndTipView::onTimeOut()
{
	auto &url = PLSSyncServerManager::instance()->getNoticeOnAutomaticExtractionOfProductSections();
	if (url.size() == 0) {
		PLS_INFO(END_MODULE, "PLSEndTipView url is empty");
	}

	OBSBasic::InitBrowserPanelSafeBlock();
	if (plsCef) {
		m_cefWidget = plsCef->create_widget(nullptr, url.toUtf8().constData(), "", panel_cookies, {}, false, QColor(30, 30, 30));
		ui->widget_content->layout()->addWidget(m_cefWidget);
	}
	timer->stop();
}
