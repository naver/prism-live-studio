#include "PLSPrismSticker.h"
#include "ui_PLSPrismSticker.h"
#include "pls-common-language.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "qt-wrappers.hpp"
#include "liblog.h"
#include "action.h"
#include "flowlayout.h"
#include "platform.hpp"
#include "PLSBasic.h"
#include "libhttp-client.h"

#include <QButtonGroup>
#include <QJsonDocument>
#include <QEventLoop>
#include <QSet>

#include <stdio.h>
#include "utils-api.h"
#include "frontend-api.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include "pls/pls-source.h"

constexpr auto CATEGORY_ID_ALL = "All";
constexpr auto CATEGORY_ID_RECENT = "Recent";
constexpr auto RECENT_LIST_KEY = "recentList";

// For Stickers flow layout.
const int FLOW_LAYOUT_SPACING = 10;
const int FLOW_LAYOUT_VSPACING = 10;
const int FLOW_LAYOUT_MARGIN_LEFT = 19;
const int FLOW_LAYOUT_MARGIN_RIGHT = 17;
const int FLOW_LAYOUT_MARGIN_TOP_BOTTOM = 20;
const int MAX_CATEGORY_ROW = 2;
const int MAX_STICKER_COUNT = 21;

// For category Tab layout.
const int H_SPACING = 8;
const int V_SPACING = 8;
const int MARGIN_LEFT = 19;
const int MARGIN_RIGHT = 20;
const int MARGIN_TOP = 15;
const int MARGIN_BOTTOM = 15;

//Update Sticker Layout
const int UPDATE_STICKER_HEIGHT = 80;

const int REQUST_TIME_OUT_MS = 10 * 1000;
const int DOWNLOAD_TIME_OUT_MS = 30 * 1000;
using namespace common;
using namespace downloader;

using namespace pls::rsm;

#define CATEGORY_INSTANCE CategoryPrismSticker::instance()
#define GET_RECENT_USED_ITEMS CATEGORY_INSTANCE->getUsedItems(RECENT_USED_GROUP_ID)

static void loadItems(const pls::rsm::Group &group, std::list<Item> &itemsOut, QSet<QString> &seenItemIds)
{
	for (const auto &item : group.items()) {
		if (seenItemIds.contains(item.itemId())) {
			continue;
		}
		seenItemIds.insert(item.itemId());
		itemsOut.push_back(item);
	}
}

static void loadAllItems(std::list<Item> &items)
{
	QSet<QString> seenItemIds;
	auto groups = CATEGORY_INSTANCE->getGroups();
	for (auto &group : groups) {
		loadItems(group, items, seenItemIds);
	}
}

static std::tuple<QWidget *, FlowLayout *> createViewPage(const QWidget *)
{
	QWidget *page = pls_new<QWidget>();
	QVBoxLayout *layout = pls_new<QVBoxLayout>(page);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto sa = pls_new<ScrollAreaWithNoDataTip>(page);
	sa->SetScrollBarRightMargin(-1);
	sa->setObjectName("stickerScrollArea");

	sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sp.setHorizontalStretch(0);
	sp.setVerticalStretch(0);
	sp.setHeightForWidth(sa->sizePolicy().hasHeightForWidth());
	sa->setSizePolicy(sp);
	sa->setWidgetResizable(true);

	QWidget *widgetContent = pls_new<QWidget>();
	widgetContent->setMouseTracking(true);
	widgetContent->setObjectName("prismStickerContainer");
	int sph = FLOW_LAYOUT_SPACING;
	int spv = FLOW_LAYOUT_VSPACING;
	auto flowLayout = pls_new<FlowLayout>(widgetContent, 0, sph, spv);
	flowLayout->setItemRetainSizeWhenHidden(false);
	int l;
	int r;
	int tb;
	l = FLOW_LAYOUT_MARGIN_LEFT;
	r = FLOW_LAYOUT_MARGIN_RIGHT;
	tb = FLOW_LAYOUT_MARGIN_TOP_BOTTOM;
	flowLayout->setContentsMargins(l, tb, r, tb);
	flowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	sa->setWidget(widgetContent);

	layout->addWidget(sa);

	QApplication::postEvent(widgetContent, new QEvent(QEvent::Resize));
	return std::tuple<QWidget *, FlowLayout *>(page, flowLayout);
};

static std::optional<std::pair<QString, QString>> getStickerConfigInfo(const pls::rsm::Item &item)
{

	std::optional<std::pair<QString, QString>> info;
	if (auto rules = item.urlAndHowSaves(); !rules.empty()) {
		auto rule = rules.front();
		QFileInfo fileInfo(rule.savedFilePath());
		QDir dstDir = fileInfo.dir();

		QString baseName = QFileInfo(rule.url().fileName()).baseName();
		auto path = dstDir.absolutePath() + "/" + baseName + "/";
		auto configFile = path + baseName + ".json";
		info.emplace(path, configFile);
	}

	return info;
}

