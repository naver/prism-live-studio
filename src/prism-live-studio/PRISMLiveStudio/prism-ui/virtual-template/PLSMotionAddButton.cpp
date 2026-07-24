#include "PLSMotionAddButton.h"
#include "ui_PLSMotionAddButton.h"
#include "utils-api.h"

PLSMotionAddButton::PLSMotionAddButton(QWidget *parent) : QPushButton(parent)
{
	ui = pls_new<Ui::PLSMotionAddButton>();
	ui->setupUi(this);
	ui->verticalLayout->setAlignment(ui->addLabel, Qt::AlignHCenter);

	m_pixDefault = QPixmap(":/resource/images/virtual/icon-addsource-default.svg");
	m_pixHover = QPixmap(":/resource/images/virtual/icon-addsource-over.svg");
	m_pixClick = QPixmap(":/resource/images/virtual/icon-addsource-click.svg");
	ui->addLabel->setPixmap(m_pixDefault);

	this->installEventFilter(this);
}

PLSMotionAddButton::~PLSMotionAddButton()
{
	pls_delete(ui);
}

bool PLSMotionAddButton::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == this) {
		if (event->type() == QEvent::HoverLeave) {
			ui->addLabel->setPixmap(m_pixDefault);
		} else if (event->type() == QEvent::HoverEnter) {
			ui->addLabel->setPixmap(m_pixHover);
		} else if (event->type() == QEvent::MouseButtonPress) {
			ui->addLabel->setPixmap(m_pixClick);
		}
	}
	return QPushButton::eventFilter(watched, event);
}
