#include "channel-login-view.hpp"
#include "ui_PLSChannelLoginView.h"

PLSChannelLoginView::PLSChannelLoginView(QJsonObject &res, QWidget *parent) : QDialog(parent), result(res), ui(new Ui::PLSChannelLoginView)
{
	ui->setupUi(this);
}

PLSChannelLoginView::~PLSChannelLoginView()
{
	delete ui;
}