PLSPrismSticker::PLSPrismSticker(QWidget *parent) : PLSSideBarDialogView({298, 817, 5, ConfigId::PrismStickerConfig}, parent)
{
	pls_uistep_v2_set_custom_show_hide_name(this, "PRISM STICKER");
	ui = pls_new<Ui::PLSPrismSticker>();
	setupUi(ui);
	qRegisterMetaType<StickerPointer>("StickerPointer");
	pls_add_css(this, {"PLSPrismSticker", "PLSStickerToastFrame", "PLSThumbnailLabel", "ScrollAreaWithNoDataTip"});
	timerLoading = pls_new<QTimer>(this);

	pls_network_state_monitor([this_guard = QPointer<PLSPrismSticker>(this)](bool accessible) {
		if (pls_is_app_exiting())
			return;
		if (!this_guard)
			return;
		this_guard->OnNetworkAccessibleChanged(accessible);
	});

	btnMore = pls_new<QPushButton>(ui->category);
	btnMore->hide();

	connect(btnMore, &QPushButton::clicked, this, &PLSPrismSticker::OnBtnMoreClicked);
	connect(PLSBasic::Get(), &PLSBasic::mainClosing, this, &PLSPrismSticker::OnAppExit, Qt::DirectConnection);
	connect(CATEGORY_INSTANCE, &CategoryPrismSticker::finishDownloadJson, this, [this](bool ok, bool timeout) {
		pls_async_call_mt(this, [ok, timeout, this] {
			// handle json downloaded
			if (!ok) {
				OnDownloadJsonFailed(timeout);
			}
		});
	});
	connect(CATEGORY_INSTANCE, &CategoryPrismSticker::finishDownloadItem, this, [this](pls::rsm::Item item, bool ok, bool timeout) {
		pls_async_call_mt(this, [this, item, ok, timeout]() {
			// handle result
			OnDownloadItemResult(item, ok, timeout);
		});
	});
	connect(CATEGORY_INSTANCE, &CategoryPrismSticker::finishLoadJson, this, [this]() {
		pls_async_call_mt(this, [this] {
			// handle json loaded
			HandleStickerData();
		});
	});

	PLSStickerDataHandler::SetClearDataFlag(false);
	timerLoading->setInterval(LOADING_TIMER_TIMEROUT);
	ui->category->installEventFilter(this);
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	setWindowTitle(tr(MIAN_PRISM_STICKER_TITLE));
	InitScrollView();
	pls_async_call_mt(this, [this] {
		// initialize for the startup
		HandleStickerData();
	});
#if defined(Q_OS_MACOS)
	setMinimumSize(300, 455 - PLS_TITLE_BAR_HEIGHT);
	setMaximumSize(1472, 1036 - PLS_TITLE_BAR_HEIGHT);
#else
	setMinimumSize(300, 455);
	setMaximumSize(1472, 1036);
#endif
	pls_uistep_v2_set_title(this, "PRISM Sticker");
}

PLSPrismSticker::~PLSPrismSticker()
{
	exit = true;
	timerLoading->stop();

	pls_delete(ui, nullptr);
}

bool PLSPrismSticker::WriteDownloadCache() const
{
	return PLSStickerDataHandler::WriteDownloadCacheToLocal(downloadCache);
}

void PLSPrismSticker::EnterUpdateStickerMode(obs_source_t *source, bool isChangedSticker, const QString &resourceId)
{
	assert(source != nullptr);
	setWindowTitle(tr(MIAN_PRISM_UPDATE_STICKER_TITLE));
	m_isChangedSticker = isChangedSticker;
	m_updateStickerMode = true;
	m_updateStickerId = resourceId;
	update_sticker_source = source;
	ShowUpdateStickerGuideView();

	if (!m_lastOutlineLabelItemID.isEmpty()) {
		setOutlineVisible(m_lastOutlineLabelItemID, false);
	}
	setOutlineVisible(resourceId, true);
}

void PLSPrismSticker::setOutlineVisible(const QString &itemId, bool visible)
{
	const auto &labels = m_itemLabels.value(itemId);
	for (const auto &label : labels) {
		if (label)
			label->SetShowOutline(visible);
	}
}

void PLSPrismSticker::LeaveUpdateStickerMode()
{
	setWindowTitle(tr(MIAN_PRISM_STICKER_TITLE));
	if (m_updateStickerGuideView) {
		m_updateStickerGuideView->hide();
	}
	update_sticker_source = nullptr;
	m_updateStickerMode = false;
	m_updateStickerId = "";
	m_isChangedSticker = false;
	if (!m_lastOutlineLabelItemID.isEmpty()) {
		setOutlineVisible(m_lastOutlineLabelItemID, false);
	}
	m_lastOutlineLabelItemID = "";
}

bool PLSPrismSticker::isUpdateStickerMode()
{
	return m_updateStickerMode;
}

void PLSPrismSticker::setCloseEventCallback(const std::function<bool(obs_source_t *update_source)> &closeEventCallback)
{
	m_closeEventCallback = closeEventCallback;
}

void PLSPrismSticker::requestCloseStickerView()
{
	reject();
}

