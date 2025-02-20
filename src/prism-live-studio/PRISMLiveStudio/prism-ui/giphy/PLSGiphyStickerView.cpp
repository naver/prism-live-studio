#include "GiphyDownloader.h"
#include "GiphyWebHandler.h"
#include "PLSFloatScrollBarScrollArea.h"
#include "PLSSearchPopupMenu.h"
#include "PLSToastMsgFrame.h"
#include "action.h"
#include "flowlayout.h"
#include "liblog.h"
#include "log/module_names.h"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "qt-wrappers.hpp"
#include "ui_PLSGiphyStickerView.h"
#include "PLSGiphyStickerView.h"
#include "utils-api.h"
#include <QButtonGroup>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMetaObject>
#include <QPainter>
#include <QToolButton>
#include <qmath.h>
#include <set>
#include "window-basic-main.hpp"
#include "PLSBasic.h"

constexpr auto TAB_INDEX = "tabIndex";
constexpr auto API_KEY = "";
constexpr auto API = "";
constexpr auto PAGE_TYPE = "pageType";
constexpr auto SEARCH_API = "search";
constexpr auto TRENDING_API = "trending";

namespace {
struct LocalGlobalVars {
	static QPointer<MovieLabel> g_prevClickedItem;
};
QPointer<MovieLabel> LocalGlobalVars::g_prevClickedItem = nullptr;
}

const int defaultLimit = 24;
const int MAX_PREVIEW_FRAME = 6;
const int MAX_LIMIT = 24;
const int MOVIE_ITEM_WIDTH = 78;
const int MOVIE_ITEM_HEIGHT = 78;
const int FLOW_LAYOUT_SPACING = 6;
const int FLOW_LAYOUT_VSPACING = 10;
const int FLOW_LAYOUT_MARGIN_LEFT_RIGHT = 17;
const int FLOW_LAYOUT_MARGIN_TOP_BOTTOM = 19;
const int MAX_INPUT_LENGTH = 100;
using namespace common;

PointerValue PLSGiphyStickerView::ConvertPointer(void *ptr)
{
	return (PointerValue)(ptr);
}

PLSGiphyStickerView::PLSGiphyStickerView(DialogInfo info, QWidget *parent) : PLSSideBarDialogView(info, parent)
{
	pls_add_css(this, {"GiphyStickers", "PLSToastMsgFrame"});
	ui = pls_new<Ui::PLSGiphyStickerView>();
	pageLimit = defaultLimit;
	qRegisterMetaType<RequestTaskData>("RequestTaskData");
	qRegisterMetaType<DownloadTaskData>("DownloadTaskData");
	setupUi(ui);
	setCursor(Qt::ArrowCursor);
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	setWindowTitle(QTStr(MIAN_GIPHY_STICKER_TITLE));
	QFrame *titleFrame = this->findChild<QFrame *>("titleBar");
	if (titleFrame)
		titleFrame->setFocusPolicy(Qt::ClickFocus);

	QFont fontTab = this->font();
	fontTab.setBold(true);
	fontTab.setPixelSize(14);
	QFontMetrics metrics(fontTab);
	int widthRecent = metrics.horizontalAdvance(QTStr("main.giphy.tab.recent"));
	int widthTrending = metrics.horizontalAdvance(QTStr("main.giphy.tab.trending"));

	connect(PLSBasic::Get(), &PLSBasic::mainClosing, this, &PLSGiphyStickerView::OnAppExit, Qt::DirectConnection);

	InitStickerJson();
	Init();

	pls_async_call(this, [this, widthRecent, widthTrending]() {
		ui->btn_recent->setFixedWidth(widthRecent + 20);
		ui->btn_trending->setFixedWidth(widthTrending + 20);
	});
}

PLSGiphyStickerView::~PLSGiphyStickerView()
{
	threadRequest.quit();
	threadRequest.wait();

	if (timerAutoLoad.isActive())
		timerAutoLoad.stop();

	if (timerSearch.isActive())
		timerSearch.stop();

	if (timerScrollSearch.isActive())
		timerScrollSearch.stop();

	if (timerScrollTrending.isActive())
		timerScrollTrending.stop();

	if (LocalGlobalVars::g_prevClickedItem)
		LocalGlobalVars::g_prevClickedItem->deleteLater();

	pls_delete(ui, nullptr);
}

void PLSGiphyStickerView::Init()
{
	this->content()->setMouseTracking(true);
	setMouseTracking(true);
	ui->widget_search->setMouseTracking(true);
	ui->frame_head->setMouseTracking(true);
	ui->widget_container->setMouseTracking(true);
	ui->label_giphy_logo->setMouseTracking(true);
	ui->lineEdit_search->setMaxLength(MAX_INPUT_LENGTH);
	ui->lineEdit_search->setPlaceholderText(DEFAULT_KEY_WORD);
	pls_flush_style(ui->lineEdit_search, "usedFor", "giphy");

	timerAutoLoad.setSingleShot(true);
	connect(&timerAutoLoad, &QTimer::timeout, [this]() {
		needUpdateTrendingList = true;
		UpdateLimit();
		AutoFillContent();
	});

	timerSearch.setSingleShot(true);
	connect(&timerSearch, &QTimer::timeout, [this]() { AutoFillSearchList(0); });

	timerScrollSearch.setSingleShot(true);
	connect(&timerScrollSearch, &QTimer::timeout, [this]() {
		if (searchResultList && !searchKeyword.isEmpty()) {
			QuerySearch(searchKeyword, pageLimit, searchResultList->GetPagination());
		}
	});

	timerScrollTrending.setSingleShot(true);
	connect(&timerScrollTrending, &QTimer::timeout, [this]() {
		if (trendingScrollList)
			QueryTrending(pageLimit, trendingScrollList->GetPagination());
	});
	connect(ui->lineEdit_search, &PLSSearchLineEdit::SearchTrigger, this, &PLSGiphyStickerView::OnSearchTigger);
	connect(ui->lineEdit_search, &PLSSearchLineEdit::SearchIconClicked, this, [this](const QString &) {
		if (showDefaultKeyWord && ui->lineEdit_search->text().isEmpty()) {
			ui->lineEdit_search->setText(DEFAULT_KEY_WORD);
			OnSearchTigger(DEFAULT_KEY_WORD);
			ui->lineEdit_search->setPlaceholderText("");
			showDefaultKeyWord = false;
		} else if (auto key = ui->lineEdit_search->text(); !key.isEmpty()) {
			OnSearchTigger(key);
		}
	});
	connect(ui->lineEdit_search, &PLSSearchLineEdit::textEdited, [this](const QString &key) { OnSearchMenuRequested(key.isEmpty()); });
	connect(ui->lineEdit_search, &PLSSearchLineEdit::SearchMenuRequested, this, &PLSGiphyStickerView::OnSearchMenuRequested, Qt::QueuedConnection);
	ui->btn_cancel->hide();

	InitMenuList();

	btnGroup = pls_new<QButtonGroup>(this);
	connect(btnGroup, &QButtonGroup::idClicked, this, &PLSGiphyStickerView::OnTabClicked);
	btnGroup->addButton(ui->btn_recent, PLSGiphyStickerView::RecentTab);
	btnGroup->addButton(ui->btn_trending, PLSGiphyStickerView::TrendingTab);
}

void PLSGiphyStickerView::InitCommonForList(ScrollAreaWithNoDataTip *scrollList) const
{
	if (scrollList) {
		scrollList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		QSizePolicy sizePolicy_(QSizePolicy::Preferred, QSizePolicy::Expanding);
		sizePolicy_.setHorizontalStretch(0);
		sizePolicy_.setVerticalStretch(0);
		sizePolicy_.setHeightForWidth(scrollList->sizePolicy().hasHeightForWidth());
		scrollList->setSizePolicy(sizePolicy_);
		scrollList->setWidgetResizable(true);
	}
}

