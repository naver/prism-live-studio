#include "PLSShoppingCalenderCombox.h"
#include <QTimer>
#include <QDebug>

PLSShoppingCalenderCombox::PLSShoppingCalenderCombox(QWidget *parent, PLSDpiHelper dpiHelper) : QPushButton(parent)
{
	setDefault(false);
	setAutoDefault(false);
	m_menu = new PLSShoppingComboxMenu(this);
	m_menu->setModal(true);
	connect(m_menu, &PLSShoppingComboxMenu::isShow, [=](bool isShow) { showComboListView(isShow); });
	connect(m_menu, &PLSShoppingComboxMenu::show, [=]() { showComboListView(true); });
	connect(m_menu->calender(), &QCalendarWidget::clicked, this, [=](const QDate &date) {
		m_menu->setHidden(true);
		emit clickDate(date);
	});
	connect(m_menu->listWidget(), &QListWidget::itemClicked, this, [=](QListWidgetItem *item) {
		m_menu->setHidden(true);
		emit clickTime(item->text());
	});
}

PLSShoppingCalenderCombox::~PLSShoppingCalenderCombox() {}

void PLSShoppingCalenderCombox::showDateCalender()
{
	double dpi = PLSDpiHelper::getDpi(this);
	int top = PLSDpiHelper::calculate(dpi, 6);
	QPoint p = this->mapToGlobal(QPoint(0, this->height() + top));
	m_menu->showDateCalender(m_date);
	m_menu->move(p);
	m_menu->exec();
}

void PLSShoppingCalenderCombox::showHour(QString hour)
{
	m_menu->showHour(hour);
	int y = this->height() / 2 - m_menu->size().height() / 2;
	QPoint p = this->mapToGlobal(QPoint(0, y));
	m_menu->listWidget()->setHidden(false);
	m_menu->move(p);
	m_menu->exec();
}

void PLSShoppingCalenderCombox::showMinute(QString minute)
{
	m_menu->showMinute(minute);
	int y = this->height() / 2 - m_menu->size().height() / 2;
	QPoint p = this->mapToGlobal(QPoint(0, y));
	m_menu->listWidget()->setHidden(false);
	m_menu->move(p);
	m_menu->exec();
}

void PLSShoppingCalenderCombox::showAp(QString ap)
{
	m_menu->showAp(ap);
	QPoint p = this->mapToGlobal(QPoint(0, 0));
	m_menu->listWidget()->setHidden(false);
	m_menu->move(p);
	m_menu->exec();
}

const QStringList &PLSShoppingCalenderCombox::apList()
{
	return m_menu->apList();
}

void PLSShoppingCalenderCombox::setDate(const QDate &date)
{
	m_date = date;
}

const QDate &PLSShoppingCalenderCombox::date()
{
	return m_date;
}

void PLSShoppingCalenderCombox::showComboListView(bool show)
{
	this->setChecked(show);
}
