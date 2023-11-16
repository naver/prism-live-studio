#include "PLSOpenSourceView.h"
#include "ui_PLSOpenSourceView.h"
#include <QTextFrame>
#include <platform.hpp>
#include <QFile>
#include "window-basic-main.hpp"
#include "pls-gpop-data.hpp"
#include "ui-config.h"
#include "utils-api.h"
#include "ChannelCommonFunctions.h"
#include "PLSSyncServerManager.hpp"

const int TEXT_EDIT_TOP_MARGIN = 15;
const int TEXT_EDIT_LEFT_MARGIN = 15;
const int TEXT_EDIT_RIGHT_MARGIN = 10;

PLSOpenSourceView::PLSOpenSourceView(QWidget *parent) : PLSDialogView(parent)
{
	pls_add_css(this, {"PLSOpenSourceView"});
	ui = pls_new<Ui::PLSOpenSourceView>();
	setupUi(ui);
	setResizeEnabled(false);
	initSize({720, 458});
	setFixedSize({720, 458});

	loadURL(PLS_SYNC_SERVER_MANAGE->getOpenSourceLicense());
	connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(on_confirmButton_clicked()));

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSOpenSourceView::~PLSOpenSourceView()
{
	pls_delete(ui);
}

void PLSOpenSourceView::loadURL(const QString &url)
{
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url(url)
								 .initBkgColor(QColor(17, 17, 17))
								 .css("html, body { background-color: #111111; }")
								 .showAtLoadEnded(true));
	ui->verticalLayout_2->addWidget(m_browserWidget);
}

void PLSOpenSourceView::on_confirmButton_clicked()
{
	this->accept();
}