void PLSPrismSticker::InitScrollView()
{
	if (nullptr == stackedWidget) {
		stackedWidget = pls_new<QStackedWidget>(this);
		auto sp = stackedWidget->sizePolicy();
		sp.setVerticalPolicy(QSizePolicy::Expanding);
		stackedWidget->setSizePolicy(sp);
	}
	ui->layout_main->insertWidget(1, stackedWidget);
}

void PLSPrismSticker::InitCategory()
{
	if (nullptr == categoryBtn) {
		QMutexLocker locker(&mutex);
		ui->category->setMouseTracking(true);
		categoryBtn = pls_new<QButtonGroup>(this);
		connect(categoryBtn, SIGNAL(buttonClicked(QAbstractButton *)), this, SLOT(OnCategoryBtnClicked(QAbstractButton *)));

		auto createBtn = [=](const QString &groupId, const QString &groupName) {
			PLSCategoryButton *btn = pls_new<PLSCategoryButton>(ui->category);
			btn->setCheckable(true);
			btn->setProperty("groupId", groupId);
			btn->setObjectName("prismStickerCategoryBtn");
			btn->SetDisplayText(groupName.isEmpty() ? groupId : groupName);
			categoryBtn->addButton(btn);
			return btn;
		};

		// Add "ALL" and "Recent" tabs
		createBtn(CATEGORY_ID_ALL, tr("All"));
		createBtn(CATEGORY_ID_RECENT, tr(MAIN_PRISM_STICKER_RECENT));
		// Other tabs
		for (auto &group : CATEGORY_INSTANCE->getGroups()) {
			auto groupId = group.groupId();
			auto displayName = (0 == groupId.compare("RandomTouch")) ? QStringLiteral("Popping") : groupId;
			createBtn(groupId, displayName);
		}

		btnMore->setObjectName("prismStickerMore");
		btnMore->setCheckable(true);
		btnMore->setFixedSize(39, 35);
		pls_uistep_v2_set_value(btnMore, "clicked", "More Category");
		QHBoxLayout *layout = pls_new<QHBoxLayout>(btnMore);
		layout->setContentsMargins(0, 0, 0, 0);
		QLabel *labelIcon = pls_new<QLabel>(btnMore);
		labelIcon->setObjectName("prismStickerMoreIcon");
		layout->addWidget(labelIcon);
		layout->setAlignment(labelIcon, Qt::AlignCenter);
		pls_flush_style(btnMore);
	}
}

void ResetDataIndex(StickerDataIndex &dataIndex)
{
	dataIndex.startIndex = 0;
	dataIndex.endIndex = 0;
	dataIndex.categoryName = "";
}

void PLSPrismSticker::SelectTab(const QString &categoryId) const
{
	if (nullptr != categoryBtn) {
		auto buttons = categoryBtn->buttons();
		for (auto &button : buttons) {
			if (button && button->property("groupId").toString() == categoryId) {
				button->setChecked(true);
				break;
			}
		}
	}
}

void PLSPrismSticker::CleanPage(const QString &categoryId)
{
	if (categoryViews.find(categoryId) != categoryViews.end() && nullptr != categoryViews[categoryId]) {
		auto sa = categoryViews[categoryId]->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
		auto items = sa->widget()->findChildren<QPushButton *>("prismStickerLabel");
		auto fl = sa->widget()->layout();
		for (auto &item : items) {
			fl->removeWidget(item);
			pls_delete(item, nullptr);
		}
	}
}

bool PLSPrismSticker::LoadStickers(QLayout *layout, ScrollAreaWithNoDataTip *targetSa, QWidget *parent, const std::list<pls::rsm::Item> &stickerList)
{
	if (targetSa == nullptr)
		return false;

	if (stickerList.empty()) {
		targetSa->SetNoDataPageVisible(true);
		targetSa->SetNoDataTipText(tr("main.prism.sticker.noData.tips"));
		return true;
	}
	targetSa->SetNoDataPageVisible(false);
	auto future = QtConcurrent::run([this, stickerList, parent, layout]() {
		for (const auto &item : stickerList) {
			if (pls_get_app_exiting())
				break;

			StickerData data(item);
			QMetaObject::invokeMethod(this, "LoadStickerAsync", Qt::QueuedConnection, Q_ARG(const StickerData &, data), Q_ARG(QWidget *, parent), Q_ARG(QLayout *, layout));
			QThread::msleep(20);
		}
	});
	return true;
}

bool PLSPrismSticker::LoadViewPage(const QString &categoryId, const QWidget *page, QLayout *layout)
{
	bool ok = false;
	auto targetSa = page->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
	if (CATEGORY_ID_RECENT == categoryId) {
		const auto &recentItems = GET_RECENT_USED_ITEMS;
		ok = LoadStickers(layout, targetSa, this, recentItems);
	} else if (CATEGORY_ID_ALL == categoryId) {
		std::list<Item> items;
		loadAllItems(items);
		ok = LoadStickers(layout, targetSa, this, items);
	} else {
		auto group = CATEGORY_INSTANCE->getGroup(categoryId);
		const auto &items = group.items();
		ok = LoadStickers(layout, targetSa, this, items);
	}
	return ok;
}

