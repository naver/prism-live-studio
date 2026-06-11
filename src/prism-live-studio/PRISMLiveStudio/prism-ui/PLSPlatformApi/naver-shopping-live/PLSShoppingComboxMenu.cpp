#include "PLSShoppingComboxMenu.h"
#include "frontend-api.h"
#include <QTimer>
#include <qevent.h>
#include "libui.h"
#include <QHBoxLayout>

const int listItemMaxCount = 7;

PLSShoppingComboxMenu::PLSShoppingComboxMenu(QWidget *parent) : QDialog(parent)
{
	pls_add_css(this, {"PLSShoppingComboxMenu"});

	m_listWidget = pls_new<QListWidget>(this);
	m_listWidget->setAutoScroll(false);
	m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_listWidget->setObjectName("listWidget");
	m_listWidget->setHidden(true);
	setSizePolicy(m_listWidget);

	m_calenderWidget = pls_new<PLSShoppingCalenderWidget>(this);
	m_calenderWidget->setHidden(true);
	setSizePolicy(m_calenderWidget);

	this->installEventFilter(this);

	connect(m_calenderWidget, &PLSShoppingCalenderWidget::sizeChanged, this, [this](QSize size) {
		int borderWidth = 1;
		int width = size.width() + borderWidth * 2;
		int height = size.height() + borderWidth * 2;
		this->setFixedSize(QSize(width, height));
	});

	auto layout = pls_new<QHBoxLayout>();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(m_calenderWidget);
	layout->addWidget(m_listWidget);
	layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
	this->setLayout(layout);

	QLocale locale = QLocale::English;
	if (IS_KR()) {
		locale = QLocale::Korean;
	}
	QTime aTime(8, 1);
	QTime pTime(16, 1);
	QString aString = locale.toString(aTime, "A");
	QString pString = locale.toString(pTime, "A");
	QList<QString> list;
	m_apList.append(aString);
	m_apList.append(pString);

	setWindowFlags(Qt::FramelessWindowHint | Qt::Popup | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
}

PLSShoppingCalenderWidget *PLSShoppingComboxMenu::calender()
{
	return m_calenderWidget;
}

QListWidget *PLSShoppingComboxMenu::listWidget()
{
	return m_listWidget;
}

void PLSShoppingComboxMenu::showDateCalender(const QDate &date)
{
	m_calenderWidget->setHidden(false);
	m_calenderWidget->setCurrentPage(date.year(), date.month());
	m_calenderWidget->setSelectedDate(date);
	m_calenderWidget->showShoppingSelectedDate(date);
}

void PLSShoppingComboxMenu::showHour(QString hour)
{
	showDateTimeList(DateTimeType::DateHour, hour);
}

void PLSShoppingComboxMenu::showMinute(QString minute)
{
	showDateTimeList(DateTimeType::DateMinute, minute);
}

void PLSShoppingComboxMenu::showAp(QString ap)
{
	int itemHeight = 40;
	int itemWeight = 52;
	if (m_listWidget->count() == 0) {
		for (int i = 0; i < m_apList.count(); i++) {
			auto item = pls_new<QListWidgetItem>(m_apList.at(i));
			item->setSizeHint(QSize(itemWeight, itemHeight));
			item->setTextAlignment(Qt::AlignCenter);
			m_listWidget->addItem(item);
			m_timeList.append(m_apList.at(i));
		}
	}

	for (int i = 0; i < m_listWidget->count(); i++) {
		QListWidgetItem *item = m_listWidget->item(i);
		item->setSizeHint(QSize(itemWeight, itemHeight));
		bool selected = false;
		if (item->text() == ap)
			selected = true;
		item->setSelected(selected);
	}

	int listHeight = m_timeList.count() * itemHeight;
	m_listWidget->setFixedSize(itemWeight, listHeight);
	this->setFixedSize(QSize(itemWeight, listHeight));
}

const QStringList &PLSShoppingComboxMenu::apList() const
{
	return m_apList;
}

void PLSShoppingComboxMenu::setSizePolicy(QWidget *widget) const
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSShoppingComboxMenu::showDateTimeList(DateTimeType type, QString selectedTime)
{
	int itemHeight = 40;
	int itemWeight = 46;
	if (m_listWidget->count() == 0) {
		int start = 1;
		int count = 0;
		if (type == DateTimeType::DateHour) {
			count = 12;
		} else if (type == DateTimeType::DateMinute) {
			count = 59;
			start = 0;
		}
		for (; start <= count; start++) {
			QString value = QString("%1").arg(start, 2, 10, QLatin1Char('0'));
			auto item = pls_new<QListWidgetItem>(value);
			item->setSizeHint(QSize(itemWeight, itemHeight));
			item->setTextAlignment(Qt::AlignCenter);
			m_listWidget->addItem(item);
			m_timeList.append(value);
		}
	}

	for (int i = 0; i < m_listWidget->count(); i++) {
		QListWidgetItem *item = m_listWidget->item(i);
		item->setSizeHint(QSize(itemWeight, itemHeight));
		bool selected = false;
		if (item->text() == selectedTime) {
			selected = true;
			m_listWidget->setCurrentItem(item);
		}
		item->setSelected(selected);
	}

	int count = m_timeList.count() > listItemMaxCount ? listItemMaxCount : m_timeList.count();
	int listHeight = count * itemHeight;
	m_listWidget->setFixedSize(itemWeight, listHeight);
	this->setFixedSize(QSize(itemWeight, listHeight));
	m_listWidget->scrollToItem(m_listWidget->currentItem(), QAbstractItemView::EnsureVisible);
}

void PLSShoppingComboxMenu::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	emit isShow(true);
}

void PLSShoppingComboxMenu::hideEvent(QHideEvent *event)
{
	QDialog::hideEvent(event);
	emit isShow(false);
}

bool PLSShoppingComboxMenu::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == this && event->type() == QEvent::MouseButtonPress) {
		QRect parentRect(0, 0, parentWidget()->width(), parentWidget()->height());
		auto mouseEvent = static_cast<QMouseEvent *>(event);
		QPoint clickPoint = parentWidget()->mapFromGlobal(mouseEvent->globalPos());
		if (parentRect.contains(clickPoint)) {
			QTimer::singleShot(1, this, [this] { this->setHidden(true); });
			return true;
		}
	}
	return QDialog::eventFilter(watched, event);
}
