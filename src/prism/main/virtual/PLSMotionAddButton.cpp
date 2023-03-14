#include "PLSMotionAddButton.h"
#include "ui_PLSMotionAddButton.h"

PLSMotionAddButton::PLSMotionAddButton(QWidget *parent) : QPushButton(parent), ui(new Ui::PLSMotionAddButton)
{
	ui->setupUi(this);
	ui->verticalLayout->setAlignment(ui->addLabel, Qt::AlignHCenter);
	this->installEventFilter(this);
}

PLSMotionAddButton::~PLSMotionAddButton()
{
	delete ui;
}

bool PLSMotionAddButton::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == this) {
		if (event->type() == QEvent::HoverLeave) {
			ui->addLabel->setStyleSheet("image:url(:/images/virtual/icon-addsource-default.svg);");
		} else if (event->type() == QEvent::HoverEnter) {
			ui->addLabel->setStyleSheet("image:url(:/images/virtual/icon-addsource-over.svg);");
		} else if (event->type() == QEvent::MouseButtonPress) {
			ui->addLabel->setStyleSheet("image:url(:/images/virtual/icon-addsource-click.svg);");
		}
	}
	return QPushButton::eventFilter(watched, event);
}