void PLSGiphyStickerView::InitMenuList()
{
	if (nullptr == searchMenu) {
		searchMenu = pls_new<PLSSearchPopupMenu>(this);
		connect(searchMenu, &PLSSearchPopupMenu::ItemDelete, this, &PLSGiphyStickerView::DeleteHistoryItem);
		connect(searchMenu, &PLSSearchPopupMenu::ItemClicked, [this](const QString &key) {
			searchMenu->hide();
			ui->lineEdit_search->setText(key);
			OnSearchTigger(key);
		});
	}
	searchMenu->hide();
	UpdateMenuList();
}

void PLSGiphyStickerView::InitRecentList()
{
	if (nullptr == recentScrollList) {

		recentScrollList = pls_new<ScrollAreaWithNoDataTip>(this);
		recentScrollList->setMouseTracking(true);
		recentScrollList->SetScrollBarRightMargin(1);
		connect(recentScrollList, &ScrollAreaWithNoDataTip::retryClicked, this, &PLSGiphyStickerView::retryRencentList);

		recentScrollList->setObjectName("recentScrollList");
		recentScrollList->SetNoDataTipText(QTStr("main.giphy.list.noData"));
		InitCommonForList(recentScrollList);
		QWidget *widgetContent = pls_new<QWidget>();
		widgetContent->setMouseTracking(true);
		widgetContent->setObjectName("recentListContainer");
		recentFlowLayout = pls_new<FlowLayout>(widgetContent, 0, FLOW_LAYOUT_SPACING, FLOW_LAYOUT_VSPACING);
		recentFlowLayout->setItemRetainSizeWhenHidden(false);
		recentFlowLayout->setContentsMargins(FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM, FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM);
		recentFlowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		recentScrollList->setWidget(widgetContent);
		listWidgets << recentScrollList;
		ui->verticalLayout_container->addWidget(recentScrollList);
	}
}

void PLSGiphyStickerView::InitTrendingList()
{
	if (nullptr == trendingScrollList) {
		trendingScrollList = pls_new<ScrollAreaWithNoDataTip>(this);
		trendingScrollList->setMouseTracking(true);
		trendingScrollList->SetScrollBarRightMargin(1);

		connect(trendingScrollList, &PLSFloatScrollBarScrollArea::ScrolledToEnd, [this]() { timerScrollTrending.start(500); });
		connect(trendingScrollList, &ScrollAreaWithNoDataTip::retryClicked, this, &PLSGiphyStickerView::retryTrendingList);

		trendingScrollList->setObjectName("trendingScrollList");
		trendingScrollList->SetNoDataTipText(QTStr("main.giphy.list.noData"));
		InitCommonForList(trendingScrollList);
		QWidget *widgetContent = pls_new<QWidget>();
		widgetContent->setMouseTracking(true);
		widgetContent->setObjectName("trendingListContainer");
		trendingFlowLayout = pls_new<FlowLayout>(widgetContent, 0, FLOW_LAYOUT_SPACING, FLOW_LAYOUT_VSPACING);
		trendingFlowLayout->setItemRetainSizeWhenHidden(false);
		trendingFlowLayout->setContentsMargins(FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM, FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM);
		trendingFlowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		trendingScrollList->setWidget(widgetContent);
		listWidgets << trendingScrollList;
		ui->verticalLayout_container->addWidget(trendingScrollList);
	}
}

void PLSGiphyStickerView::InitSearchResultList()
{
	if (nullptr == searchResultList) {
		searchResultList = pls_new<ScrollAreaWithNoDataTip>(this);
		searchResultList->setMouseTracking(true);
		searchResultList->SetScrollBarRightMargin(1);

		connect(searchResultList, &PLSFloatScrollBarScrollArea::ScrolledToEnd, [this]() { timerScrollSearch.start(500); });
		connect(searchResultList, &ScrollAreaWithNoDataTip::retryClicked, this, &PLSGiphyStickerView::retrySearchList);

		searchResultList->setObjectName("searchResultList");
		searchResultList->SetNoDataTipText(QTStr("main.giphy.list.noSearchResult"));
		InitCommonForList(searchResultList);
		QWidget *widgetContent = pls_new<QWidget>();
		widgetContent->setMouseTracking(true);
		widgetContent->setObjectName("searchResultContent");
		searchResultFlowLayout = pls_new<FlowLayout>(widgetContent, 0, FLOW_LAYOUT_SPACING, FLOW_LAYOUT_VSPACING);
		searchResultFlowLayout->setItemRetainSizeWhenHidden(false);
		searchResultFlowLayout->setContentsMargins(FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM, FLOW_LAYOUT_MARGIN_LEFT_RIGHT, FLOW_LAYOUT_MARGIN_TOP_BOTTOM);
		searchResultFlowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		searchResultList->setWidget(widgetContent);
	}
}

void PLSGiphyStickerView::ShowTabList(int tabType)
{
	switch (tabType) {
	case PLSGiphyStickerView::RecentTab:
		ShowRecentTab();
		break;
	case PLSGiphyStickerView::TrendingTab:
		ShowTrendingTab();
		break;
	default:
		break;
	}
}

static void GiphyWebHandleThread()
{
	PLS_LOG(PLS_LOG_INFO, MAIN_GIPHY_STICKER_MODULE, "[%s] Thread started.", __FUNCTION__);
}

void PLSGiphyStickerView::StartWebHandler()
{
	webHandler = pls_new<GiphyWebHandler>();
	connect(this, &PLSGiphyStickerView::request, webHandler, &GiphyWebHandler::GiphyFetch);
	connect(&threadRequest, &QThread::finished, webHandler, &GiphyWebHandler::deleteLater);
	connect(webHandler, &GiphyWebHandler::FetchResult, this, &PLSGiphyStickerView::requestRespon, Qt::QueuedConnection);
	connect(webHandler, &GiphyWebHandler::FetchError, this, &PLSGiphyStickerView::OnFetchError, Qt::QueuedConnection);
	connect(webHandler, &GiphyWebHandler::LoadingVisible, this, &PLSGiphyStickerView::OnLoadingVisible, Qt::QueuedConnection);

	auto network_monitor = [this_guard = QPointer<PLSGiphyStickerView>(this)](bool accessible) {

		if (pls_is_app_exiting())
			return;

		if (!this_guard)
			return;

		this_guard->network_accessible = accessible;
		if (accessible)
			this_guard->HideErrorToast();
		else {
			this_guard->ShowRetryPage();
		}
	};
	pls_network_state_monitor(network_monitor);
	webHandler->moveToThread(&threadRequest);
	connect(&threadRequest, &QThread::started, GiphyWebHandleThread);
	threadRequest.start();
}

void PLSGiphyStickerView::StartDownloader() const
{
	GiphyDownloader::instance()->Start();
}

const std::map<std::string, std::string, std::less<>> supportedLocal = {{"en", "English"},
									{"es", "Spanish"},
									{"pt", "Portuguese"},
									{"id", "Indonesian"},
									{"fr", "French"},
									{"ar", "Arabic"},
									{"tr", "Turkish"},
									{"th", "Thai"},
									{"vi", "Vietnamese"},
									{"de", "German"},
									{"it", "Italian"},
									{"ja", "Japanese"},
									{"zh-CN", "Chinese Simplified"},
									{"zh-TW", "Chinese Traditional"},
									{"ru", "Russian"},
									{"ko", "Korean"},
									{"pl", "Polish"},
									{"nl", "Dutch"},
									{"ro", "Romanian"},
									{"hu", "Hungarian"},
									{"sv", "Swedish"},
									{"cs", "Czech"},
									{"hi", "Hindi"},
									{"bn", "Bengali"},
									{"da", "Danish"},
									{"fa", "Farsi"},
									{"tl", "Filipino"},
									{"fi", "Finnish"},
									{"he", "Hebrew"},
									{"ms", "Malay"},
									{"no", "Norwegian"},
									{"uk", "Ukrainian"}};

QString PLSGiphyStickerView::GetRequestUrl(const QString &apiType, int limit, int offset) const
{
	QString url(API);
	return url;
}

