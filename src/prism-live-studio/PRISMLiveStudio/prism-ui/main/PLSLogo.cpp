#include "PLSLogo.h"
#include "ui_PLSLogo.h"
#include "PLSCommonFunc.h"
#include <libutils-api.h>
#include <libui.h>
#include <QHBoxLayout>

extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);

PLSLogo::PLSLogo(QWidget *parent) : QPushButton(parent)
{
	ui = pls_new<Ui::PLSLogo>();
	ui->setupUi(this);
	pls_add_css(this, {"PLSLogo"});
	this->setCursor(Qt::ArrowCursor);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	ui->separator_label->setWordWrap(false);
	ui->separator_label->hide();
	ui->version_label->setWordWrap(false);
	ui->version_label->setText(QString("v%1 ").arg(PLSLoginFunc::getPrismVersion()));

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

void PLSLogo::refreshMinimumWidthFromLayout()
{
	if (QHBoxLayout *lay = qobject_cast<QHBoxLayout *>(layout())) {
		lay->invalidate();
		lay->activate();
		int w = lay->minimumSize().width() + 4;
		if (w <= 0) {
			w = sizeHint().width() + 4;
		}
		if (w > 0) {
			setMinimumWidth(w);
		}
		updateGeometry();
		if (QWidget *p = parentWidget()) {
			if (p->layout()) {
				p->layout()->invalidate();
			}
		}
	}
}

//PRISM/jackson/20260325/PRISM_PC-5596/remove duplicate enterEvent/leaveEvent, event() already handles Enter/Leave
bool PLSLogo::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::Show:
	case QEvent::ScreenChangeInternal:
		pls_async_call_mt(this, [this]() { refreshMinimumWidthFromLayout(); });
		break;
	default:
		break;
	}

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
