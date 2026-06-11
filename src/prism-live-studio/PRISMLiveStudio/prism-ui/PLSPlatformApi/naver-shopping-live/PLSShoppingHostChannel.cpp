#include "qglobal.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif
#include "PLSShoppingHostChannel.h"
#include "ui_PLSShoppingHostChannel.h"
//#include "../../pls-app.hpp"
#include "libui.h"
#include "utils-api.h"
#include <QCloseEvent>

PLSShoppingHostChannel::PLSShoppingHostChannel(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSShoppingHostChannel>();
	pls_set_css(this, {"PLSShoppingHostChannel"});
	setupUi(ui);
	setWindowTitle(tr("Alert.Title"));
#if defined(Q_OS_MAC)
	setFixedSize({410, 233});
	initSize({410, 233});
#else
	setFixedSize({410, 273});
	initSize({410, 273});
#endif
	setResizeEnabled(false);
	ui->contentLabel->setText(setLineHeight(tr("navershopping.liveinfo.host.channel.content"), 18));
	ui->titleLabel->setText(setLineHeight(tr("navershopping.liveinfo.host.channel.title"), 20));
}

void PLSShoppingHostChannel::closeEvent(QCloseEvent *event)
{
#if defined(Q_OS_WIN)
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) {
		event->ignore();
		return;
	}
#endif
	PLSDialogView::closeEvent(event);
}

PLSShoppingHostChannel::~PLSShoppingHostChannel()
{
	pls_delete(ui);
}

void PLSShoppingHostChannel::on_agreeButton_clicked()
{
	this->accept();
}

QString PLSShoppingHostChannel::setLineHeight(QString sourceText, uint lineHeight) const
{
	QString sourceTemplate = "<p style=\"line-height:%1px\">%2<p>";
	QString text = sourceText.replace('\n', "<br/>");
	QString targetText = sourceTemplate.arg(lineHeight).arg(text);
	return targetText;
}

void PLSShoppingHostChannel::on_declineButton_clicked()
{
	this->reject();
}