void PLSGiphyStickerView::ClearTrendingList()
{
	if (trendingScrollList)
		trendingScrollList->ResetPagination();
	auto iter = listTrending.begin();
	while (iter != listTrending.end()) {
		trendingFlowLayout->removeWidget(*iter);
		pls_delete(*iter, nullptr);
		iter = listTrending.erase(iter);
	}
}

void PLSGiphyStickerView::ClearRecentList()
{
	auto iter = listRecent.begin();
	while (iter != listRecent.end()) {
		recentFlowLayout->removeWidget(*iter);
		pls_delete(*iter, nullptr);
		iter = listRecent.erase(iter);
	}
}

void PLSGiphyStickerView::ClearSearchList()
{
	if (searchResultList)
		searchResultList->ResetPagination();
	auto iter = listSearch.begin();
	while (iter != listSearch.end()) {
		MovieLabel *item = *iter;
		searchResultFlowLayout->removeWidget(item);
		pls_delete(item, nullptr);
		iter = listSearch.erase(iter);
	}
}

void PLSGiphyStickerView::ShowPageWidget(QWidget *widget) const
{
	if (!widget)
		return;
	for (QWidget *w : listWidgets) {
		if (w && w != widget) {
			w->hide();
		}
	}
	widget->show();
}

void PLSGiphyStickerView::AddHistorySearchItem(const QString &keyword)
{
	searchMenu->AddHistoryItem(keyword);
	WriteSearchHistoryToFile(keyword);
}

bool PLSGiphyStickerView::WriteRecentStickerDataToFile(const GiphyData &data, bool del)
{
	if (!jsonObjSticker.contains("recentList"))
		return false;

	QJsonValue recent = jsonObjSticker.value("recentList");
	if (!recent.isArray())
		return false;

	QJsonArray array = recent.toArray();
	auto iter = array.begin();
	bool find = false;
	int index = 0;
	while (iter != array.end()) {
		QJsonObject obj = iter->toObject();
		if (obj.value("id").toString() == data.id) {
			find = true;
			break;
		}
		iter++;
		index++;
	}

	auto createNewObj = [data](QJsonObject &obj) {
		obj.insert("id", data.id);
		obj.insert("type", data.type);
		obj.insert("title", data.title);
		obj.insert("rating", data.rating);
		obj.insert("previewUrl", data.previewUrl);
		obj.insert("originalUrl", data.originalUrl);
		obj.insert("preview_width", data.sizePreview.width());
		obj.insert("preview_height", data.sizePreview.height());
		obj.insert("original_width", data.sizeOriginal.width());
		obj.insert("original_height", data.sizeOriginal.height());
	};
	if (find)
		array.removeAt(index);

	if (!del) {
		QJsonObject obj;
		createNewObj(obj);
		array.prepend(obj);
	}
	auto size = array.size();
	if (size > MAX_RECENT_STICKER_COUNT)
		array.removeAt(size - 1);
	jsonObjSticker.insert("recentList", array);
	return true;
}

bool PLSGiphyStickerView::WriteSearchHistoryToFile(const QString &keyword, bool del)
{
	if (!jsonObjSticker.contains("historyList"))
		return false;

	QJsonValue recent = jsonObjSticker.value("historyList");
	if (!recent.isArray())
		return false;

	QJsonArray array = recent.toArray();
	auto iter = array.begin();
	bool find = false;
	int index = 0;
	QJsonObject temp;
	while (iter != array.end()) {
		QJsonObject obj = iter->toObject();
		if (obj.value("content").toString() == keyword) {
			find = true;
			temp = obj;
			break;
		}
		iter++;
		index++;
	}
	if (find) {
		array.removeAt(index);
		if (!del)
			array.prepend(temp);
	} else {
		if (!del) {
			QJsonObject obj;
			obj.insert("content", keyword);
			array.prepend(obj);
		}
	}
	jsonObjSticker.insert("historyList", array);

	return true;
}

void PLSGiphyStickerView::WriteTabIndexToConfig(int index) const
{
	config_set_int(App()->GlobalConfig(), GIPHY_STICKERS_CONFIG, TAB_INDEX, index);
	config_save(App()->GlobalConfig());
}

bool PLSGiphyStickerView::LoadArrayData(const QString &key, QJsonArray &array) const
{
	if (jsonObjSticker.contains(key)) {
		QJsonValue recent = jsonObjSticker.value(key);
		if (recent.isArray()) {
			array = recent.toArray();
			return true;
		}
	}

	return false;
}

bool PLSGiphyStickerView::CreateJsonFile() const
{
	QString userPath = pls_get_user_path(GIPHY_STICKERS_USER_PATH);
	QString userFileName = pls_get_user_path(GIPHY_STICKERS_JSON_FILE);
	QDir dir(userPath);
	if (!dir.exists())
		dir.mkpath(userPath);

	QFile file(userFileName);
	if (!file.open(QIODevice::ReadWrite)) {
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Could not open Giphy sticker json file for writing: %s\n", qPrintable(file.errorString()));
		return false;
	}

	QByteArray json = file.readAll();
	if (json.isEmpty()) {
		QJsonObject obj;
		QJsonArray arryHistoryList;
		QJsonArray arrayRecommendList;
		QJsonArray arrayRecentList;
		obj.insert("historyList", arryHistoryList);
		obj.insert("recommendList", arrayRecommendList);
		obj.insert("recentList", arrayRecentList);
		QJsonDocument document;
		document.setObject(obj);
		json = document.toJson(QJsonDocument::Indented);
		file.write(json);
	}
	file.close();
	return true;
}

void PLSGiphyStickerView::InitStickerJson()
{
	if (!CreateJsonFile())
		return;
	QString userFileName = pls_get_user_path(GIPHY_STICKERS_JSON_FILE);
	QFile file(userFileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Could not open Giphy sticker json file for writing: %s\n", qPrintable(file.errorString()));
		return;
	}

	QJsonParseError parseError;
	QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		file.close();
		return;
	}

	jsonObjSticker = document.object();
}

void PLSGiphyStickerView::AddRecentStickerList(const QJsonArray &array)
{
	ClearRecentList();
	if (array.empty()) {
		recentScrollList->SetNoDataPageVisible(true);
		recentScrollList->SetNoDataTipText(QTStr("main.giphy.list.noData"));
		return;
	}
	recentScrollList->SetNoDataPageVisible(false);
	auto iter = array.begin();
	while (iter != array.end()) {
		QJsonObject obj = iter->toObject();
		MovieLabel *pLabel = pls_new<MovieLabel>(this);
		connect(pLabel, &MovieLabel::DownLoadError, this, &PLSGiphyStickerView::OriginalDownLoadFailed);
		connect(pLabel, &MovieLabel::OriginalDownloaded, [this](const GiphyData &taskData, const QString &fileName, const QVariant &extra) {
			Q_UNUSED(fileName)
			emit StickerApply(fileName, taskData, extra);
			if (WriteRecentStickerDataToFile(taskData))
				needUpdateRecentList = true;
		});
		GiphyData data;
		data.id = obj.value("id").toString();
		data.type = obj.value("type").toString();
		data.title = obj.value("title").toString();
		data.rating = obj.value("rating").toString();
		data.previewUrl = obj.value("previewUrl").toString();
		data.originalUrl = obj.value("originalUrl").toString();
		data.sizePreview = QSize(obj.value("preview_width").toInt(), obj.value("preview_height").toInt());
		data.sizeOriginal = QSize(obj.value("original_width").toInt(), obj.value("original_height").toInt());
		recentFlowLayout->addWidget(pLabel);
		listRecent << pLabel;
		pLabel->SetData(data);
		iter++;
	}
}

void PLSGiphyStickerView::UpdateRecentList()
{
	needUpdateRecentList = false;
	if (!recentScrollList)
		return;
	QJsonArray array;
	LoadArrayData("recentList", array);
	AddRecentStickerList(array);
}

void PLSGiphyStickerView::UpdateMenuList()
{
	QJsonArray array;
	LoadArrayData("historyList", array);
	for (auto i = array.size() - 1; i >= 0; --i) {
		QJsonObject obj = array.at(i).toObject();
		searchMenu->AddHistoryItem(obj.value("content").toString());
	}
}

