#include "PLSPrismSticker.h"
#include "ui_PLSPrismSticker.h"
#include "pls-common-language.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "qt-wrappers.hpp"
//#include "main-view.hpp"
//#include "pls-app.hpp"
#include "json-data-handler.hpp"
#include "liblog.h"
#include "action.h"
#include "flowlayout.h"
//#include "PLSBgmLibraryView.h"
#include "PLSResourceManager.h"
//#include "libhttp-client/libhttp-client.h"
//#include "PLSNetworkMonitor.h"
#include "platform.hpp"
#include "PLSBasic.h"
#include "libhttp-client.h"

#include <QButtonGroup>
#include <QJsonDocument>
#include <QEventLoop>

#include <stdio.h>
//#include "unzip.h"
#include "utils-api.h"
#include "frontend-api.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

constexpr auto CATEGORY_ID_ALL = "All";
constexpr auto CATEGORY_ID_RECENT = "Recent";
constexpr auto RECENT_LIST_KEY = "recentList";
constexpr auto USE_SENSE_TIME_ID = "MASK";

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

PLSPrismSticker::PLSPrismSticker(QWidget *parent) : PLSSideBarDialogView({298, 817, 5, ConfigId::PrismStickerConfig}, parent)
{
	ui = pls_new<Ui::PLSPrismSticker>();
	setupUi(ui);
	qRegisterMetaType<StickerPointer>("StickerPointer");
	pls_add_css(this, {"PLSPrismSticker", "PLSToastMsgFrame", "PLSThumbnailLabel", "ScrollAreaWithNoDataTip"});
	timerLoading = pls_new<QTimer>(this);
	pls_network_state_monitor([this](bool accessible) { OnNetworkAccessibleChanged(accessible); });
	btnMore = pls_new<QPushButton>(ui->category);
	connect(btnMore, &QPushButton::clicked, this, &PLSPrismSticker::OnBtnMoreClicked);
	connect(PLSBasic::Get(), &PLSBasic::mainClosing, this, &PLSPrismSticker::OnAppExit, Qt::DirectConnection);
	btnMore->hide();
	PLSStickerDataHandler::SetClearDataFlag(false);
	timerLoading->setInterval(LOADING_TIMER_TIMEROUT);
	ui->category->installEventFilter(this);
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	setWindowTitle(tr(MIAN_PRISM_STICKER_TITLE));
	InitScrollView();
	HandleStickerData();
}

PLSPrismSticker::~PLSPrismSticker()
{
	exit = true;
	timerLoading->stop();

	pls_delete(ui, nullptr);
}

