#include "PLSMotionImageListView.h"
#include "ui_PLSMotionImageListView.h"
#include "PLSMotionFileManager.h"
#include "PLSMotionNetwork.h"
#include "PLSDpiHelper.h"
#include <QButtonGroup>
#include <QTimer>
#include <QDebug>
#include "PLSMotionErrorLayer.h"
#include "PLSMotionItemView.h"
#include "PLSVirtualBgManager.h"
#include "window-basic-main.hpp"

extern void sourceClearBackgroundTemplate(const QStringList &itemIds);

PLSMotionImageListView::PLSMotionImageListView(QWidget *parent, PLSMotionViewType type, const std::function<void(QWidget *)> &init)
	: QFrame(parent), ui(new Ui::PLSMotionImageListView), m_buttonGroup(new QButtonGroup(this))
{
	if (init) {
		init(this);
	}

	PLSDpiHelper dpiHelper;
	dpiHelper.setCss(this, {PLSCssIndex::PLSMotionImageListView, PLSCssIndex::PLSLoadingBtn, PLSCssIndex::PLSMotionErrorLayer});
	setCursor(Qt::ArrowCursor);
	ui->setupUi(this);
	ui->checkBoxMotionOff->setEnabled(false);
	ui->prismView->setForProperties(type == PLSMotionViewType::PLSMotionPropertyView);
	ui->freeView->setForProperties(type == PLSMotionViewType::PLSMotionPropertyView);
	m_viewType = type;

	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::checkedRemoved, this, &PLSMotionImageListView::onCheckedRemoved, Qt::QueuedConnection);

	initCheckBox();
	initButton();
	initScrollArea();
	initCategoryIndex();
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::checkedRemoved, this, &PLSMotionImageListView::onCheckedRemoved, Qt::QueuedConnection);
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::deleteMyResources, this, &PLSMotionImageListView::deleteAllResources, Qt::DirectConnection);

	if (type == PLSMotionViewType::PLSMotionDetailView) {
		ui->recentView->getImageListView()->showBottomMargin();
		ui->prismView->getImageListView()->showBottomMargin();
		ui->freeView->getImageListView()->showBottomMargin();
		ui->myView->getImageListView()->showBottomMargin();
	} else {
		connect(
			PLSBasic::Get(), &PLSBasic::backgroundTemplateSourceError, this,
			[this]() {
				if (!m_currentMotionData.itemId.isEmpty()) {
					sourceClearBackgroundTemplate({m_currentMotionData.itemId});
				}

				clearSelectedItem();
				showApplyErrorToast();
			},
			Qt::QueuedConnection);
	}

	dpiHelper.notifyDpiChanged(this, [this](double, double, bool isFirstShow) {
		if (isFirstShow) {
			QWidget *widget = pls_get_toplevel_view(this);
			widget->installEventFilter(this);
		}
	});
}

PLSMotionImageListView::~PLSMotionImageListView()
{
	QString key = VIRTUAL_CATEGORY_POSITION;
	QString categoryIndex = PLSMotionFileManager::instance()->categoryIndex(VIRTUAL_CATEGORY_INDEX);
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_CATEGORY_POSITION;
		categoryIndex = PLSMotionFileManager::instance()->categoryIndex(PROPERTY_CATEGORY_INDEX);
	}
	delete ui;
}

PLSMotionViewType PLSMotionImageListView::viewType()
{
	return m_viewType;
}

