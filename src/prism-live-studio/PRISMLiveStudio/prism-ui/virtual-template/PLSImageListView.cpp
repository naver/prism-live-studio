#include "PLSImageListView.h"
#include "ui_PLSImageListView.h"
#include "flowlayout.h"
#include "PLSMotionItemView.h"
#include "PLSMotionFileManager.h"
#include "CategoryVirtualTemplate.h"
#include "PLSMotionImageListView.h"
#include "PLSAlertView.h"
#include "obs-app.hpp"
#include "PLSMotionAddButton.h"
#include "PLSVirtualBgManager.h"
#include "utils-api.h"
#include <QScrollArea>
#include <QPushButton>
#include <QFile>
#include <QVBoxLayout>
#include <QScrollBar>
#include "liblog.h"
#include "PLSBasic.h"

const int FLOW_LAYOUT_HSPACING = 12;
const int FLOW_LAYOUT_VSPACING = 12;
const int FLOW_LAYOUT_MARGIN_LEFT_RIGHT = 0;
const int FLOW_LAYOUT_MARGIN_TOP_BOTTOM = 0;

PLSImageListView::PLSImageListView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSImageListView>();
	ui->setupUi(this);

	initScrollArea();
	initScrollAreaLayout();
}

PLSImageListView::~PLSImageListView()
{
	pls_delete(ui);

	while (!itemViews.isEmpty()) {
		auto view = itemViews.takeFirst();
		PLSMotionItemView::dealloc(view);
	}
}

int PLSImageListView::flowLayoutHSpacing() const
{
	return m_flowLayoutHSpacing;
}

void PLSImageListView::setFlowLayoutHSpacing(int flowLayoutHSpacing)
{
	m_flowLayoutHSpacing = flowLayoutHSpacing;
	m_FlowLayout->setHorizontalSpacing(flowLayoutHSpacing);
	m_FlowLayout->invalidate();
}

int PLSImageListView::flowLayoutVSpacing() const
{
	return m_flowLayoutVSpacing;
}

void PLSImageListView::setFlowLayoutVSpacing(int flowLayoutVSpacing)
{
	m_flowLayoutVSpacing = flowLayoutVSpacing;
	m_FlowLayout->setverticalSpacing(flowLayoutVSpacing);
	m_FlowLayout->invalidate();
}

void PLSImageListView::insertAddFileItem()
{
	m_addButton = new PLSMotionAddButton;
	m_addButton->setObjectName("addButtonView");
	connect(m_addButton, &PLSMotionAddButton::pressed, this, &PLSImageListView::addFileButtonClicked);
	m_FlowLayout->addWidget(m_addButton);
}

int PLSImageListView::getShowViewListCount() const
{
	return itemViews.count();
}

bool PLSImageListView::setSelectedItem(const QString &itemId)
{
	if (!m_enabled) {
		return false;
	}

	if (PLSMotionItemView *prev = findItem(m_itemId); prev) {
		prev->setSelected(false);
	}

	m_itemId = itemId;

	if (PLSMotionItemView *curr = findItem(m_itemId); curr) {
		curr->setSelected(true);
		itemViewSelectedStateChanged(curr);
		return true;
	}
	return false;
}

void PLSImageListView::setItemSelectedEnabled(bool enabled)
{
	m_enabled = enabled;

	if (!enabled) {
		clearSelectedItem();
	}
}

void PLSImageListView::clearSelectedItem()
{
	if (PLSMotionItemView *view = findItem(m_itemId); view) {
		view->setSelected(false);

		if (PLSMotionImageListView *listView = getListView(); listView) {
			listView->setCheckBoxEnabled(false);
		}
	}

	m_itemId.clear();
}

