#include "PLSPrismSticker.h"
#include "ui_PLSPrismSticker.h"
#include "pls-common-language.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "qt-wrappers.hpp"
#include "main-view.hpp"
#include "pls-app.hpp"
#include "json-data-handler.hpp"
#include "liblog.h"
#include "action.h"
#include "layout/flowlayout.h"
#include "PLSBgmLibraryView.h"
#include "PLSResources/PLSResourceMgr.h"
#include "PLSNetworkMonitor.h"
#include "PLSSyncServerManager.hpp"
#include "platform.hpp"

#include <QButtonGroup>
#include <QJsonDocument>
#include <QEventLoop>

#include <Windows.h>
#include <stdio.h>
#include "unzip.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#define CATEGORY_ID_ALL "All"
#define CATEGORY_ID_RECENT "Recent"
#define CATEGORY_ID_MORE "MORE"
#define RECENT_LIST_KEY "recentList"
#define USE_SENSE_TIME_ID "MASK"

// For Stickers flow layout.
const int FLOW_LAYOUT_SPACING = 10;
const int FLOW_LAYOUT_VSPACING = 10;
const int FLOW_LAYOUT_MARGIN_LEFT = 19;
const int FLOW_LAYOUT_MARGIN_RIGHT = 17;
const int FLOW_LAYOUT_MARGIN_TOP_BOTTOM = 20;
const int CATEGORY_TAB_HEIGHT = 109;
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

PLSPrismSticker::PLSPrismSticker(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView({298, 817, 5, ConfigId::PrismStickerConfig}, parent, dpiHelper), ui(new Ui::PLSPrismSticker)
{
	ui->setupUi(this->content());
	qRegisterMetaType<StickerPointer>("StickerPointer");
	notifyFirstShow([=]() { this->InitGeometry(true); });
	dpiHelper.setCss(this, {PLSCssIndex::PLSPrismSticker, PLSCssIndex::PLSToastMsgFrame, PLSCssIndex::PLSThumbnailLabel, PLSCssIndex::ScrollAreaWithNoDataTip});
	dpiHelper.notifyDpiChanged(this, [=](double dpi) { UpdateDpi(dpi); });
	timerLoading = new QTimer(this);
	connect(PLSNetworkMonitor::Instance(), &PLSNetworkMonitor::OnNetWorkStateChanged, this, &PLSPrismSticker::OnNetworkAccessibleChanged);
	btnMore = new QPushButton(ui->category);
	connect(btnMore, &QPushButton::clicked, this, &PLSPrismSticker::OnBtnMoreClicked);
	btnMore->hide();
	PLSStickerDataHandler::SetClearDataFlag(false);
	timerLoading->setInterval(LOADING_TIMER_TIMEROUT);
	ui->category->installEventFilter(this);
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	setWindowTitle(tr(MIAN_PRISM_STICKER_TITLE));
	obs_frontend_add_event_callback(OnFrontendEvent, this);
	InitScrollView();
	HandleStickerData();
}

PLSPrismSticker::~PLSPrismSticker()
{
	exit = true;
	timerLoading->stop();
	PLSFileDownloader::instance()->Stop();
	obs_frontend_remove_event_callback(OnFrontendEvent, this);
	delete ui;
}

bool PLSPrismSticker::SaveStickerJsonData()
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
		recentListArray.push_back(itemObj);
	}
	recentListObj.insert(RECENT_LIST_KEY, recentListArray);

	int oldSize = file.readAll().size();
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

bool PLSPrismSticker::WriteDownloadCache()
{
	QMutexLocker locker(&mutex);
	return PLSStickerDataHandler::WriteDownloadCacheToLocal(downloadCache);
}

