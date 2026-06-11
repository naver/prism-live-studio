#include "PLSToastButton.hpp"
#include "ui_PLSToastButton.h"
#include "login-common-helper.hpp"
#include <qdebug.h>
#include <QPainter>
#include <QStyleOption>
#include <QMouseEvent>
#include "libutils-api.h"
#include <qtimer.h>

PLSToastButton::PLSToastButton(QWidget *parent) : QWidget(parent)
{
	setMouseTracking(true);
	ui = pls_new<Ui::PLSToastButton>();
	ui->setupUi(this);
	this->installEventFilter(this);
	connect(ui->pushButton, &QPushButton::clicked, this, &PLSToastButton::clickButton);
	m_numLabel = pls_new<QLabel>(this);
	m_numLabel->setObjectName("alertNumLabel");
	m_numLabel->setAlignment(Qt::AlignCenter);
	m_numLabel->setProperty("maxNum", false);
	LoginCommonHelpers::refreshStyle(m_numLabel);
	m_numLabel->raise();
	ui->pushButton->lower();
	m_numLabel->hide();
}

PLSToastButton::~PLSToastButton()
{
	pls_delete(ui);
}

void PLSToastButton::setNum(const int num)
{
	qDebug() << "num =" << num;
	if (num > 0) {
		m_numLabel->setProperty("maxNum", false);
		LoginCommonHelpers::refreshStyle(m_numLabel);
		m_numLabel->setNum(num);

		if (num >= 9) {
			m_numLabel->setProperty("maxNum", true);
			LoginCommonHelpers::refreshStyle(m_numLabel);
			m_numLabel->setText("9+");
		}
		m_numLabel->show();
	} else {
		m_numLabel->hide();
	}
	m_num = num;
	setNumLabelPositon();
	update();
}

int PLSToastButton::num() const
{
	return m_num;
}

QString PLSToastButton::getNumText() const
{
	if (m_num > 9) {
		return QString("9+");
	} else {
		return QString::number(m_num);
	}
}

void PLSToastButton::setNumLabelPositon()
{
	QPoint center(this->rect().center());
	m_numLabel->move(static_cast<int>(center.x() + 2), this->rect().topLeft().x());
}

bool PLSToastButton::eventFilter(QObject *o, QEvent *e)
{
	if (auto event = static_cast<QMouseEvent *>(e); o == this && event->type() == QMouseEvent::MouseButtonPress && event->button() == Qt::LeftButton) {
		ui->pushButton->clicked();
		return true;
	}
	return QWidget::eventFilter(o, e);
}

void PLSToastButton::setShowAlert(bool showAlert)
{
	ui->pushButton->setProperty("showAlert", showAlert);
	LoginCommonHelpers::refreshStyle(ui->pushButton);
}

QPushButton *PLSToastButton::getButton()
{
	return ui->pushButton;
}
