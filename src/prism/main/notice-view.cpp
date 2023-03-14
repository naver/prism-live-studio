#include "notice-view.hpp"
#include "ui_PLSNoticeView.h"
#include "window-basic-main.hpp"
#include "log/log.h"
#include <QDesktopServices>

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

PLSNoticeView::PLSNoticeView(const QString &content, const QString &title, const QString &detailURL, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSDialogView(parent, dpiHelper), ui(new Ui::PLSNoticeView), m_content(content), m_title(title), m_detailURL(detailURL), timer(new QTimer(this))
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSNoticeView});
	initUI(dpiHelper);
	ui->topLabel->setText(m_title);
	if (m_detailURL.isEmpty()) {
		ui->learnMoreButton->hide();
	}
}

PLSNoticeView::~PLSNoticeView()
{
	delete ui;
}

void PLSNoticeView::initUI(PLSDpiHelper dpiHelper)
{
	//setup view rect and content
	ui->setupUi(this->content());
	dpiHelper.setFixedSize(this, {720, 486});
	setResizeEnabled(false);
	m_pressed = false;

	if (!cef) {
		return;
	}

	//delay the cef widge
	connect(timer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
	timer->start(100);

	//setup signals and slots
	initConnect();
}

void PLSNoticeView::initConnect()
{
	connect(ui->confirmButton, SIGNAL(clicked()), this, SLOT(on_confirmButton_clicked()));
	connect(ui->learnMoreButton, SIGNAL(clicked()), this, SLOT(on_learnMoreButton_clicked()));
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

void PLSNoticeView::onTimeOut()
{
	PLSBasic::InitBrowserPanelSafeBlock();
	if (cef) {
		m_cefWidget = cef->create_widget(nullptr, m_content.toUtf8().constData(), "", panel_cookies);
		ui->topWidget->addWidget(m_cefWidget);
		ui->topWidget->setCurrentWidget(m_cefWidget);
	}
	timer->stop();
}

bool PLSNoticeView::isInCustomControl(QWidget *child) const
{
	if (!child || child == this) {
		return false;
	} else if (child == ui->topWidget) {
		return true;
	} else if (child == m_cefWidget) {
		return true;
	} else {
		return isInCustomControl(child->parentWidget());
	}
}
