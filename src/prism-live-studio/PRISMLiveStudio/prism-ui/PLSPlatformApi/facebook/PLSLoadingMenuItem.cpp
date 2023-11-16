#include "PLSLoadingMenuItem.h"
#include "ui_PLSLoadingMenuItem.h"
#include "frontend-api.h"
#include <QTimer>
#include "libutils-api.h"
#include "libui.h"

const int PARENT_WIDTH = 272;

PLSLoadingMenuItem::PLSLoadingMenuItem(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSLoadingMenuItem>();
	ui->setupUi(this);
	ui->loadingMenuTitleLabel->installEventFilter(this);
}

PLSLoadingMenuItem::~PLSLoadingMenuItem()
{
	pls_delete(ui);
}

void PLSLoadingMenuItem::setTitle(const QString &title)
{
	m_title = title;
}

void PLSLoadingMenuItem::setSelected(bool selected)
{
	ui->loadingMenuTitleLabel->setProperty("selected", selected);
	pls_flush_style_recursive(this);
}

bool PLSLoadingMenuItem::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->loadingMenuTitleLabel && event->type() == QEvent::Show) {
		QFontMetrics fontMetric(ui->loadingMenuTitleLabel->font());
		double margin = 10;
		auto maxWidth = PARENT_WIDTH - margin * 2;
		QString str = fontMetric.elidedText(m_title, Qt::ElideRight, maxWidth);
		ui->loadingMenuTitleLabel->setText(str);
	}
	return QWidget::eventFilter(watched, event);
}
