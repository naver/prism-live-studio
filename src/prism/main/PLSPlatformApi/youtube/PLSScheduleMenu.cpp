#include "PLSScheduleMenu.h"
#include <QListWidget>
#include <QHBoxLayout>
#include <QWidgetAction>
#include <qdebug.h>
#include <QEvent>
#include <QKeyEvent>

PLSScheduleMenu::PLSScheduleMenu(QWidget *parent) : QMenu(parent)
{

	m_listWidget = new QListWidget(this);

	connect(m_listWidget, &QListWidget::itemClicked, this, &PLSScheduleMenu::itemDidSelect);

	m_listWidget->setObjectName("scheduleListWidget");

	QHBoxLayout *layout = new QHBoxLayout(m_listWidget);
	layout->setContentsMargins(0, 0, 0, 0);
	m_listWidget->setLayout(layout);
	QWidgetAction *action = new QWidgetAction(this);
	action->setDefaultWidget(m_listWidget);
	this->addAction(action);
	this->installEventFilter(this);
	this->setWindowFlag(Qt::NoDropShadowWindowHint);
}

void PLSScheduleMenu::setupDatas(const vector<ComplexItemData> &datas, int btnWidth)
{
	const static int maxShowCount = 5;
	int currentCount = datas.size() > maxShowCount ? maxShowCount : datas.size();
	m_listWidget->clear();
	m_listWidget->setFixedSize(btnWidth, currentCount * 70 + 4);
	this->setFixedSize(btnWidth, currentCount * 70 + 4);

	for (int i = 0; i < datas.size(); i++) {
		QListWidgetItem *item = new QListWidgetItem();
		item->setData(Qt::UserRole, QVariant::fromValue<ComplexItemData>(datas[i]));
		PLSScheduleMenuItem *widget = new PLSScheduleMenuItem(datas[i], btnWidth);
		m_listWidget->addItem(item);
		m_listWidget->setItemWidget(item, widget);
	}
}

void PLSScheduleMenu::itemDidSelect(QListWidgetItem *item)
{
	ComplexItemData data = item->data(Qt::UserRole).value<ComplexItemData>();
	emit scheduleItemClicked(data._id);
	this->setHidden(true);
}

bool PLSScheduleMenu::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (i_Object == this && i_Event->type() == QEvent::KeyPress) {
		QKeyEvent *key = static_cast<QKeyEvent *>(i_Event);

		if ((key->key() != Qt::Key_Enter) && (key->key() != Qt::Key_Return)) {
			return true;
		}

		const QPoint localPoint = m_listWidget->viewport()->mapFromGlobal(QCursor::pos());
		QListWidgetItem *itemLocal = m_listWidget->itemAt(localPoint);
		if (itemLocal != nullptr) {
			itemDidSelect(itemLocal);
		} else {
			this->setHidden(true);
		}

		return true;
	} else {
		return QWidget::eventFilter(i_Object, i_Event);
	}
}

PLSScheduleMenu::~PLSScheduleMenu() {}
