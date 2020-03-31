#include "pls-setting-general-view.hpp"
#include "ui_PLSSettingGeneralView.h"
#include <QDebug>
#include <frontend-api.h>
#include <QFileInfo>
#include "alert-view.hpp"
#include "log/log.h"
#include "pls-common-define.hpp"
#include "pls-app.hpp"

PLSSettingGeneralView::PLSSettingGeneralView(QWidget *parent) : QWidget(parent), ui(new Ui::PLSSettingGeneralView)
{
	ui->setupUi(this);
	initUi();
	ui->pushButton_logout->setVisible(false);
	ui->pushButton_del_account->setVisible(false);
	ui->pushButton_change_pwd->setVisible(false);
}

PLSSettingGeneralView::~PLSSettingGeneralView()
{
	delete ui;
}

void PLSSettingGeneralView::initUi()
{
	ui->nickName->setText(pls_get_prism_nickname());
	ui->userCode->setText(pls_get_prism_usercode());
	ui->email->setText(pls_get_prism_email());
	
}
void PLSSettingGeneralView::setEnable(bool enable)
{
	
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
	
}

void PLSSettingGeneralView::on_pushButton_del_account_clicked()
{
	
}

void PLSSettingGeneralView::on_pushButton_change_pwd_clicked()
{
}
