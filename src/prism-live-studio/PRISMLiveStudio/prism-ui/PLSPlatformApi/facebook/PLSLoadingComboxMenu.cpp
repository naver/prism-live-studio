#include "PLSLoadingComboxMenu.h"
#include "ui_PLSLoadingComboxMenu.h"
#include <QWidgetAction>
#include "PLSLoadingTitleItem.h"
#include "PLSLoadingMenuItem.h"
#include "frontend-api.h"
#include <QMouseEvent>
#include <QPainterPath>
#include "libui.h"

const int TAG_HEIGHT = 40;

PLSLoadingComboxMenu::PLSLoadingComboxMenu(QWidget *parent) : QMenu(parent)
{
	ui = pls_new<Ui::PLSLoadingComboxMenu>();
	ui->setupUi(this);
	m_listWidget = pls_new<QListWidget>(this);
	m_listWidget->setObjectName("loadingListWidget");
	m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { emit itemDidSelect(item); });
	auto action = pls_new<QWidgetAction>(this);
	action->setDefaultWidget(m_listWidget);
	this->addAction(action);
	this->installEventFilter(this);
	pls_add_css(this, {"PLSLoadingComboxMenu"});
	setWindowFlags(windowFlags() | Qt::NoDropShadowWindowHint);
}

PLSLoadingComboxMenu::~PLSLoadingComboxMenu()
{
	pls_delete(ui);
}

void PLSLoadingComboxMenu::showLoading(QWidget *parent)
{
	int gameItemHeight = TAG_HEIGHT;
	m_parent = parent;
	m_listWidget->clear();
	auto loadingItem = pls_new<PLSLoadingTitleItem>();
	auto item = pls_new<QListWidgetItem>();
	auto itemData = PLSLoadingComboxItemData();
	itemData.type = PLSLoadingComboxItemType::Fa_Loading;
	item->setData(Qt::UserRole, QVariant::fromValue<PLSLoadingComboxItemData>(itemData));
	item->setSizeHint(QSize(0, gameItemHeight));
	m_listWidget->addItem(item);
	m_listWidget->setItemWidget(item, loadingItem);
	int borderHeight = 1;
	int paddingHeight = 1;
	m_listWidget->setFixedSize(parent->width() - borderHeight * 2, gameItemHeight + paddingHeight * 2);
	this->setFixedSize(parent->width(), gameItemHeight + paddingHeight * 2 + borderHeight * 2);
}

void PLSLoadingComboxMenu::showTitleListView(QWidget *parent, int showIndex, QStringList list)
{
	showTitleItemsView(parent, showIndex, PLSLoadingComboxItemType::Fa_NormalTitle, list);
}

void PLSLoadingComboxMenu::showGuideTipView(QWidget *parent, QString guide)
{
	QStringList list;
	list.append(guide);
	showTitleItemsView(parent, MENU_DONT_SELECTED_INDEX, PLSLoadingComboxItemType::Fa_Guide, list);
}

void PLSLoadingComboxMenu::setSelectedTitleItemIndex(int index) const
{
	if (m_items.size() <= index) {
		return;
	}
	for (auto item : m_items) {
		item->setSelected(false);
	}
	PLSLoadingMenuItem *titleItem = m_items.at(index);
	titleItem->setSelected(true);
}

bool PLSLoadingComboxMenu::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress) {
		QRect parentRect(0, 0, m_parent->width(), m_parent->height());
		auto mouseEvent = static_cast<QMouseEvent *>(event);
		QPoint clickPoint = m_parent->mapFromGlobal(mouseEvent->globalPos());
		if (parentRect.contains(clickPoint)) {
			QTimer::singleShot(1, [this] { this->setHidden(true); });
			return true;
		}
	}
	return QMenu::eventFilter(watched, event);
}

void PLSLoadingComboxMenu::setupCornerRadius()
{
	const int radius = 3;
	QPainterPath path;
	path.addRoundedRect(this->rect(), radius, radius);
	auto mask = QRegion(path.toFillPolygon().toPolygon());
	this->setMask(mask);
}

void PLSLoadingComboxMenu::showTitleItemsView(QWidget *parent, int showIndex, PLSLoadingComboxItemType type, QStringList list)
{
	m_parent = parent;
	m_listWidget->clear();
	m_items.clear();
	int gameItemHeight = TAG_HEIGHT;
	for (int i = 0; i < list.size(); i++) {
		QString title = list.at(i);
		auto titleItem = pls_new<PLSLoadingMenuItem>();
		titleItem->setTitle(title);
		auto item = pls_new<QListWidgetItem>();
		auto itemData = PLSLoadingComboxItemData();
		itemData.showIndex = i;
		itemData.title = title;
		itemData.type = type;
		item->setData(Qt::UserRole, QVariant::fromValue<PLSLoadingComboxItemData>(itemData));
		item->setSizeHint(QSize(0, gameItemHeight));
		m_listWidget->addItem(item);
		m_listWidget->setItemWidget(item, titleItem);
		m_items.insert(i, titleItem);
	}
	if (showIndex != MENU_DONT_SELECTED_INDEX) {
		m_listWidget->setCurrentRow(showIndex);
		setSelectedTitleItemIndex(showIndex);
	}
	int count = list.size();
	if (count > 5) {
		count = 5;
	}
	const int paddingHeight = 1;
	m_listWidget->setFixedSize(parent->width(), gameItemHeight * count + paddingHeight * 2);
	this->setFixedSize(parent->width(), gameItemHeight * count + paddingHeight * 2);
#if defined(Q_OS_WIN)
	setupCornerRadius();
#endif
}
