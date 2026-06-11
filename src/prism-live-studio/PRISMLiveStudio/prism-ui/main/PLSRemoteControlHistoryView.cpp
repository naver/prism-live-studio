#include "PLSRemoteControlHistoryView.h"
#include "ui_PLSRemoteControlHistoryView.h"

#include "window-basic-main.hpp"
#include <frontend-api.h>

PLSRemoteControlHistoryView::PLSRemoteControlHistoryView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSRemoteControlHistoryView>();
	ui->setupUi(this);
	ui->releaseBtn->setText(tr("remotecontrol.connect.status.disconnect"));
	connect(ui->releaseBtn, &QPushButton::clicked, this, [] { pls_on_frontend_event(pls_frontend_event::PLS_FRONTEND_EVENT_REMOTE_CONTROL_CLICK_CLOSE_CONNECT); });
}

PLSRemoteControlHistoryView::~PLSRemoteControlHistoryView() noexcept(true)
{
	pls_delete(ui);
}

void PLSRemoteControlHistoryView::setName(const QString &name)
{
	_name = name;
	ui->nameLabel->setText(QTStr("remotecontrol.connect.connect.hint").arg(name));
}