void PLSImageListView::deleteItem(const QString &itemId, bool isRecent)
{
	PLS_DEBUG("ss", "delete item: %s", itemId.toUtf8().constData());

	bool found = false;
	for (int i = 0, count = m_mds.count(); i < count; ++i) {
		if (m_mds[i].itemId == itemId) {
			found = true;
			m_mds.removeAt(i);
			break;
		}
	}

	if (found) {
		updateItems(m_mds);
	}

	if (!isRecent && (itemId == m_itemId)) {
		if (PLSMotionImageListView *listView = getListView(); listView) {
			listView->setSelectedItemFailed(itemId);
		}
	}
}

void PLSImageListView::deleteAll()
{
	m_mds.clear();
	while (!itemViews.isEmpty()) {
		PLSMotionItemView *view = itemViews.takeLast();
		m_FlowLayout->removeWidget(view);
		PLSMotionItemView::dealloc(view);
	}
}

void PLSImageListView::setFilterButtonVisible(bool visible)
{
	m_filterWidget->setVisible(visible);
}

void PLSImageListView::setDeleteAllButtonVisible(bool visible)
{
	m_deleteAllWidget->setVisible(visible);
}

void PLSImageListView::itemViewSelectedStateChanged(const PLSMotionItemView *item)
{
	if (PLSMotionImageListView *listView = getListView(); listView) {
		const auto &motionData = item->motionData();
		listView->setCheckBoxEnabled(motionData.type == MotionType::MOTION && (!motionData.canDelete));
	}
}

int PLSImageListView::getVerticalScrollBarPosition() const
{
	const QScrollBar *scrollBar = m_scrollList->verticalScrollBar();
	return scrollBar->value();
}

void PLSImageListView::setVerticalScrollBarPosition(int value)
{
	m_scrollBarPosition = value;
}

bool PLSImageListView::isForProperites()
{
	if (const PLSMotionImageListView *listView = getListView(); listView) {
		return listView->viewType() == PLSMotionViewType::PLSMotionPropertyView;
	}
	return false;
}

void PLSImageListView::updateItems(const QList<MotionData> &mds)
{
	this->m_mds = mds;

	bool properties = isForProperites();
	if (itemViews.count() <= mds.count()) {
		for (int i = 0, count = itemViews.count(); i < count; ++i) {
			const MotionData &md = mds[i];
			PLSMotionItemView *view = itemViews[i];
			view->setMotionData(md, properties);
			view->setIconHidden(md.type != MotionType::MOTION);
			view->setSelected(m_enabled && (m_itemId == md.itemId));
		}

		for (int i = 0, j = itemViews.count(), count = mds.count() - itemViews.count(); i < count; ++i, ++j) {
			const MotionData &md = mds[j];
			PLSMotionItemView *view = PLSMotionItemView::alloc(md, properties, this, &PLSImageListView::movieViewClicked, &PLSImageListView::deleteFileButtonClicked);
			view->setObjectName("movieView");
			view->setIconHidden(md.type != MotionType::MOTION);
			m_FlowLayout->addWidget(view);
			pls_flush_style_recursive(view);
			view->setSelected(m_enabled && (m_itemId == md.itemId));
			itemViews.append(view);
			view->show();
		}
	} else {
		for (int i = 0, count = mds.count(); i < count; ++i) {
			const MotionData &md = mds[i];
			PLSMotionItemView *view = itemViews[i];
			view->setMotionData(md, properties);
			view->setIconHidden(md.type != MotionType::MOTION);
			view->setSelected(m_enabled && (m_itemId == md.itemId));
		}

		while (itemViews.count() > mds.count()) {
			PLSMotionItemView *view = itemViews.takeLast();
			m_FlowLayout->removeWidget(view);
			PLSMotionItemView::dealloc(view);
		}
	}
	m_FlowLayout->invalidate();
}

void PLSImageListView::updateItem(const MotionData &md)
{
	for (auto &it : m_mds) {
		if (it.itemId == md.itemId) {
			it = md;
			break;
		}
	}

	for (auto it : itemViews) {
		if (it->motionData().itemId == md.itemId) {
			it->setMotionData(md, isForProperites());
			break;
		}
	}
}