void PLSPrismSticker::InitScrollView()
{
	if (nullptr == stackedWidget) {
		stackedWidget = new QStackedWidget(this);
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
		categoryBtn = new QButtonGroup(this);
		PLSDpiHelper dpiHelper;
		connect(categoryBtn, SIGNAL(buttonClicked(QAbstractButton *)), this, SLOT(OnCategoryBtnClicked(QAbstractButton *)));

		auto createBtn = [=](const QString &groupId, const QString &groupName) {
			QApplication::processEvents();
			CategoryButton *btn = new CategoryButton(ui->category);
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
		dpiHelper.setFixedSize(btnMore, {39, 35});
		QHBoxLayout *layout = new QHBoxLayout(btnMore);
		layout->setContentsMargins(0, 0, 0, 0);
		QLabel *labelIcon = new QLabel(btnMore);
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

bool PLSPrismSticker::LoadLocalJsonFile(const QString &fileName)
{
	QByteArray array;
	auto handleItems = [this](const QJsonArray &array, const QString &groupId, std::vector<StickerData> &items) {
		auto iter = array.constBegin();
		int count = 0;
		while (iter != array.constEnd()) {
			auto item = iter->toObject();
			auto itemId = item.value("itemId").toString();
			auto idSplit = itemId.split("_");
			if (idSplit.size() > 0) {
				if (idSplit[0] == USE_SENSE_TIME_ID) {
					iter++;
					continue;
				}
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
	};

	if (PLSJsonDataHandler::getJsonArrayFromFile(array, fileName)) {
		auto obj = QJsonDocument::fromJson(array).object();
		if (!obj.isEmpty()) {
			if (obj.contains("group") && obj.value("group").isArray()) {

				QMutexLocker locker(&mutex);
				// Recently used Tab
				StickerDataIndex dataIndex;
				dataIndex.categoryName = tr(MAIN_PRISM_STICKER_RECENT);
				stickers.emplace_back(std::make_pair(CATEGORY_ID_RECENT, dataIndex));

				// Other Tabs
				ResetDataIndex(dataIndex);
				auto groups = obj.value("group").toArray();
				auto iter = groups.constBegin();
				int index = 0;
				while (iter != groups.constEnd()) {
					auto group = iter->toObject();
					QString groupId = group.value("groupId").toString();
					auto items = group.value("items").toArray();
					if (items.size() > 0) {
						int count = handleItems(items, groupId, allStickerData);
						dataIndex.startIndex = index;
						index += count;
						dataIndex.endIndex = index - 1;
						stickers.emplace_back(std::make_pair(groupId, dataIndex));
					}
					iter++;
				}

				// All Tab
				dataIndex.startIndex = 0;
				dataIndex.categoryName = tr("All");
				dataIndex.endIndex = allStickerData.size() - 1;
				stickers.insert(stickers.begin(), std::make_pair(CATEGORY_ID_ALL, dataIndex));
				return true;
			}
		}
	}
	return false;
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

void PLSPrismSticker::SwitchToCategory(const QString &categoryId)
{
	categoryTabId = categoryId;
	if (nullptr != categoryBtn) {
		auto buttons = categoryBtn->buttons();
		for (auto &button : buttons) {
			if (button && button->property("groupId").toString() == categoryId) {
				button->setChecked(true);
				break;
			}
		}
	}

	auto cleanPage = [=](const QString &categoryId) {
		if (categoryViews.find(categoryId) != categoryViews.end()) {
			if (nullptr != categoryViews[categoryId]) {
				auto sa = categoryViews[categoryId]->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
				auto items = sa->widget()->findChildren<QPushButton *>("prismStickerLabel");
				auto fl = sa->widget()->layout();
				for (auto &item : items) {
					fl->removeWidget(item);
					delete item;
					item = nullptr;
				}
			}
		}
	};

	auto createViewPage = [=]() {
		QWidget *page = new QWidget;
		QVBoxLayout *layout = new QVBoxLayout(page);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		auto sa = new ScrollAreaWithNoDataTip(page);
		sa->SetScrollBarRightMargin(-1);
		sa->setObjectName("stickerScrollArea");

		sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Expanding);
		sp.setHorizontalStretch(0);
		sp.setVerticalStretch(0);
		sp.setHeightForWidth(sa->sizePolicy().hasHeightForWidth());
		sa->setSizePolicy(sp);
		sa->setWidgetResizable(true);

		QWidget *widgetContent = new QWidget();
		widgetContent->setMouseTracking(true);
		widgetContent->setObjectName("prismStickerContainer");
		auto dpi = PLSDpiHelper::getDpi(this);
		int sph = qFloor(dpi * FLOW_LAYOUT_SPACING);
		int spv = PLSDpiHelper::calculate(dpi, FLOW_LAYOUT_VSPACING);
		auto flowLayout = new FlowLayout(widgetContent, 0, sph, spv);
		flowLayout->setItemRetainSizeWhenHidden(false);
		int l, r, tb;
		l = qFloor(dpi * FLOW_LAYOUT_MARGIN_LEFT);
		r = qFloor(dpi * FLOW_LAYOUT_MARGIN_RIGHT);
		tb = PLSDpiHelper::calculate(dpi, FLOW_LAYOUT_MARGIN_TOP_BOTTOM);
		flowLayout->setContentsMargins(l, tb, r, tb);
		flowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		sa->setWidget(widgetContent);

		layout->addWidget(sa);

		QApplication::postEvent(widgetContent, new QEvent(QEvent::Resize));
		return std::tuple<QWidget *, FlowLayout *>(page, flowLayout);
	};

	auto loadStickers = [=](QLayout *layout, ScrollAreaWithNoDataTip *targetSa, QWidget *parent, const std::vector<StickerData> &stickerList, StickerDataIndex index, bool /*async*/) {
		if (targetSa == nullptr)
			return false;
		if (stickerList.size() == 0) {
			targetSa->SetNoDataPageVisible(true);
			targetSa->SetNoDataTipText(tr("main.prism.sticker.noData.tips"));
			return true;
		}
		targetSa->SetNoDataPageVisible(false);
		QtConcurrent::run([=]() {
			for (size_t i = index.startIndex; i <= index.endIndex; ++i) {
				if (exit)
					break;
				if (i >= stickerList.size() || stickerList.size() == 0)
					break;
				const auto &data = stickerList[i];
				QMetaObject::invokeMethod(this, "LoadStickerAsync", Qt::QueuedConnection, Q_ARG(const StickerData &, data), Q_ARG(QWidget *, parent), Q_ARG(QLayout *, layout));
				QThread::msleep(20);
			}
		});
		return true;
	};

	auto loadViewPage = [=](QWidget *page, QLayout *layout) {
		bool ok = false;
		StickerDataIndex dataIndex;
		if (GetStickerDataIndex(categoryId, stickers, dataIndex)) {
			auto targetSa = page->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
			if (CATEGORY_ID_RECENT == categoryId) {
				dataIndex.endIndex = recentStickerData.size() - 1;
				ok = loadStickers(layout, targetSa, this, recentStickerData, dataIndex, false);
			} else {
				ok = loadStickers(layout, targetSa, this, allStickerData, dataIndex, true);
			}
		}
		return ok;
	};

	auto iter = categoryViews.find(categoryId);
	if (iter != categoryViews.end() && iter->second) {
		if (CATEGORY_ID_RECENT == categoryId && needUpdateRecent) {
			needUpdateRecent = false;
			cleanPage(categoryId);
			loadViewPage(iter->second, GetFlowlayout(categoryId));
		}
		stackedWidget->setCurrentWidget(iter->second);
	} else {
		auto page = createViewPage();
		stackedWidget->addWidget(std::get<0>(page));
		categoryViews[categoryId] = std::get<0>(page);
		stackedWidget->setCurrentWidget(std::get<0>(page));
		bool ok = loadViewPage(std::get<0>(page), std::get<1>(page));
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
	auto dpi = PLSDpiHelper::getDpi(this);
	int l, t, r, b, hSpacing, vSpacing;
	l = PLSDpiHelper::calculate(dpi, MARGIN_LEFT);
	r = PLSDpiHelper::calculate(dpi, MARGIN_RIGHT);
	t = PLSDpiHelper::calculate(dpi, MARGIN_TOP);
	b = PLSDpiHelper::calculate(dpi, MARGIN_BOTTOM);
	hSpacing = PLSDpiHelper::calculate(dpi, H_SPACING);
	vSpacing = PLSDpiHelper::calculate(dpi, V_SPACING);
	int length = l + r;
	for (const auto &button : categoryBtn->buttons()) {
		length += button->width();
	}
	length += (categoryBtn->buttons().size() - 1) * hSpacing;
	if (showMoreBtn)
		ui->category->setFixedHeight(PLSDpiHelper::calculate(dpi, (rowWidth < length) ? 109 : 65));
	int width = ui->category->width();
	QRect effectiveRect(l, t, width - 2 * r, width - 2 * b);
	int x = effectiveRect.x();
	int y = effectiveRect.y();
	int lineHeight = 0;
	std::vector<std::vector<int>> indexVector;
	std::vector<int> rowIndex;
	int count = categoryBtn->buttons().size();
	for (int i = 0; i < count; ++i) {
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
		ui->category->setFixedHeight(rectHeight + PLSDpiHelper::calculate(dpi, 1));
		showAllCategory();
		return;
	}

	if (indexVector.size() > MAX_CATEGORY_ROW) {
		for (int index : indexVector[0])
			categoryBtn->buttons().at(index)->show();
		std::vector<int> maxRow = indexVector[MAX_CATEGORY_ROW - 1];
		auto rowCount = maxRow.size();
		int lastIndex = maxRow[rowCount - 1];
		auto geometry = categoryBtn->buttons().at(lastIndex)->geometry();
		if (ui->category->width() - geometry.right() - vSpacing - r >= PLSDpiHelper::calculate(dpi, 39)) {
			btnMore->setGeometry(QRect(QPoint(geometry.right() + hSpacing, geometry.y()), btnMore->size()));
			btnMore->show();
		} else {
			btnMore->hide();
			for (int i = int(maxRow.size() - 1); i >= 0; i--) {
				geometry = categoryBtn->buttons().at(maxRow[i])->geometry();
				if (geometry.width() >= PLSDpiHelper::calculate(dpi, 39)) {
					lastIndex = maxRow[i] - 1;
					categoryBtn->buttons().at(maxRow[i])->hide();
					btnMore->setGeometry(QRect(QPoint(geometry.x(), geometry.y()), btnMore->size()));
					btnMore->show();
					break;
				}
			}
		}
		setCategoryVisible(0, lastIndex, true);
		setCategoryVisible(++lastIndex, categoryBtn->buttons().size() - 1, false);
	} else {
		btnMore->hide();
		showAllCategory();
	}
}

bool PLSPrismSticker::CreateRecentJsonFile()
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

	QJsonDocument document;
	QByteArray json = file.readAll();
	if (json.isEmpty()) {
		QJsonObject obj;
		QJsonArray arrayRecentList;
		obj.insert(RECENT_LIST_KEY, arrayRecentList);
		QJsonDocument document;
		document.setObject(obj);
		QByteArray json = document.toJson(QJsonDocument::Indented);
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
	if (nullptr == m_pNodataPage) {
		m_pNodataPage = new NoDataPage(this);
		m_pNodataPage->setMouseTracking(true);
		m_pNodataPage->setObjectName("iconNetworkErrorPage");
		m_pNodataPage->SetNoDataTipText(tips);
		m_pNodataPage->SetPageType(NoDataPage::Page_NetworkError);
		const char *method = (type == Timeout) ? SLOT(OnRetryOnTimeOut()) : SLOT(OnRetryOnNoNetwork());
		PLS_INFO(MAIN_PRISM_STICKER, "Show retry page reason: %s", (type == Timeout) ? "Requst time out" : "Network unavailable");
		connect(m_pNodataPage, SIGNAL(retryClicked()), this, method);
	}

	UpdateNodataPageGeometry();
	m_pNodataPage->show();
}

void PLSPrismSticker::HideNoNetworkPage()
{
	if (m_pNodataPage != nullptr) {
		delete m_pNodataPage;
		m_pNodataPage = nullptr;
	}
}

void PLSPrismSticker::UpdateNodataPageGeometry()
{
	if (m_pNodataPage == nullptr)
		return;
	m_pNodataPage->SetPageType(NoDataPage::Page_NetworkError);
	if (stickers.size() == 0) {
		m_pNodataPage->resize(content()->size());
		m_pNodataPage->move(content()->mapTo(this, QPoint(0, 0)));
	} else {
		int y = ui->category->geometry().bottom();
		double dpi = PLSDpiHelper::getDpi(this);
		m_pNodataPage->resize(width() - PLSDpiHelper::calculate(dpi, 2), content()->height() - y - PLSDpiHelper::calculate(dpi, 1));
		m_pNodataPage->move(PLSDpiHelper::calculate(dpi, 1), ui->category->mapTo(this, QPoint(0, ui->category->height())).y());
	}
}

void PLSPrismSticker::UserApplySticker(const StickerData &data, StickerPointer label)
{
	// There are normally three steps to apply a sticker.
	// 1. download resources if needed.
	// 2. uncompress zip file if needed.
	// 3. remux images files to a video file.
	DWORD timeStart = GetTickCount();
	auto path = PLSStickerDataHandler::GetStickerResourcePath(data);
	auto configFile = path + PLSStickerDataHandler::GetStickerConfigJsonFileName(data);
	QEventLoop loop;
	// Do remux if needed.
	QFutureWatcher<StickerHandleResult> watcher;
	connect(&watcher, SIGNAL(finished()), &loop, SLOT(quit()));
	QFuture<StickerHandleResult> future = QtConcurrent::run([=]() {
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
				if (!file.remove())
					PLS_INFO(MAIN_PRISM_STICKER, "Remove: %s failed", GetFileName(zipFile.toStdString()).c_str());
			} else {
				PLS_INFO(MAIN_PRISM_STICKER, "Sticker resource zipFile:%s does not exsit, download resource", GetFileName(zipFile.toStdString()).c_str());
				DownloadResource(data, label);
				result.breakFlow = true;
				return result;
			}
		}

		// Get config json file.
		QFile file(configFile);
		if (!file.exists()) {
			PLS_INFO(MAIN_PRISM_STICKER, "Sticker resource config file:%s does not exsit, download resource", GetFileName(configFile.toStdString()).c_str());
			DownloadResource(data, label);
			result.breakFlow = true;
			return result;
		}

		StickerParamWrapper *wrapper = PLSStickerDataHandler::CreateStickerParamWrapper(data.category);
		if (!wrapper->Serialize(configFile)) {
			delete wrapper;
			wrapper = nullptr;
			return result;
		}
		bool ok = true;
		for (const auto &config : wrapper->m_config) {
			auto remuxedFile = path + config.resourceDirectory + ".mp4";
			auto resourcePath = path + config.resourceDirectory;
			if (!QFile::exists(remuxedFile)) {
				if (!PLSStickerDataHandler::MediaRemux(resourcePath, remuxedFile, config.fps)) {
					ok = false;
					std::string name = GetFileName(remuxedFile.toUtf8().constData());
					PLS_WARN(MAIN_PRISM_STICKER, "remux file:%s failed", name.c_str());
					continue;
				}
			}

			QString imageFile = getTargetImagePath(resourcePath, data.category, data.id, Orientation::landscape == config.orientation);

			if (Orientation::landscape == config.orientation) {
				result.landscapeVideoFile = remuxedFile;
				result.landscapeImage = imageFile; // TODO
			} else {
				result.portraitVideo = remuxedFile;
				result.portraitImage = imageFile; // TODO
			}
		}
		delete wrapper;
		wrapper = nullptr;
		result.success = ok;
		result.AdjustParam();
		return result;
	});
	watcher.setFuture(future);
	loop.exec();
	StickerHandleResult result = watcher.result();
	if (result.breakFlow)
		return;
	if (label) {
		label->SetShowLoad(false);
		DWORD gap = GetTickCount() - timeStart;
		if (gap < LOADING_TIME_MS) {
			QTimer::singleShot(LOADING_TIME_MS - gap, this, [=]() { label->SetShowOutline(false); });
		} else {
			label->SetShowOutline(false);
		}
	}
	if (result.success && !exit) {
		emit StickerApplied(result);
		UpdateRecentList(data);
	}
}

void PLSPrismSticker::UpdateDpi(double dpi)
{
	for (const auto &view : categoryViews) {
		if (!view.second)
			continue;
		auto sa = view.second->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
		auto layout = qobject_cast<FlowLayout *>(sa->widget()->layout());
		if (layout) {
			layout->setHorizontalSpacing(qFloor(dpi * FLOW_LAYOUT_SPACING));
			layout->setverticalSpacing(PLSDpiHelper::calculate(dpi, FLOW_LAYOUT_VSPACING));
			int l, r, tb;
			l = qFloor(dpi * FLOW_LAYOUT_MARGIN_LEFT);
			r = qFloor(dpi * FLOW_LAYOUT_MARGIN_RIGHT);
			tb = PLSDpiHelper::calculate(dpi, FLOW_LAYOUT_MARGIN_TOP_BOTTOM);
			layout->setContentsMargins(l, tb, r, tb);
		}
	}
}

void PLSPrismSticker::ShowToast(const QString &tips)
{
	if (nullptr == toastTip) {
		toastTip = new PLSToastMsgFrame(this);
	}
	UpdateToastPos();
	toastTip->SetMessage(tips);
	toastTip->SetShowWidth(width() - 2 * PLSDpiHelper::calculate(dpi, 10));
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
		double dpi = PLSDpiHelper::getDpi(this);
		QPoint pos;
		pos = ui->category->mapTo(this, QPoint(PLSDpiHelper::calculate(dpi, 10), ui->category->height() + PLSDpiHelper::calculate(dpi, 10)));
		toastTip->move(pos);
	}
}

void PLSPrismSticker::DownloadResource(const StickerData &data, StickerPointer label) {}

QLayout *PLSPrismSticker::GetFlowlayout(const QString &categoryId)
{
	QLayout *layout = nullptr;
	if (categoryViews.find(categoryId) != categoryViews.end()) {
		if (nullptr != categoryViews[categoryId]) {
			auto sa = categoryViews[categoryId]->findChild<ScrollAreaWithNoDataTip *>("stickerScrollArea");
			layout = sa->widget()->layout();
		}
	}
	return layout;
}

QString PLSPrismSticker::getTargetImagePath(QString resourcePath, QString category, QString id, bool landscape)
{
	QString imageFile{};
	int index = 0;

	const auto &reaction = PLSSyncServerManager::instance()->getStickerReaction();
	auto items = reaction[category];

	auto isMatchSticker = [&](const QVariant &stickerData) {
		auto varMap = stickerData.toMap();
		if (varMap.isEmpty()) {
			return false;
		}
		auto stickerId = varMap.value("itemId").toString();
		if (0 == id.compare(stickerId)) {
			index = landscape ? varMap.value("landscapeFrame").toInt() : varMap.value("portraitFrame").toInt();
			return true;
		}

		return false;
	};

	auto it = std::find_if(items.begin(), items.end(), isMatchSticker);
	if (it == items.cend()) {
		return imageFile;
	}

	QDir dir(resourcePath);
	if (dir.exists()) {
		QStringList list = dir.entryList(QDir::Files);
		index -= 1;
		if (index >= 0 && index < list.size())
			imageFile = resourcePath + "/" + list.at(index);
	}
	return imageFile;
}

void PLSPrismSticker::OnFrontendEvent(obs_frontend_event event, void *param)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		PLSPrismSticker *view = static_cast<PLSPrismSticker *>(param);
		if (view) {
			view->SetExitFlag(true);
			if (!PLSStickerDataHandler::GetClearDataFlag())
				view->SaveStickerJsonData();
			view->WriteDownloadCache();
			view->deleteLater();
		}
	}
}

static bool retryCallback(const DownloadTaskData &data)
{
	if (data.outputPath == pls_get_user_path(PRISM_STICKER_CACHE_PATH))
		return true;
	return false;
}

void PLSPrismSticker::showEvent(QShowEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::PrismStickerConfig, true);
	PLSDialogView::showEvent(event);
	showMoreBtn = true;
	isShown = true;
	if (isDataReady) {
		QTimer::singleShot(0, this, [=]() {
			AdjustCategoryTab();
			SwitchToCategory((recentStickerData.size() > 0) ? CATEGORY_ID_RECENT : CATEGORY_ID_ALL);
		});
		PLSFileDownloader::instance()->Retry(retryCallback);
	}
}

