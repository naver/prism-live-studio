#include "login-with-email-view.hpp"
#include "ui_PLSLoginWithEmailView.h"

#include "reset-password-email-view.hpp"
#include "pls-common-define.hpp"
#include "../../pls-common-language.hpp"
#include "login-common-helper.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include <QDebug>
#include "PLSAlertView.h"
#include <QSpacerItem>
#include "PLSCommonFunc.h"
#include "PLSCommonConst.h"
#include "PLSLoginDataHandler.h"
#include "PLSRecentLoginStore.hpp"
#include "login-select-platform-view.hpp"
#include "PLSLoginMainView.h"
#include "libutils-api.h"
#include <qdesktopservices.h>
#include "network-state.h"
#include "frontend-api.h"
#include "pls-channel-const.h"
using namespace common;
LoginWithEmailView::LoginWithEmailView(QStackedWidget *stackWidget, QWidget *parent) : QFrame(parent), m_parentStackWidget(stackWidget)
{
	ui = pls_new<Ui::LoginWithEmailView>();
	ui->setupUi(this);
	connect(ui->loginEmailAddressLineEdit, &QLineEdit::textEdited, this, &LoginWithEmailView::updateLoginBtnAvailable);
	connect(ui->loginPasswordLineEdit, &QLineEdit::textEdited, this, &LoginWithEmailView::updateLoginBtnAvailable);
	connect(ui->loginPasswordLineEdit, SIGNAL(returnPressed()), ui->loginBtn, SIGNAL(clicked()));
	connect(ui->loginEmailAddressLineEdit, SIGNAL(returnPressed()), ui->loginBtn, SIGNAL(clicked()));
	initUi();
	ui->tipIconBtn->installEventFilter(this);
	ui->label_3->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->tipIcon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

LoginWithEmailView::~LoginWithEmailView()
{
	pls_delete(ui);
}

void LoginWithEmailView::initUi()
{
	setLoginButtonStatus(false);
	ui->verticalSpacer_3->changeSize(0, 10, QSizePolicy::Preferred, QSizePolicy::Fixed);
	ui->loginEmailAddressLineEdit->clear();
	ui->loginEmailAddressLineEdit->setMaxLength(9999);
	ui->tipIconBtn->setVisible(false);
	ui->loginBtn->setEnabled(false);
	ui->loginBtn->setProperty(STATUS, STATUS_DISABLE);
	ui->loginBtn->setDefault(true);
	ui->loginPasswordLineEdit->setVisible(true);
	ui->loginPasswordLineEdit->clear();
	ui->loginForgotPasswordBtn->setVisible(true);
	ui->loginTipsLabel->setVisible(true);

	ui->label->setVisible(true);
	ui->label_2->setVisible(true);
	ui->loginSeparatorLabel->setVisible(true);
	translateLanguage();
	initBackButton();
	ui->loginListBtn->adjustSize();
	ui->loginListBtn_2->adjustSize();
	ui->loginBackTipsLabel->adjustSize();
	ui->loginSeparatorLabel->adjustSize();
	ui->loginForgotPasswordBtn->adjustSize();
	ui->loginListBtn->setContentsMargins(0, 0, 0, 0);
	ui->loginWithEmailLabel->setText(tr("login.login.with.email"));
	setWindowTitle(tr("login.login.with.email"));
	pls_uistep_v2_set_title(this, QStringLiteral("login.login.with.email"));
}

void LoginWithEmailView::setEmailStr(const QString &emailStr)
{
	ui->loginEmailAddressLineEdit->setText(emailStr);
}

void LoginWithEmailView::translateLanguage()
{
	ui->loginEmailAddressLineEdit->setPlaceholderText(tr(LOGIN_EMAIL));
	ui->loginPasswordLineEdit->setPlaceholderText(tr(LOGIN_PASSWORD));
}

void LoginWithEmailView::changeToNCB2BUi()
{
	setLoginButtonStatus(false);
	ui->verticalSpacer_3->changeSize(0, 0, QSizePolicy::Preferred, QSizePolicy::Fixed);
	ui->loginEmailAddressLineEdit->clear();
	ui->loginTipsLabel->setVisible(false);
	m_isNCB2BLogin = true;
	ui->loginWithEmailLabel->setText(LOGIN_NAVER_CLOUDB2B__VIEW);
	ui->tipIconBtn->setVisible(true);
	ui->loginPasswordLineEdit->setVisible(false);
	ui->loginForgotPasswordBtn->setVisible(false);
	ui->loginEmailAddressLineEdit->setPlaceholderText(tr("Login.Ncb2b.Service.Name.Placehode"));
	ui->loginEmailAddressLineEdit->setMaxLength(20);
	ui->label->setVisible(false);
	ui->label_2->setVisible(false);
	ui->loginSeparatorLabel->setVisible(false);
	QFontMetrics fontWidth(ui->label_3->font());
	auto availableWidth = fontWidth.horizontalAdvance(ui->label_3->text()) + 4 + ui->tipIcon->width();
	ui->tipIconBtn->setFixedWidth(availableWidth);
	setWindowTitle(LOGIN_NAVER_CLOUDB2B__VIEW);
	pls_uistep_v2_set_title(this, LOGIN_NAVER_CLOUDB2B__VIEW);
}

void LoginWithEmailView::setLoginButtonStatus(bool isOk)
{
	ui->loginBtn->blockSignals(isOk);
}

bool LoginWithEmailView::eventFilter(QObject *watch, QEvent *e)
{
	if (watch == ui->tipIconBtn) {
		if (e->type() == QEvent::Enter) {
			ui->label_3->setProperty("status", "hover");
			ui->tipIcon->setProperty("status", "hover");
		} else if (e->type() == QEvent::Leave) {
			ui->label_3->setProperty("status", "normal");
			ui->tipIcon->setProperty("status", "normal");
		}
		pls_flush_style(ui->label_3);
		pls_flush_style(ui->tipIcon);
	}
	return QFrame::eventFilter(watch, e);
}

void LoginWithEmailView::initBackButton()
{
	QString currentLangure(pls_get_locale());
	if (0 == currentLangure.compare(KO_KR, Qt::CaseInsensitive)) {
		ui->loginListBtn->setVisible(false);
		ui->label->setVisible(false);
	} else {
		ui->loginListBtn_2->setVisible(false);
		ui->label_2->setVisible(false);
	}
	pls_flush_style(ui->loginEmailAddressLineEdit);
}

void LoginWithEmailView::loginRequest()
{
	QUrl url(pls_http_api_func::getPrismAuthGateWay() + pls_launcher_const::EMAIL_LOGIN_URL);
	QJsonObject userLoginInfoMap;
	userLoginInfoMap.insert(LOGIN_USERINFO_EMAIL, ui->loginEmailAddressLineEdit->text());
	userLoginInfoMap.insert(LOGIN_USERINFO_PASSWORD, ui->loginPasswordLineEdit->text());

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .rawHeaders(PLSLoginDataHandler::instance()->getRequestApiDefaultHeader())
				   .body(userLoginInfoMap)
				   .jsonContentType()                              //
				   .withLog()                                      //
				   .receiver({this, PLSLoginMainView::instance()}) //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey())
				   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
				   .jsonOkResult([this](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   PLS_INFO(PLS_LOGIN_MODULE, "login with eamil success");
					   auto cookidByteArray = reply.header(QNetworkRequest::SetCookieHeader);
					   auto userInfo = doc.object();
					   pls_async_call_mt(m_parentStackWidget, [this, userInfo, cookidByteArray]() {
						   PLSLoginDataHandler::instance()->savePrismUserInfo(userInfo, cookidByteArray, false, "email", PLSRecentLoginStore::KindEmail);
						   LoginCommonHelpers::loginResultHandler(m_parentStackWidget, true);
					   });
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "login with eamil failed");
					   PLSErrorHandler::NetworkData data;
					   data.errData = reply.data();
					   data.netError = reply.error();
					   data.statusCode = reply.statusCode();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
					   extraData.errPhase = PLSErrPhaseLogin;
					   pls_async_call_mt(this, [data, extraData, this]() {
						   responseErrorHandler(data, extraData);
					   });
				   }));
}

