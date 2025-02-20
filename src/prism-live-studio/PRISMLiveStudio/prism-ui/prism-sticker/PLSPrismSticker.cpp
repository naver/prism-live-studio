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

#include <stdio.h>
#include "utils-api.h"
#include "frontend-api.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

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

const int REQUST_TIME_OUT_MS = 10 * 1000;
const int DOWNLOAD_TIME_OUT_MS = 30 * 1000;
using namespace common;
using namespace downloader;

using namespace pls::rsm;

#define CATEGORY_INSTANCE CategoryPrismSticker::instance() 
#define GET_RECENT_USED_ITEMS CATEGORY_INSTANCE->getUsedItems(RECENT_USED_GROUP_ID)

static void loadItems(const pls::rsm::Group &group, std::list<Item> &itemsOut)
{
	for (const auto &item : group.items()) {
		itemsOut.push_back(item);
	}
}

static void loadAllItems(std::list<Item> &items)
{
	auto groups = CATEGORY_INSTANCE->getGroups();
	for (auto &group : groups) {
		loadItems(group, items);
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
	ui = pls_new<Ui::PLSPrismSticker>();
	setupUi(ui);
	qRegisterMetaType<StickerPointer>("StickerPointer");
	pls_add_css(this, {"PLSPrismSticker", "PLSToastMsgFrame", "PLSThumbnailLabel", "ScrollAreaWithNoDataTip"});
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
			if (!ok){
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
		for (const auto &item: stickerList) {
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
	auto dateTime = QDateTime::currentMSecsSinceEpoch();
	StickerHandleResult result = PLSStickerDataHandler::RemuxItemResource(item);
	if (!result.success) {
		PLS_WARN(MAIN_PRISM_STICKER, "Failed to remux file: %s", qUtf8Printable(item.itemId()));
		return;
	}

	if (label) {
		label->SetShowLoad(false);
		qint64 gap = QDateTime::currentMSecsSinceEpoch() - dateTime;
		if (gap < LOADING_TIME_MS) {
			QTimer::singleShot(LOADING_TIME_MS - gap, this, [label]() {
				PLS_INFO(MAIN_PRISM_STICKER, "UserApplySticker: single shot timer triggered.");
				label->SetShowOutline(false);
			});
		} else {
			label->SetShowOutline(false);
		}
	}

	if (!pls_get_app_exiting()) {
		emit StickerApplied(result);
		UpdateRecentList(item);
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
		toastTip = pls_new<PLSToastMsgFrame>(this);
	}
	UpdateToastPos();
	toastTip->SetMessage(tips);
	toastTip->SetShowWidth(width() - 2 * 10);
	toastTip->ShowToast();
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
	}
}

void PLSPrismSticker::DownloadResource(const StickerData &data, StickerPointer label)
{
	
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
	PLSSideBarDialogView::hideEvent(event);
}

void PLSPrismSticker::closeEvent(QCloseEvent *event)
{
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
			toastTip->SetShowWidth(width() - 2 * 10);
		}
	}
	return PLSSideBarDialogView::eventFilter(watcher, event);
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
	label->setObjectName("prismStickerLabel");
	label->SetTimer(timerLoading);
	label->setProperty("stickerData", QVariant::fromValue<StickerData>(data));
	label->SetCachePath(pls_get_user_path(PRISM_STICKER_CACHE_PATH));
	label->SetUrl(QUrl(data.thumbnailUrl), data.version);
	connect(label, &QPushButton::clicked, this, [this, label]() {
		if (nullptr == lastClicked)
			lastClicked = label;
		else {
			lastClicked->SetShowOutline(false);
			lastClicked = label;
		}
		if (label->IsShowLoading()) {
			label->SetShowOutline(true);
			return;
		}
		label->SetShowOutline(true);
		label->SetShowLoad(true);
		auto sticker_data = label->property("stickerData").value<StickerData>();
		QString log("User click sticker: \"%1/%2\"");
		PLS_UI_STEP(MAIN_PRISM_STICKER, qUtf8Printable(log.arg(sticker_data.category).arg(sticker_data.id)), ACTION_CLICK);
		DownloadResource(sticker_data, label);
	});
	layout->addWidget(label);
	layout->update();
}

void PLSPrismSticker::DownloadCategoryJson()
{
	
}

void PLSPrismSticker::DoDownloadJsonFile()
{
	
}

bool PLSPrismSticker::NetworkAccessible() const
{
	return pls_get_network_state();
}
