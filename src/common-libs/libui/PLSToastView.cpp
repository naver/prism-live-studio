#include "PLSToastView.h"
#include "ui_PLSToastView.h"

#include <QTimer>
#include <QList>
#include <QMap>
#include <QStyle>

#include <libutils-api.h>

// class PLSToastView Implements
PLSToastView::PLSToastView(QWidget *parent) : PLSDialogView(parent)
{
	setHasCaption(false);
	ui = pls_new<Ui::PLSToastView>();
	setupUi(ui);
	QObject::connect(ui->closeButton, &QPushButton::clicked, this, &PLSToastView::hide);
	ui->closeButton->hide();
}

PLSToastView::~PLSToastView()
{
	pls_delete(ui, nullptr);
}

PLSToastView::Icon PLSToastView::getIcon() const
{
	return this->icon;
}

void PLSToastView::setIcon(Icon icon_)
{
	this->icon = icon_;
}

QPixmap PLSToastView::getPixmap() const
{
	return ui->iconLabel->pixmap(Qt::ReturnByValue);
}

void PLSToastView::setPixmap(const QPixmap &pixmap)
{
	ui->iconLabel->setPixmap(pixmap);
}

void PLSToastView::setMessage(const QString &message)
{
	ui->messageLabel->setText(message);
}

void PLSToastView::show(Icon icon_, const QString &message, int autoHide)
{
	if (timer) {
		timer->stop();
		pls_delete(timer, nullptr);
	}

	setIcon(icon_);
	setMessage(message);

	style()->unpolish(this);
	style()->polish(this);

	if (autoHide > 0) {
		timer = pls_new<QTimer>(this);
		timer->setSingleShot(true);
		connect(timer, &QTimer::timeout, this, &PLSToastView::autoHide);
		timer->start(autoHide);
	}

	PLSDialogView::show();
}

void PLSToastView::autoHide()
{
	hide();
	pls_delete(timer, nullptr);
}