void PLSMotionImageListView::clickItemWithSendSignal(const MotionData &motionData)
{
	if (PLSMotionNetwork::instance()->isPathEqual(m_currentMotionData, motionData)) {
		return;
	}

	m_currentMotionData = motionData;

	QString key = VIRTUAL_BACKGROUND_RECENT_LIST;
	QString categoryKey = VIRTUAL_CATEGORY_INDEX;
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_RECENT_LIST;
		categoryKey = PROPERTY_CATEGORY_INDEX;
	}
	setSelectedItem(motionData.itemId);
	if (motionData.dataType == DataType::MYLIST) {
		PLSMotionFileManager::instance()->saveCategoryIndex(m_buttonId, categoryKey);
	} else if (motionData.dataType == DataType::PRISM) {
		PLSMotionFileManager::instance()->saveCategoryIndex(m_buttonId, categoryKey);
	} else if (motionData.dataType == DataType::FREE) {
		PLSMotionFileManager::instance()->saveCategoryIndex(m_buttonId, categoryKey);
	}

	PLSMotionFileManager::instance()->insertMotionData(motionData, key);

	setCheckBoxEnabled(motionData.type == MotionType::MOTION && PLSMotionNetwork::instance()->isPrismOrFree(motionData));

	emit currentResourceChanged(motionData.itemId, static_cast<int>(motionData.type), motionData.resourcePath, motionData.staticImgPath, motionData.thumbnailPath,
				    PLSMotionNetwork::instance()->isPrismOrFree(motionData), motionData.foregroundPath, motionData.foregroundStaticImgPath);
}

void PLSMotionImageListView::clickItemWithSendSignal(int groupIndex, int itemIndex)
{
	if (groupIndex < 0) {
		return;
	}

	//stackWidget choose correspond index
	m_buttonId = groupIndex;
	m_buttonGroup->button(groupIndex)->setChecked(true);
	ui->stackedWidget->setCurrentIndex(groupIndex);

	if (itemIndex < 0) {
		return;
	}

	//get QList data by corespond groupIndex
	QList<MotionData> list;
	QString key = VIRTUAL_BACKGROUND_RECENT_LIST;
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_RECENT_LIST;
	}

	if (m_buttonId == 0) {
		list = PLSMotionFileManager::instance()->getRecentMotionList(key);
	} else if (m_buttonId == 1) {
		list = PLSMotionNetwork::instance()->getPrismCacheList();
	} else if (m_buttonId == 2) {
		list = PLSMotionNetwork::instance()->getFreeCacheList();
	} else if (m_buttonId == 3) {
		list = PLSMotionFileManager::instance()->getMyMotionList();
	}

	if (itemIndex >= list.size())
		return;

	//call the click item view with motionData
	MotionData motionData = list.at(itemIndex);
	clickItemWithSendSignal(motionData);
}

void PLSMotionImageListView::setSelectedItem(const QString &itemId)
{
	if (!m_itemSelectedEnabled) {
		return;
	}

	bool prismResult = ui->prismView->setSelectedItem(itemId);
	bool freeResult = ui->freeView->setSelectedItem(itemId);
	bool myResult = ui->myView->setSelectedItem(itemId);
	bool recentResult = ui->recentView->setSelectedItem(itemId);
	if (!prismResult && !freeResult && !myResult && !recentResult) {
		emit setSelectedItemFailed(itemId);
	}
}

void PLSMotionImageListView::deleteItemWithSendSignal(const MotionData &motionData, bool isVbUsed, bool isSourceUsed, bool isRecent)
{
	QString itemId = motionData.itemId;
	DataType dataType = motionData.dataType;

	QString key;
	if (dataType == DataType::PROP_RECENT) {
		key = PROPERTY_RECENT_LIST;
		ui->recentView->deleteItem(itemId);
	} else if (dataType == DataType::VIRTUAL_RECENT) {
		key = VIRTUAL_BACKGROUND_RECENT_LIST;
		ui->recentView->deleteItem(itemId);
	} else if (dataType == DataType::MYLIST) {
		key = MY_FILE_LIST;
		ui->myView->deleteItem(itemId);
	}

	PLSMotionFileManager::instance()->deleteMotionData(this, itemId, key, isVbUsed, isSourceUsed);

	if (!isRecent && (((m_viewType == PLSMotionViewType::PLSMotionDetailView) && isVbUsed) || ((m_viewType == PLSMotionViewType::PLSMotionPropertyView) && isSourceUsed))) {
		clearSelectedItem();
	}

	if (!isRecent) {
		deleteCurrentResource(itemId);
	}
}

void PLSMotionImageListView::setItemSelectedEnabled(bool enabled)
{
	m_itemSelectedEnabled = enabled;
	ui->prismView->setItemSelectedEnabled(enabled);
	ui->freeView->setItemSelectedEnabled(enabled);
	ui->recentView->setItemSelectedEnabled(enabled);
	ui->myView->setItemSelectedEnabled(enabled);
	if (!enabled) {
		m_currentMotionData = MotionData();
	}
}

