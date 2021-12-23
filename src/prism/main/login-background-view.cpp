#include "ui_PLSLoginBackgroundView.h"

#include "login-background-view.hpp"
#include "login-loading-view.hpp"
#include "login-select-platform-view.hpp"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
#include <util/config-file.h>
#include "pls-app.hpp"
#include <QDesktopServices>
#include <QMetaEnum>
#include <QTranslator>
#include <QUrl>
#include <QSpacerItem>
#include "log/log.h"
#include "win-update/update-view.hpp"
#include <qdir.h>

#define LOGOUPDATE 30
LoginBackgroundView::LoginBackgroundView(QStackedWidget *stackWidget, const QString &downLoadFileUrl, QWidget *parent)
	: QFrame(parent), ui(new Ui::LoginBackgroundView), m_loginLodaingView(nullptr), m_selectLoginPlatformView(nullptr), m_loginStackWidget(stackWidget)
{
	initUi(downLoadFileUrl);
}

LoginBackgroundView::~LoginBackgroundView()
{
	delete ui;
}

void LoginBackgroundView::initUi(const QString &downloadFileUrl)
{
	ui->setupUi(this);
	m_loginLodaingView = new LoginLoadingView();
	connect(this, &LoginBackgroundView::startCheckUpdate, m_loginLodaingView, &LoginLoadingView::checkUpdateStatus, Qt::QueuedConnection);
	m_selectLoginPlatformView = new PLSSelectLoginPlatformView;
	m_selectLoginPlatformView->setLoginStackWidget(m_loginStackWidget);
	ui->loginMainStackedWidget->addWidget(m_loginLodaingView);
	ui->loginMainStackedWidget->addWidget(m_selectLoginPlatformView);
	ui->loginMainStackedWidget->setCurrentWidget(m_loginLodaingView);
	ui->horizontalLayout_3->setAlignment(Qt::AlignCenter);
	ui->horizontalLayout->setAlignment(Qt::AlignCenter);

	translateLanguage();
	ui->loginTermsOfUseBtn2->adjustSize();
	ui->loginTermsOfUseBtn->adjustSize();
	ui->loginPrivacyPolicyBtn->adjustSize();
	ui->loginPrismInfoLabel->adjustSize();
	ui->loginAndLabel->adjustSize();
	ui->loginEndLabel->adjustSize();
	ui->loginPrismLogoLabel->setScaledContents(true);

	ui->loginPrismLogoLabel->setMovie(&m_logoMovie);
	m_logoMovie.setFileName(":/images/login-begin/1_login_begin.png");
	m_logoMovie.setFormat("APNG");
	m_logoMovie.setCacheMode(QMovie::CacheAll);

	setConnect();

	emit startCheckUpdate(downloadFileUrl);
}

void LoginBackgroundView::translateLanguage()
{

	const char *lang = static_cast<PLSApp *>(QCoreApplication::instance())->GetLocale();
	QString currentLangure(lang);
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
	m_logoMovie.start();
	m_startTimer.start(2700); //first apng loading time.
}
void LoginBackgroundView::setConnect()
{
	connect(m_loginLodaingView, &LoginLoadingView::stopPLSLoadingEvent, [=]() { ui->loginMainStackedWidget->setCurrentWidget(m_selectLoginPlatformView); });
	QObject::connect(ui->loginPrivacyPolicyBtn, &QPushButton::clicked, this, &LoginBackgroundView::onLoginPrivacyPolicyBtn);
	QObject::connect(ui->loginTermsOfUseBtn, &QPushButton::clicked, this, &LoginBackgroundView::onLoginTermsOfUseBtn);
	QObject::connect(ui->loginTermsOfUseBtn2, &QPushButton::clicked, this, &LoginBackgroundView::onLoginTermsOfUseBtn);
	QObject::connect(&m_startTimer, &QTimer::timeout, this, &LoginBackgroundView::startBeginLogo);
}

QString LoginBackgroundView::getLanguageType()
{
	const char *lang = static_cast<PLSApp *>(QCoreApplication::instance())->GetLocale();
	QString currentLangure(lang);
	if (currentLangure == "pt-BR") {
		currentLangure = "en-US";
	}
	return currentLangure.toLower().replace('-', '_');
}

void LoginBackgroundView::onLoginPrivacyPolicyBtn()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, " LoginPrivacyPolicy Button", ACTION_CLICK);
	QString privacyPolicyUrl = QString(PLS_PRIVACY_URL).arg(getLanguageType());
	QDesktopServices::openUrl(QUrl(privacyPolicyUrl));
}

void LoginBackgroundView::onLoginTermsOfUseBtn()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, " LoginTermsOfUse Button", ACTION_CLICK);
	QString termOfusersUrl = QString(PLS_TERM_OF_USE_URL).arg(getLanguageType());
	QDesktopServices::openUrl(QUrl(termOfusersUrl));
}
void LoginBackgroundView::startBeginLogo()
{
	m_startTimer.stop();
	m_logoMovie.stop();
	m_logoMovie.setFileName(":/images/login-loop/2_login_loop.png");
	m_logoMovie.start();
}