std::list<pls::rsm::Item> PLSPrismSticker::GetRecentUsedItem()
{
	return std::list<pls::rsm::Item>();
}

void PLSPrismSticker::SwitchToCategory(const QString &categoryId)
{
	categoryTabId = categoryId;
	SelectTab(categoryId);

	auto iter = categoryViews.find(categoryId);
	if (iter != categoryViews.end() && iter->second) {
		if (CATEGORY_ID_RECENT == categoryId && needUpdateRecent) {
			needUpdateRecent = false;
			CleanPage(categoryId);
			LoadViewPage(categoryId, iter->second, GetFlowlayout(categoryId));
		}
		stackedWidget->setCurrentWidget(iter->second);
	} else {
		auto page = createViewPage(this);
		stackedWidget->addWidget(std::get<0>(page));
		categoryViews[categoryId] = std::get<0>(page);
		stackedWidget->setCurrentWidget(std::get<0>(page));
		bool ok = LoadViewPage(categoryId, std::get<0>(page), std::get<1>(page));
		auto sa = qobject_cast<ScrollAreaWithNoDataTip *>(std::get<0>(page)->parentWidget());
		if (sa)
			sa->SetNoDataPageVisible(!ok);
	}
}

void PLSPrismSticker::AdjustCategoryTab()
{
	if (!categoryBtn)
		return;
	int rowWidth = ui->category->width();
	int l = MARGIN_LEFT;
	int r = MARGIN_RIGHT;
	int t = MARGIN_TOP;
	int b = MARGIN_BOTTOM;
	int hSpacing = H_SPACING;
	int vSpacing = V_SPACING;
	int length = l + r;
	for (const auto &button : categoryBtn->buttons()) {
		length += button->width();
	}
	length += (categoryBtn->buttons().size() - 1) * hSpacing;
	if (showMoreBtn)
		ui->category->setFixedHeight((rowWidth < length) ? 109 : 65);
	int width = ui->category->width();
	QRect effectiveRect(l, t, width - 2 * r, width - 2 * b);
	int x = effectiveRect.x();
	int y = effectiveRect.y();
	int lineHeight = 0;
	std::vector<std::vector<int>> indexVector;
	std::vector<int> rowIndex;
	auto count = categoryBtn->buttons().size();
	for (qsizetype i = 0; i < count; ++i) {
		auto button = categoryBtn->buttons().at(i);
		int spaceX = hSpacing;
		int spaceY = vSpacing;
		int nextX = x + button->width() + spaceX;
		if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
			indexVector.emplace_back(rowIndex);
			rowIndex.clear();
			x = effectiveRect.x();
			y = y + lineHeight + spaceY;
			nextX = x + button->width() + spaceX;
			lineHeight = 0;
		}

		rowIndex.emplace_back(i);
		button->setGeometry(QRect(QPoint(x, y), button->size()));
		x = nextX;
		lineHeight = qMax(lineHeight, button->height());
		if (i == count - 1) {
			indexVector.emplace_back(rowIndex);
		}
	}

	auto showAllCategory = [=]() {
		for (const auto &button : categoryBtn->buttons())
			button->show();
	};

	auto setCategoryVisible = [=](int from, int to, bool visible) {
		for (int i = from; i <= to; i++)
			categoryBtn->buttons().at(i)->setVisible(visible);
	};

	int rectHeight = y + lineHeight + b;
	if (!showMoreBtn) {
		btnMore->hide();
		ui->category->setFixedHeight(rectHeight + 1);
		if (auto *lay = ui->layout_main) {
			lay->activate();
		}
		showAllCategory();
		return;
	}

	if (indexVector.size() <= MAX_CATEGORY_ROW) {
		btnMore->hide();
		showAllCategory();
		return;
	}

	for (int index : indexVector[0])
		categoryBtn->buttons().at(index)->show();
	std::vector<int> maxRow = indexVector[MAX_CATEGORY_ROW - 1];
	auto rowCount = maxRow.size();
	int lastIndex = maxRow[rowCount - 1];
	auto geometry = categoryBtn->buttons().at(lastIndex)->geometry();
	const int offset = 39;
	if (ui->category->width() - geometry.right() - vSpacing - r >= offset) {
		btnMore->setGeometry(QRect(QPoint(geometry.right() + hSpacing, geometry.y()), btnMore->size()));
		btnMore->show();
	} else {
		btnMore->hide();
		for (auto i = int(maxRow.size() - 1); i >= 0; i--) {
			geometry = categoryBtn->buttons().at(maxRow[i])->geometry();
			if (geometry.width() >= offset) {
				lastIndex = maxRow[i] - 1;
				categoryBtn->buttons().at(maxRow[i])->hide();
				btnMore->setGeometry(QRect(QPoint(geometry.x(), geometry.y()), btnMore->size()));
				btnMore->show();
				break;
			}
		}
	}
	setCategoryVisible(0, lastIndex, true);
	int toIdx = static_cast<int>(categoryBtn->buttons().size() - 1);
	setCategoryVisible(lastIndex + 1, toIdx, false);
}