void PLSGiphyStickerView::DeleteHistoryItem(const QString &key)
{
	WriteSearchHistoryToFile(key, true);
}

void PLSGiphyStickerView::QuerySearch(const QString &keyword, int limit, int offset)
{
	
}

void PLSGiphyStickerView::QueryTrending(int limit, int offset)
{
	
}

void PLSGiphyStickerView::ShowSearchPage()
{
	// hide the trending scroll list to improve the performance of display.
	if (trendingScrollList) {
		if (trendingScrollList->isVisible())
			trendingScrollList->hide();
		trendingScrollList->SetLoadingVisible(false);
	}
	isSearchMode = true;
	ui->btn_cancel->show();
	searchMenu->hide();
	QPoint pos;
	QSize sizeContent;
	GetSearchListGeometry(sizeContent, pos);
	searchResultList->resize(sizeContent);
	searchResultList->move(pos);
	searchResultList->show();
}

void PLSGiphyStickerView::ShowErrorToast(const QString &tips)
{
	if (nullptr == networkErrorToast) {
		networkErrorToast = pls_new<PLSToastMsgFrame>(this);
	}
	UpdateErrorToastPos();
	networkErrorToast->SetMessage(tips);
	networkErrorToast->SetShowWidth(this->width() - 2 * 10);
	networkErrorToast->ShowToast();
}

void PLSGiphyStickerView::UpdateErrorToastPos()
{
	if (!networkErrorToast)
		return;
	QPoint pos;
	if (isSearchMode) {
		if (!searchResultList)
			return;
		pos = searchResultList->mapTo(this, QPoint(10, 10));
	} else {
		pos = ui->frame_head->mapTo(this, QPoint(10, ui->frame_head->height() + 10));
	}
	networkErrorToast->move(pos);
}

void PLSGiphyStickerView::HideErrorToast()
{
	if (networkErrorToast)
		networkErrorToast->HideToast();
}

void PLSGiphyStickerView::AutoFillContent()
{
	if (searchResultList && searchResultList->isVisible() && !searchKeyword.isEmpty()) {
		AutoFillSearchList(searchResultList->GetPagination());
	} else if (trendingScrollList && trendingScrollList->isVisible()) {
		AutoFillTrendingList();
	}
}

//After window resized,page count should be recalculated.
void PLSGiphyStickerView::UpdateLimit()
{
	int newPageCount = GetColumCountByWidth(this->width(), FLOW_LAYOUT_MARGIN_LEFT_RIGHT);
	if (newPageCount < defaultLimit)
		newPageCount = defaultLimit;
	if (newPageCount != pageLimit)
		qDebug() << "Update Limit to: " << newPageCount;
	pageLimit = newPageCount;
}

inline int PLSGiphyStickerView::GetColumCountByWidth(int width, int marginLeftRight) const
{
	int itemWidth = MOVIE_ITEM_WIDTH;
	int hSpacing = FLOW_LAYOUT_SPACING;
	int colums = (width - 2 * marginLeftRight + hSpacing) / (hSpacing + itemWidth);
	return colums;
}

inline int PLSGiphyStickerView::GetRowCountByHeight(int height, int marginTopBottom) const
{
	int itemHeight = MOVIE_ITEM_HEIGHT;
	int vSpacing = FLOW_LAYOUT_VSPACING;
	auto totalWidth = static_cast<float>(height - 2 * marginTopBottom + vSpacing);
	int rows = qRound(totalWidth / float(vSpacing + itemHeight));
	return rows;
}

void PLSGiphyStickerView::UpdateLayoutSpacing(FlowLayout *layout, double dpi) const
{
	if (layout) {
		layout->setHorizontalSpacing(qFloor(dpi * FLOW_LAYOUT_SPACING));
		layout->setverticalSpacing(FLOW_LAYOUT_VSPACING);
	}
}

int PLSGiphyStickerView::GetItemCountToFillContent(const QSize &sizeContent, int leftRightMargin, int topBottomMargin, int &rows, int &colums)
{
	colums = GetColumCountByWidth(sizeContent.width(), leftRightMargin);
	rows = GetRowCountByHeight(sizeContent.height(), topBottomMargin);
	int total = colums * rows;
	return total;
}

void PLSGiphyStickerView::AutoFillSearchList(int StartPagenation)
{
	int rows;
	int colums;
	QPoint pos;
	QSize sizeContent;
	GetSearchListGeometry(sizeContent, pos);
	int leftRightMargin = searchResultFlowLayout->contentsMargins().left();
	int topBottomMargin = searchResultFlowLayout->contentsMargins().top();
	GetItemCountToFillContent(sizeContent, leftRightMargin, topBottomMargin, rows, colums);
	int totalCount = ++rows * colums;
	int pagenation = StartPagenation;
	if (totalCount > pagenation) {
		int offset = totalCount - pagenation;
		// the maximum limit of API is 100.
		if (offset > MAX_LIMIT) {
			int requestCount = offset / MAX_LIMIT;

			for (int i = 0; i < requestCount; ++i) {
				QuerySearch(searchKeyword, MAX_LIMIT, pagenation + i * MAX_LIMIT);
			}

			int other = offset % MAX_LIMIT;
			if (other > 0)
				QuerySearch(searchKeyword, other, pagenation + requestCount * MAX_LIMIT);
		} else {
			QuerySearch(searchKeyword, offset, pagenation);
		}
	}
}

void PLSGiphyStickerView::AutoFillTrendingList(bool oneMoreRow)
{
	needUpdateTrendingList = false;
	QSize listContentSize = trendingScrollList->widget()->size();
	if (listContentSize.height() > trendingScrollList->height())
		return;
	int rows;
	int colums;
	int leftRightMargin = trendingFlowLayout->contentsMargins().left();
	int topBottomMargin = trendingFlowLayout->contentsMargins().top();
	GetItemCountToFillContent(listContentSize, leftRightMargin, topBottomMargin, rows, colums);
	int totalCount = (oneMoreRow ? ++rows : rows) * colums;
	int pagenation = trendingScrollList->GetPagination();
	if (totalCount > pagenation) {
		int offset = totalCount - pagenation;
		// the maximum limit of API is 100.
		if (offset > MAX_LIMIT) {
			int requestCount = offset / MAX_LIMIT;
			for (int i = 0; i < requestCount; ++i) {
				QueryTrending(MAX_LIMIT, pagenation + i * MAX_LIMIT);
			}

			int other = offset % MAX_LIMIT;
			if (other > 0)
				QueryTrending(other, pagenation + requestCount * MAX_LIMIT);
		} else {
			QueryTrending(offset, pagenation);
		}
	}
}

void PLSGiphyStickerView::GetSearchListGeometry(QSize &sizeContent, QPoint &position) const
{
	int nY = ui->lineEdit_search->mapTo(this, QPoint(0, ui->lineEdit_search->height())).y() + 13;
	position.setX(1);
	position.setY(nY);
	sizeContent.setWidth(this->width() - 2);
	sizeContent.setHeight(this->height() - nY - 1);
}

void PLSGiphyStickerView::AddSearchItem(const GiphyData &data)
{
	if (stopProcess)
		return;
	MovieLabel *pLabel = pls_new<MovieLabel>(this);
	connect(pLabel, &MovieLabel::OriginalDownloaded, this, &PLSGiphyStickerView::OriginalDownload);
	connect(pLabel, &MovieLabel::DownLoadError, this, &PLSGiphyStickerView::OriginalDownLoadFailed);
	searchResultFlowLayout->addWidget(pLabel);
	pLabel->SetData(data);
	listSearch << pLabel;
}

void PLSGiphyStickerView::ShowRetryPage()
{
	QList<ScrollAreaWithNoDataTip *> listScrollArea;
	listScrollArea << recentScrollList << trendingScrollList << searchResultList;
	for (auto page : listScrollArea) {
		if (page) {
			page->SetShowRetryPage();
			page->SetRetryTipText(QTStr("main.giphy.network.toast.error"));
		}
	}
}

