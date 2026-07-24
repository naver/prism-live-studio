#include "ui_PLSLoginBackgroundView.h"

#include "login-background-view.hpp"
#include "login-select-platform-view.hpp"
#include "pls-common-define.hpp"
#include "prism-version.h"
#include <QDesktopServices>
#include <QMetaEnum>
#include <QTranslator>
#include <QUrl>
#include <QSpacerItem>
#include "liblog.h"
#include "log/module_names.h"
#include <qdir.h>
#include "libutils-api.h"
#include "PLSCommonFunc.h"
#include "PLSCommonConst.h"
#include <QSpacerItem>
#include "PLSBasic.h"
#include "PLSLoginMainView.h"
#include "obs-app.hpp"

using namespace common;
LoginBackgroundView::LoginBackgroundView(QStackedWidget *stackWidget, const QString &downLoadFileUrl, QWidget *parent) : QFrame(parent), m_loginStackWidget(stackWidget)
{
	ui = pls_new<Ui::LoginBackgroundView>();
	initUi(downLoadFileUrl);
}

LoginBackgroundView::~LoginBackgroundView()
{
	pls_delete(ui);
}

void LoginBackgroundView::initUi(const QString &downloadFileUrl)
{
	pls_unused(downloadFileUrl);
	ui->setupUi(this);
	m_selectLoginPlatformView = pls_new<PLSSelectLoginPlatformView>();
	m_selectLoginPlatformView->setLoginStackWidget(m_loginStackWidget);
	ui->loginMainStackedWidget->addWidget(m_selectLoginPlatformView);

	ui->horizontalLayout_3->setAlignment(Qt::AlignCenter);
	ui->horizontalLayout->setAlignment(Qt::AlignCenter);

	translateLanguage();
	ui->loginTermsOfUseBtn2->adjustSize();
	ui->loginTermsOfUseBtn->adjustSize();
	ui->loginPrivacyPolicyBtn->adjustSize();
	ui->loginPrismInfoLabel->adjustSize();
	ui->loginAndLabel->adjustSize();
	ui->loginEndLabel->adjustSize();
	ui->loginPrismLogoLabel->setMovie(&m_logoMovie);
	setConnect();
}

void LoginBackgroundView::translateLanguage()
{
	QString currentLangure(pls_get_locale());
	if (0 == currentLangure.compare("vi-VN", Qt::CaseInsensitive) || 0 == currentLangure.compare("es-ES", Qt::CaseInsensitive)) {
		auto margins = ui->horizontalLayout->contentsMargins();
		margins.setRight(0);
		ui->horizontalLayout->setContentsMargins(margins);
		dynamic_cast<QSpacerItem *>(ui->horizontalLayout->itemAt(0))->changeSize(0, 0);
	}
	if (0 == currentLangure.compare(KO_KR, Qt::CaseInsensitive)) {
		ui->loginTermsOfUseBtn->setVisible(false);
	} else if (0 == currentLangure.compare("id-ID", Qt::CaseInsensitive)) {
		ui->loginTermsOfUseBtn->setVisible(false);
		ui->loginEndLabel->setVisible(false);
	} else {
		ui->loginTermsOfUseBtn2->setVisible(false);
		ui->loginEndLabel->setVisible(false);
	}
}
void LoginBackgroundView::showEvent(QShowEvent *showEvent)
{
	QFrame::showEvent(showEvent);
	if (m_isFirstShow) {
		m_logoMovie.setFileName(":/resource/images/prism-login/login-begin/1_login_begin.png");
		m_logoMovie.setFormat("APNG");
		m_logoMovie.start();
		m_startTimer.start(2700); //first apng loading time.
		m_isFirstShow = false;
		PLS_INFO(LAUNCHER_LOGIN, "login gif start show");
	} else {
		m_logoMovie.setFileName(":/resource/images/prism-login/login-loop/2_login_loop.png");
		m_logoMovie.start();
	}
}
void LoginBackgroundView::hideEvent(QHideEvent *hideEvent)
{
	QFrame::hideEvent(hideEvent);
	m_logoMovie.setFileName("");
	m_logoMovie.stop();
}
void LoginBackgroundView::setConnect() const
{
	QObject::connect(ui->loginPrivacyPolicyBtn, &QPushButton::clicked, this, &LoginBackgroundView::onLoginPrivacyPolicyBtn);
	QObject::connect(ui->loginTermsOfUseBtn, &QPushButton::clicked, this, &LoginBackgroundView::onLoginTermsOfUseBtn);
	QObject::connect(ui->loginTermsOfUseBtn2, &QPushButton::clicked, this, &LoginBackgroundView::onLoginTermsOfUseBtn);
	QObject::connect(&m_startTimer, &QTimer::timeout, this, &LoginBackgroundView::startBeginLogo);
}

QString LoginBackgroundView::getLanguageType() const
{

	QString currentLangure(pls_get_locale());
	if (currentLangure == "ko-KR" || currentLangure == "en-US") {
		return currentLangure.toLower().replace('-', '_');
	} else {
		currentLangure = "en-US";
	}
	return currentLangure.toLower().replace('-', '_');
}

void LoginBackgroundView::onLoginPrivacyPolicyBtn() const
{
	PLS_UI_STEP(LAUNCHER_LOGIN, " LoginPrivacyPolicy Button", ACTION_CLICK);
	QString privacyPolicyUrl = QString(pls_http_api_func::getPrivacyUrl()).arg(PLSBasic::instance()->getSupportLanguage());
	pls_async_invoke([privacyPolicyUrl]() { QDesktopServices::openUrl(QUrl(privacyPolicyUrl, QUrl::TolerantMode)); });
	PLS_UI_ACTION("show privacy policy view");
}

void LoginBackgroundView::onLoginTermsOfUseBtn() const
{
	PLS_UI_STEP(LAUNCHER_LOGIN, " LoginTermsOfUse Button", ACTION_CLICK);
	QString termOfusersUrl = QString(pls_http_api_func::getTermOfUserUrl()).arg(PLSBasic::instance()->getSupportLanguage());
	pls_async_invoke([termOfusersUrl]() { QDesktopServices::openUrl(QUrl(termOfusersUrl, QUrl::TolerantMode)); });
	PLS_UI_ACTION("show term of use view");
}

void LoginBackgroundView::startBeginLogo()
{
	m_startTimer.stop();
	m_logoMovie.stop();
	PLS_INFO(LAUNCHER_LOGIN, "login gif 1 section play end ");
	m_logoMovie.setFileName(":/resource/images/prism-login/login-loop/2_login_loop.png");
	m_logoMovie.start();
	PLS_INFO(LAUNCHER_LOGIN, "login gif 2 section play start");
}