bool PLSPrismSticker::SaveStickerJsonData() const
{
	QString userFileName = pls_get_user_path(PRISM_STICKER_RECENT_JSON_FILE);
	QFile file(userFileName);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
		PLS_ERROR(MAIN_PRISM_STICKER, "Could not open Prism sticker recent_used json for writing: %s\n", qUtf8Printable(file.errorString()));
		return false;
	}

	QJsonObject recentListObj;
	QJsonArray recentListArray;
	for (const auto &item : recentStickerData) {
		QJsonObject itemObj;
		itemObj.insert("id", item.id);
		itemObj.insert("title", item.title);
		itemObj.insert("thumbnailUrl", item.thumbnailUrl);
		itemObj.insert("resourceUrl", item.resourceUrl);
		itemObj.insert("category", item.category);
		itemObj.insert("version", item.version);
		recentListArray.push_back(itemObj);
	}
	recentListObj.insert(RECENT_LIST_KEY, recentListArray);

	auto oldSize = file.readAll().size();
	QJsonDocument document;
	document.setObject(recentListObj);
	QByteArray json = document.toJson(QJsonDocument::Indented);
	if (json.size() < oldSize)
		file.resize(json.size());
	file.seek(0);
	file.write(json);
	file.flush();
	file.close();
	return true;
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
		for (auto &group : stickers) {
			group.second.categoryName = (0 == group.first.compare("RandomTouch")) ? "Popping" : group.second.categoryName;
			createBtn(group.first, group.second.categoryName);
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

static int handleItems(const QJsonArray &json, const QString &groupId, std::vector<StickerData> &items)
{
	auto iter = json.constBegin();
	int count = 0;
	while (iter != json.constEnd()) {
		auto item = iter->toObject();
		auto itemId = item.value("itemId").toString();
		auto idSplit = itemId.split("_");
		if (!idSplit.empty() && idSplit[0] == USE_SENSE_TIME_ID) {
			iter++;
			continue;
		}
		StickerData data;
		data.id = itemId;
		data.version = item.value("version").toInt(0);
		data.title = item.value("title").toString();
		data.resourceUrl = item.value("resourceUrl").toString();
		data.thumbnailUrl = item.value("thumbnailUrl").toString();
		data.category = groupId;
		items.emplace_back(data);
		count++;
		iter++;
	}
	return count;
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

bool PLSPrismSticker::LoadLocalJsonFile(const QString &fileName)
{
	QByteArray array;

	if (!PLSJsonDataHandler::getJsonArrayFromFile(array, fileName))
		return false;

	auto obj = QJsonDocument::fromJson(array).object();
	if (obj.isEmpty())
		return false;

	if (!obj.contains("group") || !obj.value("group").isArray())
		return false;

	QMutexLocker locker(&mutex);
	// Recently used Tab
	StickerDataIndex dataIndex;
	dataIndex.categoryName = tr(MAIN_PRISM_STICKER_RECENT);
	stickers.emplace_back(CATEGORY_ID_RECENT, dataIndex);

	// Other Tabs
	ResetDataIndex(dataIndex);
	auto groups = obj.value("group").toArray();
	auto iter = groups.constBegin();
	int index = 0;
	while (iter != groups.constEnd()) {
		auto group = iter->toObject();
		QString groupId = group.value("groupId").toString();
		auto items = group.value("items").toArray();
		if (!items.empty()) {
			int count = handleItems(items, groupId, allStickerData);
			dataIndex.startIndex = index;
			index += count;
			dataIndex.endIndex = index - 1;
			stickers.emplace_back(groupId, dataIndex);
		}
		iter++;
	}

	// All Tab
	dataIndex.startIndex = 0;
	dataIndex.categoryName = tr("All");
	dataIndex.endIndex = allStickerData.size() - 1;
	stickers.emplace(stickers.begin(), CATEGORY_ID_ALL, dataIndex);
	return true;
}

static bool GetStickerDataIndex(const QString &categoryId, const std::vector<std::pair<QString, StickerDataIndex>> &targetData, StickerDataIndex &dataIndex)
{
	for (const auto &data : targetData) {
		if (data.first == categoryId) {
			dataIndex = data.second;
			return true;
		}
	}
	return false;
}

void ParseJsonArrayToVector(const QJsonArray &array, std::vector<StickerData> &data)
{
	for (const auto &item : array) {
		auto obj = item.toObject();
		StickerData itemData;
		itemData.id = obj["id"].toString();
		itemData.title = obj["title"].toString();
		itemData.thumbnailUrl = obj["thumbnailUrl"].toString();
		itemData.resourceUrl = obj["resourceUrl"].toString();
		itemData.category = obj["category"].toString();
		itemData.version = obj["version"].toInteger();
		data.emplace_back(itemData);
	}
}

bool GetStickerThumbnailFileName(const StickerData &data, QString &in)
{
	if (data.resourceUrl.isEmpty() || data.thumbnailUrl.isEmpty())
		return false;
	QString filePath = pls_get_user_path(PRISM_STICKER_CACHE_PATH);
	QUrl thumbnail(data.thumbnailUrl);
	in.append(filePath).append(thumbnail.fileName());
	return true;
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

bool PLSPrismSticker::LoadStickers(QLayout *layout, ScrollAreaWithNoDataTip *targetSa, QWidget *parent, const std::vector<StickerData> &stickerList, const StickerDataIndex &index, bool /*async*/)
{
	if (targetSa == nullptr)
		return false;
	if (stickerList.empty()) {
		targetSa->SetNoDataPageVisible(true);
		targetSa->SetNoDataTipText(tr("main.prism.sticker.noData.tips"));
		return true;
	}
	targetSa->SetNoDataPageVisible(false);
	auto future = QtConcurrent::run([this, index, stickerList, parent, layout]() {
		for (size_t i = index.startIndex; i <= index.endIndex; ++i) {
			if (pls_get_app_exiting() || i >= stickerList.size() || stickerList.empty())
				break;
			const auto &data = stickerList[i];
			QMetaObject::invokeMethod(this, "LoadStickerAsync", Qt::QueuedConnection, Q_ARG(const StickerData &, data), Q_ARG(QWidget *, parent), Q_ARG(QLayout *, layout));
			QThread::msleep(20);
		}
	});
	return true;
}

bool PLSPrismSticker::LoadViewPage(const QString &categoryId, const QWidget *page, QLayout *layout)
{
	bool ok = false;
	StickerDataIndex dataIndex;
	if (GetStickerDataIndex(categoryId, stickers, dataIndex)) {
		auto targetSa = page->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
		if (CATEGORY_ID_RECENT == categoryId) {
			dataIndex.endIndex = recentStickerData.size() - 1;
			ok = LoadStickers(layout, targetSa, this, recentStickerData, dataIndex, false);
		} else {
			ok = LoadStickers(layout, targetSa, this, allStickerData, dataIndex, true);
		}
	}
	return ok;
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

bool PLSPrismSticker::CreateRecentJsonFile() const
{
	QString userPath = pls_get_user_path(PRISM_STICKER_USER_PATH);
	QString userFileName = pls_get_user_path(PRISM_STICKER_RECENT_JSON_FILE);
	QDir dir(userPath);
	if (!dir.exists())
		dir.mkpath(userPath);

	QFile file(userFileName);
	if (!file.open(QIODevice::ReadWrite)) {
		PLS_ERROR(MAIN_PRISM_STICKER, "Could not open Prism sticker recent_used json file for writing: %s\n", qUtf8Printable(file.errorString()));
		return false;
	}

	QByteArray json = file.readAll();
	if (json.isEmpty()) {
		QJsonObject obj;
		QJsonArray arrayRecentList;
		obj.insert(RECENT_LIST_KEY, arrayRecentList);
		QJsonDocument document;
		document.setObject(obj);
		json = document.toJson(QJsonDocument::Indented);
		file.write(json);
	}
	file.close();
	return true;
}

bool PLSPrismSticker::InitRecentSticker()
{
	if (!CreateRecentJsonFile())
		return false;
	QString userFileName = pls_get_user_path(PRISM_STICKER_RECENT_JSON_FILE);
	QFile file(userFileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		PLS_ERROR(MAIN_PRISM_STICKER, "Could not open Prism sticker recent_used json file for writing: %s\n", qUtf8Printable(file.errorString()));
		return false;
	}

	QJsonParseError parseError;
	QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		file.close();
		return false;
	}

	auto jsonObjSticker = document.object();
	recentStickerData.clear();
	ParseJsonArrayToVector(jsonObjSticker[RECENT_LIST_KEY].toArray(), recentStickerData);
	file.close();
	return true;
}

bool PLSPrismSticker::UpdateRecentList(const StickerData &data)
{
	auto iter = recentStickerData.begin();
	for (const auto &item : recentStickerData) {
		if (item.id == data.id) {
			recentStickerData.erase(iter);
			break;
		}
		iter++;
	}
	recentStickerData.insert(recentStickerData.begin(), data);
	if (recentStickerData.size() > MAX_STICKER_COUNT)
		recentStickerData.erase(recentStickerData.end() - 1);
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
	if (stickers.empty()) {
		m_pNodataPage->resize(content()->size());
		m_pNodataPage->move(content()->mapTo(this, QPoint(0, 0)));
	} else {
		int y = ui->category->geometry().bottom();
		m_pNodataPage->resize(width() - 2, content()->height() - y - 1);
		m_pNodataPage->move(1, ui->category->mapTo(this, QPoint(0, ui->category->height())).y());
	}
}

StickerHandleResult PLSPrismSticker::RemuxFile(const StickerData &data, StickerPointer label, QString path, QString configFile)
{
	StickerHandleResult result;
	result.data = data;
	QDir dir(path);
	if (!dir.exists()) {
		// Search for the zip package.
		auto zipFile = PLSStickerDataHandler::GetStickerResourceFile(data);
		QFile file(zipFile);
		if (file.exists()) {
			QString error;
			QString targetPath = PLSStickerDataHandler::GetStickerResourceParentDir(data);
			if (!PLSStickerDataHandler::UnCompress(zipFile, targetPath, error)) {
				PLS_WARN(MAIN_PRISM_STICKER, "%s", qUtf8Printable(error));
				return result;
			}
			if (!file.remove()) {
				PLS_INFO(MAIN_PRISM_STICKER, "Remove: %s failed", qUtf8Printable(getFileName(zipFile)));
			}
		} else {
			PLS_INFO(MAIN_PRISM_STICKER, "Sticker resource zipFile:%s does not exsit, download resource", qUtf8Printable(getFileName(zipFile)));
			DownloadResource(data, label);
			result.breakFlow = true;
			return result;
		}
	}

	// Get config json file.
	QFile file(configFile);
	if (!file.exists()) {
		PLS_INFO(MAIN_PRISM_STICKER, "Sticker resource config file:%s does not exsit, download resource", qUtf8Printable(getFileName(configFile)));
		DownloadResource(data, label);
		result.breakFlow = true;
		return result;
	}

	StickerParamWrapper *wrapper = PLSStickerDataHandler::CreateStickerParamWrapper(data.category);
	if (!wrapper->Serialize(configFile)) {
		pls_delete(wrapper, nullptr);
		return result;
	}
	bool ok = true;
	for (const auto &config : wrapper->m_config) {
		auto remuxedFile = path + config.resourceDirectory + ".mp4";
		auto resourcePath = path + config.resourceDirectory;
		if (!QFile::exists(remuxedFile) && !PLSStickerDataHandler::MediaRemux(resourcePath, remuxedFile, config.fps)) {
			ok = false;
			PLS_WARN(MAIN_PRISM_STICKER, "remux file:%s failed", qUtf8Printable(getFileName(remuxedFile)));
			continue;
		}

		QString imageFile = PLSStickerDataHandler::getTargetImagePath(resourcePath, data.category, data.id, Orientation::landscape == config.orientation);

		if (Orientation::landscape == config.orientation) {
			result.landscapeVideoFile = remuxedFile;
			result.landscapeImage = imageFile; // TODO
		} else {
			result.portraitVideo = remuxedFile;
			result.portraitImage = imageFile; // TODO
		}
	}
	pls_delete(wrapper, nullptr);
	result.success = ok;
	result.AdjustParam();
	return result;
}

void PLSPrismSticker::UserApplySticker(const StickerData &data, StickerPointer label)
{

	// There are normally three steps to apply a sticker.
	// 1. download resources if needed.
	// 2. uncompress zip file if needed.
	// 3. remux images files to a video file.
	qint64 dateTime = QDateTime::currentMSecsSinceEpoch();
	auto path = PLSStickerDataHandler::GetStickerResourcePath(data);
	auto configFile = path + PLSStickerDataHandler::GetStickerConfigJsonFileName(data);

	QPointer<PLSPrismSticker> guard(this);
	QEventLoop loop;
	// Do remux if needed.
	QFutureWatcher<StickerHandleResult> watcher;
	connect(&watcher, SIGNAL(finished()), &loop, SLOT(quit()));
	QFuture<StickerHandleResult> future = QtConcurrent::run([this, data, path, label, configFile]() { return RemuxFile(data, label, path, configFile); });
	watcher.setFuture(future);
	loop.exec();

	if (pls_get_app_exiting() || !guard)
		return;

	StickerHandleResult result = watcher.result();
	if (result.breakFlow)
		return;
	if (label) {
		label->SetShowLoad(false);
		qint64 gap = QDateTime::currentMSecsSinceEpoch() - dateTime;
		if (gap < LOADING_TIME_MS) {
			QTimer::singleShot(LOADING_TIME_MS - gap, this, [label]() { label->SetShowOutline(false); });
		} else {
			label->SetShowOutline(false);
		}
	}
	if (result.success && !pls_get_app_exiting()) {
		emit StickerApplied(result);
		UpdateRecentList(data);
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

void PLSPrismSticker::DownloadResource(const StickerData &data, StickerPointer label) {}

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
		QTimer::singleShot(0, this, [this]() {
			InitCategory();
			AdjustCategoryTab();
			SwitchToCategory((!recentStickerData.empty()) ? CATEGORY_ID_RECENT : CATEGORY_ID_ALL);
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
	auto watcher = static_cast<QFutureWatcher<bool> *>(sender());
	bool ok = watcher->result();
	if (ok) {
		isDataReady = true;
		if (isShown) {
			QTimer::singleShot(0, this, [this]() {
				InitCategory();
				if (!NetworkAccessible()) {
					ShowNoNetworkPage(tr("main.giphy.network.toast.error"), NoNetwork);
				}
				QTimer::singleShot(0, this, [this]() {
					AdjustCategoryTab();
					SwitchToCategory((!recentStickerData.empty()) ? CATEGORY_ID_RECENT : CATEGORY_ID_ALL);
				});
			});
		}
	} else {
		auto path = watcher->property("fileName").toString();
		PLS_WARN(MAIN_PRISM_STICKER, "[Prism-Sticker]Load %s Failed", qUtf8Printable(getFileName(path)));
	}
}

void PLSPrismSticker::HandleDownloadInitJson(const TaskResponData &result)
{
	if (result.resultType != ResultStatus::ERROR_OCCUR) {
		HandleStickerData();
	} else {
		ShowToast(QTStr("main.giphy.network.download.faild"));
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
	if (stickers.empty())
		DownloadCategoryJson();
}

void PLSPrismSticker::OnRetryOnNoNetwork()
{
	PLS_UI_STEP(MAIN_PRISM_STICKER, "User click retry to download reaction json", ACTION_CLICK);

	if (NetworkAccessible() && !stickers.empty()) {
		HideNoNetworkPage();
	} else {
		DownloadCategoryJson();
	}

	if (!NetworkAccessible()) {
		QTimer::singleShot(0, this, [this]() { ShowToast(QTStr("main.giphy.network.toast.error")); });
	}
}

void PLSPrismSticker::HandleDownloadResult(const TaskResponData &result, const StickerData &data, StickerPointer label)
{
	if (result.resultType != ResultStatus::ERROR_OCCUR) {
		PLS_INFO(MAIN_PRISM_STICKER, "Download Sticker file: '%s/%s' successfully", qUtf8Printable(data.category), qUtf8Printable(data.id));
		PLSStickerDataHandler::WriteDownloadCache(data.id, result.taskData.version, downloadCache);
		//Uncompress zip file.
		auto zipFile = PLSStickerDataHandler::GetStickerResourceFile(data);
		QFile file(zipFile);
		QString error;
		QString targetPath = PLSStickerDataHandler::GetStickerResourceParentDir(data);
		if (!PLSStickerDataHandler::UnCompress(zipFile, targetPath, error)) {
			PLS_WARN(MAIN_PRISM_STICKER, "%s", qUtf8Printable(error));
			return;
		}
		if (!file.remove())
			PLS_INFO(MAIN_PRISM_STICKER, "Remove: %s failed", qUtf8Printable(getFileName(zipFile)));

		UserApplySticker(data, label);
	} else {
		PLS_INFO(MAIN_PRISM_STICKER, "Download %s failed!", qUtf8Printable(result.taskData.url));
		if (label) {
			label->SetShowLoad(false);
			label->SetShowOutline(false);
			QString tips = (result.subType == ErrorSubType::Error_Timeout) ? QTStr("main.giphy.network.request.timeout") : QTStr("main.giphy.network.download.faild");
			ShowToast(tips);
		}
	}
}

void PLSPrismSticker::DownloadJsonFileTimeOut()
{
	pls_check_app_exiting();
	HideLoading();
	ShowNoNetworkPage(tr("main.giphy.network.request.timeout"), Timeout);
	auto ret = pls_show_download_failed_alert(this);
	if (ret == PLSAlertView::Button::Ok) {
		PLS_INFO(MAIN_BEAUTY_MODULE, "Prism Sticker: User select retry download.");
		DoDownloadJsonFile();
	}
}

void PLSPrismSticker::OnDownloadJsonFailed()
{
	pls_check_app_exiting();
	auto ret = pls_show_download_failed_alert(this);
	if (ret == PLSAlertView::Button::Ok) {
		PLS_INFO(MAIN_BEAUTY_MODULE, "Prism Sticker: User select retry download.");
		DoDownloadJsonFile();
	}
}

void PLSPrismSticker::OnAppExit()
{
	if (!PLSStickerDataHandler::GetClearDataFlag())
		SaveStickerJsonData();
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

	QString fileName = pls_get_user_path(PRISM_STICKER_JSON_FILE);
	QFile file(fileName);
	if (!file.exists()) {
		PLS_WARN(MAIN_PRISM_STICKER, "Sticker reaction.json file not exist, redownload");
		DownloadCategoryJson();
		return;
	}
	QFutureWatcher<bool> *watcher = pls_new<QFutureWatcher<bool>>(this);
	watcher->setProperty("fileName", fileName);
	connect(watcher, SIGNAL(finished()), this, SLOT(OnHandleStickerDataFinished()));
	QFuture<bool> future = QtConcurrent::run([this, fileName]() {
		bool ok = LoadLocalJsonFile(fileName);
		ok = ok && InitRecentSticker();
		return ok;
	});
	watcher->setFuture(future);
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
		UserApplySticker(sticker_data, label);
	});
	layout->addWidget(label);
	layout->update();
}

void PLSPrismSticker::DownloadCategoryJson() {}

bool PLSPrismSticker::NetworkAccessible() const
{
	return pls_get_network_state();
}

void PLSPrismSticker::DoDownloadJsonFile() {}