bool PLSGiphyStickerView::IsNetworkAccessible() const
{
	return pls_get_network_state();
}

void PLSGiphyStickerView::retrySearchList()
{
	if (!searchResultList)
		return;
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "retry button", ACTION_CLICK);
	if (!pls_get_network_state()) {
		ShowErrorToast(QTStr("main.giphy.network.toast.error"));
		return;
	}
	searchResultList->HideRetryPage();
	AutoFillSearchList(0);

	if (RecentTab == currentType)
		retryRencentList();
	else if (TrendingTab == currentType)
		retryTrendingList();
}

void PLSGiphyStickerView::retryTrendingList()
{
	if (!trendingScrollList)
		return;
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "retry button", ACTION_CLICK);
	if (!pls_get_network_state()) {
		ShowErrorToast(QTStr("main.giphy.network.toast.error"));
		return;
	}
	trendingScrollList->HideRetryPage();
	AutoFillTrendingList();
	needUpdateRecentList = true;
}

void PLSGiphyStickerView::retryRencentList()
{
	if (!recentScrollList)
		return;
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "retry button", ACTION_CLICK);
	if (!pls_get_network_state()) {
		ShowErrorToast(QTStr("main.giphy.network.toast.error"));
		return;
	}
	recentScrollList->HideRetryPage();
	trendingScrollList->HideRetryPage();
	UpdateRecentList();
	needUpdateTrendingList = true;
}

void PLSGiphyStickerView::SetSearchData(const ResponData &data)
{
	if (!isSearching)
		return;
	ShowSearchPage();
	auto size = data.giphyData.size();
	PLS_INFO(MAIN_GIPHY_STICKER_MODULE, "request search '%s' data size=%d", data.task.keyword.toStdString().c_str(), size);
	bool noData = (size == 0);
	if (data.pageData.offset == 0) {
		stopProcess = true;
		searchResultList->SetNoDataPageVisible(noData);
		searchResultList->SetNoDataTipText(QTStr("main.giphy.list.noSearchResult"));
		ClearSearchList();
		lastSearchResponOffset = -1;
	}
	if (noData)
		return;
	// avoid repeat data
	if (data.pageData.offset <= lastSearchResponOffset) {
		PLS_INFO(MAIN_GIPHY_STICKER_MODULE, "search list error paganation data respon, last offset=%d,current offset=%d", lastSearchResponOffset, data.pageData.offset);
		return;
	}
	lastSearchResponOffset = data.pageData.offset;
	int nextPagination = data.pageData.offset + data.pageData.count;
	searchResultList->SetPagintion(nextPagination);
	if (size > 0 && !data.task.keyword.isEmpty())
		AddHistorySearchItem(data.task.keyword);
	stopProcess = false;
	for (GiphyData sticker_data : data.giphyData) {
		if (pls_get_app_exiting())
			return;
		if (stopProcess)
			break;
		AddSearchItem(sticker_data);
	}
}

void PLSGiphyStickerView::ShowRecentTab()
{
	bool init = (nullptr == recentScrollList);
	InitRecentList();
	ShowPageWidget(recentScrollList);
	if (init || needUpdateRecentList)
		UpdateRecentList();
}

void PLSGiphyStickerView::ShowTrendingTab()
{
	bool init = (nullptr == trendingScrollList);
	InitTrendingList();
	ShowPageWidget(trendingScrollList);
	if (init || needUpdateTrendingList)
		pls_async_call(this, [this]() { AutoFillTrendingList(); });
}

void PLSGiphyStickerView::ClickRecentTab()
{
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "Recent Tab", ACTION_CLICK);
	currentType = RecentTab;
	if (!IsNetworkAccessible()) {
		recentScrollList->SetRetryTipText(QTStr("main.giphy.network.toast.error"));
		recentScrollList->SetRetryPageVisible(true);
	}
}

void PLSGiphyStickerView::ClickTrendingTab()
{
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "Trending Tab", ACTION_CLICK);
	currentType = TrendingTab;
	if (!IsNetworkAccessible()) {
		trendingScrollList->SetRetryTipText(QTStr("main.giphy.network.toast.error"));
		trendingScrollList->SetRetryPageVisible(true);
	}
}

bool PLSGiphyStickerView::GetOtherNetworkErrorString(QNetworkReply::NetworkError networkError, QString &errorString)
{
	switch (networkError) {
	case QNetworkReply::ConnectionRefusedError:
	case QNetworkReply::RemoteHostClosedError:
	case QNetworkReply::HostNotFoundError:
		errorString = QTStr("main.giphy.network.toast.error");
		break;
	default:
		errorString = QTStr("main.giphy.network.unknownError");
		ShowErrorToast(errorString);
		return false;
	}
	return true;
}

bool PLSGiphyStickerView::SearchResultListShowing() const
{
	if (searchResultList != nullptr && searchResultList->isVisible())
		return true;

	return false;
}

bool PLSGiphyStickerView::IsSignalConnected(const QMetaMethod &signal) const
{
	return this->isSignalConnected(signal);
}

void PLSGiphyStickerView::SaveStickerJsonData() const
{
	QString userFileName = pls_get_user_path(GIPHY_STICKERS_JSON_FILE);
	QFile file(userFileName);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Could not open Giphy sticker json file for writing: %s\n", qPrintable(file.errorString()));
		return;
	}

	auto oldSize = file.readAll().size();
	QJsonDocument document;
	document.setObject(jsonObjSticker);
	QByteArray json = document.toJson(QJsonDocument::Indented);
	if (json.size() < oldSize)
		file.resize(json.size());
	file.seek(0);
	file.write(json);
	file.flush();
	file.close();
}

void PLSGiphyStickerView::hideEvent(QHideEvent *event)
{
	showFromProperty = false;
	PLSSideBarDialogView::hideEvent(event);
	if (isSearchMode)
		on_btn_cancel_clicked();
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::GiphyStickersConfig, false);
}

void PLSGiphyStickerView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSGiphyStickerView::resizeEvent(QResizeEvent *event)
{
	if (searchMenu && searchMenu->isVisible()) {
		searchMenu->SetContentWidth(ui->lineEdit_search->width());
		searchMenu->move(ui->lineEdit_search->mapTo(this, QPoint(0, ui->lineEdit_search->height())));
	}

	if (searchResultList && searchResultList->isVisible()) {
		int nY = ui->lineEdit_search->mapTo(this, QPoint(0, ui->lineEdit_search->height())).y() + 13;
		int width = this->width() - 2;
		searchResultList->resize(width, this->height() - nY - 1);
		searchResultList->move(1, nY);
	}

	if (networkErrorToast && networkErrorToast->isVisible()) {
		networkErrorToast->SetShowWidth(this->width() - 2 * 10);
		UpdateErrorToastPos();
	}

	timerAutoLoad.start(500);
	PLSSideBarDialogView::resizeEvent(event);
}

void PLSGiphyStickerView::mousePressEvent(QMouseEvent *event)
{
	if (searchMenu && !searchMenu->geometry().contains(event->pos())) {
		searchMenu->hide();
	}
	PLSSideBarDialogView::mousePressEvent(event);
}

void PLSGiphyStickerView::showEvent(QShowEvent *event)
{
	PLS_INFO(MAIN_GIPHY_STICKER_MODULE, "PLSGiphyStickerView geometry: %d, %d, %dx%d", geometry().x(), geometry().y(), geometry().width(), geometry().height());

	PLSSideBarDialogView::showEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::GiphyStickersConfig, true);
	if (isFirstShow) {
		isFirstShow = false;
		StartWebHandler();
		StartDownloader();
		ui->btn_trending->setChecked(true);
		pls_async_call(this, [this]() { OnTabClicked(TrendingTab); });
	}
}

void PLSGiphyStickerView::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		if (searchMenu) {
			searchMenu->hide();
		}
	}
	PLSSideBarDialogView::keyPressEvent(event);
}

