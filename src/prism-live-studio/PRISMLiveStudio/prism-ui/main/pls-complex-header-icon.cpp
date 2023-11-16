#include "pls-complex-header-icon.hpp"
#include "ui_PLSComplexHeaderIcon.h"

#include <qsvgrenderer.h>
#include "frontend-api.h"
#include "pls-shared-functions.h"

PLSComplexHeaderIcon::PLSComplexHeaderIcon(QWidget *parent) : QLabel(parent)
{
	ui = pls_new<Ui::PLSComplexHeaderIcon>();
	ui->setupUi(this);
	ui->PlatformLabel->hide();
}

PLSComplexHeaderIcon::~PLSComplexHeaderIcon()
{
	pls_delete(ui);
}

void PLSComplexHeaderIcon::setPixmap(const QString &pix)
{
	QPixmap pixmap(pix);

	pls_shared_circle_mask_image(pixmap);
	this->QLabel::setPixmap(pixmap);
}

void PLSComplexHeaderIcon::setPixmap(const QString &pix, const QSize &size)
{
	QPixmap pixmap(size * 4);
	if (pix.contains(".svg")) {
		//svg handler
		QSvgRenderer svgRenderer(pix);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		svgRenderer.render(&painter);
	} else {
		pls_get_prism_user_thumbnail(pixmap);
	}

	pixmap = pixmap.scaled(size * 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	pls_shared_circle_mask_image(pixmap);
	this->QLabel::setPixmap(pixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void PLSComplexHeaderIcon::setPlatformPixmap(const QString &pix, const QSize &size)
{
	QSvgRenderer svgRenderer(pix);
	QPixmap pixmap(size);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	svgRenderer.render(&painter);
	setPlatformPixmap(pixmap);
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
