#include "login-background-view.hpp"
#include "login-select-platform-view.hpp"
#include "signup-with-email-view.hpp"
#include "login-terms-of-agree-view.hpp"

#include "reset-password-email-view.hpp"
#include "json-data-handler.hpp"
#include "pls-net-url.hpp"
#include "network-access-manager.hpp"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "login-user-info.hpp"
#include "login-common-helper.hpp"
#include "log/log.h"
#include "pls-common-language.hpp"
#include "alert-view.hpp"
#include "pls-app.hpp"

#define MAXLENGTH 20

SignupWithEmailView::SignupWithEmailView(QStackedWidget *stackWidget, QWidget *parent)
	: QFrame(parent), ui(new Ui::SignupWithEmailView), m_loginStackFrame(stackWidget), m_networkAccessManager(PLSNetworkAccessManager::getInstance())
{
	initUi();
}

SignupWithEmailView::~SignupWithEmailView()
{
	disconnect(m_networkAccessManager, &PLSNetworkAccessManager::replyResultData, this, &SignupWithEmailView::replyResultDataHandler);
	disconnect(m_networkAccessManager, &PLSNetworkAccessManager::replyErrorDataWithSatusCode, this, &SignupWithEmailView::replyErrorHandler);
	delete ui;
}

void SignupWithEmailView::translateLanguage()
{
	ui->signEmailAddressLineEdit->setPlaceholderText(tr(LOGIN_EMAIL));
	ui->signUsernameLineEdit->setPlaceholderText(tr(LOGIN_NICKNAME));
	ui->lineEdit->setPlaceholderText(tr(LOGIN_PASSWORD));
}

void SignupWithEmailView::initUi()
{
	ui->setupUi(this);
	ui->createNewAccountBtn->setEnabled(false);
	ui->createNewAccountBtn->setProperty(STATUS, STATUS_DISABLE);
	//g_prismOrderList init in module login-select-platform;
	extern QStringList g_prismOrderList;
	int count = pls_get_login_info_count();

	for (auto orderStr : g_prismOrderList) {
		PLSLoginInfo *loginInfo = nullptr;
		for (int index = 0; index != count; ++index) {
			loginInfo = pls_get_login_info(index);
			if (loginInfo->name().toLower().contains(orderStr)) {
				break;
			}
		}
		QPushButton *button = PLSSelectLoginPlatformView::creatSnsLoginBtn(loginInfo, true);
		ui->horizontalLayout->addWidget(button);
		if (QWidget *w = findSnsLoginView(m_loginStackFrame)) {
			static_cast<PLSSelectLoginPlatformView *>(w)->snsLoginHandler(loginInfo, button);
		}
	}

	initBackButton();
	setConnect();
	translateLanguage();

	ui->signLoginBtn->adjustSize();
	ui->signLoginBtn_2->adjustSize();
	ui->signBackToLabel->adjustSize();
}

void SignupWithEmailView::clearView()
{
	ui->signUsernameLineEdit->clear();
	ui->signEmailAddressLineEdit->clear();
	ui->lineEdit->clear();
	ui->signTipsLabel->clear();
}

void SignupWithEmailView::initBackButton()
{
	const char *lang = static_cast<PLSApp *>(QCoreApplication::instance())->GetLocale();
	QString currentLangure(lang);
	if (0 == currentLangure.compare(KO_KR, Qt::CaseInsensitive)) {
		ui->signLoginBtn->setVisible(false);
	} else {
		ui->signLoginBtn_2->setVisible(false);
	}
}

void SignupWithEmailView::setConnect()
{
	connect(ui->signEmailAddressLineEdit, &QLineEdit::textEdited, this, &SignupWithEmailView::updateCreateNewAccountBtnAvailable);
	connect(ui->signUsernameLineEdit, &QLineEdit::textEdited, this, &SignupWithEmailView::updateCreateNewAccountBtnAvailable);
	connect(ui->lineEdit, &QLineEdit::textEdited, this, &SignupWithEmailView::updateCreateNewAccountBtnAvailable);
	connect(m_networkAccessManager, &PLSNetworkAccessManager::replyResultData, this, &SignupWithEmailView::replyResultDataHandler);
	connect(m_networkAccessManager, &PLSNetworkAccessManager::replyErrorDataWithSatusCode, this, &SignupWithEmailView::replyErrorHandler);
	connect(ui->lineEdit, SIGNAL(returnPressed()), ui->createNewAccountBtn, SIGNAL(clicked()));
	connect(ui->signUsernameLineEdit, SIGNAL(returnPressed()), ui->createNewAccountBtn, SIGNAL(clicked()));
	connect(ui->signEmailAddressLineEdit, SIGNAL(returnPressed()), ui->createNewAccountBtn, SIGNAL(clicked()));
}

