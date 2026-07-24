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

	QUrl openSourceURL = QUrl::fromLocalFile(PLS_SYNC_SERVER_MANAGE->getOpenSourceLicense());
	loadURL(openSourceURL.toString());
	connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(on_confirmButton_clicked()));

#if defined(Q_OS_WIN)
	ui->verticalLayout->removeWidget(ui->topTitle);
	setTitleWidget(ui->topTitle);
#endif

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
								 .css("html, body { "
								      "  background-color: #111111; "
								      "  margin: 0; padding: 0; "
								      "  max-width: 100%; "
								      "  overflow-x: hidden; "
								      "  box-sizing: border-box; "
								      "} "

								      "body, * { "
								      "  word-wrap: break-word; "
								      "  overflow-wrap: anywhere; "
								      "  word-break: break-word; "
								      "} "

								      "pre, code, pre * { "
								      "  white-space: pre-wrap; "
								      "  word-break: break-word; "
								      "  overflow-wrap: anywhere; "
								      "  max-width: 100%; "
								      "} "

								      "table, img, svg, canvas, video { "
								      "  max-width: 100%; "
								      "  height: auto; "
								      "  box-sizing: border-box; "
								      "} "

								      ":root { "
								      "  width: 100%; "
								      "}")
								 .showAtLoadEnded(true)
								 .allowPopups(false));
	ui->verticalLayout_2->addWidget(m_browserWidget);
}

void PLSOpenSourceView::on_confirmButton_clicked()
{
	this->accept();
}