bool PLSGiphyStickerView::event(QEvent *e)
{
	if (e && e->type() == customType) {
		auto addSticker = static_cast<AddStickerEvent *>(e);
		ResponData data = addSticker->GetData();
		SetSearchData(data);
		return true;
	}
	return PLSSideBarDialogView::event(e);
}

void PLSGiphyStickerView::OnTabClicked(int index)
{
	ShowTabList(index);
	switch (index) {
	case RecentTab:
		ClickRecentTab();
		break;
	case TrendingTab:
		ClickTrendingTab();
		break;
	default:
		currentType = TrendingTab;
		break;
	}

	WriteTabIndexToConfig(index);
}

void PLSGiphyStickerView::CreateMovieLabels(QVector<GiphyData> datas, int index, int total)
{
	if (pls_get_app_exiting())
		return;
	if (index >= 0 && index < total) {
		MovieLabel *pLabel = pls_new<MovieLabel>(this);
		connect(pLabel, &MovieLabel::OriginalDownloaded, this, &PLSGiphyStickerView::OriginalDownload);
		connect(pLabel, &MovieLabel::DownLoadError, this, &PLSGiphyStickerView::OriginalDownLoadFailed);
		pLabel->SetData(datas[index]);
		listTrending << pLabel;
		if (trendingFlowLayout) {
			trendingFlowLayout->addWidget(pLabel);
			if (index == total - 1)
				trendingFlowLayout->update();
		}
		/**
		 * async recursion, jump to next index.
		 */
		QMetaObject::invokeMethod(this, "CreateMovieLabels", Qt::QueuedConnection, Q_ARG(QVector<GiphyData>, datas), Q_ARG(int, ++index), Q_ARG(int, total));
	}
}

void PLSGiphyStickerView::OnAppExit()
{
	SaveStickerJsonData();
}

void PLSGiphyStickerView::requestRespon(const ResponData &data)
{
	if (pls_get_app_exiting())
		return;

	if (data.task.randomId <= 0)
		return;

	HideErrorToast();
	if (data.task.randomId == ConvertPointer(trendingScrollList)) {
		static int lastTrendingResponOffset = -1;
		// avoid repeat data
		if (data.pageData.offset <= lastTrendingResponOffset) {
			PLS_INFO(MAIN_GIPHY_STICKER_MODULE, "trending list error paganation data respon, last offset=%d,current offset=%d", lastTrendingResponOffset, data.pageData.offset);
			return;
		}
		lastTrendingResponOffset = data.pageData.offset;
		bool noData = data.giphyData.empty();
		if (data.pageData.offset == 0) {
			trendingScrollList->SetNoDataPageVisible(noData);
			trendingScrollList->SetNoDataTipText(QTStr("main.giphy.list.noData"));
			ClearTrendingList();
		}
		if (noData)
			return;
		int nextPagination = data.pageData.offset + data.pageData.count;
		trendingScrollList->SetPagintion(nextPagination);
		QMetaObject::invokeMethod(this, "CreateMovieLabels", Qt::QueuedConnection, Q_ARG(QVector<GiphyData>, data.giphyData), Q_ARG(int, 0), Q_ARG(int, data.giphyData.size()));
	} else if (data.task.randomId == ConvertPointer(searchResultList)) {
		AddStickerEvent *e = pls_new<AddStickerEvent>(data);
		QApplication::postEvent(this, e);
	}
}

void PLSGiphyStickerView::OnFetchError(const RequestTaskData &task, const RequestErrorInfo &errorInfo)
{
	if (task.randomId <= 0)
		return;

	if (searchMenu)
		searchMenu->hide();

	QString errorTips;
	switch (errorInfo.errorType) {
	case RequestErrorType::RequstTimeOut:
		errorTips = QTStr("main.giphy.network.request.timeout");
		break;
	case RequestErrorType::OtherNetworkError:
		if (!GetOtherNetworkErrorString(errorInfo.networkError, errorTips))
			return;
		break;
	default:
		break;
	}

	if (task.randomId == ConvertPointer(trendingScrollList)) {
		trendingScrollList->SetShowRetryPage();
		trendingScrollList->SetNoDataTipText(errorTips);

	} else if (task.randomId == ConvertPointer(searchResultList)) {
		if (searchResultList->isVisible()) {
			searchResultList->SetShowRetryPage();
			searchResultList->SetNoDataTipText(errorTips);
		} else
			ShowErrorToast(errorTips);
	} else {
		ShowErrorToast(errorTips);
	}
}

void PLSGiphyStickerView::OnSearchTigger(const QString &keyword)
{
	if (keyword.isEmpty())
		return;
	if (searchMenu)
		searchMenu->hide();
	if (!IsNetworkAccessible()) {
		ShowErrorToast(QTStr("main.giphy.network.toast.error"));
		return;
	}

	webHandler->DiscardTask();

	isSearching = true;
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "Search line edit tiggered", ACTION_CLICK);
	searchKeyword = keyword.simplified();
	InitSearchResultList();

	stopProcess = true;
	timerSearch.start(500);
}

void PLSGiphyStickerView::OnSearchMenuRequested(bool show)
{
	ui->lineEdit_search->setPlaceholderText("");
	showDefaultKeyWord = false;
	if (searchMenu) {
		delete searchMenu;
		searchMenu = nullptr;
		InitMenuList();
		if (show) {
			searchMenu->Show(ui->lineEdit_search->mapTo(this, QPoint(0, ui->lineEdit_search->height())), ui->lineEdit_search->width());
			searchMenu->raise();
			searchMenu->update();
		}
		searchMenu->setVisible(show);
	}
}

void PLSGiphyStickerView::on_btn_cancel_clicked()
{
	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "Search cancel button", ACTION_CLICK);
	bool networkValid = IsNetworkAccessible();
	if (TrendingTab == currentType) {
		trendingScrollList->show();
		if (networkValid)
			retryTrendingList();
	} else if (RecentTab == currentType)
		retryRencentList();
	isSearching = false;
	isSearchMode = false;
	ui->btn_cancel->hide();
	ui->lineEdit_search->clear();
	lastSearchResponOffset = -1;
	searchKeyword.clear();
	if (searchResultList) {
		searchResultList->hide();
		searchResultList->ResetPagination();
		ClearSearchList();
	}

	if (networkErrorToast && networkErrorToast->isVisible()) {
		ShowErrorToast(networkErrorToast->GetMessageContent());
	}
}

void PLSGiphyStickerView::OriginalDownload(const GiphyData &stickerData, const QString &fileName, const QVariant &extra)
{
	emit StickerApply(fileName, stickerData, extra);
	if (WriteRecentStickerDataToFile(stickerData))
		UpdateRecentList();
}

void PLSGiphyStickerView::OriginalDownLoadFailed()
{
	if (!network_accessible) {
		ShowErrorToast(QTStr("main.giphy.network.toast.error"));
		return;
	}
	ShowErrorToast(QTStr("main.giphy.network.download.faild"));
}

void PLSGiphyStickerView::OnNetworkAccessibleChanged(bool accessible)
{
	network_accessible = accessible;
	if (accessible)
		HideErrorToast();
	else {
		ShowRetryPage();
	}
}

void PLSGiphyStickerView::OnLoadingVisible(const RequestTaskData &task, bool visible)
{
	if (task.randomId == 0)
		return;

	if (task.randomId == ConvertPointer(trendingScrollList))
		trendingScrollList->SetLoadingVisible(visible);
	else if (task.randomId == ConvertPointer(searchResultList)) {
		if (!searchResultList->isVisible())
			trendingScrollList->SetLoadingVisible(visible);
		else
			searchResultList->SetLoadingVisible(visible);
	}
}