void PLSMotionImageListView::clearSelectedItem()
{
	m_currentMotionData = MotionData();
	ui->prismView->clearSelectedItem();
	ui->freeView->clearSelectedItem();
	ui->recentView->clearSelectedItem();
	ui->myView->clearSelectedItem();
}

void PLSMotionImageListView::setCheckState(bool checked)
{
	ui->checkBoxMotionOff->setChecked(checked);
}

void PLSMotionImageListView::setCheckBoxEnabled(bool enabled)
{
	ui->checkBoxMotionOff->setEnabled(enabled);
}

void PLSMotionImageListView::setFilterButtonVisible(bool visible)
{
	ui->prismView->setFilterButtonVisible(visible);
	ui->freeView->setFilterButtonVisible(visible);
	ui->recentView->setFilterButtonVisible(visible);
	ui->myView->setFilterButtonVisible(visible);
}

void PLSMotionImageListView::showApplyErrorToast()
{
	if (m_errorToast == nullptr) {
		m_errorToast = new PLSMotionErrorLayer(this);
	}

	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		m_errorToast->setToastText(tr("virtual.resource.execute.fail.tip.no.newline"));
		m_errorToast->setToastTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}

	m_errorToast->showView();
	adjustErrTipSize();
}

void PLSMotionImageListView::switchToSelectItem(const QString &itemId)
{
	if (!m_itemSelectedEnabled) {
		return;
	}

	bool prismResult = ui->prismView->setSelectedItem(itemId);
	bool freeResult = ui->freeView->setSelectedItem(itemId);
	bool myResult = ui->myView->setSelectedItem(itemId);
	bool recentResult = ui->recentView->setSelectedItem(itemId);

	int buttonId = 1; // prism button
	if (!prismResult && !freeResult && !myResult && !recentResult) {
		emit setSelectedItemFailed(itemId);
	} else if (prismResult) {
		buttonId = 1; // prism button
	} else if (freeResult) {
		buttonId = 2; // free button
	} else if (myResult) {
		buttonId = 3; // my button
	}

	if (m_buttonId != buttonId) {
		m_buttonId = buttonId;
		m_buttonGroup->button(m_buttonId)->setChecked(true);
		ui->stackedWidget->setCurrentIndex(m_buttonId);
	}
}

bool PLSMotionImageListView::isRecentTab(PLSImageListView *view) const
{
	if (ui->recentView->getImageListView() == view) {
		return true;
	}
	return false;
}

void PLSMotionImageListView::deleteAllResources()
{
	ui->myView->deleteAll();
	QList<MotionData> myList = PLSMotionFileManager::instance()->getMyMotionList();
	QStringList idList;
	for (const MotionData &data : myList) {
		ui->recentView->deleteItem(data.itemId);
		idList.append(data.itemId);
	}
	if (idList.size() > 0) {
		emit removeAllMyResource(idList);
	}
}

bool PLSMotionImageListView::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == this) {
		if (event->type() == QEvent::Resize) {
			adjustErrTipSize(static_cast<QResizeEvent *>(event)->size());
		} else if (event->type() == QEvent::Move) {
			adjustErrTipSize();
		}
	} else if (PLSWidgetDpiAdapter *adapter = dynamic_cast<PLSWidgetDpiAdapter *>(obj); adapter) {
		if (event->type() == QEvent::Move) {
			adjustErrTipSize();
		}
	}

	return __super::eventFilter(obj, event);
}

void PLSMotionImageListView::initCheckBox()
{
	QSizePolicy policy = ui->checkBoxMotionOff->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	ui->checkBoxMotionOff->setSizePolicy(policy);
	connect(ui->checkBoxMotionOff, &QCheckBox::clicked, this, &PLSMotionImageListView::checkState);
}

