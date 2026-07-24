#include "reset-password-email-view.hpp"
#include "ui_PLSResetPasswordEmailView.h"
#include <QMouseEvent>
#include <QNetworkRequest>

#include "reset-password-email-view.hpp"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "login-common-helper.hpp"
#include "liblog.h"
#include "log/module_names.h"

#include "PLSCommonFunc.h"
#include "PLSAlertView.h"
#include "PLSCommonConst.h"
#include "PLSCommonFunc.h"
#include "PLSLoginDataHandler.h"
#include "login-select-platform-view.hpp"
#include <QUrlQuery>
#include "libhttp-client.h"

using namespace common;
PLSResetPasswordEmailView::PLSResetPasswordEmailView(QWidget *parent) : PLSDialogView(parent)
{
	pls_uistep_v2_set_title(this, pls_uistep_v2_get_english("login.forgot.password"));

	ui = pls_new<Ui::PLSResetPasswordEmailView>();
	pls_set_css(this, {"PrismPasswordView"});
	setupUi(ui);
	setWindowTitle(QString());
	setResizeEnabled(false);

	ui->okBtn->setEnabled(false);
	ui->okBtn->setProperty(STATUS, STATUS_DISABLE);
	ui->emailAddressLineEdit->setPlaceholderText(tr(LOGIN_EMAIL));
	ui->resetPasswordTipsLabel->adjustSize();

	connect(ui->emailAddressLineEdit, &QLineEdit::textEdited, this, &PLSResetPasswordEmailView::emailEditTextChange);
	addMacTopMargin();
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("login.forgot.password"));
#endif
}

void PLSResetPasswordEmailView::emailEditTextChange(const QString &text)
{
	if (text.isEmpty()) {
		ui->okBtn->setEnabled(false);
		ui->okBtn->setProperty(STATUS, STATUS_DISABLE);
		ui->resetPasswordTipsLabel->setText(tr("login.forgot.password.tips"));
		ui->resetPasswordTipsLabel->setProperty("status", true);
	} else {
		ui->okBtn->setEnabled(true);
		ui->okBtn->setProperty(STATUS, STATUS_ENABLE);
	}

	pls_flush_style(ui->okBtn);
	pls_flush_style(ui->resetPasswordTipsLabel);
}

void PLSResetPasswordEmailView::loginRequest()
{
	QUrl url(pls_http_api_func::getPrismAuthGateWay() + pls_launcher_const::EMAIL_FOGETTON_URL);
	QUrlQuery query;
	query.addQueryItem(LOGIN_USERINFO_EMAIL, ui->emailAddressLineEdit->text());
	url.setQuery(query);
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .rawHeaders(PLSLoginDataHandler::instance()->getRequestApiDefaultHeader())
				   .jsonContentType()                                                                          //
				   .withLog(pls_http_api_func::getPrismAuthGateWay() + pls_launcher_const::EMAIL_FOGETTON_URL) //
				   .receiver(this)                                                                             //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey())
				   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
				   .jsonOkResult([this](const pls::http::Reply &reply, const QJsonDocument &) {
					   auto cookie = reply.header(QNetworkRequest::SetCookieHeader);
					   pls_async_call_mt(this, [this, cookie]() {
						   PLSLoginUserInfo::getInstance()->setPrismCookie(cookie);
						   PLS_INFO(PLS_LOGIN_MODULE, "reset password with email success");
						   PLS_INFO(PLS_LOGIN_MODULE, QString("reset password success please check email.").toUtf8().data());
						   PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_RESET_PASSWORD_LINK_SENT, PLSErrKeyAllAlert, {},
										 PLSErrorHandler::ExtraData("PLSResetPasswordEmailView::loginRequest"), this);
						   accept();
					   });
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "reset password with email failed");
					   PLSErrorHandler::NetworkData data;
					   data.errData = reply.data();
					   data.netError = reply.error();
					   data.statusCode = reply.statusCode();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
					   extraData.errPhase = PLSErrPhaseLogin;

					   pls_async_call_mt(this, [this, data, extraData] {
						   PLSErrorHandler::showAlert(data, "Email", "", extraData);
						   ui->okBtn->blockSignals(false);
					   });
				   }));
}

void PLSResetPasswordEmailView::on_okBtn_clicked()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, "resetPasswordEmail OKButton", ACTION_CLICK);

	LoginCommonHelpers::sanitizeEmailLineEdit(ui->emailAddressLineEdit);
	if (!LoginCommonHelpers::isValidEmailByRegExp(ui->emailAddressLineEdit->text())) {
		ui->resetPasswordTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_EMAIL));
		ui->resetPasswordTipsLabel->setProperty("status", false);
	} else {
		ui->resetPasswordTipsLabel->setText(tr("login.forgot.password.tips"));
		ui->resetPasswordTipsLabel->setProperty("status", true);
		loginRequest();
		ui->okBtn->blockSignals(true);
	}
	pls_flush_style(ui->resetPasswordTipsLabel);
}

void PLSResetPasswordEmailView::on_cancelBtn_clicked()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, "resetPasswordEmail CancelButton", ACTION_CLICK);
	reject();
}
