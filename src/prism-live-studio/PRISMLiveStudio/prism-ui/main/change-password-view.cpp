#include "change-password-view.hpp"

#include <qnetworkcookie.h>

#include "json-data-handler.hpp"
#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "login-user-info.hpp"
#include "login-common-helper.hpp"
#include "PLSAlertView.h"
#include "log/log.h"
#include "frontend-api.h"
#include "utils-api.h"
#include "libhttp-client.h"
#include <QLineEdit>

using namespace common;

PLSChangePasswordView::PLSChangePasswordView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSChangePasswordView>();
	setupUi(ui);

	setWindowTitle(QString());
	setResizeEnabled(false);

	setConnect();
	translateLanguage();
	ui->okBtn->setEnabled(false);
	ui->okBtn->setProperty(STATUS, false);
	addMacTopMargin();
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("login.change.password"));
#endif
}

void PLSChangePasswordView::translateLanguage()
{
	ui->currentPasswordLineEdit->setPlaceholderText(tr(LOGIN_OLD_PASSWORD));
	ui->newPasswordLineEdit->setPlaceholderText(tr(LOGIN_NEW_PASSWORD));
}

void PLSChangePasswordView::setConnect() const
{
	connect(ui->currentPasswordLineEdit, &QLineEdit::textEdited, this, &PLSChangePasswordView::editTextChange);
	connect(ui->newPasswordLineEdit, &QLineEdit::textEdited, this, &PLSChangePasswordView::editTextChange);
	connect(ui->okBtn, &QPushButton::clicked, this, &PLSChangePasswordView::onOkButtonClicked);
	connect(ui->cancelBtn, &QPushButton::clicked, this, &PLSChangePasswordView::onCancelButtonClicked);
}

void PLSChangePasswordView::replyErrorHandler(const QJsonObject &obj, const int statusCode)
{
	auto code = obj.value(LOGIN_CODE).toInt();
	if (400 == statusCode) {
		if (5050 == code) {
			PLS_ERROR(PLS_LOGIN_MODULE, tr(LOGIN_EXIST_PASSWORD_NOT_MATCH).toUtf8().data());
			pls_alert_error_message(nullptr, tr("Alert.Title"), tr(LOGIN_EXIST_PASSWORD_NOT_MATCH));
		} else if (5060 == code) {
			PLS_ERROR(PLS_LOGIN_MODULE, tr(LOGIN_SAME_AS_EXIST_PASSWORD).toUtf8().data());
			pls_alert_error_message(nullptr, tr("Alert.Title"), tr(LOGIN_SAME_AS_EXIST_PASSWORD));
		}
	}
	if (500 == statusCode || 401 == statusCode) {
		PLS_ERROR(PLS_LOGIN_MODULE, tr(LOGIN_SERVER_UNKNOWN_ERROR).toUtf8().data());
		pls_alert_error_message(nullptr, tr("Alert.Title"), tr(LOGIN_SERVER_UNKNOWN_ERROR));
	}
	if (0 == statusCode) {
		PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("login.check.note.network"));
	}
	if (obj.value("errorCode").toString() == "025") {
		PLSAlertView::warning(this, tr("Alert.Title"), tr("Prism.Login.Systemtime.Error"));
	}
	ui->okBtn->blockSignals(false);
}

void PLSChangePasswordView::onOkButtonClicked()
{

	if (LoginCommonHelpers::isValidPasswordByRegExp(ui->newPasswordLineEdit->text())) {
		QJsonObject changeInfoMap;
		changeInfoMap.insert("oldPw", ui->currentPasswordLineEdit->text());
		changeInfoMap.insert("newPw", ui->newPasswordLineEdit->text());

		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Put)
					   .cookie({{COOKIE_NEO_SES, PLSLoginUserInfo::getInstance()->getToken()}})
					   .body(changeInfoMap)
					   .jsonContentType() //
					   .withLog()         //
					   .receiver(this)    //
					   .workInMainThread()
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .hmacUrl(PLS_CHANGE_PASSWORD.arg(PRISM_SSL), PLS_PC_HMAC_KEY.toUtf8().constData())
					   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
					   .jsonOkResult([this](const pls::http::Reply &reply, const QJsonDocument &doc) {
						   pls_unused(reply, doc);
						   pls_async_call_mt(this, [this]() {
							   PLS_INFO(PLS_LOGIN_MODULE, QString("change password success.").toUtf8().data());
							   done(Accepted);
							   ui->okBtn->blockSignals(false);
						   });
					   })
					   .failResult([this](const pls::http::Reply &reply) {
						   auto statusCode = reply.statusCode();
						   auto obj = reply.object();
						   pls_async_call_mt(this, [this, statusCode, obj]() { replyErrorHandler(obj, statusCode); });
					   }));
		ui->okBtn->blockSignals(true);
	} else {
		ui->changepasswordTipsLabel->setText(tr(LOGIN_PRISM_ERROR_INVALID_PASSWORD));
	}
}

void PLSChangePasswordView::onCancelButtonClicked()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, "changepassword cancel_Button", ACTION_CLICK);
	done(Rejected);
}

void PLSChangePasswordView::editTextChange(const QString &)
{

	if (ui->currentPasswordLineEdit->text().isEmpty() || ui->newPasswordLineEdit->text().isEmpty()) {
		ui->okBtn->setEnabled(false);
		ui->okBtn->setProperty(STATUS, false);
	} else {
		ui->okBtn->setEnabled(true);
		ui->okBtn->setProperty(STATUS, true);
	}
	LoginCommonHelpers::refreshStyle(ui->okBtn);
}
void PLSChangePasswordView::on_forgotPasswordBtn_clicked()
{

}
