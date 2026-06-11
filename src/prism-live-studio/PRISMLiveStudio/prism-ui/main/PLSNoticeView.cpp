#include "PLSNoticeView.hpp"
#include "ui_PLSNoticeView.h"
#include "window-basic-main.hpp"
#include "log/log.h"
#include <QDesktopServices>

PLSNoticeView::PLSNoticeView(const QString &content, const QString &title, const QString &detailURL, QWidget *parent)
	: PLSDialogView(parent), m_content(content), m_detailURL(detailURL), m_title(title)
{
	ui = pls_new<Ui::PLSNoticeView>();
	initUI();
	ui->topLabel->setText(m_title);
	if (m_detailURL.isEmpty()) {
		ui->learnMoreButton->hide();
	}
	auto closeEvent = [this](QCloseEvent *) -> bool {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSNoticeView::~PLSNoticeView()
{
	pls_delete(ui);
}

void PLSNoticeView::initUI()
{
	//setup view rect and content
	setupUi(ui);
	setResizeEnabled(false);
	pls_add_css(this, {"PLSNoticeView"});

	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url(m_content)
								 .initBkgColor(QColor(17, 17, 17))
								 .css("html, body { background-color: #111111; }")
								 .showAtLoadEnded(true));

	if (!m_content.isEmpty()) {
		ui->horizontalMiddleLayout->addWidget(m_browserWidget);
	}
#if defined(Q_OS_MACOS)
	setProperty("type", "Mac");
	setWindowTitle("");
#elif defined(Q_OS_WIN)
	setProperty("type", "Win");
#endif
}

void PLSNoticeView::on_confirmButton_clicked()
{
	PLS_UI_STEP(NOTICE_MODULE, " PLSNoticeView Confirm Button", ACTION_CLICK);
	this->accept();
}

void PLSNoticeView::on_learnMoreButton_clicked()
{
	PLS_UI_STEP(NOTICE_MODULE, " PLSNoticeView LearnMore Button", ACTION_CLICK);
	QDesktopServices::openUrl(QUrl(m_detailURL));
	this->accept();
}