void SignupWithEmailView::on_signLoginBtn_clicked()
{
	PLS_INIT_UI_STEP(PLS_LOGIN_MODULE, " go back snsLogin Button", ACTION_CLICK);

	LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_BACKGROUND_VIEW);
}

void SignupWithEmailView::on_signLoginBtn_2_clicked()
{
	on_signLoginBtn_clicked();
}

void SignupWithEmailView::updateCreateNewAccountBtnAvailable(const QString &)
{
	if (ui->signEmailAddressLineEdit->text().isEmpty() || ui->signUsernameLineEdit->text().isEmpty() || ui->lineEdit->text().isEmpty()) {
		ui->createNewAccountBtn->setProperty(STATUS, STATUS_DISABLE);
		ui->createNewAccountBtn->setEnabled(false);
	} else {
		ui->createNewAccountBtn->setProperty(STATUS, STATUS_ENABLE);
		ui->createNewAccountBtn->setEnabled(true);
	}
	LoginCommonHelpers::refreshStyle(ui->createNewAccountBtn);
	if (ui->signUsernameLineEdit->text().length() > MAXLENGTH) {
		ui->signUsernameLineEdit->setText(ui->signUsernameLineEdit->text().left(MAXLENGTH));
		ui->signTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_NICKNAME));
	} else if (ui->lineEdit->text().length() > MAXLENGTH) {
		ui->lineEdit->setText(ui->lineEdit->text().left(MAXLENGTH));
		ui->signTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_PASSWORD));
	} else {
		ui->signTipsLabel->setText(QString());
	}
}

void SignupWithEmailView::on_createNewAccountBtn_clicked()
{
	PLS_INIT_UI_STEP(PLS_LOGIN_MODULE, " create new account button", ACTION_CLICK);

	if (!LoginCommonHelpers::isValidEmailByRegExp(ui->signEmailAddressLineEdit->text())) {
		ui->signTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_EMAIL));
	} else if (!LoginCommonHelpers::isValidPasswordByRegExp(ui->lineEdit->text())) {
		ui->signTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_PASSWORD));
	} else {
		ui->signTipsLabel->setText(QString());
		signupRequest(true);
		ui->createNewAccountBtn->blockSignals(true);
	}
}

void SignupWithEmailView::signupRequest(bool isAgree)
{
	if (m_networkAccessManager) {
		QVariantMap userSignupInfoMap;
		userSignupInfoMap.insert(LOGIN_USERINFO_EMAIL, ui->signEmailAddressLineEdit->text());
		userSignupInfoMap.insert(LOGIN_USERINFO_PASSWORD, ui->lineEdit->text());
		userSignupInfoMap.insert(LOGIN_USERINFO_NICKNAME, ui->signUsernameLineEdit->text());
		userSignupInfoMap.insert(LOGIN_USERINFO_AGREEMENT, isAgree);

		QVariantMap headInfoMap;
		headInfoMap.insert(HTTP_HEAD_CONTENT_TYPE, HTTP_CONTENT_TYPE_VALUE);
		headInfoMap.insert(HTTP_HEAD_CC_TYPE, pls_get_gcc_data());
		m_networkAccessManager->createHttpRequest(PLSNetworkAccessManager::PostOperation, PLS_EMAIL_SIGNUP_URL.arg(PRISM_SSL), true, headInfoMap, userSignupInfoMap);
	}
}

void SignupWithEmailView::responseErrorHandler(const QByteArray &array)
{
	QVariant code;
	PLSJsonDataHandler::getValueFromByteArray(array, LOGIN_CODE, code);

	if (SIGNUP_NO_AGREE == code.toInt()) {
		showTermOfAgreeView();
	} else if (JOIN_EXIST_ID == code.toInt()) {
		exitEmailHandler();
	} else if (JOIN_SESSION_EXPIRED == code.toInt()) {
		// TODO::add popup
		qDebug() << "JOIN_SESSION_EXPIRED";
	} else if (INTERNAL_ERROR == code.toInt()) {
		// TODO::add popup
		qDebug() << "JOIN_FAILED";
	}
}

