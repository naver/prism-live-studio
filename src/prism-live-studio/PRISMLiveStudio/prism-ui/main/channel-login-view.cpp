#include "channel-login-view.hpp"
#include "ui_PLSChannelLoginView.h"
#include "utils-api.h"

PLSChannelLoginView::PLSChannelLoginView(QJsonObject &res, QWidget *parent) : QDialog(parent), result(res)
{
	ui = pls_new<Ui::PLSChannelLoginView>();
	ui->setupUi(this);
}

PLSChannelLoginView::~PLSChannelLoginView()
{
	pls_delete(ui, nullptr);
}
