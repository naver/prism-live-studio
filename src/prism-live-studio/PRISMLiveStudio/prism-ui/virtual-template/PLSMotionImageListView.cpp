#include "PLSMotionImageListView.h"
#include "ui_PLSMotionImageListView.h"
#include "PLSMotionFileManager.h"
#include "PLSMotionNetwork.h"
#include <QButtonGroup>
#include <QTimer>
#include <QDebug>
#include "PLSMotionErrorLayer.h"
#include "PLSMotionItemView.h"
#include "PLSVirtualBgManager.h"
#include "window-basic-main.hpp"
#include "utils-api.h"
#include "PLSAlertView.h"
#include "pls/pls-source.h"
#include "liblog.h"
#include "PLSBasic.h"
#include "log/module_names.h"

extern void sourceClearBackgroundTemplate(const QStringList &itemIds);

PLSMotionImageListView::PLSMotionImageListView(QWidget *parent, PLSMotionViewType type, const std::function<void(QWidget *)> &init) : QFrame(parent), m_buttonGroup(pls_new<QButtonGroup>(this))
{
	ui = pls_new<Ui::PLSMotionImageListView>();
	if (init) {
		init(this);
	}

	setProperty("showHandCursor", true);
	pls_add_css(this, {"PLSMotionImageListView", "PLSLoadingBtn", "PLSMotionErrorLayer"});
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
			PLSBasic::instance(), &PLSBasic::backgroundTemplateSourceError, this,
			[this]() {
				if (!m_currentMotionData.itemId.isEmpty()) {
					sourceClearBackgroundTemplate({m_currentMotionData.itemId});
				}

				clearSelectedItem();
				showApplyErrorToast();
			},
			Qt::QueuedConnection);
	}

	QWidget *widget = pls_get_toplevel_view(this);
	widget->installEventFilter(this);
	//
	//dpiHelper.notifyDpiChanged(this, [this](double, double, bool isFirstShow) {
	//	if (isFirstShow) {
	//		QWidget *widget = pls_get_toplevel_view(this);
	//		widget->installEventFilter(this);
	//	}
	//});
}

PLSMotionImageListView::~PLSMotionImageListView()
{
	pls_object_remove(this);

	QString key = VIRTUAL_CATEGORY_POSITION;
	QString categoryIndex = PLSMotionFileManager::instance()->categoryIndex(VIRTUAL_CATEGORY_INDEX);
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_CATEGORY_POSITION;
		categoryIndex = PLSMotionFileManager::instance()->categoryIndex(PROPERTY_CATEGORY_INDEX);
	}
	pls_delete(ui);
}

PLSMotionViewType PLSMotionImageListView::viewType() const
{
	return m_viewType;
}

QString getCategoryString(int dataType)
{
	auto type = static_cast<DataType>(dataType);
	switch (type) {
	case DataType::UNKOWN:
		return "unknown";
	case DataType::PROP_RECENT:
	case DataType::VIRTUAL_RECENT:
		return "recent";
	case DataType::PRISM:
		return "prism";
	case DataType::FREE:
		return "free";
	case DataType::MYLIST:
		return "my";
	default:
		return "unknown";
	}
}

static QString processResourcePath(bool prismResource, const QString &resourcePath)
{
	if (prismResource)
		return pls_get_relative_config_path(resourcePath);
	return resourcePath;
}

