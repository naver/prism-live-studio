#include "reset-password-email-view.hpp"
#include "ui_PLSResetPasswordEmailView.h"
#include <QMouseEvent>
#include <QNetworkRequest>

#include "reset-password-email-view.hpp"
#include "json-data-handler.hpp"
#include "pls-net-url.hpp"
#include "network-access-manager.hpp"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "login-user-info.hpp"
#include "login-common-helper.hpp"
#include "log/log.h"
#include "alert-view.hpp"
#include "frontend-api.h"

PLSResetPasswordEmailView::PLSResetPasswordEmailView(QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSDialogView(parent, dpiHelper), ui(new Ui::PLSResetPasswordEmailView), m_networkAccessManager(PLSNetworkAccessManager::getInstance()), m_isMove(false)
{
	dpiHelper.setCss(this, {PLSCssIndex::PrismPasswordView});

	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	ui->setupUi(this->content());
	setWindowTitle(QString());
	setResizeEnabled(false);

	ui->okBtn->setEnabled(false);
	ui->okBtn->setProperty(STATUS, STATUS_DISABLE);
	ui->emailAddressLineEdit->setPlaceholderText(tr(LOGIN_EMAIL));
	ui->resetPasswordTipsLabel->adjustSize();

	connect(ui->emailAddressLineEdit, &QLineEdit::textEdited, this, &PLSResetPasswordEmailView::emailEditTextChange);
	connect(m_networkAccessManager, &PLSNetworkAccessManager::replyResultData, this, &PLSResetPasswordEmailView::replyResultDataHandler);
	connect(m_networkAccessManager, &PLSNetworkAccessManager::replyErrorDataWithSatusCode, this, &PLSResetPasswordEmailView::replyErrorHandler);
	connect(ui->cancelBtn, &QPushButton::clicked, this, &PLSResetPasswordEmailView::on_cancelBtn_clicked);
	connect(ui->okBtn, &QPushButton::clicked, this, &PLSResetPasswordEmailView::on_okBtn_clicked);
}

PLSResetPasswordEmailView::~PLSResetPasswordEmailView() {}

void PLSResetPasswordEmailView::emailEditTextChange(const QString &text)
{
	if (text.isEmpty()) {
		ui->okBtn->setEnabled(false);
		ui->okBtn->setProperty(STATUS, STATUS_DISABLE);
	} else {
		ui->okBtn->setEnabled(true);
		ui->okBtn->setProperty(STATUS, STATUS_ENABLE);
	}
	LoginCommonHelpers::refreshStyle(ui->okBtn);
}

void PLSResetPasswordEmailView::loginRequest()
{
	QVariantMap uerEmailInfo;
	uerEmailInfo.insert(LOGIN_USERINFO_EMAIL, ui->emailAddressLineEdit->text());
	QVariantMap headInfo;
	headInfo.insert(HTTP_HEAD_CONTENT_TYPE, HTTP_CONTENT_TYPE_VALUE);
	m_networkAccessManager->createHttpRequest(PLSNetworkAccessManager::GetOperation, PLS_EMAIL_FOGETTON_URL.arg(PRISM_SSL), true, headInfo, uerEmailInfo);
}

void PLSResetPasswordEmailView::on_okBtn_clicked()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, "resetPasswordEamil OKButton", ACTION_CLICK);

	loginRequest();
	ui->okBtn->blockSignals(true);
}

void PLSResetPasswordEmailView::on_cancelBtn_clicked()
{
	PLS_UI_STEP(PLS_LOGIN_MODULE, "resetPasswordEamil CancelButton", ACTION_CLICK);
	reject();
}

void PLSResetPasswordEmailView::replyResultDataHandler(int statusCode, const QString &url, const QByteArray array)
{
	Q_UNUSED(url)
	if (PLS_EMAIL_FOGETTON_URL.arg(PRISM_SSL) == url) {
		QVariant code;
		PLSJsonDataHandler::getValueFromByteArray(array, LOGIN_CODE, code);
		if (NO_EXIST_EMAIL == code.toInt()) {
			PLS_INFO(PLS_LOGIN_MODULE, tr(LOGIN_CAN_NOT_FIND_EMAIL).toUtf8().data());
			PLSAlertView::warning(this, tr("Alert.Title"), tr(LOGIN_CAN_NOT_FIND_EMAIL));

		} else if (HTTP_STATUS_CODE_200 == statusCode) {
			PLS_INFO(PLS_LOGIN_MODULE, QString("reset password success please check email.").toUtf8().data());
			PLSAlertView::information(this, tr("Alert.Title"), tr(LOGIN_PRISM_SENT_RESET_PASSWORD_LINK));
			accept();
		}
	}
	ui->okBtn->blockSignals(false);
}

void PLSResetPasswordEmailView::replyErrorHandler(int statusCode, const QString &url, const QString &, const QString &errorInfo)
{
	Q_UNUSED(url)
	if (PLS_EMAIL_FOGETTON_URL.arg(PRISM_SSL) == url) {
		if (HTTP_STATUS_CODE_400 == statusCode) {
			PLS_ERROR(PLS_LOGIN_MODULE, tr(LOGIN_CAN_NOT_FIND_EMAIL).toUtf8().data());
			PLSAlertView::warning(this, tr("Alert.Title"), tr(LOGIN_CAN_NOT_FIND_EMAIL));
		} else if (HTTP_STATUS_CODE_500 == statusCode || HTTP_STATUS_CODE_401 == statusCode) {
			PLS_ERROR(PLS_LOGIN_MODULE, tr(LOGIN_SERVER_UNKNOWN_ERROR).toUtf8().data());
			PLSAlertView::warning(this, tr("Alert.Title"), tr(LOGIN_SERVER_UNKNOWN_ERROR));
		} else if (0 == statusCode && !errorInfo.isEmpty()) {
			PLSAlertView::warning(this, tr("Alert.Title"), tr("login.check.note.network"));
		}
	}
	ui->okBtn->blockSignals(false);
}