bool PLSPrismSticker::UpdateRecentList(const pls::rsm::Item &item)
{
	if (item.groups().empty())
		return false;

	CATEGORY_INSTANCE->useItem(RECENT_USED_GROUP_ID, item);
	needUpdateRecent = true;
	return true;
}

void PLSPrismSticker::ShowNoNetworkPage(const QString &tips, RetryType type)
{
	HideNoNetworkPage();
	if (!m_pNodataPage) {
		m_pNodataPage = pls_new<NoDataPage>(this);
		m_pNodataPage->setMouseTracking(true);
		m_pNodataPage->setObjectName("iconNetworkErrorPage");
		m_pNodataPage->SetNoDataTipText(tips);
		m_pNodataPage->SetPageType(NoDataPage::PageType::Page_NetworkError);
		const char *method = (type == Timeout) ? SLOT(OnRetryOnTimeOut()) : SLOT(OnRetryOnNoNetwork());
		PLS_INFO(MAIN_PRISM_STICKER, "Show retry page reason: %s", (type == Timeout) ? "Requst time out" : "Network unavailable");
		connect(m_pNodataPage, SIGNAL(retryClicked()), this, method);
	}

	UpdateNodataPageGeometry();
	if (m_pNodataPage)
		m_pNodataPage->show();
}

void PLSPrismSticker::HideNoNetworkPage()
{
	if (m_pNodataPage) {
		pls_delete(m_pNodataPage);
	}
}

void PLSPrismSticker::UpdateNodataPageGeometry()
{
	if (!m_pNodataPage)
		return;
	m_pNodataPage->SetPageType(NoDataPage::PageType::Page_NetworkError);
	if (!categoryBtn || !CategoryTabsVisible()) {
		m_pNodataPage->resize(content()->size());
		m_pNodataPage->move(content()->mapTo(this, QPoint(0, 0)));
	} else {
		int y = ui->category->geometry().bottom();
		m_pNodataPage->resize(width() - 2, content()->height() - y - 1);
		m_pNodataPage->move(1, ui->category->mapTo(this, QPoint(0, ui->category->height())).y());
	}
}

void PLSPrismSticker::ApplySticker(const pls::rsm::Item &item, StickerPointer label)
{
	StickerHandleResult result = PLSStickerDataHandler::RemuxItemResource(item);
	if (!result.success) {
		PLS_WARN(MAIN_PRISM_STICKER, "Failed to remux file: %s", qUtf8Printable(item.itemId()));
		return;
	}

	bool wasOutlineVisible = label && label->IsShowOutline();

	if (label) {
		label->SetShowLoad(false);
		PLS_INFO(MAIN_PRISM_STICKER, "UserApplySticker: single shot timer triggered.");
		if (!m_updateStickerMode) {
			label->SetShowOutline(false);
		}
	}

	if (!pls_get_app_exiting()) {
		if (m_updateStickerMode) {
			if (label && label->IsShowOutline()) {
				emit StickerUpdatedResource(result, update_sticker_source);
				UpdateRecentList(item);
			}
		} else if (wasOutlineVisible) {
			emit StickerAddedApplied(result);
			UpdateRecentList(item);
		}

		/*QString categoryId = CATEGORY_ID_RECENT;
		auto iter = categoryViews.find(categoryId);
		if (iter != categoryViews.end() && iter->second) {
			CleanPage(categoryId);
			LoadViewPage(categoryId, iter->second, GetFlowlayout(categoryId));
		}	*/
	}
}

void PLSPrismSticker::UpdateDpi(double dpi) const
{
	for (const auto &view : categoryViews) {
		if (!view.second)
			continue;
		auto sa = view.second->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
		auto layout = qobject_cast<FlowLayout *>(sa->widget()->layout());
		if (layout) {
			layout->setHorizontalSpacing(qFloor(dpi * FLOW_LAYOUT_SPACING));
			layout->setverticalSpacing(FLOW_LAYOUT_VSPACING);
			int l;
			int r;
			int tb;
			l = qFloor(dpi * FLOW_LAYOUT_MARGIN_LEFT);
			r = qFloor(dpi * FLOW_LAYOUT_MARGIN_RIGHT);
			tb = FLOW_LAYOUT_MARGIN_TOP_BOTTOM;
			layout->setContentsMargins(l, tb, r, tb);
		}
	}
}

void PLSPrismSticker::ShowToast(const QString &tips)
{
	if (nullptr == toastTip) {
		toastTip = pls_new<PLSStickerToastFrame>(this);
	}
	PLS_UI_ACTION("Sticker View Toast Init");
	UpdateToastPos();
	toastTip->SetMessage(tips);
	toastTip->ShowToast();
	toastTip->show();
	toastTip->raise();
}

void PLSPrismSticker::HideToast()
{
	if (toastTip)
		toastTip->HideToast();
}