void PLSPrismSticker::hideEvent(QHideEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::PrismStickerConfig, false);
	HideToast();
	PLSDialogView::hideEvent(event);
}

void PLSPrismSticker::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSPrismSticker::resizeEvent(QResizeEvent *event)
{
	PLSDialogView::resizeEvent(event);
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
			double dpi = PLSDpiHelper::getDpi(this);
			toastTip->SetShowWidth(width() - 2 * PLSDpiHelper::calculate(dpi, 10));
		}
	}
	return __super::eventFilter(watcher, event);
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
	QFutureWatcher<bool> *watcher = static_cast<QFutureWatcher<bool> *>(sender());
	bool ok = watcher->result();
	if (ok) {
		isDataReady = true;
		if (isShown) {
			QTimer::singleShot(0, this, [=]() {
				InitCategory();
				AdjustCategoryTab();
				if (!NetworkAccessible()) {
					ShowNoNetworkPage(tr("main.giphy.network.toast.error"), NoNetwork);
				}
				QTimer::singleShot(0, this, [=]() { SwitchToCategory((recentStickerData.size() > 0) ? CATEGORY_ID_RECENT : CATEGORY_ID_ALL); });
			});
		}
	} else {
		auto path = watcher->property("fileName").toString();
		auto fileName = GetFileName(path.toStdString());
		PLS_WARN(MAIN_PRISM_STICKER, "[Prism-Sticker]Load %s Failed", fileName.c_str());
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
	if (NetworkAccessible() && stickers.size() > 0) {
		HideNoNetworkPage();
	} else {
		DownloadCategoryJson();
	}
}