void LoginWithEmailView::clickEmailLogin()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, " email login Button", ACTION_CLICK);
	LoginCommonHelpers::sanitizeEmailLineEdit(ui->loginEmailAddressLineEdit);
	if (!LoginCommonHelpers::isValidEmailByRegExp(ui->loginEmailAddressLineEdit->text())) {
		ui->loginTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_EMAIL));
	} else if (ui->loginPasswordLineEdit->text().isEmpty()) {
		return;
	} else {
		ui->loginTipsLabel->setText(QString());
		loginRequest();
		ui->loginBtn->blockSignals(true);
	}
}

void LoginWithEmailView::clickNCB2BLogin()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, " get B2B service ID Button", ACTION_CLICK);
	if (!pls::NetworkState::instance()->isAvailable()) {

		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_CHECK_NOTE_NETWORK, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("LoginWithEmailView::clickNCB2BLogin"));
		return;
	}

	QRegularExpression rep("^[\uAC00-\uD7AFa-zA-Z0-9]{3,20}$");
	auto text = ui->loginEmailAddressLineEdit->text();
	auto trimmedtext = text.trimmed();
	if ((text != trimmedtext) || !rep.match(ui->loginEmailAddressLineEdit->text()).hasMatch()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_NCB2B_SERVICE_NOT_FOUND, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("LoginWithEmailView::clickNCB2BLogin"));
		return;
	}

	PLSLoginDataHandler::instance()->getNCPServiceId(
		ui->loginEmailAddressLineEdit->text(), [this](const QString &url) { emit getNCB2BAuthUrl(url); },
		[this](int statusCode, int code) {
			pls_unused(statusCode, code);
			pls_check_app_exiting();
			ui->loginBtn->blockSignals(false);
		},
		NCB2B);
	ui->loginBtn->blockSignals(true);
}