void PLSPrismSticker::UpdateToastPos()
{
	if (toastTip) {
		auto pos = ui->category->mapTo(this, QPoint(10, ui->category->height() + 10));
		toastTip->move(pos);
		toastTip->setFixedWidth(width() - 2 * 10);
		toastTip->calcFixedHeight();
	}
}

void PLSPrismSticker::ShowUpdateStickerGuideView()
{
	PLS_UI_ACTION("Update Sticker Source Guide View Init");
	if (!m_updateStickerGuideView) {
		m_updateStickerGuideView = pls_new<PLSUpdateStickerGuideView>(this);
		connect(m_updateStickerGuideView, &PLSUpdateStickerGuideView::onFinishButtonClicked, this, [this] {
			emit StickerUpdatedApplied(update_sticker_source);
			ShowToast(tr("main.prism.update.sticker.apply.toast"));
		});
	}
	UpdateGuideViewPosSize();
	m_updateStickerGuideView->setDefaultIcon(true);
	m_updateStickerGuideView->updateOkButtonEnabled(false);
	m_updateStickerGuideView->show();
	m_updateStickerGuideView->raise();
}

void PLSPrismSticker::UpdateGuideViewPosSize()
{
	if (!m_updateStickerGuideView) {
		return;
	}
	auto pos = ui->category->mapTo(this, QPoint(10, ui->category->height() + 10));
	int viewWidth = width() - 2 * 10;
	m_updateStickerGuideView->setFixedWidth(viewWidth);
	m_updateStickerGuideView->calcFixedHeight();
	m_updateStickerGuideView->move(pos);
}

void PLSPrismSticker::DownloadResource(const StickerData &data, StickerPointer label)
{
	if (requestDownloadLabels.find(data.id) == requestDownloadLabels.end()) {
		requestDownloadLabels.insert(data.id, label);
		PLS_INFO(MAIN_PRISM_STICKER, "Start to download resource: '%s'", qUtf8Printable(data.id));
		CATEGORY_INSTANCE->downloadItem(data.id);
	}
}

QLayout *PLSPrismSticker::GetFlowlayout(const QString &categoryId)
{
	QLayout *layout = nullptr;
	if (categoryViews.find(categoryId) != categoryViews.end() && nullptr != categoryViews[categoryId]) {
		auto sa = categoryViews[categoryId]->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
		layout = sa->widget()->layout();
	}
	return layout;
}

static bool retryCallback(const DownloadTaskData &data)
{
	if (data.outputPath == pls_get_user_path(PRISM_STICKER_CACHE_PATH))
		return true;
	return false;
}

void PLSPrismSticker::showEvent(QShowEvent *event)
{
	PLS_INFO(MAIN_PRISM_STICKER, "PLSPrismSticker geometry: %d, %d, %dx%d", geometry().x(), geometry().y(), geometry().width(), geometry().height());

	PLSBasic::instance()->getMainView()->updateSideBarButtonStyle(ConfigId::PrismStickerConfig, true);
	PLSSideBarDialogView::showEvent(event);
	showMoreBtn = true;
	isShown = true;
	if (isDataReady) {
		pls_async_call(this, [this]() {
			InitCategory();
			AdjustCategoryTab();
			SwitchToCategory(GET_RECENT_USED_ITEMS.empty() ? CATEGORY_ID_ALL : CATEGORY_ID_RECENT);
		});
		PLSFileDownloader::instance()->Retry(retryCallback);
	}
}

void PLSPrismSticker::hideEvent(QHideEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::PrismStickerConfig, false);
	HideToast();
	LeaveUpdateStickerMode();
	if (isDataReady) {
		showMoreBtn = true;
		InitCategory();
		AdjustCategoryTab();
		SwitchToCategory(GET_RECENT_USED_ITEMS.empty() ? CATEGORY_ID_ALL : CATEGORY_ID_RECENT);
	}
	PLSSideBarDialogView::hideEvent(event);
}

void PLSPrismSticker::closeEvent(QCloseEvent *event)
{
#if defined(Q_OS_MACOS)
	if (pls_libutil_api_mac::pls_get_is_app_quitting_by_dock()) {
		PLSSideBarDialogView::closeEvent(event);
		return;
	}
#endif
	if (m_closeEventCallback && !m_closeEventCallback(update_sticker_source)) {
		event->ignore();
		return;
	}
	hide();
	event->ignore();
}

void PLSPrismSticker::resizeEvent(QResizeEvent *event)
{
	PLSSideBarDialogView::resizeEvent(event);
	if (m_pWidgetLoadingBG != nullptr) {
		m_pWidgetLoadingBG->setGeometry(content()->geometry());
	}
	UpdateNodataPageGeometry();
	AdjustCategoryTab();
}

bool PLSPrismSticker::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->category && event->type() == QEvent::Resize) {
		UpdateNodataPageGeometry();
		if (toastTip && toastTip->isVisible()) {
			UpdateToastPos();
		}
		if (m_updateStickerGuideView && m_updateStickerGuideView->isVisible()) {
			UpdateGuideViewPosSize();
		}
	}
	return PLSSideBarDialogView::eventFilter(watcher, event);
}

