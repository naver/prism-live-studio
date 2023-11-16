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
			ui->userIconLabel->setPlatformPixmap(QString(":/resource/images/sns-profile/img-%1-profile.png").arg(PLSLoginUserInfo::getInstance()->getAuthType().toLower()),
							     QSize(34 * 4, 34 * 4));
		} else {
			ui->userIconLabel->setPlatformPixmap(QString(":/resource/images/sns-profile/img-%1-profile.svg").arg(PLSLoginUserInfo::getInstance()->getAuthType().toLower()),
							     QSize(34 * 4, 34 * 4));
		}
	}
	ui->userIconLabel->setPixmap(filePath, QSize(110, 110));
}
void PLSSettingGeneralView::setEnable(bool enable)
{
	ui->pushButton_change_pwd->setEnabled(enable);
	ui->pushButton_del_account->setEnabled(enable);
	ui->pushButton_logout->setEnabled(enable);
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

void PLSSettingGeneralView::checkPasswdTxt()
{
	//if (!ui->pushButton_change_pwd->isVisible()) {
	//	return;
	//}
	//auto dpi = 1
	//auto leftW = int(ui->horizontalLayout_2->geometry().width() - ui->pushButton_del_account->width() - ui->pushButton_change_pwd->width() - ui->pushButton_logout->width() -
	//		 (2 * 6 /*margin*/ + 2 * 6 /*padding*/) * dpi);
	//if (leftW > 0) {
	//	checkWidgetTxt(ui->pushButton_change_pwd, 0);
	//	return;
	//}
	//checkWidgetTxt(ui->pushButton_change_pwd, int(dpi * 6 * 2));
}

bool PLSSettingGeneralView::eventFilter(QObject *object, QEvent *event)
{
	if (object == ui->nickName && event->type() == QEvent::Resize) {
		QMetaObject::invokeMethod(
			this,
			[this]() {
				QFontMetrics fontWidth(ui->nickName->font());
				if (fontWidth.horizontalAdvance(PLSLoginUserInfo::getInstance()->getNickname()) > ui->nickName->width()) {
					ui->nickName->setText(fontWidth.elidedText(PLSLoginUserInfo::getInstance()->getNickname(), Qt::ElideRight, ui->nickName->width()));
				} else {
					ui->nickName->setText(PLSLoginUserInfo::getInstance()->getNickname());
				}
			},
			Qt::QueuedConnection);
		return true;
	}
	/*if (event->type() == QEvent::Resize) {
		checkPasswdTxt();
	}*/

	return QWidget::eventFilter(object, event);
}
