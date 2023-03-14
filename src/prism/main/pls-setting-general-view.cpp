#include "pls-setting-general-view.hpp"
#include "ui_PLSSettingGeneralView.h"
#include "login-user-info.hpp"
#include <QDebug>
#include <frontend-api.h>
#include "login-web-handler.hpp"
#include <QFileInfo>
#include "alert-view.hpp"
#include "log/log.h"
#include "pls-common-define.hpp"
#include "pls-app.hpp"
#include "window-basic-settings.hpp"

PLSSettingGeneralView::PLSSettingGeneralView(QWidget *parent, PLSDpiHelper dpiHelper) : QWidget(parent), ui(new Ui::PLSSettingGeneralView)
{
	ui->setupUi(this);
	dpiHelper.notifyDpiChanged(this, [=](double dpi) { initUi(dpi); });
}

PLSSettingGeneralView::~PLSSettingGeneralView()
{
	delete ui;
}

void PLSSettingGeneralView::initUi(double dpi)
{
	ui->nickName->setText(PLSLoginUserInfo::getInstance()->getNickname());
	ui->userCode->setText(PLSLoginUserInfo::getInstance()->getUserCode());

	QString filePath = ":/images/img-setting-profile-blank.svg";
	if (PLSLoginUserInfo::getInstance()->getAuthType() == LOGIN_USERINFO_EMAIL) {
		ui->email->setText(PLSLoginUserInfo::getInstance()->getEmail());
	} else {
		QString snsIconPath = pls_prism_user_thumbnail_path();
		if (QFileInfo::exists(snsIconPath)) {
			filePath = snsIconPath;
		}
		ui->email->setVisible(false);
		ui->pushButton_change_pwd->setVisible(false);

		if ("whale" == PLSLoginUserInfo::getInstance()->getAuthType().toLower()) {
			ui->userIconLabel->setPlatformPixmap(QString(":/images/img-%1-profile.png").arg(PLSLoginUserInfo::getInstance()->getAuthType().toLower()),
							     QSize(PLSDpiHelper::calculate(dpi, 34), PLSDpiHelper::calculate(dpi, 34)));
		} else {
			ui->userIconLabel->setPlatformPixmap(QString(":/images/img-%1-profile.svg").arg(PLSLoginUserInfo::getInstance()->getAuthType().toLower()),
							     QSize(PLSDpiHelper::calculate(dpi, 34), PLSDpiHelper::calculate(dpi, 34)));
		}
	}
	ui->userIconLabel->setPixmap(filePath, QSize(PLSDpiHelper::calculate(dpi, 110), PLSDpiHelper::calculate(dpi, 110)));
}
void PLSSettingGeneralView::setEnable(bool enable)
{
	ui->pushButton_change_pwd->setEnabled(enable);
	ui->pushButton_del_account->setEnabled(enable);
	ui->pushButton_logout->setEnabled(enable);
}
void PLSSettingGeneralView::changeEvent(QEvent *e)
{
	QWidget::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void PLSSettingGeneralView::on_pushButton_logout_clicked()
{
	PLS_UI_STEP(SETTING_MODULE, " logout Button", ACTION_CLICK);
	PLSBasicSettings *dialogView = qobject_cast<PLSBasicSettings *>(pls_get_toplevel_view(this));
	if (PLSAlertView::Button::Yes ==
	    PLSAlertView::information(dialogView, tr("Confirm"), tr("main.message.logout_alert"), PLSAlertView::Button::Yes | PLSAlertView::Button::No, PLSAlertView::Button::Yes)) {
		dialogView->done(LoginInfoType::PrismLogoutInfo);
		dialogView->setReturnValue(LoginInfoType::PrismLogoutInfo);
	}
	qDebug() << __FUNCTION__;
}

void PLSSettingGeneralView::on_pushButton_del_account_clicked()
{
	PLS_UI_STEP(SETTING_MODULE, " del account Button", ACTION_CLICK);

	PLSBasicSettings *dialogView = qobject_cast<PLSBasicSettings *>(pls_get_toplevel_view(this));
	if (PLSAlertView::Button::Yes ==
	    PLSAlertView::information(dialogView, tr("Confirm"), tr("setting.account.note.confirmSignout"), PLSAlertView::Button::Yes | PLSAlertView::Button::No, PLSAlertView::Button::Yes)) {
		dialogView->done(LoginInfoType::PrismSignoutInfo);
		dialogView->setReturnValue(LoginInfoType::PrismSignoutInfo);
	}
	qDebug() << __FUNCTION__;
}

void PLSSettingGeneralView::on_pushButton_change_pwd_clicked()
{
}
