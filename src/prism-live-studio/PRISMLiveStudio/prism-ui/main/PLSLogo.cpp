#include "PLSLogo.h"
#include "ui_PLSLogo.h"
#include <libutils-api.h>
#include <libui.h>

const int LEFTMARGIN = 0;
const int PADDING = 5;
const int LOGOWIDTH = 140;
extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);

PLSLogo::PLSLogo(QWidget *parent) : QPushButton(parent)
{
	ui = pls_new<Ui::PLSLogo>();
	ui->setupUi(this);
	pls_add_css(this, {"PLSLogo"});
	this->setCursor(Qt::ArrowCursor);
	setVisableTips(false);
	QPixmap pix;
	loadPixmap(pix, ":/resource/images/main-logo.png", QSize(140, 18) * 4);

	ui->logo->setPixmap(pix);
}
PLSLogo::~PLSLogo()
{
	pls_delete(ui);
}

void PLSLogo::setVisableTips(bool isVisable)
{
	ui->update_info->setVisible(isVisable);
	m_isVisableTips = isVisable;
	m_tipText = ui->update_info->text();
	setProperty("updateSize", isVisable);
	pls_flush_style(this);
}

void PLSLogo::leaveEvent(QEvent *event)
{
	QPushButton::leaveEvent(event);
	if (isEnabled()) {
		pls_flush_style_recursive(ui->update_info, "hover", false);
	}
}

void PLSLogo::enterEvent(QEnterEvent *event)
{

	QPushButton::enterEvent(event);
	if (isEnabled()) {
		pls_flush_style_recursive(ui->update_info, "hover", true);
	}
}

bool PLSLogo::event(QEvent *event)
{
	if (isEnabled()) {
		QEvent::Type type = event->type();
		switch (type) {
		case QEvent::Enter:
			pls_flush_style_recursive(ui->update_info, "hover", true);
			break;
		case QEvent::Leave:
			pls_flush_style_recursive(ui->update_info, "hover", false);
			break;
		case QEvent::MouseButtonPress:
			pls_flush_style_recursive(ui->update_info, "press", true);
			break;
		case QEvent::MouseButtonRelease:
			pls_flush_style_recursive(ui->update_info, "press", false);
			break;
		default:
			break;
		}
	}
	return QPushButton::event(event);
}
