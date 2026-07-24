#include "login-custom-button.hpp"
#include "libutils-api.h"

LoginCustomButton::LoginCustomButton(QWidget *parent) : QPushButton(parent)
{
	ui = pls_new<Ui::PLSLoginCustomButton>();
	ui->setupUi(this);
	this->layout()->setContentsMargins(11, 0, 0, 0);
	ui->picButtonLabel->setScaledContents(true);
}

LoginCustomButton::~LoginCustomButton()
{
	pls_delete(ui);
}

void LoginCustomButton::setButtonPicture(const QString &picPath)
{
	ui->picButtonLabel->setStyleSheet(QString("image: url(\"%1\");min-width:30px;max-width:30px;min-height:30px;max-height:30px;").arg(picPath));
}