PLSMotionItemView *PLSImageListView::findItem(const QString &itemId)
{
	for (PLSMotionItemView *view : itemViews) {
		if (view->motionData().itemId == itemId) {
			return view;
		}
	}
	return nullptr;
}

void PLSImageListView::triggerSelectedSignal()
{
	if (m_itemId.isEmpty()) {
		return;
	}

	auto view = findItem(m_itemId);
	if (!view || !view->selected()) {
		return;
	}

	//check if current selected item not display on list view
	if (PLSMotionImageListView *listView = getListView(); listView) {
		listView->clickItemWithSendSignal(view->motionData());
	}
}

void PLSImageListView::scrollToItem(const QString &itemId)
{
	QMetaObject::invokeMethod(
		this,
		[this, itemId]() {
			auto view = findItem(itemId);
			if (view == nullptr) {
				return;
			}
			m_scrollBarPosition = view->pos().y();
			scrollToSelectedItemIndex();
		},
		Qt::QueuedConnection);
}

void PLSImageListView::setAutoUpdateCloseButtons(bool autoUpdateCloseButtons)
{
	this->m_autoUpdateCloseButtons = autoUpdateCloseButtons;
}

void PLSImageListView::updateCloseButtons()
{
	QPoint cursorPos = QCursor::pos();
	for (int i = 0, count = itemViews.count(); i < count; ++i) {
		itemViews[i]->setCloseButtonHiddenByCursorPosition(cursorPos);
	}
}

void PLSImageListView::showBottomMargin() const
{
	m_scrollAreaWidget->layout()->setContentsMargins(0, 0, 0, 20);
}

PLSMotionImageListView *PLSImageListView::getListView()
{
	for (QWidget *parent = this->parentWidget(); parent; parent = parent->parentWidget()) {
		if (auto view = dynamic_cast<PLSMotionImageListView *>(parent); view) {
			return view;
		}
	}
	return nullptr;
}

void PLSImageListView::scrollToSelectedItemIndex() const
{
	if (m_scrollBarPosition > 0) {
		m_scrollList->verticalScrollBar()->setValue(m_scrollBarPosition);
	}
}