void PLSMotionImageListView::initButton()
{
	m_buttonGroup->addButton(ui->btn_recent, 0);
	m_buttonGroup->addButton(ui->btn_prism, 1);
	m_buttonGroup->addButton(ui->btn_free, 2);
	m_buttonGroup->addButton(ui->btn_my, 3);
	connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &PLSMotionImageListView::buttonGroupSlot);
	m_buttonId = 0;
}

void PLSMotionImageListView::initScrollArea()
{
	ui->gridLayout->setAlignment(Qt::AlignTop);
	ui->topScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	if (m_viewType == PLSMotionViewType::PLSMotionDetailView) {
		ui->gridLayout->setHorizontalSpacing(2);
		ui->checkBoxMotionOff->setHidden(true);
		setFilterButtonVisible(false);
	} else if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		ui->gridLayout->setHorizontalSpacing(15);
		setFilterButtonVisible(true);
	}
}

void PLSMotionImageListView::initCategoryIndex()
{
	ui->prismView->initListView();
	ui->freeView->initListView(true);

	//load category list
	QString key = VIRTUAL_BACKGROUND_RECENT_LIST;
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_RECENT_LIST;
	}
	QList<MotionData> recentList = PLSMotionFileManager::instance()->getRecentMotionList(key);
	ui->recentView->updateMotionList(recentList);
	QList<MotionData> myList = PLSMotionFileManager::instance()->getMyMotionList();
	ui->myView->updateMotionList(myList);

	m_buttonId = 1; // default prism tab
	m_buttonGroup->button(m_buttonId)->setChecked(true);
	ui->stackedWidget->setCurrentIndex(m_buttonId);
}

PLSImageListView *PLSMotionImageListView::getImageListView(int index)
{
	if (index == 1) {
		return ui->prismView->getImageListView();
	} else if (index == 2) {
		return ui->freeView->getImageListView();
	} else if (index == 3) {
		return ui->myView->getImageListView();
	} else if (index == 0) {
		return ui->recentView->getImageListView();
	}
	return nullptr;
}

void PLSMotionImageListView::adjustErrTipSize(QSize superSize)
{
	if (m_errorToast == nullptr || m_errorToast->isHidden()) {
		return;
	}
	if (superSize.isEmpty()) {
		superSize = this->size();
	}
	auto viewHeight = PLSDpiHelper::calculate(this, 50);
	auto gap = PLSDpiHelper::calculate(this, 10);
	QPoint pt = this->mapToGlobal(QPoint(gap, superSize.height() - viewHeight - gap));
	m_errorToast->setGeometry(pt.x(), pt.y(), superSize.width() - 2 * gap, PLSDpiHelper::calculate(this, 50));
}

void PLSMotionImageListView::onCheckedRemoved(const MotionData &md, bool isVbUsed, bool isSourceUsed)
{
	deleteCurrentResource(md.itemId);

	if (((m_viewType == PLSMotionViewType::PLSMotionDetailView) && isVbUsed) || ((m_viewType == PLSMotionViewType::PLSMotionPropertyView) && isSourceUsed)) {
		ui->myView->deleteItemEx(md.itemId);
		clearSelectedItem();
	}
}

void PLSMotionImageListView::buttonGroupSlot(int buttonId)
{
	if (m_buttonId == buttonId) {
		return;
	}
	if (ui->stackedWidget->count() <= buttonId) {
		return;
	}
	m_buttonId = buttonId;

	//change category clear scroll position
	QString positionKey = VIRTUAL_CATEGORY_POSITION;
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		positionKey = PROPERTY_CATEGORY_POSITION;
	}

	//show list view
	QString key = VIRTUAL_BACKGROUND_RECENT_LIST;
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_RECENT_LIST;
	}
	if (m_buttonId == 0) {
		QList<MotionData> list = PLSMotionFileManager::instance()->getRecentMotionList(key);
		ui->recentView->updateMotionList(list);
	} else if (m_buttonId == 3) {
		QList<MotionData> list = PLSMotionFileManager::instance()->getMyMotionList();
		ui->myView->updateMotionList(list);
	}
	m_buttonGroup->button(m_buttonId)->setChecked(true);
	ui->stackedWidget->setCurrentIndex(m_buttonId);
	emit clickCategoryIndex(m_buttonId);
}
