#include "PLSShoppingCalenderCombox.h"
#include <QTimer>
#include "utils-api.h"

PLSShoppingCalenderCombox::PLSShoppingCalenderCombox(QWidget *parent) : QPushButton(parent)
{
	setDefault(false);
	setAutoDefault(false);
	m_menu = pls_new<PLSShoppingComboxMenu>(this);
	connect(m_menu, &PLSShoppingComboxMenu::isShow, [this](bool isShow) { showComboListView(isShow); });
	connect(m_menu->calender(), &QCalendarWidget::clicked, this, [this](const QDate &date) {
		m_menu->setHidden(true);
		emit clickDate(date);
	});
	connect(m_menu->listWidget(), &QListWidget::itemClicked, this, [this](const QListWidgetItem *item) {
		m_menu->setHidden(true);
		emit clickTime(item->text());
	});
}

void PLSShoppingCalenderCombox::showDateCalender()
{
	int top = 6;
	QPoint p = this->mapToGlobal(QPoint(0, this->height() + top));
	m_menu->showDateCalender(m_date);
	m_menu->move(p);
	m_menu->show();
}

void PLSShoppingCalenderCombox::showHour(QString hour)
{
	m_menu->showHour(hour);
	int y = this->height() / 2 - m_menu->size().height() / 2;
	QPoint p = this->mapToGlobal(QPoint(0, y));
	m_menu->listWidget()->setHidden(false);
	m_menu->move(p);
	m_menu->show();
}

void PLSShoppingCalenderCombox::showMinute(QString minute)
{
	m_menu->showMinute(minute);
	int y = this->height() / 2 - m_menu->size().height() / 2;
	QPoint p = this->mapToGlobal(QPoint(0, y));
	m_menu->listWidget()->setHidden(false);
	m_menu->move(p);
	m_menu->show();
}

void PLSShoppingCalenderCombox::showAp(QString ap)
{
	m_menu->showAp(ap);
	QPoint p = this->mapToGlobal(QPoint(0, 0));
	m_menu->listWidget()->setHidden(false);
	m_menu->move(p);
	m_menu->show();
}

const QStringList &PLSShoppingCalenderCombox::apList() const
{
	return m_menu->apList();
}

void PLSShoppingCalenderCombox::setDate(const QDate &date)
{
	m_date = date;
}

const QDate &PLSShoppingCalenderCombox::date() const
{
	return m_date;
}

void PLSShoppingCalenderCombox::showComboListView(bool show)
{
	this->setChecked(show);
}