void PLSPrismSticker::HandleDownloadResult(const TaskResponData &result, const StickerData &data, StickerPointer label)
{
	if (result.resultType != ResultStatus::ERROR_OCCUR) {
		PLS_INFO(MAIN_PRISM_STICKER, "Download Sticker file: '%s/%s' successfully", qUtf8Printable(data.category), qUtf8Printable(data.id));
		auto task = result.taskData;
		mutex.lock();
		PLSStickerDataHandler::WriteDownloadCache(data.id, task.version, downloadCache);
		mutex.unlock();

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
			PLS_INFO(MAIN_PRISM_STICKER, "Remove: %s failed", GetFileName(zipFile.toStdString()).c_str());

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
	mutex.lock();
	PLSStickerDataHandler::ReadDownloadCacheLocal(downloadCache);
	mutex.unlock();

	QString fileName = pls_get_user_path(PRISM_STICKER_JSON_FILE);
	QFile file(fileName);
	if (!file.exists()) {
		PLS_WARN(MAIN_PRISM_STICKER, "Sticker reaction.json file not exist, redownload");
		DownloadCategoryJson();
		return;
	}
	QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
	watcher->setProperty("fileName", fileName);
	connect(watcher, SIGNAL(finished()), this, SLOT(OnHandleStickerDataFinished()));
	QFuture<bool> future = QtConcurrent::run([=]() {
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
		m_pWidgetLoadingBG = new QWidget(parent);
		m_pWidgetLoadingBG->setObjectName("loadingBG");
		m_pWidgetLoadingBG->setGeometry(parent->geometry());
		m_pWidgetLoadingBG->show();

		auto layout = new QHBoxLayout(m_pWidgetLoadingBG);
		auto loadingBtn = new QPushButton(m_pWidgetLoadingBG);
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
		delete m_pWidgetLoadingBG;
		m_pWidgetLoadingBG = nullptr;
	}
}

void PLSPrismSticker::LoadStickerAsync(const StickerData &data, QWidget *parent, QLayout *layout)
{
	auto label = new PLSThumbnailLabel(parent);
	label->setObjectName("prismStickerLabel");
	label->SetTimer(timerLoading);
	label->setProperty("stickerData", QVariant::fromValue<StickerData>(data));
	label->SetCachePath(pls_get_user_path(PRISM_STICKER_CACHE_PATH));
	label->SetUrl(QUrl(data.thumbnailUrl), data.version);
	connect(label, &QPushButton::clicked, this, [=]() {
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
		auto data = label->property("stickerData").value<StickerData>();
		QString log("User click sticker: \"%1/%2\"");
		PLS_UI_STEP(MAIN_PRISM_STICKER, qUtf8Printable(log.arg(data.category).arg(data.id)), ACTION_CLICK);
		UserApplySticker(data, label);
	});
	layout->addWidget(label);
	layout->update();
}

bool PLSPrismSticker::DownloadCategoryJson()
{
	return true;
}

bool PLSPrismSticker::NetworkAccessible()
{
	return PLSNetworkMonitor::Instance()->IsInternetAvailable();
}
