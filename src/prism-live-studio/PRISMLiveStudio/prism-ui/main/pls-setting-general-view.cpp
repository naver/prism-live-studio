#include "pls-setting-general-view.hpp"
#include "ui_PLSSettingGeneralView.h"
#include "login-user-info.hpp"
#include <QDebug>
#include <frontend-api.h>
#include <QFileInfo>
#include "log/log.h"
#include "pls-common-define.hpp"
#include "window-basic-settings.hpp"
#include "libui.h"
#include "window-basic-settings.hpp"
#include "PLSAlertView.h"
#include "ChannelCommonFunctions.h"
#include "PLSLoginDataHandler.h"

using namespace common;
PLSSettingGeneralView::PLSSettingGeneralView(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSSettingGeneralView>();
	ui->setupUi(this);
	ui->nickName->installEventFilter(this);
	ui->pushButton_change_pwd->installEventFilter(this);
	this->installEventFilter(this);
	initUi();
}

PLSSettingGeneralView::~PLSSettingGeneralView()
{
	pls_delete(ui);
}

void PLSSettingGeneralView::initUi()
{
	ui->userCode->setText(PLSLoginUserInfo::getInstance()->getUserCode());

	QString filePath = ":/resource/images/sns-profile/img-setting-profile-blank.svg";
	if (PLSLoginUserInfo::getInstance()->getAuthType() == LOGIN_USERINFO_EMAIL) {
		ui->email->setText(PLSLoginUserInfo::getInstance()->getEmail());
	} else {
		QString snsIconPath = pls_prism_user_thumbnail_path();
		if (QFileInfo::exists(snsIconPath)) {
			filePath = snsIconPath;
		}
		ui->email->setVisible(false);
		ui->pushButton_change_pwd->setVisible(false);
		ui->horizontalLayout_2->addStretch(1);

		if ("whale" == PLSLoginUserInfo::getInstance()->getAuthType().toLower()) {
			ui->userIconLabel->setPlatformPixmap(QString(":/resource/images/sns-profile/img-%1-profile.png").arg(PLSLoginUserInfo::getInstance()->getLoginPlatformName().toLower()),
							     QSize(34 * 4, 34 * 4));
		} else {
			auto iconPath = getPlatformImageFromName(PLSLOGINUSERINFO->getNCPPlatformServiceName(), 0);
			if (QFile::exists(iconPath) && !PLSLOGINUSERINFO->getNCPPlatformServiceName().isEmpty()) {
				QPixmap pixmap(iconPath);
				ui->userIconLabel->setPlatformPixmap(pixmap);
			} else {
				ui->userIconLabel->setPlatformPixmap(QString(":/resource/images/sns-profile/img-%1-profile.svg").arg(PLSLoginUserInfo::getInstance()->getLoginPlatformName().toLower()),
								     QSize(34 * 4, 34 * 4));
			}
		}
	}
	ui->userIconLabel->setPixmap(filePath, QSize(110, 110));
	ui->nickName->setText(PLSLoginUserInfo::getInstance()->getNickname());
}
void PLSSettingGeneralView::setEnable(bool enable)
{
	ui->pushButton_change_pwd->setEnabled(enable);
	ui->pushButton_del_account->setEnabled(enable);
	ui->pushButton_logout->setEnabled(enable);
}

void PLSSettingGeneralView::setNickNameWidth(int width)
{
	const int otherWidth = 180;
	QFontMetrics fontWidth(ui->nickName->font());
	auto nickName = PLSLoginUserInfo::getInstance()->getNickname();
	if (fontWidth.horizontalAdvance(nickName) > width - otherWidth) {
		ui->nickName->setText(fontWidth.elidedText(nickName, Qt::ElideRight, width - otherWidth));
	} else {
		ui->nickName->setText(nickName);
	}
}

void PLSSettingGeneralView::on_pushButton_logout_clicked()
{
	PLS_UI_STEP(SETTING_MODULE, " logout Button", ACTION_CLICK);
	if (OBSBasicSettings *dialogView = qobject_cast<OBSBasicSettings *>(pls_get_toplevel_view(this));
	    PLSAlertView::Button::Yes ==
	    PLSAlertView::information(dialogView, tr("Confirm"), tr("main.message.logout_alert"), PLSAlertView::Button::Yes | PLSAlertView::Button::No, PLSAlertView::Button::Yes)) {
		dialogView->setResult(static_cast<int>(LoginInfoType::PrismLogoutInfo));
		dialogView->done(static_cast<int>(LoginInfoType::PrismLogoutInfo));
	}
	qDebug() << __FUNCTION__;
}

void PLSSettingGeneralView::on_pushButton_del_account_clicked()
{
	PLS_UI_STEP(SETTING_MODULE, " del account Button", ACTION_CLICK);

	OBSBasicSettings *dialogView = qobject_cast<OBSBasicSettings *>(pls_get_toplevel_view(this));
	if (PLSAlertView::Button::Yes ==
	    PLSAlertView::information(dialogView, tr("Confirm"), tr("setting.account.note.confirmSignout"), PLSAlertView::Button::Yes | PLSAlertView::Button::No, PLSAlertView::Button::Yes)) {
		dialogView->setResult(static_cast<int>(LoginInfoType::PrismSignoutInfo));
		dialogView->done(static_cast<int>(LoginInfoType::PrismSignoutInfo));
	}
	qDebug() << __FUNCTION__;
}

void PLSSettingGeneralView::on_pushButton_change_pwd_clicked()
{
}

template<typename WidgetType> void checkWidgetTxt(WidgetType *wid, int padding = 10)
{
	auto srcTxt = wid->property("srcTxt").toString();

	if (srcTxt.isEmpty()) {
		srcTxt = wid->text();
		wid->setProperty("srcTxt", srcTxt);
	}
	auto contentsRect = wid->contentsRect();
	QFontMetrics fontWidth(wid->font());
	if (fontWidth.horizontalAdvance(srcTxt) > contentsRect.width() - padding) {
		wid->setText(fontWidth.elidedText(srcTxt, Qt::ElideRight, contentsRect.width() - padding));
	} else {
		wid->setText(srcTxt);
	}
}