void LoginWithEmailView::emailLoginBtnCheck()
{
	if (ui->loginPasswordLineEdit->text().isEmpty() || ui->loginEmailAddressLineEdit->text().isEmpty()) {
		ui->loginBtn->setProperty(STATUS, STATUS_DISABLE);
		ui->loginBtn->setEnabled(false);
	} else {
		ui->loginBtn->setProperty(STATUS, STATUS_ENABLE);
		ui->loginBtn->setEnabled(true);
	}
}

void LoginWithEmailView::ncB2BLoginBtnCheck()
{
	if (ui->loginEmailAddressLineEdit->text().isEmpty()) {
		ui->loginBtn->setProperty(STATUS, STATUS_DISABLE);
		ui->loginBtn->setEnabled(false);
	} else {
		ui->loginBtn->setProperty(STATUS, STATUS_ENABLE);
		ui->loginBtn->setEnabled(true);
	}
}

void LoginWithEmailView::responseErrorHandler(const PLSErrorHandler::NetworkData &netData, const PLSErrorHandler::ExtraData &extraData)
{
	PLSErrorHandler::showAlert(netData, "Email", "PRISMLoginFailedAgain", extraData);
	ui->loginBtn->blockSignals(false);
}

void LoginWithEmailView::on_loginListBtn_clicked()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, " go back loginList Button", ACTION_CLICK);
	LoginCommonHelpers::setCurrentStackWidget(m_parentStackWidget, LOGIN_BACKGROUND_VIEW);
	PLSLoginMainView::instance()->setWindowTitle(tr("login.login_cap"));
	m_isNCB2BLogin = false;
	PLS_UI_ACTION("show sns login view");
}

void LoginWithEmailView::on_loginListBtn_2_clicked()
{
	on_loginListBtn_clicked();
}

void LoginWithEmailView::on_loginForgotPasswordBtn_clicked() const
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, " reset password with email  Button", ACTION_CLICK);
	PLSResetPasswordEmailView resetPasswordView;

	resetPasswordView.exec();
}

void LoginWithEmailView::on_loginBtn_clicked()
{
	if (m_isNCB2BLogin) {
		clickNCB2BLogin();
	} else {
		clickEmailLogin();
	}
}

void LoginWithEmailView::updateLoginBtnAvailable(const QString &)
{
	if (m_isNCB2BLogin) {
		ncB2BLoginBtnCheck();
	} else {
		emailLoginBtnCheck();
	}

	pls_flush_style(ui->loginBtn);
}

void LoginWithEmailView::on_tipIconBtn_clicked()
{
	pls_async_invoke([]() { QDesktopServices::openUrl(QStringLiteral("https://guide.prismlive.com/desktop/announcement/general/naver-cloud-b2b-product")); });
	PLS_UI_ACTION("Show B2B Product Guide Web View");
}
