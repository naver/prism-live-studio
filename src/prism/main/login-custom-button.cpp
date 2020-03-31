#include "login-custom-button.hpp"

LoginCustomButton::LoginCustomButton(QWidget *parent) : QPushButton(parent), ui(new Ui::PLSLoginCustomButton)
{
	ui->setupUi(this);
	this->layout()->setContentsMargins(11, 0, 0, 0);
}

LoginCustomButton::~LoginCustomButton()
{
	delete ui;
}

void LoginCustomButton::setButtonPicture(const QString &picPath)
{
	ui->picButtonLabel->setPixmap(QPixmap(picPath));
}
