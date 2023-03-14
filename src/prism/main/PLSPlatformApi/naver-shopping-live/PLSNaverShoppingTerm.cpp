#include "PLSNaverShoppingTerm.h"
#include "ui_PLSNaverShoppingTerm.h"
#include <QTextFrame>
#include "window-basic-main.hpp"
#include <QtGui/qdesktopservices.h>

#define TEXT_EDIT_TOP_MARGIN 15
#define TEXT_EDIT_LEFT_MARGIN 15
#define TEXT_EDIT_RIGHT_MARGIN 10

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

PLSNaverShoppingTerm::PLSNaverShoppingTerm(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), ui(new Ui::PLSNaverShoppingTerm)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSNaverShoppingTerm});
	dpiHelper.setFixedSize(this, {720, 488});
	ui->setupUi(this->content());
	QMetaObject::connectSlotsByName(this);
	QSizePolicy sizePolicy = ui->closeButton->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	ui->closeButton->setSizePolicy(sizePolicy);
	setResizeEnabled(false);
}

PLSNaverShoppingTerm::~PLSNaverShoppingTerm()
{
	timer->stop();
	delete ui;
}

void PLSNaverShoppingTerm::setURL(const QString &url)
{
	//update url
	m_updateInfoUrl = url;

	//delay the cef widge
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
	timer->start(100);
}

void PLSNaverShoppingTerm::setMoreUrl(const QString &url)
{
	m_viewAllUrl = url;
}

void PLSNaverShoppingTerm::setMoreLabelTitle(const QString &title)
{
	ui->moreTitleLabel->setText(title);
}

void PLSNaverShoppingTerm::setMoreButtonHidden(bool hidden)
{
	ui->moreButton->setHidden(hidden);
}

void PLSNaverShoppingTerm::setCancelButtonHidden(bool hidden)
{
	ui->closeButton->setHidden(hidden);
}

void PLSNaverShoppingTerm::setOKButtonTitle(const QString &title)
{
	ui->confirmButton->setText(title);
}

void PLSNaverShoppingTerm::on_closeButton_clicked()
{
	this->reject();
}

void PLSNaverShoppingTerm::on_confirmButton_clicked()
{
	this->accept();
}

void PLSNaverShoppingTerm::on_moreButton_clicked()
{
	QDesktopServices::openUrl(m_viewAllUrl);
}

void PLSNaverShoppingTerm::onTimeOut()
{
	PLSBasic::InitBrowserPanelSafeBlock();
	if (cef) {
		m_cefWidget = cef->create_widget(nullptr, m_updateInfoUrl.toUtf8().constData(), "", panel_cookies);
		ui->cefBackView->addWidget(m_cefWidget);
		ui->cefBackView->setCurrentWidget(m_cefWidget);
	}
	timer->stop();
}