MovieLabel::MovieLabel(QWidget *parent) : QLabel(parent)
{
	this->setCursor(Qt::PointingHandCursor);
	this->setAutoFillBackground(true);
	this->setMouseTracking(true);
	this->setFocusPolicy(Qt::StrongFocus);

	this->setProperty("useDefaultIcon", true);
	pls_flush_style(this);

	labelLoad = pls_new<QLabel>(this);
	labelLoad->setObjectName("labelIconLoading");
	labelLoad->hide();

	connect(GiphyDownloader::instance(), &GiphyDownloader::downloadResult, this, &MovieLabel::downloadCallback, Qt::QueuedConnection);
	connect(this, SIGNAL(excuteTask(const DownloadTaskData &)), GiphyDownloader::instance(), SLOT(excuteTask(const DownloadTaskData &)));
}

MovieLabel::~MovieLabel()
{
	if (loadingEvent) {
		loadingEvent->deleteLater();
		loadingEvent = nullptr;
	}
	timer.stop();
	imageFrames.clear();
}

void MovieLabel::SetData(const GiphyData &data)
{
	giphyData = data;
	DownloadPreview();
}

void MovieLabel::SetShowLoad(bool visible)
{
	if (visible) {
		if (nullptr == loadingEvent)
			loadingEvent = new PLSLoadingEvent;
		loadingEvent->startLoadingTimer(labelLoad);
	} else {
		if (loadingEvent) {
			loadingEvent->stopLoadingTimer();
			loadingEvent->deleteLater();
		}
	}
	labelLoad->move(this->width() / 2 - labelLoad->width() / 2, this->height() / 2 - labelLoad->height() / 2);
	labelLoad->setVisible(visible);
	labelLoad->raise();
}

void MovieLabel::SetClicked(bool clicked_)
{
	clicked = clicked_;
	this->setProperty(STATUS_CLICKED, clicked_);
	pls_flush_style(this);
}

bool MovieLabel::Clicked() const
{
	return clicked;
}

void MovieLabel::DownloadCallback(const TaskResponData &result, void *param)
{
	auto movieLabel = (MovieLabel *)(param);
	if (movieLabel) {
		QMetaObject::invokeMethod(movieLabel, "downloadCallback", Qt::QueuedConnection, Q_ARG(const TaskResponData &, result));
	}
}

void MovieLabel::DownloadOriginal()
{
	DownloadTaskData currentTask;
	currentTask.SourceSize = giphyData.sizeOriginal;
	currentTask.url = giphyData.originalUrl;
	currentTask.needRetry = false;
	currentTask.uniqueId = giphyData.id;
	currentTask.type = StickerDownloadType::ORIGINAL;

	if (nullptr == downloadOriginalObj) {
		downloadOriginalObj = pls_new<QObject>(this);
	}
	SetShowLoad(true);
	currentTask.randomId = PLSGiphyStickerView::ConvertPointer(downloadOriginalObj);
	emit excuteTask(currentTask);
}

void MovieLabel::LoadFrames()
{
	imageFrames.clear();
	QFile file(giphyFileInfo.fileName);
	if (!file.exists()) {
		setProperty("useDefaultIcon", true);
		pls_flush_style(this);
		DownloadPreview();
		return;
	}
	QImage image(giphyFileInfo.taskData.SourceSize, QImage::Format_RGB16);
	QImageReader imageReader;
	imageReader.setFileName(giphyFileInfo.fileName);
	imageReader.setQuality(30);
	int imageCount = imageReader.imageCount();
	bool needDownsample = false;
	if (imageCount > MAX_PREVIEW_FRAME) {
		needDownsample = true;
	}
	for (int i = 0; i < imageCount; ++i) {
		if (imageReader.read(&image)) {
			if (needDownsample && i % 2 != 0)
				continue;
			imageFrames.emplace_back(QPixmap::fromImage(image));
		}
	}
	imageDelay = imageReader.nextImageDelay();
}

void MovieLabel::ResizeScale()
{
	scaledSize = giphyFileInfo.taskData.SourceSize;
	if (scaledSize.width() > STICKER_DISPLAY_SIZE || scaledSize.height() > STICKER_DISPLAY_SIZE) {
		float radio = 1.0;
		if (scaledSize.width() > scaledSize.height()) {
			if (scaledSize.width() > 0)
				radio = STICKER_DISPLAY_SIZE / (float)scaledSize.width();

		} else {
			if (scaledSize.height() > 0)
				radio = STICKER_DISPLAY_SIZE / (float)scaledSize.height();
		}
		scaledSize.setWidth((int)(radio * (float)scaledSize.width()));
		scaledSize.setHeight((int)(radio * (float)scaledSize.height()));
	}

	UpdateRect();
}

void MovieLabel::UpdateRect()
{
	int displayWidth = scaledSize.width();
	int displayHeight = scaledSize.height();
	rectTarget.setRect((this->width() - displayWidth) / 2, (this->height() - displayHeight) / 2, displayWidth, displayHeight);
}

void MovieLabel::DownloadPreview()
{
	DownloadTaskData currentTask;
	currentTask.SourceSize = giphyData.sizePreview;
	currentTask.url = giphyData.previewUrl;
	currentTask.uniqueId = giphyData.id;
	currentTask.type = StickerDownloadType::THUMBNAIL;
	currentTask.randomId = PLSGiphyStickerView::ConvertPointer(this);
	emit excuteTask(currentTask);
}

void MovieLabel::mousePressEvent(QMouseEvent *ev)
{
	if (ev->button() != Qt::LeftButton) {
		QLabel::mousePressEvent(ev);
		return;
	}

	PLS_UI_STEP(MAIN_GIPHY_STICKER_MODULE, "Sticker movie label", ACTION_CLICK);
	if (LocalGlobalVars::g_prevClickedItem && LocalGlobalVars::g_prevClickedItem != this) {
		LocalGlobalVars::g_prevClickedItem->SetClicked(false);
	}
	this->SetClicked(true);
	LocalGlobalVars::g_prevClickedItem = this;
	QLabel::mousePressEvent(ev);
	DownloadOriginal();
}

void MovieLabel::resizeEvent(QResizeEvent *ev)
{
	labelLoad->move(this->width() / 2 - labelLoad->width() / 2, this->height() / 2 - labelLoad->height() / 2);
	UpdateRect();
	QLabel::resizeEvent(ev);
}

void MovieLabel::timerEvent(QTimerEvent *event)
{
	if (this->visibleRegion().isEmpty()) {
		if (!imageFrames.empty())
			imageFrames.clear();
		QLabel::timerEvent(event);
		return;
	}

	if (imageFrames.empty()) {
		LoadFrames();
	}

	if (event->timerId() == timer.timerId() && !imageFrames.empty()) {
		if (step >= imageFrames.size() - 1) {
			step = -1;
		}
		step++;
		update();
	}
	QLabel::timerEvent(event);
}

void MovieLabel::paintEvent(QPaintEvent *event)
{
	QPainter dc(this);
	dc.setRenderHint(QPainter::SmoothPixmapTransform);
	if (!imageFrames.empty() && step >= 0 && step < imageFrames.size()) {
		dc.drawPixmap(rectTarget, imageFrames[step]);
	}
	QLabel::paintEvent(event);
}

void MovieLabel::showEvent(QShowEvent *event)
{
	if (imageDelay > 0 && !timer.isActive())
		timer.start(imageDelay, this);
	QLabel::showEvent(event);
}

void MovieLabel::hideEvent(QHideEvent *event)
{
	if (timer.isActive())
		timer.stop();
	imageFrames.clear();
	QLabel::hideEvent(event);
}

