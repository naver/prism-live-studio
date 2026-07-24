#include "login-background-view.hpp"
#include "login-select-platform-view.hpp"
#include "signup-with-email-view.hpp"
#include "login-terms-of-agree-view.hpp"

#include "reset-password-email-view.hpp"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "login-common-helper.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSAlertView.h"
#include "PLSCommonFunc.h"
#include "login-select-platform-view.hpp"
#include "PLSLoginDataHandler.h"
#include "PLSRecentLoginStore.hpp"
#include "PLSLoginMainView.h"
#include "login-with-email-view.hpp"
#include "PLSApp.h"

using namespace common;
constexpr auto MAXLENGTH = 20;

SignupWithEmailView::SignupWithEmailView(QStackedWidget *stackWidget, QWidget *parent) : QFrame(parent), m_loginStackFrame(stackWidget)
{
	ui = pls_new<Ui::SignupWithEmailView>();
	initUi();
}

SignupWithEmailView::~SignupWithEmailView()
{
	pls_delete(ui);
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

	auto snsLoginInfo = PLSSelectLoginPlatformView::getLoginParamInfo();
	for (auto loginParam : snsLoginInfo) {
		auto obj = loginParam.second.first;
		obj.insert(LOGIN_DEFAULT_ICON_SIZE, 36);
		QPushButton *button = PLSSelectLoginPlatformView::creatSnsLoginBtn(loginParam.first, obj, true);
		ui->horizontalLayout->addWidget(button);
		if (QWidget *w = findSnsLoginView(m_loginStackFrame)) {
			static_cast<PLSSelectLoginPlatformView *>(w)->snsLoginHandler(loginParam.second.second, button);
		}
	}

	if (pls_get_gcc() != common::HTTP_GCC_KR) {
		auto w1 = ui->horizontalLayout->itemAt(static_cast<int>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::NAVER) - 1)->widget();
		auto w2 = ui->horizontalLayout->itemAt(static_cast<int>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::LINE) - 1)->widget();
		QWidget placeholder;
		ui->horizontalLayout->replaceWidget(w1, &placeholder);
		ui->horizontalLayout->replaceWidget(w2, w1);
		ui->horizontalLayout->replaceWidget(&placeholder, w2);
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
	QString currentLangure(pls_get_locale());
	if (0 == currentLangure.compare(KO_KR, Qt::CaseInsensitive)) {
		ui->signLoginBtn->setVisible(false);
	} else {
		ui->signLoginBtn_2->setVisible(false);
	}
}

void SignupWithEmailView::setConnect() const
{
	connect(ui->signEmailAddressLineEdit, &QLineEdit::textEdited, this, &SignupWithEmailView::updateCreateNewAccountBtnAvailable);
	connect(ui->signUsernameLineEdit, &QLineEdit::textEdited, this, &SignupWithEmailView::updateCreateNewAccountBtnAvailable);
	connect(ui->lineEdit, &QLineEdit::textEdited, this, &SignupWithEmailView::updateCreateNewAccountBtnAvailable);
	connect(ui->lineEdit, SIGNAL(returnPressed()), ui->createNewAccountBtn, SIGNAL(clicked()));
	connect(ui->signUsernameLineEdit, SIGNAL(returnPressed()), ui->createNewAccountBtn, SIGNAL(clicked()));
	connect(ui->signEmailAddressLineEdit, SIGNAL(returnPressed()), ui->createNewAccountBtn, SIGNAL(clicked()));
}

void SignupWithEmailView::on_signLoginBtn_clicked()
{
	PLS_UI_STEP(LAUNCHER_LOGIN, " go back snsLogin Button", ACTION_CLICK);

	LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_BACKGROUND_VIEW);
#if defined(Q_OS_MACOS)
	PLSLoginMainView::instance()->setWindowTitle(tr("login.login_cap"));
#endif
	PLS_UI_ACTION("show sns login view");
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
	pls_flush_style(ui->createNewAccountBtn);
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
	PLS_UI_STEP(LAUNCHER_LOGIN, " create new account button", ACTION_CLICK);

	LoginCommonHelpers::sanitizeEmailLineEdit(ui->signEmailAddressLineEdit);
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
	QUrl url(pls_http_api_func::getPrismAuthGateWay() + pls_launcher_const::EMAIL_SIGNUP_URL);
	QJsonObject userSignupInfoMap;
	userSignupInfoMap.insert(LOGIN_USERINFO_EMAIL, ui->signEmailAddressLineEdit->text());
	userSignupInfoMap.insert(LOGIN_USERINFO_PASSWORD, ui->lineEdit->text());
	userSignupInfoMap.insert(LOGIN_USERINFO_NICKNAME, ui->signUsernameLineEdit->text());
	userSignupInfoMap.insert(LOGIN_USERINFO_AGREEMENT, isAgree);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .rawHeaders(PLSLoginDataHandler::instance()->getRequestApiDefaultHeader())
				   .body(userSignupInfoMap)
				   .jsonContentType() //
				   .withLog()         //
				   .receiver(qApp)    //
				   .workInNewThread()
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey())
				   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
				   .jsonOkResult([this](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   auto cookidByteArray = reply.header(QNetworkRequest::SetCookieHeader);
					   pls_async_call_mt(this, [this, cookidByteArray, doc]() {
						   PLS_INFO(PLS_LOGIN_MODULE, "signup with eamil success");
						   PLSLoginDataHandler::instance()->savePrismUserInfo(doc.object(), cookidByteArray, true, "sign email",
												 PLSRecentLoginStore::KindEmail);
						   LoginCommonHelpers::loginResultHandler(m_loginStackFrame, true);
					   });
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto resultObj = reply.object();
					   PLSErrorHandler::NetworkData data;
					   data.errData = reply.data();
					   data.netError = reply.error();
					   data.statusCode = reply.statusCode();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
					   extraData.errPhase = PLSErrPhaseLogin;

					   pls_async_call_mt(this, [this, data, extraData, resultObj]() {
						   PLS_INFO(LAUNCHER_LOGIN, "prism sign failed");
						   auto code = resultObj.value(LOGIN_CODE).toInt();
						   if (SIGNUP_NO_AGREE == code) {
							   showTermOfAgreeView();
						   } else {
							   auto retData = PLSErrorHandler::showAlert(data, "Email", "", extraData);

							   if (retData.prismCode == PLSErrorHandler::ErrCode::PRISM_LOGIN_EMAIL_SIGN_USER_EXIST) {
								   if (PLSAlertView::Button::Ok == retData.clickedBtn) {
									   ui->lineEdit->clear();
									   ui->signEmailAddressLineEdit->clear();
									   ui->signUsernameLineEdit->clear();
									   auto w = LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
									   if (LoginWithEmailView *widget = nullptr; widget = dynamic_cast<LoginWithEmailView *>(w)) {
										   widget->initUi();
									   }
								   } else {
									   clearInputInfo();
								   }
							   }
						   }
						   ui->createNewAccountBtn->blockSignals(false);
					   });
				   }));
}

void SignupWithEmailView::showTermOfAgreeView()
{
	if (PLSTermsOfAgreeView::showTermsDialog()) {
		signupRequest(true);
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

QWidget *SignupWithEmailView::findSnsLoginView(const QStackedWidget *stackWidget)
{
	if (stackWidget) {
		QWidget *backgroundView = stackWidget->findChild<QWidget *>(LOGIN_BACKGROUND_VIEW);
		if (backgroundView) {
			return static_cast<LoginBackgroundView *>(backgroundView)->findChild<QWidget *>(LOGIN_SNS_VIEW);
		}
	}
	return nullptr;
}
