#include "plslogindlg.h"
#include "PLSAlertView.h"
#include "utils-api.h"
#include "liblog.h"
#include "action.h"
#include "pls-net-url.hpp"
#include "frontend-api.h"
#include "PLSErrorHandler.h"

#include "obs-module.h"
#include <qboxlayout.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkrequest.h>
#include <qnetworkreply.h>
#include <qpointer.h>
#include <obs.h>

constexpr auto NAVER_SHOPPING_LOGIN = "NaverShoppingLive-login";

PLSLoginDlg::PLSLoginDlg(QWidget *parent) : PLSDialogView(parent)
{
	pls_uistep_v2_set_custom_show_hide_name(this, "PLSNavershoppingLoginDlg");
	setHasMinButton(false);
	setHasMaxResButton(false);
	setHasHLine(false);
	setResizeEnabled(false);
	pls_add_css(this, {"PLSLoginDlg"});
	ui = pls_new<Ui::PLSLoginDlg>();
	setupUi(ui);
	initSize({440, 340});

	QHBoxLayout *hl = pls_new<QHBoxLayout>(ui->naverLoginButton);
	hl->setContentsMargins(0, 0, 0, 0);
	hl->setSpacing(3);

	hl->addStretch(1);

	QLabel *icon = pls_new<QLabel>(ui->naverLoginButton);
	icon->setObjectName("icon");
	hl->addWidget(icon);

	QLabel *text = pls_new<QLabel>(tr("NaverLogin"), ui->naverLoginButton);
	text->setObjectName("text");
	hl->addWidget(text);

	hl->addStretch(1);
}

PLSLoginDlg::~PLSLoginDlg()
{
	pls_delete(ui, nullptr);
}

QString PLSLoginDlg::loginUrl() const
{
	return m_loginUrl;
}

void PLSLoginDlg::on_naverLoginButton_clicked()
{
	PLS_UI_STEP(obs_module_name(), "Naver login button", ACTION_CLICK);
	m_loginUrl = CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN;
	PLS_INFO(NAVER_SHOPPING_LOGIN, "login url with naver: %s", m_loginUrl.toUtf8().constData());
	accept();
}

void PLSLoginDlg::on_storeLoginButton_clicked()
{
	PLS_UI_STEP(obs_module_name(), "Smart store login button", ACTION_CLICK);
	pls_navershopping_get_store_login_url(
		this,
		[this](const QString &url) {
			if (pls_object_is_valid(this)) {
				m_loginUrl = url;
				PLS_INFO(NAVER_SHOPPING_LOGIN, "login url with smart store: %s", m_loginUrl.toUtf8().constData());
				accept();
			}
		},
		[this](const QByteArray &data) {
			if (pls_object_is_valid(this)) {
				this->activateWindow();

				PLSErrorHandler::ExtraData extraData(CHANNEL_NAVER_SHOPPING_LIVE_GET_STORE_LOGIN);
				QString errorCode, errorMsg;
				pls_navershopping_get_error_code_message(data, errorCode, errorMsg);
				if (!errorCode.isEmpty()) {
					extraData.pathValueMap["errorCode"] = errorCode;
				}
				if (!errorMsg.isEmpty()) {
					extraData.pathValueMap["errorMessage"] = errorMsg;
				}
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_GET_SMART_STORE_URL_FAILED, "Naver Shopping LIVE", "PRISMLoginFailedAgain",
								      extraData);
			}
		});
}
