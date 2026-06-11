#include "PLSMotionImageListView.h"
#include "ui_PLSMotionImageListView.h"
#include "PLSMotionFileManager.h"
#include "PLSMotionDefine.h"
#include "CategoryVirtualTemplate.h"
#include "PLSPrismListView.h"
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
	setCursor(Qt::ArrowCursor);
	ui->setupUi(this);
	ui->checkBoxMotionOff->setEnabled(false);
	m_viewType = type;

	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::checkedRemoved, this, &PLSMotionImageListView::onCheckedRemoved, Qt::QueuedConnection);
	connect(CategoryVirtualTemplate::instance(), &CategoryVirtualTemplate::allDownloadFinished, this, &PLSMotionImageListView::onAllDownloadFinished, Qt::QueuedConnection);
	connect(CategoryVirtualTemplate::instance(), &CategoryVirtualTemplate::resourceDownloadFinished, this, &PLSMotionImageListView::onRetryDownloadCallback, Qt::QueuedConnection);

	initCheckBox();
	initCategory(type);
	initScrollArea();
	initCategoryIndex();
	pls_add_css(this, {"PLSMotionImageListView", "PLSLoadingBtn", "PLSMotionErrorLayer"});
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::checkedRemoved, this, &PLSMotionImageListView::onCheckedRemoved, Qt::QueuedConnection);
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::deleteMyResources, this, &PLSMotionImageListView::deleteAllResources, Qt::DirectConnection);

	if (type == PLSMotionViewType::PLSMotionDetailView) {
		ui->recentView->getImageListView()->showBottomMargin();
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
}

PLSMotionImageListView::~PLSMotionImageListView()
{
	pls_object_remove(this);
	pls_delete(ui);
}