void SignupWithEmailView::showTermOfAgreeView()
{
	PLSTermsOfAgreeView view;
	if (QDialog::Accepted == view.exec()) {
		signupRequest(true);
	}
}

void SignupWithEmailView::exitEmailHandler()
{
	PLS_INIT_INFO(PLS_LOGIN_MODULE, tr(LOGIN_PRISM_ALREADY_SIGN_UP).toUtf8().data());

	if (PLSAlertView::Button::Ok ==
	    PLSAlertView::information(pls_get_toplevel_view(this), tr("Confirm"), tr(LOGIN_PRISM_ALREADY_SIGN_UP), PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel, PLSAlertView::Button::Ok)) {
		LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
	} else {
		clearInputInfo();
	}
}

void SignupWithEmailView::clearInputInfo()
{
	ui->lineEdit->clear();
	ui->signEmailAddressLineEdit->clear();
	ui->signUsernameLineEdit->clear();
	updateCreateNewAccountBtnAvailable(QString());
	ui->signEmailAddressLineEdit->setFocus();
}

QWidget *SignupWithEmailView::findSnsLoginView(QStackedWidget *stackWidget)
{
	if (stackWidget) {
		QWidget *backgroundView = stackWidget->findChild<QWidget *>(LOGIN_BACKGROUND_VIEW);
		if (backgroundView) {
			return static_cast<LoginBackgroundView *>(backgroundView)->findChild<QWidget *>(LOGIN_SNS_VIEW);
		}
	}
	return nullptr;
}

void SignupWithEmailView::replyResultDataHandler(int statusCode, const QString &url, const QByteArray array)
{
	if (0 == QString::fromUtf8(PLS_EMAIL_SIGNUP_URL.arg(PRISM_SSL).toUtf8().data()).compare(url)) {
		if (HTTP_STATUS_CODE_202 == statusCode || HTTP_STATUS_CODE_500 == statusCode) {
			// TODO: error
			responseErrorHandler(array);
		} else if (HTTP_STATUS_CODE_200 == statusCode) {
			PLSLoginUserInfo::getInstance()->setUserLoginInfo(array);
			PLSLoginUserInfo::getInstance()->setPrismCookieConfigInfo(m_networkAccessManager->getCookieForUrl(url));
			PLS_INIT_INFO(PLS_LOGIN_MODULE, QString("signup with eamil success").toUtf8().data());

			// TODO sign up success loading main window
			LoginCommonHelpers::loginResultHandler(m_loginStackFrame, true);
		}
	}
	ui->createNewAccountBtn->blockSignals(false);
}

void SignupWithEmailView::replyErrorHandler(int statusCode, const QString &url, const QString &body, const QString &errorInfo)
{
	// TODO msg:login.unknown.error
	Q_UNUSED(url)
	if (0 == QString::fromUtf8(PLS_EMAIL_SIGNUP_URL.arg(PRISM_SSL).toUtf8().data()).compare(url)) {
		QVariant code;
		PLSJsonDataHandler::getValueFromByteArray(body.toUtf8(), LOGIN_CODE, code);
		if (0 == statusCode && !errorInfo.isEmpty()) {
			PLSAlertView::warning(pls_get_toplevel_view(this), tr("Alert.Title"), tr("login.check.note.network"));
		} else if (statusCode == HTTP_STATUS_CODE_500 && -1 == code.toInt()) {
			PLSAlertView::warning(pls_get_toplevel_view(this), tr("Alert.Title"), tr(LOGIN_SERVER_UNKNOWN_ERROR));
		} else if (HTTP_STATUS_CODE_400 == statusCode && 2400 == code.toInt()) {
			PLSAlertView::warning(pls_get_toplevel_view(this), tr("Alert.Title"), tr(LOGIN_PRISM_ERROR_INVALID_PASSWORD));
		} else {
			PLSAlertView::warning(pls_get_toplevel_view(this), tr("Alert.Title"), tr(LOGIN_SERVER_UNKNOWN_ERROR));
		}
	}
	ui->createNewAccountBtn->blockSignals(false);
}
