#include "pls-complex-header-icon.hpp"
#include "ui_PLSComplexHeaderIcon.h"

#include "channels/ChannelsDataApi/ChannelCommonFunctions.h"

PLSComplexHeaderIcon::PLSComplexHeaderIcon(QWidget *parent) : QLabel(parent), ui(new Ui::PLSComplexHeaderIcon)
{
	ui->setupUi(this);
	ui->PlatformLabel->hide();
}

PLSComplexHeaderIcon::~PLSComplexHeaderIcon()
{
	delete ui;
}

void PLSComplexHeaderIcon::setPixmap(const QString &pix)
{
	QPixmap pixmap(pix);
	circleMaskImage(pixmap);
	this->QLabel::setPixmap(pixmap);
}

void PLSComplexHeaderIcon::setPixmap(const QPixmap &pix, const QSize &size)
{
	QPixmap pixmap(pix);
	circleMaskImage(pixmap);
	this->QLabel::setPixmap(pixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void PLSComplexHeaderIcon::setPlatformPixmap(const QString &pix)
{
	setPlatformPixmap(QPixmap(pix));
}

void PLSComplexHeaderIcon::setPlatformPixmap(const QPixmap &pix)
{
	ui->PlatformLabel->setPixmap(pix);
	ui->PlatformLabel->show();
}

void PLSComplexHeaderIcon::changeEvent(QEvent *e)
{
	QLabel::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}