static void updateCurrentClickedForTemplateSource(const MotionData &motionData)
{
	OBSSceneItem sceneItem = PLSBasic::instance()->GetCurrentSceneItem();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	if (!source) {
		return;
	}
	if (0 != strcmp(common::PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, obs_source_get_id(source))) {
		return;
	}
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "method", "get_current_clicked_id");
	pls_source_get_private_data(source, data);

	const char *id = obs_data_get_string(data, "itemId");
	if (0 == motionData.itemId.compare(id)) {
		bool prismResource = PLSMotionNetwork::instance()->isPrismOrFree(motionData);
		obs_data_set_bool(data, "prism_resource", prismResource);
		obs_data_set_string(data, "item_id", motionData.itemId.toUtf8().constData());
		obs_data_set_int(data, "item_type", static_cast<int>(motionData.type));
		obs_data_set_string(data, "file_path", processResourcePath(prismResource, motionData.resourcePath).toUtf8().constData());
		obs_data_set_string(data, "image_file_path", processResourcePath(prismResource, motionData.staticImgPath).toUtf8().constData());
		obs_data_set_string(data, "thumbnail_file_path", processResourcePath(prismResource, motionData.thumbnailPath).toUtf8().constData());
		obs_data_set_int(data, "category_id", static_cast<int>(motionData.dataType));
		obs_source_update(source, data);
		obs_source_update_properties(source);
	}

	obs_data_release(data);
}

static void setCurrentClickedForTemplateSource(QString id)
{
	OBSSceneItem sceneItem = PLSBasic::Get()->GetCurrentSceneItem();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	if (!source) {
		return;
	}
	if (0 != strcmp(common::PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, obs_source_get_id(source))) {
		return;
	}
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "method", "set_current_clicked_id");
	obs_data_set_string(data, "itemId", id.toUtf8().constData());
	pls_source_set_private_data(source, data);
	obs_data_release(data);
}

QWidget *getTopLevelWidget()
{
	for (auto w : QApplication::topLevelWidgets()) {
		if (0 != w->property("topLevelFlag").toString().compare(QStringLiteral("propertyWindow")))
			continue;

		if (auto dialog = dynamic_cast<PLSDialogView *>(w); dialog) {
			return w;
		}
	}
	return nullptr;
}

void PLSMotionImageListView::clickItemWithSendSignal(const MotionData &motionData, bool force)
{
	if (!force && PLSMotionNetwork::instance()->isPathEqual(m_currentMotionData, motionData)) {
		return;
	}

	m_currentMotionData = motionData;
	setCurrentClickedForTemplateSource(motionData.itemId);

	bool isValidMotionData = PLSMotionFileManager::instance()->isValidMotionData(motionData);
	if (!isValidMotionData && !motionData.canDelete) {
		if (!RetryDownload())
			return;
	}

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

	if (isValidMotionData) {
		emit currentResourceChanged(motionData.itemId, static_cast<int>(motionData.type), motionData.resourcePath, motionData.staticImgPath, motionData.thumbnailPath,
					    PLSMotionNetwork::instance()->isPrismOrFree(motionData), motionData.foregroundPath, motionData.foregroundStaticImgPath,
					    static_cast<int>(motionData.dataType));
	}
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
		m_errorToast = pls_new<PLSMotionErrorLayer>(this);
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

bool PLSMotionImageListView::isRecentTab(const PLSImageListView *view) const
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
	if (!idList.isEmpty()) {
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
	}

	/*else if (auto adapter = dynamic_cast<PLSWidgetDpiAdapter *>(obj); adapter && event->type() == QEvent::Move) {
		adjustErrTipSize();
	}*/

	return QFrame::eventFilter(obj, event);
}

void PLSMotionImageListView::initCheckBox()
{
	QSizePolicy policy = ui->checkBoxMotionOff->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	ui->checkBoxMotionOff->setSizePolicy(policy);
	connect(ui->checkBoxMotionOff, &PLSCheckBox::clicked, this, &PLSMotionImageListView::checkState);
}

void PLSMotionImageListView::initButton()
{
	m_buttonGroup->addButton(ui->btn_recent, 0);
	m_buttonGroup->addButton(ui->btn_prism, 1);
	m_buttonGroup->addButton(ui->btn_free, 2);
	m_buttonGroup->addButton(ui->btn_my, 3);
	connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &PLSMotionImageListView::buttonGroupSlot);
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
	auto viewHeight = 50;
	auto gap = 10;
	QPoint pt = this->mapToGlobal(QPoint(gap, superSize.height() - viewHeight - gap));
	m_errorToast->setGeometry(pt.x(), pt.y(), superSize.width() - 2 * gap, 50);
}