void MovieLabel::downloadCallback(const TaskResponData &responData)
{
	if (responData.taskData.randomId == 0)
		return;
	if (responData.taskData.randomId == PLSGiphyStickerView::ConvertPointer(this)) {
		if (responData.resultType == ResultStatus::GIPHY_NO_ERROR) {
			if (!responData.fileName.isEmpty() && QFile::exists(responData.fileName)) {
				giphyFileInfo = responData;
				needRetry = false;
				this->setProperty("useDefaultIcon", false);
				pls_flush_style(this);
				ResizeScale();
				LoadFrames();
				timer.start(imageDelay, this);
			} else {
				PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "sticker file name is not exsit,use default icon");
				this->setProperty("useDefaultIcon", true);
				pls_flush_style(this);
			}

		} else {
			PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "sticker download error,use default icon");
			emit DownLoadError();
			this->setProperty("useDefaultIcon", true);
			pls_flush_style(this);
		}
	} else if (responData.taskData.randomId == PLSGiphyStickerView::ConvertPointer(downloadOriginalObj)) {
		SetShowLoad(false);
		SetClicked(false);
		if (responData.resultType == ResultStatus::GIPHY_NO_ERROR) {
			QString fileName = responData.fileName;
			if (!fileName.isEmpty()) {
				emit OriginalDownloaded(giphyData, fileName, responData.taskData.extraData);
			} else
				PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "original sticker file name is empty");
		} else {
			PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "original sticker download error");
			PLS_LOGEX(PLS_LOG_ERROR, MAIN_GIPHY_STICKER_MODULE, {{PTS_LOG_TYPE, PTS_TYPE_EVENT}}, "Download Giphy ItemId('%s') failed", qUtf8Printable(responData.taskData.uniqueId));
			emit DownLoadError();
		}
	}
}

NoDataPage::NoDataPage(QWidget *parent) : QFrame(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	this->setCursor(Qt::ArrowCursor);
	QVBoxLayout *layout_main = pls_new<QVBoxLayout>(this);
	layout_main->setContentsMargins(0, 0, 0, 0);
	layout_main->setSpacing(0);

	QVBoxLayout *layout = pls_new<QVBoxLayout>();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(18);
	layout->setAlignment(Qt::AlignHCenter);

	iconNoData = pls_new<QLabel>(this);
	iconNoData->setObjectName("iconNoData");

	textNoDataTip = pls_new<QLabel>(this);
	textNoDataTip->setAlignment(Qt::AlignCenter);
	textNoDataTip->setObjectName("textNoData");
	textNoDataTip->setWordWrap(false);

	btnRetry = pls_new<QPushButton>(this);
	btnRetry->setFocusPolicy(Qt::NoFocus);
	connect(btnRetry, &QPushButton::clicked, this, &NoDataPage::retryClicked);
	btnRetry->setObjectName("networkErrorRetryBtn");
	btnRetry->setText(QTStr("Retry"));

	layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
	layout->addWidget(iconNoData, 0, Qt::AlignHCenter);
	layout->addWidget(textNoDataTip, 0, Qt::AlignHCenter);
	layout->addWidget(btnRetry, 0, Qt::AlignHCenter);
	layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

	spaceBottom = pls_new<QLabel>(this);
	spaceBottom->setObjectName("noDataPageBottomSpace");

	layout_main->addItem(layout);
	layout_main->addWidget(spaceBottom, 0, Qt::AlignHCenter);

	SetPageType(PageType::Page_Nodata);
}

void NoDataPage::SetPageType(PageType pageType)
{
	iconNoData->setProperty(PAGE_TYPE, static_cast<int>(pageType));
	pls_flush_style(iconNoData);
	switch (pageType) {
	case PageType::Page_Nodata:
		btnRetry->hide();
		break;
	case PageType::Page_NetworkError:
		btnRetry->show();
		break;
	default:
		break;
	}
}

void NoDataPage::SetNoDataTipText(const QString &tipText)
{
	if (textNoDataTip) {
		textNoDataTip->setText(tipText);
		textNoDataTip->adjustSize();
	}
}

ScrollAreaWithNoDataTip::ScrollAreaWithNoDataTip(QWidget *parent) : PLSFloatScrollBarScrollArea(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	this->setCursor(Qt::ArrowCursor);
	pageNoData = pls_new<NoDataPage>(this);
	connect(pageNoData, &NoDataPage::retryClicked, this, &ScrollAreaWithNoDataTip::retryClicked);
	pageNoData->setObjectName("pageNoData");
	pageNoData->hide();
}

void ScrollAreaWithNoDataTip::ResetPagination()
{
	pagination = 0;
}

void ScrollAreaWithNoDataTip::SetPagintion(int page)
{
	pagination = page;
}

int ScrollAreaWithNoDataTip::GetPagination() const
{
	return pagination;
}

void ScrollAreaWithNoDataTip::ShowNoDataPage()
{
	if (pageNoData) {
		pageNoData->show();
		pageNoData->SetPageType(NoDataPage::PageType::Page_Nodata);
		pageNoData->raise();
		UpdateNodataPageGeometry();
	}
}

void ScrollAreaWithNoDataTip::HideNoDataPage()
{
	if (pageNoData) {
		pageNoData->hide();
	}
}

void ScrollAreaWithNoDataTip::SetNoDataPageVisible(bool visible)
{
	if (pageNoData) {
		pageNoData->setVisible(visible);
		pageNoData->SetPageType(NoDataPage::PageType::Page_Nodata);
		pageNoData->raise();
		UpdateNodataPageGeometry();
	}
}

void ScrollAreaWithNoDataTip::SetNoDataTipText(const QString &tipText)
{
	if (pageNoData) {
		pageNoData->SetNoDataTipText(tipText);
	}
}

void ScrollAreaWithNoDataTip::SetShowRetryPage()
{
	if (pageNoData) {
		pageNoData->show();
		pageNoData->SetPageType(NoDataPage::PageType::Page_NetworkError);
		pageNoData->raise();
		UpdateNodataPageGeometry();
	}
}

void ScrollAreaWithNoDataTip::HideRetryPage()
{
	if (pageNoData) {
		pageNoData->hide();
	}
}

void ScrollAreaWithNoDataTip::SetRetryPageVisible(bool visible)
{
	if (pageNoData) {
		pageNoData->setVisible(visible);
		pageNoData->SetPageType(NoDataPage::PageType::Page_NetworkError);
		pageNoData->raise();
		UpdateNodataPageGeometry();
	}
}

void ScrollAreaWithNoDataTip::SetRetryTipText(const QString &tipText)
{
	if (pageNoData) {
		pageNoData->SetNoDataTipText(tipText);
	}
}

bool ScrollAreaWithNoDataTip::IsNoDataPageVisible() const
{
	return pageNoData->isVisible();
}

void ScrollAreaWithNoDataTip::ShowLoading()
{
	if (nullptr == loadingLayer) {
		loadingLayer = pls_new<QFrame>(this);
		loadingLayer->setObjectName("scrollAreaLoadingLayer");
		loadingLayer->setMouseTracking(true);
		QVBoxLayout *layout = pls_new<QVBoxLayout>(loadingLayer);
		QLabel *loadingLabel = pls_new<QLabel>(loadingLayer);
		loadingLabel->setObjectName("scrollAreaLoadingIcon");
		layout->addWidget(loadingLabel);
		layout->setAlignment(loadingLabel, Qt::AlignCenter);
	}
	UpdateLayerGeometry();
	loadingEvent.startLoadingTimer(loadingLayer->findChild<QLabel *>("scrollAreaLoadingIcon"));
	loadingLayer->show();
	loadingLayer->raise();
}

void ScrollAreaWithNoDataTip::HideLoading()
{
	if (!loadingLayer)
		return;
	loadingLayer->hide();
	loadingEvent.stopLoadingTimer();
}

void ScrollAreaWithNoDataTip::SetLoadingVisible(bool visible)
{
	if (visible)
		ShowLoading();
	else
		HideLoading();
}

void ScrollAreaWithNoDataTip::UpdateLayerGeometry()
{
	if (!loadingLayer)
		return;
	loadingLayer->resize(this->width() - 2, this->height() - 1);
	loadingLayer->move(1, 0);
}

void ScrollAreaWithNoDataTip::UpdateNodataPageGeometry()
{
	if (pageNoData) {
		pageNoData->move(0, 0);
		pageNoData->resize(this->size().width(), this->size().height());
		pageNoData->raise();
		pageNoData->update();
	}
}

void ScrollAreaWithNoDataTip::resizeEvent(QResizeEvent *event)
{
	UpdateNodataPageGeometry();
	UpdateLayerGeometry();
	PLSFloatScrollBarScrollArea::resizeEvent(event);
}