void PLSPrismSticker::reject()
{
	if (m_closeEventCallback && !m_closeEventCallback(update_sticker_source)) {
		return;
	}
	hide();
}

void PLSPrismSticker::OnBtnMoreClicked()
{
	PLS_UI_STEP(MAIN_PRISM_STICKER, "User click more Prism sticker", ACTION_CLICK);
	btnMore->hide();
	// remove the FixHeight constraints
	ui->category->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	showMoreBtn = false;
	AdjustCategoryTab();
}

void PLSPrismSticker::OnHandleStickerDataFinished()
{
	isDataReady = true;
	if (isShown) {
		pls_async_call(this, [this]() {
			InitCategory();
			pls_async_call(this, [this]() {
				AdjustCategoryTab();
				SwitchToCategory(GET_RECENT_USED_ITEMS.empty() ? CATEGORY_ID_ALL : CATEGORY_ID_RECENT);
				if (!NetworkAccessible()) {
					ShowNoNetworkPage(tr("main.giphy.network.toast.error"), NoNetwork);
				}
			});
		});
	}
}

void PLSPrismSticker::OnNetworkAccessibleChanged(bool accessible)
{
	if (pls_is_app_exiting())
		return;

	if (!accessible) {
		ShowNoNetworkPage(tr("main.giphy.network.toast.error"), NoNetwork);
	}
}

void PLSPrismSticker::OnRetryOnTimeOut()
{
	PLS_UI_STEP(MAIN_PRISM_STICKER, "User click retry to download reaction json", ACTION_CLICK);
	HandleStickerData();
}

bool PLSPrismSticker::CategoryTabsVisible()
{
	return (categoryBtn && !categoryBtn->buttons().isEmpty());
}

void PLSPrismSticker::OnRetryOnNoNetwork()
{
	PLS_UI_STEP(MAIN_PRISM_STICKER, "User click retry to download reaction json", ACTION_CLICK);

	bool networkValid = NetworkAccessible();
	bool categoryUiLoaded = CategoryTabsVisible();
	if (networkValid && categoryUiLoaded) {
		HideNoNetworkPage();
	} else if (!networkValid && categoryUiLoaded) {
		pls_async_call(this, [this]() { ShowToast(QTStr("main.giphy.network.toast.error")); });
	} else {
		HandleStickerData();
	}
}

void PLSPrismSticker::OnDownloadItemResult(const pls::rsm::Item &item, bool ok, bool timeout)
{
	StickerPointer requestlabel;
	if (auto find = requestDownloadLabels.find(item.itemId()); find != requestDownloadLabels.end()) {
		if (auto label = find.value(); label) {
			requestlabel = label;
		}

		requestDownloadLabels.erase(find);
	}

	if (!requestlabel)
		return;

	if (ok) {
		PLS_INFO(MAIN_PRISM_STICKER, "Download Sticker file: '%s' successfully", qUtf8Printable(item.dir()));
		PLSStickerDataHandler::WriteDownloadCache(item.itemId(), item.version(), downloadCache);
		ApplySticker(item, requestlabel);
	} else {
		if (requestlabel) {
			requestlabel->SetShowLoad(false);
			requestlabel->SetShowOutline(false);
		}
		PLS_LOGEX(PLS_LOG_ERROR, MAIN_PRISM_STICKER, {{PTS_LOG_TYPE, PTS_TYPE_EVENT}}, "Download Sticker ItemId('%s') failed", qUtf8Printable(item.itemId()));
		QString tips = timeout ? QTStr("main.giphy.network.request.timeout") : QTStr("main.giphy.network.download.faild");
		ShowToast(tips);
	}
}

void PLSPrismSticker::OnDownloadJsonFailed(bool timeout)
{
	pls_check_app_exiting();
	HideLoading();
	if (timeout)
		ShowNoNetworkPage(tr("main.giphy.network.request.timeout"), Timeout);
	auto ret = pls_show_download_failed_alert(this);
	if (ret == PLSAlertView::Button::Ok) {
		PLS_INFO(MAIN_BEAUTY_MODULE, "Prism Sticker: User select retry download.");
		DownloadCategoryJson();
	}
}

void PLSPrismSticker::OnAppExit()
{
	// ICategory now is responsible for saving recent used items, removed old logic
	WriteDownloadCache();
}

void PLSPrismSticker::OnCategoryBtnClicked(QAbstractButton *button)
{
	if (!button)
		return;
	QString log("category: %1");
	const auto category = button->property("groupId").toString();
	if (categoryTabId.isEmpty() || !categoryTabId.isEmpty() && category != categoryTabId) {
		PLS_UI_STEP(MAIN_PRISM_STICKER, log.arg(category).toUtf8().constData(), ACTION_CLICK);
		SwitchToCategory(category);
	}
}