void PLSMotionImageListView::onCheckedRemoved(const MotionData &md, bool isVbUsed, bool isSourceUsed)
{
	deleteCurrentResource(md.itemId);

	if (((m_viewType == PLSMotionViewType::PLSMotionDetailView) && isVbUsed) || ((m_viewType == PLSMotionViewType::PLSMotionPropertyView) && isSourceUsed)) {
		ui->myView->deleteItemEx(md.itemId);
		clearSelectedItem();
	}
}

void PLSMotionImageListView::onRetryDownloadClicked(const std::function<void(const MotionData &md)> &ok, const std::function<void(const MotionData &md)> &fail)
{
	downloadNotCachedCount = PLSMotionNetwork::instance()->GetResourceNotCachedCount();
	QList<MotionData> prismMotionList = PLSMotionNetwork::instance()->getPrismCacheList();
	for (MotionData &motion : prismMotionList) {
		bool existed = imageItemViewExisted(motion.itemId);
		PLSMotionFileManager::instance()->redownloadResource(motion, existed, ok, fail);
	}
	QList<MotionData> freeMotionList = PLSMotionNetwork::instance()->getFreeCacheList();
	for (MotionData &motion : freeMotionList) {
		bool existed = imageItemViewExisted(motion.itemId);
		PLSMotionFileManager::instance()->redownloadResource(motion, existed, ok, fail);
	}
}

void PLSMotionImageListView::onRetryDownloadFinished()
{
	// download not finished
	if (reDownloadFailedCount + reDownloadSuccessCount != downloadNotCachedCount) {
		return;
	}

	// download not failed
	if (0 == reDownloadFailedCount) {
		PLS_INFO(MAIN_VIRTUAL_BACKGROUND, "redownload all virtual background resources success.");
	} else {
		PLSAlertView::warning(this, QTStr("Alert.title"), QTStr("virtual.resource.part.download.failed"));
	}

	reDownloadFailedCount = 0;
	reDownloadSuccessCount = 0;
	downloadNotCachedCount = 0;
}

void PLSMotionImageListView::clearImage() const
{
	OBSSceneItem sceneItem = PLSBasic::Get()->GetCurrentSceneItem();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);
	if (!source) {
		return;
	}
	if (0 != strcmp(common::PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, obs_source_get_id(source))) {
		return;
	}
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "method", "clear_image_texture");
	pls_source_set_private_data(source, data);
	obs_data_release(data);
}

bool PLSMotionImageListView::RetryDownload()
{
	QPointer<PLSMotionImageListView> guard(this);
	auto ret = pls_show_download_failed_alert(getTopLevelWidget());
	if (!pls_object_is_valid(guard))
		return false;

	if (ret == PLSAlertView::Button::Ok) {
		clearImage();
		onRetryDownloadClicked(
			[pthis = QPointer<PLSMotionImageListView>(this)](const MotionData &md) {
				updateCurrentClickedForTemplateSource(md);

				if (!pthis || !pls_object_is_valid(pthis)) {
					return;
				}

				if (pthis->m_currentMotionData.itemId == md.itemId) {
					pthis->clickItemWithSendSignal(md, true);
				}
				pthis->reDownloadSuccessCount++;
				pthis->onRetryDownloadFinished();
			},
			[pthis = QPointer<PLSMotionImageListView>(this)](const MotionData &md) {
				if (!pthis || !pls_object_is_valid(pthis)) {
					return;
				}

				pls_unused(md);
				pthis->reDownloadFailedCount++;
				pthis->onRetryDownloadFinished();
			});
	}

	return true;
}

bool PLSMotionImageListView::imageItemViewExisted(QString itemId)
{
	if (PLSImageListView *listView = getImageListView(m_buttonId); listView) {
		if (auto item = listView->findItem(itemId); item) {
			return true;
		}
		return false;
	}
	return false;
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