void PLSImageListView::initScrollArea()
{
	m_scrollList = pls_new<QScrollArea>(this);
	m_scrollList->setObjectName("MotionScrollArea");
	m_scrollList->setTabletTracking(true);
	m_scrollList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	QSizePolicy sizePolicy_(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sizePolicy_.setHorizontalStretch(0);
	sizePolicy_.setVerticalStretch(0);
	sizePolicy_.setHeightForWidth(m_scrollList->sizePolicy().hasHeightForWidth());
	m_scrollList->setSizePolicy(sizePolicy_);
	m_scrollList->setWidgetResizable(true);
}

void PLSImageListView::initScrollAreaLayout()
{
	auto widgetContent = pls_new<QWidget>();
	widgetContent->setMouseTracking(true);
	widgetContent->setObjectName("widgetContent");
	widgetContent->installEventFilter(this);
	m_scrollAreaWidget = widgetContent;

	QVBoxLayout *vlayout = pls_new<QVBoxLayout>(widgetContent);
	vlayout->setSpacing(0);
	vlayout->setContentsMargins(0, 0, 0, 0);

	m_FlowLayout = pls_new<FlowLayout>(nullptr, 0, m_flowLayoutVSpacing, m_flowLayoutVSpacing);
	m_FlowLayout->setItemRetainSizeWhenHidden(false);
	m_FlowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_FlowLayout->setContentsMargins(FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM, FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM);

	//remove all background widget
	m_deleteAllWidget = pls_new<QWidget>(widgetContent);
	m_deleteAllWidget->setObjectName("deleteAllWidget");
	m_deleteAllWidget->setVisible(false);

	auto deleteLayout = pls_new<QHBoxLayout>(m_deleteAllWidget);
	deleteLayout->setContentsMargins(0, 0, 0, 0);
	deleteLayout->setSpacing(0);

	auto deleteAllBtn = pls_new<QPushButton>(tr("virtual.mylist.deleteAll.tip"));
	deleteAllBtn->setObjectName("deleteAllBtn");
	connect(deleteAllBtn, &QPushButton::clicked, this, &PLSImageListView::deleteAllButtonClicked);
	deleteLayout->addWidget(deleteAllBtn, 0, Qt::AlignLeft);

	//filter background widget
	m_filterWidget = pls_new<QWidget>();
	auto filterLayout = pls_new<QHBoxLayout>(m_filterWidget);
	filterLayout->setContentsMargins(0, 30, 0, 30);
	filterLayout->setSpacing(0);

	auto filterBtn = pls_new<QPushButton>(tr("Filters"));
	filterBtn->setObjectName("previewBtn");
	connect(filterBtn, &QPushButton::clicked, this, &PLSImageListView::filterButtonClicked);
	filterLayout->addWidget(filterBtn, 0, Qt::AlignLeft);

	vlayout->addLayout(m_FlowLayout);
	vlayout->addWidget(m_deleteAllWidget);
	vlayout->addStretch(10);
	vlayout->addWidget(m_filterWidget);

	m_scrollList->setWidget(widgetContent);
	ui->verticalLayout->addWidget(m_scrollList);
}

void PLSImageListView::movieViewClicked(const PLSMotionItemView *movieView)
{
	if (movieView->selected() || !m_enabled) {
		return;
	}

	const MotionData &motionData = movieView->motionData();
	if (!PLSMotionFileManager::instance()->isValidMotionData(motionData)) {
		if (motionData.canDelete) {
			pls_alert_error_message(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.Title"), QTStr("virtual.resource.localfile.disappear.tip"));
			return;
		} else {
#if 0
			PLSAlertView::warning(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.Title"), QTStr("virtual.resource.file.disappaer.tip"));
			return;
#endif
		}
	}

	if (PLSMotionImageListView *listView = getListView(); listView) {
		listView->clickItemWithSendSignal(motionData);
	}
}

void PLSImageListView::filterButtonClicked()
{
	if (PLSMotionImageListView *listView = getListView(); listView) {
		listView->filterButtonClicked();
	}
}

void PLSImageListView::deleteAllButtonClicked()
{
	if (PLSAlertView::question(PLSBasic::instance()->GetPropertiesWindow(), tr("Alert.Title"), tr("virtual.mylist.deleteAll.popup"), PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel) !=
	    PLSAlertView::Button::Ok) {
		return;
	}

	if (PLSMotionImageListView *listView = getListView(); listView) {
		MotionFileManage->deleteAllMyResources(listView);
	}
}

void PLSImageListView::deleteFileButtonClicked(const MotionData &data)
{
	PLSMotionImageListView *top = getListView();
	if (!top) {
		return;
	}

	bool isVbUsed = false;
	bool isSourceUsed = false;
	PLSVirtualBgManager::checkResourceIsUsed(data.itemId, isVbUsed, isSourceUsed);

	bool isRecent = top->isRecentTab(this);
	if (!isRecent) {
		// recent no popup
		if (isVbUsed || isSourceUsed) {
			if (PLSAlertView::warning(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.Title"), QTStr("virtual.resource.file.using.delete.tip"),
						  PLSAlertView::Button::Yes | PLSAlertView::Button::No) != PLSAlertView::Button::Yes) {
				return;
			}
		} else {
			if (PLSAlertView::warning(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.Title"), QTStr("virtual.resource.file.delete.tip"),
						  PLSAlertView::Button::Yes | PLSAlertView::Button::No) != PLSAlertView::Button::Yes) {
				return;
			}
		}
	}

	top->deleteItemWithSendSignal(data, isVbUsed, isSourceUsed, isRecent);
}

bool PLSImageListView::eventFilter(QObject *obj, QEvent *evt)
{
	if (m_autoUpdateCloseButtons && (obj == m_scrollAreaWidget) && (evt->type() == QEvent::Resize)) {
		updateCloseButtons();
	}

	return QFrame::eventFilter(obj, evt);
}