void PLSPrismSticker::HandleStickerData()
{
	PLSStickerDataHandler::ReadDownloadCacheLocal(downloadCache);
	auto groups = CATEGORY_INSTANCE->getGroups();
	if (groups.empty()) {
		PLS_WARN(MAIN_PRISM_STICKER, "PRISM Sticker groups are empty, re-download.");
		CATEGORY_INSTANCE->download();
		return;
	}

	OnHandleStickerDataFinished();
}

void PLSPrismSticker::ShowLoading(QWidget *parent)
{
	HideLoading();
	if (nullptr == m_pWidgetLoadingBG) {
		m_pWidgetLoadingBG = pls_new<QWidget>(parent);
		m_pWidgetLoadingBG->setObjectName("loadingBG");
		m_pWidgetLoadingBG->setGeometry(parent->geometry());
		m_pWidgetLoadingBG->show();

		auto layout = pls_new<QHBoxLayout>(m_pWidgetLoadingBG);
		auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
		layout->addWidget(loadingBtn);
		loadingBtn->setObjectName("loadingBtn");
		loadingBtn->show();
	}
	auto loadingBtn = m_pWidgetLoadingBG->findChild<QPushButton *>("loadingBtn");
	if (loadingBtn)
		m_loadingEvent.startLoadingTimer(loadingBtn);
}

void PLSPrismSticker::HideLoading()
{
	if (nullptr != m_pWidgetLoadingBG) {
		m_loadingEvent.stopLoadingTimer();
		pls_delete(m_pWidgetLoadingBG, nullptr);
	}
}

void PLSPrismSticker::LoadStickerAsync(const StickerData &data, QWidget *parent, QLayout *layout)
{
	auto label = pls_new<PLSThumbnailLabel>(parent);
	QPointer<PLSThumbnailLabel> guardedLabel(label);
	m_itemLabels[data.id].append(guardedLabel);
	connect(label, &PLSThumbnailLabel::selectedOutlineItem, this, [this, guardedLabel](const QPixmap &pix) {
		if (guardedLabel) {
			// Add to lastClickedLabels list if not already present
			auto sticker_data = guardedLabel->property("stickerData").value<StickerData>();
			m_lastOutlineLabelItemID = sticker_data.id;
			if (m_updateStickerGuideView) {
				m_updateStickerGuideView->updateGuideIcon(pix);
				m_updateStickerGuideView->updateOkButtonEnabled(guardedLabel->IsShowLoading() ? false : m_isChangedSticker);
			}
		}
	});

	connect(
		label, &PLSThumbnailLabel::itemDownloadedFinished, this,
		[this, guardedLabel](const QPixmap &pix) {
			if (m_updateStickerGuideView && guardedLabel && guardedLabel->IsShowOutline()) {
				m_updateStickerGuideView->updateGuideIcon(pix);
				m_updateStickerGuideView->updateOkButtonEnabled(m_isChangedSticker);
			}
		},
		Qt::QueuedConnection);

	connect(label, &QPushButton::clicked, this, [this, guardedLabel]() {
		if (!guardedLabel)
			return;

		// Clear all previously selected labels
		setOutlineVisible(m_lastOutlineLabelItemID, false);

		if (guardedLabel->IsShowLoading()) {
			guardedLabel->SetShowOutline(true);
			return;
		}
		guardedLabel->SetShowLoad(true);
		guardedLabel->SetShowOutline(true);
		auto sticker_data = guardedLabel->property("stickerData").value<StickerData>();
		QString log("User click sticker: \"%1/%2\"");
		PLS_UI_STEP(MAIN_PRISM_STICKER, qUtf8Printable(log.arg(sticker_data.category).arg(sticker_data.id)), ACTION_CLICK);
		DownloadResource(sticker_data, guardedLabel);
	});

	label->setObjectName("prismStickerLabel");
	label->SetTimer(timerLoading);
	label->setProperty("stickerData", QVariant::fromValue<StickerData>(data));
	label->SetCachePath(pls_get_user_path(PRISM_STICKER_CACHE_PATH));
	label->SetInfo(QUrl(data.thumbnailUrl), data.version);
	pls_uistep_v2_set_value(label, "clicked", QString("%1/%2").arg(data.category).arg(data.id));

	layout->addWidget(label);
	layout->update();

	//Update the currently selected sticker object, and send a message to update the icon and selection status.
	if (m_updateStickerMode && data.id == m_updateStickerId) {
		label->SetShowOutline(true);
	}
}

void PLSPrismSticker::DownloadCategoryJson()
{
	if (!NetworkAccessible()) {
		pls_async_call(this, [this]() { ShowNoNetworkPage(tr("main.giphy.network.toast.error"), NoNetwork); });
		return;
	}

	HideNoNetworkPage();
	ShowLoading(content());
	/**
	 * start download
	 */
	DoDownloadJsonFile();
}

void PLSPrismSticker::DoDownloadJsonFile()
{
	CATEGORY_INSTANCE->download();
}

bool PLSPrismSticker::NetworkAccessible() const
{
	return pls_get_network_state();
}