PLSMotionViewType PLSMotionImageListView::viewType() const
{
	return m_viewType;
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

	const char *id = obs_data_get_string(data, MOTION_ITEM_ID_KEY.toStdString().c_str());
	if (0 == motionData.itemId.compare(id)) {
		bool prismResource = !motionData.canDelete;
		obs_data_set_bool(data, "prism_resource", prismResource);
		obs_data_set_string(data, "item_id", motionData.itemId.toUtf8().constData());
		obs_data_set_int(data, "item_type", static_cast<int>(motionData.type));
		obs_data_set_string(data, "file_path", processResourcePath(prismResource, motionData.resourcePath).toUtf8().constData());
		obs_data_set_string(data, "image_file_path", processResourcePath(prismResource, motionData.staticImgPath).toUtf8().constData());
		obs_data_set_string(data, "thumbnail_file_path", processResourcePath(prismResource, motionData.thumbnailPath).toUtf8().constData());
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
	obs_data_set_string(data, MOTION_ITEM_ID_KEY.toStdString().c_str(), id.toUtf8().constData());
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
	if (!force && CategoryVirtualTemplateInstance->isPathEqual(m_currentMotionData, motionData)) {
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

	PLSMotionFileManager::instance()->insertMotionData(motionData, key);

	setCheckBoxEnabled(motionData.type == MotionType::MOTION && !motionData.canDelete);

	if (isValidMotionData) {
		emit currentResourceChanged(motionData.itemId, static_cast<int>(motionData.type), motionData.resourcePath, motionData.staticImgPath, motionData.thumbnailPath, !motionData.canDelete,
					    motionData.foregroundPath, motionData.foregroundStaticImgPath);
	}
}

void PLSMotionImageListView::clickItemWithSendSignal(int groupIndex, int itemIndex)
{
	if (groupIndex < 0) {
		return;
	}

	//stackWidget choose correspond index
	m_buttonId = groupIndex;
	auto button = m_buttonGroup->button(groupIndex);
	button->setChecked(true);
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
		list = CategoryVirtualTemplateInstance->getRecentList();
	} else if (m_buttonId == ui->stackedWidget->count() - 1) {
		list = CategoryVirtualTemplateInstance->getMyList();
	} else {
		list = CategoryVirtualTemplateInstance->getGroupList(button->property("groupId").toString());
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

	bool result = false;
	for (auto key : categoryViews.keys()) {
		auto view = static_cast<PLSPrismListView *>(categoryViews.value(key));
		if (!view) {
			continue;
		}
		result = result || view->setSelectedItem(itemId);
	}

	bool myResult = ui->myView->setSelectedItem(itemId);
	bool recentResult = ui->recentView->setSelectedItem(itemId);
	if (!myResult && !recentResult) {
		emit setSelectedItemFailed(itemId);
	}
}

void PLSMotionImageListView::deleteItemWithSendSignal(const MotionData &motionData, bool isVbUsed, bool isSourceUsed, bool isRecent)
{
	QString itemId = motionData.itemId;
	QString key;
	if (motionData.groupId == MY_STR) {
		key = MY_FILE_LIST;
	} else if (motionData.groupId == RECENT_STR) {
		key = PROPERTY_RECENT_LIST;
		ui->recentView->deleteItem(itemId);
	}

	PLSMotionFileManager::instance()->deleteMotionData(this, motionData, key, isVbUsed, isSourceUsed);

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

	for (auto key : categoryViews.keys()) {
		auto view = static_cast<PLSPrismListView *>(categoryViews.value(key));
		if (!view) {
			continue;
		}
		view->setItemSelectedEnabled(enabled);
	}

	ui->recentView->setItemSelectedEnabled(enabled);
	ui->myView->setItemSelectedEnabled(enabled);
	if (!enabled) {
		m_currentMotionData = MotionData();
	}
}

void PLSMotionImageListView::clearSelectedItem()
{
	m_currentMotionData = MotionData();
	for (auto key : categoryViews.keys()) {
		auto view = static_cast<PLSPrismListView *>(categoryViews.value(key));
		if (!view) {
			continue;
		}
		view->clearSelectedItem();
	}

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
	for (auto key : categoryViews.keys()) {
		auto view = static_cast<PLSPrismListView *>(categoryViews.value(key));
		if (!view) {
			continue;
		}
		view->setFilterButtonVisible(visible);
	}

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

	bool result = false;
	int buttonId = 1; // prism button
	for (auto key : categoryViews.keys()) {
		auto view = static_cast<PLSPrismListView *>(categoryViews.value(key));
		if (!view) {
			continue;
		}
		if (view->setSelectedItem(itemId)) {
			buttonId = key;
			result = true;
		}
	}

	bool myResult = ui->myView->setSelectedItem(itemId);
	bool recentResult = ui->recentView->setSelectedItem(itemId);

	if (!result && !myResult && !recentResult) {
		emit setSelectedItemFailed(itemId);
	} else if (!result) {
		if (myResult) {
			buttonId = ui->stackedWidget->count() - 1; // my button
		} else if (recentResult) {
			buttonId = 0; // recent button
		}
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
	QList<MotionData> myList = CategoryVirtualTemplateInstance->getMyList();
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

void PLSMotionImageListView::initCategory(PLSMotionViewType type)
{
	m_buttonGroup->addButton(ui->btn_recent, 0);

	auto groups = CategoryVirtualTemplateInstance->getGroups();
	auto index = 1;
	for (auto group : groups) {
		if (!group || group.isCustom()) {
			continue;
		}
		auto groupId = group.groupId();
		QPushButton *btn = pls_new<QPushButton>(this);
		btn->setCheckable(true);
		if (groupId == PRISM_STR) { // for css
			btn->setObjectName("btn_prism");
		} else if (groupId == FREE_STR) {
			btn->setObjectName("btn_free");
		} else {
			btn->setText(groupId);
			btn->setObjectName("motionCategoryBtn");
		}
		btn->setProperty("groupId", group.groupId());
		pls_flush_style(btn);
		m_buttonGroup->addButton(btn, index);
		ui->horizontalLayout_scrollarea->insertWidget(index, btn);

		auto view = pls_new<PLSPrismListView>(groupId, this);
		view->setForProperties(type == PLSMotionViewType::PLSMotionPropertyView);
		if (type == PLSMotionViewType::PLSMotionDetailView) {
			view->getImageListView()->showBottomMargin();
		}
		ui->stackedWidget->insertWidget(index, view);
		categoryViews.insert(index, view);
		index++;
	}

	m_buttonGroup->addButton(ui->btn_my, index);
	connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &PLSMotionImageListView::buttonGroupSlot);
	m_buttonId = 0;
}

void PLSMotionImageListView::initScrollArea()
{
	ui->horizontalLayout_scrollarea->setAlignment(Qt::AlignTop);
	ui->topScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	if (m_viewType == PLSMotionViewType::PLSMotionDetailView) {
		ui->horizontalLayout_scrollarea->setSpacing(2);
		ui->checkBoxMotionOff->setHidden(true);
		setFilterButtonVisible(false);
	} else if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		ui->horizontalLayout_scrollarea->setSpacing(15);
		setFilterButtonVisible(true);
	}
}

void PLSMotionImageListView::initCategoryIndex()
{
	//load category list
	QString key = VIRTUAL_BACKGROUND_RECENT_LIST;
	if (m_viewType == PLSMotionViewType::PLSMotionPropertyView) {
		key = PROPERTY_RECENT_LIST;
	}
	QList<MotionData> recentList = CategoryVirtualTemplateInstance->getRecentList();
	ui->recentView->updateMotionList(recentList);
	QList<MotionData> myList = CategoryVirtualTemplateInstance->getMyList();
	ui->myView->updateMotionList(myList);

	m_buttonId = 1; // default prism tab
	m_buttonGroup->button(m_buttonId)->setChecked(true);
	ui->stackedWidget->setCurrentIndex(m_buttonId);
}

PLSImageListView *PLSMotionImageListView::getImageListView(int index)
{
	auto size = categoryViews.size();
	if (index == 0) {
		return ui->recentView->getImageListView();
	} else if (index == size - 1) {
		return ui->myView->getImageListView();
	} else {
		auto iter = categoryViews.find(index);
		if (iter == categoryViews.end()) {
			return nullptr;
		}
		if (auto view = static_cast<PLSPrismListView *>(iter.value()); view) {
			return view->getImageListView();
		}
		return nullptr;
	}
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

void PLSMotionImageListView::onRetryDownloadClicked()
{
	if (pls::rsm::State::Downloading == CategoryVirtualTemplateInstance->getState()) {
		return;
	}
	retryClicked = true;
	CategoryVirtualTemplateInstance->download();
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
	auto ret = pls_show_download_failed_alert(PLSBasic::instance()->GetPropertiesWindow());
	if (!pls_object_is_valid(guard))
		return false;

	if (ret == PLSAlertView::Button::Ok) {
		clearImage();
		onRetryDownloadClicked();
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

void PLSMotionImageListView::onRetryDownloadCallback(const MotionData &md, bool update)
{
	if (!retryClicked) {
		return;
	}
	updateCurrentClickedForTemplateSource(md);
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
		QList<MotionData> list = CategoryVirtualTemplateInstance->getRecentList();
		ui->recentView->updateMotionList(list);
	} else if (m_buttonId == ui->stackedWidget->count() - 1) {
		QList<MotionData> list = CategoryVirtualTemplateInstance->getMyList();
		ui->myView->updateMotionList(list);
	}
	auto button = m_buttonGroup->button(m_buttonId);
	button->setChecked(true);
	ui->stackedWidget->setCurrentIndex(m_buttonId);
	emit clickCategoryIndex(m_buttonId);
}

void PLSMotionImageListView::onAllDownloadFinished(bool ok)
{
	PLS_INFO(MAIN_VIRTUAL_BACKGROUND, "download all virtual background resources : %s.", ok ? "success" : "failed");

	if (!ok && retryClicked) {
		PLSAlertView::warning(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.title"), QTStr("virtual.resource.part.download.failed"));
	}
	retryClicked = false;
}