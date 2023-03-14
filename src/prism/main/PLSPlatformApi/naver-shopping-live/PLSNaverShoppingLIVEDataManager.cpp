#include "PLSNaverShoppingLIVEDataManager.h"

#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "PLSNaverShoppingLIVEImageProcessFinished.h"

#include "frontend-api.h"
#include "json-data-handler.hpp"
#include "log.h"
#include "PLSDpiHelper.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QPainter>

#define NAVER_SHOPPING_DATA_DIR "PRISMLiveStudio/naver_shopping/"
#define SEARCH_KEYWORDS_FILE_NAME "search_keywords.json"
#define DOWNLOAD_IMAGE_CACHE_FILE_NAME "download_image_cache.json"
#define RECENT_PRODUCTS_FILE_NAME "recent_products.json"
#define SMART_STORE_INFO_FILE_NAME "smart_store_info.json"
#define OTHER_INFOS_FILE_NAME "other_infos.json"
#define NAVERSHOPPINGLIVE_DATAMANAGER "NaverShoppingLIVE/DataManager"

static inline void createDir(const QDir &dir)
{
	if (!dir.exists()) {
		dir.mkpath(dir.absolutePath());
	}
}

void PLSPLSNaverShoppingLIVEImageLoadProcessor::process(PLSNaverShoppingLIVEImageProcessFinished *finished, const QString &url, const QString &imagePath)
{
	QPixmap normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap;
	bool ok = PLSNaverShoppingLIVEDataManager::instance()->getThumbnailPixmap(normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap, url, imagePath);
	finished->processFinished(ok, thread(), url, normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap);
}

void PLSPLSNaverShoppingLIVEImageLoadThread::process()
{
	while (running) {
		std::tuple<PLSNaverShoppingLIVEImageProcessFinished *, QString, QString> resource;
		if (pop(resource) && running) {
			processor->process(std::get<0>(resource), std::get<1>(resource), std::get<2>(resource));
		}
	}
}

// product status
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_WAIT = "WAIT";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE = "SALE";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_OUTOFSTOCK = "OUTOFSTOCK";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_UNADMISSION = "UNADMISSION";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_REJECTION = "REJECTION";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SUSPENSION = "SUSPENSION";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_CLOSE = "CLOSE";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_PROHIBITION = "PROHIBITION";
const QString PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_DELETE = "DELETE";

static qint64 lastItemId = 0;

PLSNaverShoppingLIVEDataManager::PLSNaverShoppingLIVEDataManager(QObject *parent) : QObject(parent)
{
	loadSearchKeywords();
	loadDownloadImageCache();
	loadRecentProductNos();
	loadSmartStoreInfo();
	loadOtherInfos();

	defaultImage.load(QString(":/images/navershopping/img-nshopping-loading.svg"));
	liveProductBadgeImage.load(QString(":/images/navershopping/badge-nshoppinglive.svg"));

	PLSLiveInfoNaverShoppingLIVEProductItemView::addBatchCache(MAX_LIVEINFO_PRODUCT_COUNT);
	PLSNaverShoppingLIVEProductItemView::addBatchCache(PRODUCT_PAGE_SIZE);
	PLSNaverShoppingLIVESearchKeywordItemView::addBatchCache(MAX_SEARCH_KEYWORDS_COUNT);

	imageLoadThread = new PLSPLSNaverShoppingLIVEImageLoadThread();
	imageLoadThread->start();

	QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, [=] {
		delete imageLoadThread;
		imageLoadThread = nullptr;

		clearThumbnailPixmaps();
	});
}

PLSNaverShoppingLIVEDataManager::~PLSNaverShoppingLIVEDataManager() {}

PLSNaverShoppingLIVEDataManager *PLSNaverShoppingLIVEDataManager::instance()
{
	static PLSNaverShoppingLIVEDataManager manager;
	return &manager;
}

QString PLSNaverShoppingLIVEDataManager::getDirPath(const QString &path)
{
	QFileInfo fi(pls_get_user_path(NAVER_SHOPPING_DATA_DIR + path));
	createDir(fi.absoluteDir());
	return fi.absoluteFilePath();
}

QString PLSNaverShoppingLIVEDataManager::getFilePath(const QString &path)
{
	QFileInfo fi(pls_get_user_path(NAVER_SHOPPING_DATA_DIR + path));
	createDir(fi.dir());
	return fi.absoluteFilePath();
}

QString PLSNaverShoppingLIVEDataManager::getCacheDirPath()
{
	return getDirPath("Cache");
}

bool PLSNaverShoppingLIVEDataManager::priceOrRateIsInteger(double price)
{
	return (static_cast<qint64>(price) * 100) == static_cast<qint64>(price * 100);
}

qint64 PLSNaverShoppingLIVEDataManager::priceOrRateToInteger(double price)
{
	return static_cast<qint64>(price * 100);
}

QString PLSNaverShoppingLIVEDataManager::convertPrice(double price)
{
	QString sPrice = priceOrRateIsInteger(price) ? QString::number(static_cast<qint64>(price)) : QString::number(price, 'f', 2);
	int dot = sPrice.lastIndexOf('.');
	if (dot < 0) {
		dot = sPrice.length();
	}

	for (int i = 0; dot > 0; --dot, ++i) {
		if ((i != 0) && ((i % 3) == 0)) {
			sPrice.insert(dot, ',');
		}
	}
	return sPrice;
}

QString PLSNaverShoppingLIVEDataManager::convertRate(double rate)
{
	return (priceOrRateIsInteger(rate) ? QString::number(static_cast<qint64>(rate)) : QString::number(rate, 'f', 2)) + '%';
}

QStringList PLSNaverShoppingLIVEDataManager::getSearchKeywords() const
{
	return searchKeywords;
}

QStringList PLSNaverShoppingLIVEDataManager::addSearchKeyword(const QString &searchKeyword)
{
	if (searchKeyword.startsWith("http://") || searchKeyword.startsWith("https://")) {
		return searchKeywords;
	}

	if (!searchKeywords.isEmpty() && (searchKeywords.first() == searchKeyword)) {
		return searchKeywords;
	}

	searchKeywords.removeOne(searchKeyword);
	searchKeywords.prepend(searchKeyword);
	while (searchKeywords.count() > MAX_SEARCH_KEYWORDS_COUNT) {
		searchKeywords.removeLast();
	}
	saveSearchKeywords();
	return searchKeywords;
}

void PLSNaverShoppingLIVEDataManager::removeSearchKeyword(const QString &searchKeyword)
{
	if (searchKeywords.removeOne(searchKeyword)) {
		saveSearchKeywords();
	}
}

void PLSNaverShoppingLIVEDataManager::clearSearchKeywords()
{
	if (!searchKeywords.isEmpty()) {
		searchKeywords.clear();
		saveSearchKeywords();
	}
}

void PLSNaverShoppingLIVEDataManager::loadSearchKeywords()
{
	QString searchKeywordsFile = getFilePath(SEARCH_KEYWORDS_FILE_NAME);

	QByteArray bytes;
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, searchKeywordsFile);
	if (bytes.isEmpty()) {
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live load seach keywords failed. reason: %s", parseError.errorString().toUtf8().constData());
		return;
	}

	for (auto it : doc.array()) {
		searchKeywords.append(it.toString());
		if (searchKeywords.count() >= MAX_SEARCH_KEYWORDS_COUNT) {
			break;
		}
	}
}

void PLSNaverShoppingLIVEDataManager::saveSearchKeywords()
{
	QJsonArray array;
	for (const auto &searchKeyword : searchKeywords) {
		array.append(searchKeyword);
	}

	QString searchKeywordsFile = getFilePath(SEARCH_KEYWORDS_FILE_NAME);

	QJsonDocument doc(array);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), searchKeywordsFile)) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live save seach keywords failed.");
	}
}

void PLSNaverShoppingLIVEDataManager::downloadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &url, std::function<void(bool ok, const QString &imagePath)> callback, QObject *receiver,
						    PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid, int timeout)
{
	QString imagePath;
	if (findValidDownloadImagePath(imagePath, url)) {
		callback(true, imagePath);
		return;
	}

	PLSNaverShoppingLIVEAPI::downloadImage(
		platform, url,
		[this, callback, url](bool ok, const QString &imagePath) {
			if (ok) {
				addDownloadImagePath(imagePath, url);
			}

			callback(ok, imagePath);
		},
		receiver, receiverIsValid, timeout);
}

void PLSNaverShoppingLIVEDataManager::downloadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &url, std::function<void()> beforeDownload,
						    std::function<void(bool ok, const QString &imagePath)> callback, QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid,
						    int timeout)
{
	QString imagePath;
	if (findValidDownloadImagePath(imagePath, url)) {
		callback(true, imagePath);
		return;
	}

	beforeDownload();
	PLSNaverShoppingLIVEAPI::downloadImage(
		platform, url,
		[this, callback, url](bool ok, const QString &imagePath) {
			if (ok) {
				addDownloadImagePath(imagePath, url);
			}

			callback(ok, imagePath);
		},
		receiver, receiverIsValid, timeout);
}

void PLSNaverShoppingLIVEDataManager::downloadImages(PLSPlatformNaverShoppingLIVE *platform, const QStringList &urls, std::function<void(const QMap<QString, QString> &imagePaths)> callback,
						     QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid, int timeout)
{
	QMap<QString, QString> imagePaths;

	QStringList newUrls;
	for (const QString &url : urls) {
		QString imagePath;
		if (findValidDownloadImagePath(imagePath, url)) {
			imagePaths.insert(url, imagePath);
		} else {
			newUrls.append(url);
		}
	}

	if (!newUrls.isEmpty()) {
		PLSNaverShoppingLIVEAPI::downloadImages(
			platform, newUrls,
			[this, callback, newUrls, imagePaths](const QList<QPair<bool, QString>> &newImagePaths) mutable {
				for (int i = 0, count = newImagePaths.count(); i < count; ++i) {
					const auto &newImagePath = newImagePaths[i];
					if (newImagePath.first) {
						const auto &url = newUrls[i];
						addDownloadImagePath(newImagePath.second, url);
						imagePaths.insert(url, newImagePath.second);
					}
				}

				callback(imagePaths);
			},
			receiver, receiverIsValid, timeout);
	} else {
		callback(imagePaths);
	}
}

void PLSNaverShoppingLIVEDataManager::loadDownloadImageCache()
{
	QString downloadImageCacheFile = getFilePath(DOWNLOAD_IMAGE_CACHE_FILE_NAME);

	QByteArray bytes;
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, downloadImageCacheFile);
	if (bytes.isEmpty()) {
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live load download image cache failed. reason: %s", parseError.errorString().toUtf8().constData());
		return;
	}

	for (auto it : doc.array()) {
		QJsonObject obj = it.toObject();
		downloadImageCache.insert(JSON_getString(obj, url), JSON_getString(obj, path));
	}
}

void PLSNaverShoppingLIVEDataManager::saveDownloadImageCache()
{
	QJsonArray array;
	for (auto iter = downloadImageCache.begin(), endIter = downloadImageCache.end(); iter != endIter; ++iter) {
		array.append(QJsonObject{JSON_mkObject(url, iter.key()), JSON_mkObject(path, iter.value())});
	}

	QString downloadImageCacheFile = getFilePath(DOWNLOAD_IMAGE_CACHE_FILE_NAME);

	QJsonDocument doc(array);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), downloadImageCacheFile)) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live save download image cache failed.");
	}
}

bool PLSNaverShoppingLIVEDataManager::findDownloadImagePath(QString &imagePath, const QString &imageUrl) const
{
	QReadLocker locker(&downloadImageCacheRWLock);
	auto iter = downloadImageCache.constFind(imageUrl);
	if (iter != downloadImageCache.constEnd()) {
		imagePath = iter.value();
		return true;
	}
	return false;
}

bool PLSNaverShoppingLIVEDataManager::findValidDownloadImagePath(QString &imagePath, const QString &imageUrl) const
{
	if (!findDownloadImagePath(imagePath, imageUrl))
		return false;
	else if (!imagePath.isEmpty() && QFile::exists(imagePath))
		return true;
	return false;
}

void PLSNaverShoppingLIVEDataManager::addDownloadImagePath(const QString &imagePath, const QString &imageUrl)
{
	QWriteLocker locker(&downloadImageCacheRWLock);
	downloadImageCache.insert(imageUrl, imagePath);
	saveDownloadImageCache();
}

bool PLSNaverShoppingLIVEDataManager::hasRecentProductNos() const
{
	return !recentProductNos.isEmpty();
}

QList<qint64> PLSNaverShoppingLIVEDataManager::getRecentProductNos() const
{
	return recentProductNos;
}

QList<qint64> PLSNaverShoppingLIVEDataManager::getRecentProductNos(bool &hasNextPage, int currentPage, int pageSize) const
{
	hasNextPage = ((currentPage + 1) * pageSize) < recentProductNos.count();

	QList<qint64> onePageRecentProductNos;
	for (int i = currentPage * pageSize, count = qMin(recentProductNos.count(), (currentPage + 1) * pageSize); i < count; ++i) {
		onePageRecentProductNos.append(recentProductNos[i]);
	}
	return onePageRecentProductNos;
}

bool PLSNaverShoppingLIVEDataManager::addRecentProductNo(qint64 productNo)
{
	if (!recentProductNos.isEmpty() && (recentProductNos.first() == productNo)) {
		return false;
	}

	recentProductNos.removeOne(productNo);
	recentProductNos.prepend(productNo);
	while (recentProductNos.count() > MAX_RECENT_COUNT) {
		recentProductNos.removeLast();
	}
	saveRecentProductNos();
	return true;
}

void PLSNaverShoppingLIVEDataManager::addRecentProductNos(const QList<qint64> &productNos)
{
	for (qint64 productNo : productNos) {
		addRecentProductNo(productNo);
	}
}

void PLSNaverShoppingLIVEDataManager::removeRecentProductNo(qint64 productNo)
{
	if (recentProductNos.removeOne(productNo)) {
		saveRecentProductNos();
	}
}

void PLSNaverShoppingLIVEDataManager::clearRecentProductNos()
{
	if (!recentProductNos.isEmpty()) {
		recentProductNos.clear();
		saveRecentProductNos();
	}
}

void PLSNaverShoppingLIVEDataManager::loadRecentProductNos()
{
	QString recentProductsFile = getFilePath(RECENT_PRODUCTS_FILE_NAME);

	QByteArray bytes;
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, recentProductsFile);
	if (bytes.isEmpty()) {
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live load recent products failed. reason: %s", parseError.errorString().toUtf8().constData());
		return;
	}

	for (auto it : doc.array()) {
		recentProductNos.append(JSON_toInt64(it));
		if (recentProductNos.count() >= MAX_RECENT_COUNT) {
			break;
		}
	}
}

void PLSNaverShoppingLIVEDataManager::saveRecentProductNos()
{
	QJsonArray array;
	for (const auto &recentProductNo : recentProductNos) {
		array.append(recentProductNo);
	}

	QString recentProductsFile = getFilePath(RECENT_PRODUCTS_FILE_NAME);

	QJsonDocument doc(array);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), recentProductsFile)) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live save recent products failed.");
	}
}

int PLSNaverShoppingLIVEDataManager::getLatestUseTab() const
{
	return latestUseTab;
}

void PLSNaverShoppingLIVEDataManager::setLatestUseTab(int latestUseTab)
{
	if (this->latestUseTab != latestUseTab) {
		this->latestUseTab = latestUseTab;
		saveOtherInfos();
	}
}

void PLSNaverShoppingLIVEDataManager::clearLatestUseTab(bool autoSave)
{
	this->latestUseTab = -1;
	if (autoSave) {
		saveOtherInfos();
	}
}

QString PLSNaverShoppingLIVEDataManager::getLatestStoreId() const
{
	return latestStoreId;
}

void PLSNaverShoppingLIVEDataManager::setLatestStoreId(const QString &latestStoreId)
{
	if (this->latestStoreId != latestStoreId) {
		this->latestStoreId = latestStoreId;
		saveOtherInfos();
	}
}

void PLSNaverShoppingLIVEDataManager::clearLatestStoreId(bool autoSave)
{
	this->latestStoreId.clear();
	if (autoSave) {
		saveOtherInfos();
	}
}

void PLSNaverShoppingLIVEDataManager::loadOtherInfos()
{
	QString otherInfosFile = getFilePath(OTHER_INFOS_FILE_NAME);

	QByteArray bytes;
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, otherInfosFile);
	if (bytes.isEmpty()) {
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live load other info failed. reason: %s", parseError.errorString().toUtf8().constData());
		return;
	}

	QJsonObject obj = doc.object();
	latestUseTab = JSON_getIntEx(obj, latestUseTab, -1);
	latestStoreId = JSON_getString(obj, latestStoreId);
}

void PLSNaverShoppingLIVEDataManager::saveOtherInfos()
{
	QJsonObject obj{JSON_mkObject(latestUseTab, latestUseTab), JSON_mkObject(latestStoreId, latestStoreId)};

	QString otherInfosFile = getFilePath(OTHER_INFOS_FILE_NAME);

	QJsonDocument doc(obj);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), otherInfosFile)) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live save other infos failed.");
	}
}

QSvgRenderer &PLSNaverShoppingLIVEDataManager::getDefaultImage()
{
	return defaultImage;
}

QSvgRenderer &PLSNaverShoppingLIVEDataManager::getLiveProductBadgeImage()
{
	return liveProductBadgeImage;
}

bool PLSNaverShoppingLIVEDataManager::hasThumbnailPixmap(const QString &url)
{
	QReadLocker readLocker(&downloadImagePixmapCacheRWLock);
	auto iter = downloadImagePixmapCache.find(url);
	if (iter == downloadImagePixmapCache.end()) {
		return false;
	} else if (std::get<0>(iter.value()).isNull()) {
		return false;
	}
	return true;
}

static bool isThumbnailPixmapSizeValid(const QSize &size)
{
	return (size.width() > 1) && (size.height() > 1);
}

static bool loadPixmap(QPixmap &pixmap, const QString &, const QString &imagePath)
{
	if (!pixmap.load(imagePath)) {
		return false;
	} else if (pixmap.isNull() || pixmap.size().isEmpty()) {
		return false;
	}
	return true;
}

static QPixmap scaledCrop(const QPixmap &original, const QSize &size)
{
	if ((double(original.width()) / double(original.height())) >= (double(size.width()) / double(size.height()))) {
		int width = (int)(double(original.width()) * double(size.height()) / double(original.height()));
		int height = size.height();
		QPixmap scaled = original.scaledToHeight(height);
		return scaled.copy((width - size.width()) / 2, (height - size.height()) / 2, size.width(), size.height());
	} else {
		int width = size.width();
		int height = (int)(double(original.height()) * double(size.width()) / double(original.width()));
		QPixmap scaled = original.scaledToWidth(width);
		return scaled.copy((width - size.width()) / 2, (height - size.height()) / 2, size.width(), size.height());
	}
}

static QPixmap roundedPixmap(const QPixmap &pixmap, int radius)
{
	QPixmap image(pixmap.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = image.rect();

	QPainterPath path;
	path.addRoundedRect(rect, radius, radius);
	painter.setClipPath(path);

	painter.fillRect(rect, QColor(30, 30, 31));
	painter.drawPixmap(rect, pixmap);

	return image;
}

static QPixmap roundedLivePixmap(const QPixmap &pixmap, QSvgRenderer &liveBadge, const QSize &liveSize, int radius)
{
	QPixmap image(pixmap.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = image.rect();

	QPainterPath path;
	path.addRoundedRect(rect, radius, radius);
	painter.setClipPath(path);

	painter.fillRect(rect, QColor(30, 30, 31));
	painter.drawPixmap(rect, pixmap);

	liveBadge.render(&painter, QRectF(0, 0, liveSize.width(), liveSize.height()));

	return image;
}

static QPixmap hoveredRoundedPixmap(const QSize &size, const QPixmap &crop, const QMargins &margin, int radius)
{
	QPixmap image(size);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = image.rect();
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::yellow);
	painter.drawRoundedRect(rect, radius, radius);

	rect -= margin;
	painter.drawPixmap(rect, crop);

	return image;
}

bool PLSNaverShoppingLIVEDataManager::getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &hoveredPixmap, QPixmap &livePixmap, QPixmap &liveHoveredPixmap, const QString &url, const QString &imagePath)
{
	QSize size, liveSize;
	QMargins margin;
	int radius = 0;
	getThumbnailPixmapSize(size, margin, radius, liveSize);
	if (!isThumbnailPixmapSizeValid(size) || (radius <= 0)) {
		normalPixmap = hoveredPixmap = livePixmap = liveHoveredPixmap = QPixmap();
		return false;
	}

	QPixmap original;
	if (getThumbnailPixmap(original, normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap, url, imagePath, size)) {
		return true;
	}

	bool needLoadOriginal = original.isNull();
	if (needLoadOriginal && !loadPixmap(original, url, imagePath)) {
		normalPixmap = hoveredPixmap = livePixmap = liveHoveredPixmap = QPixmap();
		return false;
	}

	QPixmap scaled = scaledCrop(original, size);
	normalPixmap = roundedPixmap(scaled, radius);
	livePixmap = roundedLivePixmap(scaled, liveProductBadgeImage, liveSize, radius);

	QPixmap hoveredCrop = normalPixmap.copy(normalPixmap.rect() - margin);
	hoveredPixmap = hoveredRoundedPixmap(size, roundedPixmap(hoveredCrop, radius), margin, radius);
	QPixmap liveHoveredCrop = livePixmap.copy(livePixmap.rect() - margin);
	liveHoveredPixmap = hoveredRoundedPixmap(size, roundedPixmap(liveHoveredCrop, radius), margin, radius);

	QWriteLocker writeLocker(&downloadImagePixmapCacheRWLock);
	auto &pixmaps = downloadImagePixmapCache[url];
	if (needLoadOriginal) {
		std::get<0>(pixmaps) = original;
	}

	std::get<1>(pixmaps) = normalPixmap;
	std::get<2>(pixmaps) = hoveredPixmap;
	std::get<3>(pixmaps) = livePixmap;
	std::get<4>(pixmaps) = liveHoveredPixmap;
	return true;
}

bool PLSNaverShoppingLIVEDataManager::getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &hoveredPixmap, QPixmap &livePixmap, QPixmap &liveHoveredPixmap, const QString &url, const QString &,
							 const QSize &size)
{
	// url => (imagePixmap, scaledImagePixmapNormal, scaledImagePixmapHover)
	QReadLocker readLocker(&downloadImagePixmapCacheRWLock);
	auto iter = downloadImagePixmapCache.find(url);
	if (iter == downloadImagePixmapCache.end()) {
		return false;
	}

	auto &pixmaps = iter.value();
	QPixmap &normal = std::get<1>(pixmaps);
	QPixmap &hovered = std::get<2>(pixmaps);
	QPixmap &live = std::get<3>(pixmaps);
	QPixmap &liveHovered = std::get<4>(pixmaps);
	if (!normal.isNull() && (normal.size() == size) && !hovered.isNull() && (hovered.size() == size) && !live.isNull() && (live.size() == size) && !liveHovered.isNull() &&
	    (liveHovered.size() == size)) {
		normalPixmap = normal;
		hoveredPixmap = hovered;
		livePixmap = live;
		liveHoveredPixmap = liveHovered;
		return true;
	}
	return false;
}

bool PLSNaverShoppingLIVEDataManager::getThumbnailPixmap(QPixmap &original, QPixmap &normalPixmap, QPixmap &hoveredPixmap, QPixmap &livePixmap, QPixmap &liveHoveredPixmap, const QString &url,
							 const QString &, const QSize &size)
{
	// url => (imagePixmap, scaledImagePixmapNormal, scaledImagePixmapHover)
	QReadLocker readLocker(&downloadImagePixmapCacheRWLock);
	auto iter = downloadImagePixmapCache.find(url);
	if (iter == downloadImagePixmapCache.end()) {
		return false;
	}

	auto &pixmaps = iter.value();
	original = std::get<0>(pixmaps);

	QPixmap &normal = std::get<1>(pixmaps);
	QPixmap &hovered = std::get<2>(pixmaps);
	QPixmap &live = std::get<3>(pixmaps);
	QPixmap &liveHovered = std::get<4>(pixmaps);
	if (!normal.isNull() && (normal.size() == size) && !hovered.isNull() && (hovered.size() == size) && !live.isNull() && (live.size() == size) && !liveHovered.isNull() &&
	    (liveHovered.size() == size)) {
		normalPixmap = normal;
		hoveredPixmap = hovered;
		livePixmap = live;
		liveHoveredPixmap = liveHovered;
		return true;
	}
	return false;
}

void PLSNaverShoppingLIVEDataManager::getThumbnailPixmapAsync(PLSNaverShoppingLIVEImageProcessFinished *finished, const QString &url, const QString &imagePath, const QSize &size, double dpi)
{
	if (!finished) {
		return;
	} else if (url.isEmpty() || imagePath.isEmpty()) {
		finished->processFinished(false, thread(), url, QPixmap(), QPixmap(), QPixmap(), QPixmap());
		return;
	}

	int radius = PLSDpiHelper::calculate(dpi, 3);
	if (size.isEmpty() || (radius <= 0)) {
		finished->processFinished(false, thread(), url, QPixmap(), QPixmap(), QPixmap(), QPixmap());
		return;
	}

	QString tmpImagePath = imagePath;
	if (tmpImagePath.isEmpty() && !findValidDownloadImagePath(tmpImagePath, url)) {
		finished->processFinished(false, thread(), url, QPixmap(), QPixmap(), QPixmap(), QPixmap());
		return;
	}

	QMargins margin = PLSDpiHelper::calculate(dpi, QMargins(2, 2, 2, 2));
	QSize liveSize = PLSDpiHelper::calculate(dpi, QSize(49, 17));
	updateThumbnailPixmapSize(size, margin, radius, liveSize);

	QPixmap normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap;
	if (!getThumbnailPixmap(normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap, url, tmpImagePath, size)) {
		imageLoadThread->push(std::make_tuple(finished, url, tmpImagePath));
	} else {
		finished->processFinished(true, thread(), url, normalPixmap, hoveredPixmap, livePixmap, liveHoveredPixmap);
	}
}

void PLSNaverShoppingLIVEDataManager::updateThumbnailPixmap(const QString &url, const QPixmap &imagePixmap)
{
	{
		QWriteLocker writeLocker(&downloadImagePixmapCacheRWLock);
		// url => (imagePixmap, scaledImagePixmapNormal, scaledImagePixmapHover)
		downloadImagePixmapCache[url] = std::tuple<QPixmap, QPixmap, QPixmap, QPixmap, QPixmap>(imagePixmap, QPixmap(), QPixmap(), QPixmap(), QPixmap());
	}

	if (imagePixmap.isNull() || imagePixmap.size().isEmpty()) {
		return;
	}

	QSize size, liveSize;
	QMargins margin;
	int radius = 0;

	getThumbnailPixmapSize(size, margin, radius, liveSize);
	if (isThumbnailPixmapSizeValid(size) && radius > 0) {
		QPixmap scaled = scaledCrop(imagePixmap, size);
		QPixmap normalPixmap = roundedPixmap(scaled, radius);
		QPixmap livePixmap = roundedLivePixmap(scaled, liveProductBadgeImage, liveSize, radius);

		QPixmap hoveredCrop = normalPixmap.copy(normalPixmap.rect() - margin);
		QPixmap hoveredPixmap = hoveredRoundedPixmap(size, roundedPixmap(hoveredCrop, radius), margin, radius);
		QPixmap liveHoveredCrop = livePixmap.copy(livePixmap.rect() - margin);
		QPixmap liveHoveredPixmap = hoveredRoundedPixmap(size, roundedPixmap(liveHoveredCrop, radius), margin, radius);

		QWriteLocker writeLocker(&downloadImagePixmapCacheRWLock);
		auto &pixmaps = downloadImagePixmapCache[url];
		std::get<1>(pixmaps) = normalPixmap;
		std::get<2>(pixmaps) = hoveredPixmap;
		std::get<3>(pixmaps) = livePixmap;
		std::get<4>(pixmaps) = liveHoveredPixmap;
	}
}

void PLSNaverShoppingLIVEDataManager::removeThumbnailPixmap(const QString &url)
{
	QWriteLocker writeLocker(&downloadImagePixmapCacheRWLock);
	// url => (imagePixmap, scaledImagePixmapNormal, scaledImagePixmapHover)
	downloadImagePixmapCache.remove(url);
}

void PLSNaverShoppingLIVEDataManager::clearThumbnailPixmaps()
{
	QWriteLocker writeLocker(&downloadImagePixmapCacheRWLock);
	// url => (imagePixmap, scaledImagePixmapNormal, scaledImagePixmapHover)
	downloadImagePixmapCache.clear();
}

void PLSNaverShoppingLIVEDataManager::updateThumbnailPixmapSize(const QSize &size, const QMargins &margin, int radius, const QSize &liveSize)
{
	QWriteLocker writeLocker(&downloadImagePixmapInfoRWLock);
	downloadImagePixmapSize = size;
	downloadImagePixmapLiveSize = liveSize;
	downloadImagePixmapMargin = margin;
	downloadImagePixmapRadius = radius;
}

void PLSNaverShoppingLIVEDataManager::getThumbnailPixmapSize(QSize &size, QMargins &margin, int &radius, QSize &liveSize)
{
	QReadLocker readLocker(&downloadImagePixmapInfoRWLock);
	size = downloadImagePixmapSize;
	liveSize = downloadImagePixmapLiveSize;
	margin = downloadImagePixmapMargin;
	radius = downloadImagePixmapRadius;
}

bool PLSNaverShoppingLIVEDataManager::hasSmartStoreInfo() const
{
	return !smartStoreInfo.storeId.isEmpty() && !smartStoreInfo.storeName.isEmpty() && !smartStoreInfo.accessToken.isEmpty();
}

QString PLSNaverShoppingLIVEDataManager::getSmartStoreId() const
{
	return smartStoreInfo.storeId;
}

QString PLSNaverShoppingLIVEDataManager::getSmartStoreName() const
{
	return smartStoreInfo.storeName;
}

QString PLSNaverShoppingLIVEDataManager::getSmartStoreAccessToken() const
{
	return smartStoreInfo.accessToken;
}

void PLSNaverShoppingLIVEDataManager::setSmartStoreAccessToken(PLSPlatformNaverShoppingLIVE * /*platform*/, const QString &accessToken)
{
	smartStoreInfo.accessToken = accessToken;
	saveSmartStoreInfo();
}

void PLSNaverShoppingLIVEDataManager::setSmartStoreInfo(const QString &storeId, const QString &storeName)
{
	smartStoreInfo.storeId = storeId;
	smartStoreInfo.storeName = storeName;
	saveSmartStoreInfo();
}

void PLSNaverShoppingLIVEDataManager::clearSmartStoreInfo()
{
	smartStoreInfo.storeId.clear();
	smartStoreInfo.storeName.clear();
	smartStoreInfo.accessToken.clear();
	saveSmartStoreInfo();
}

void PLSNaverShoppingLIVEDataManager::loadSmartStoreInfo()
{
	QString smartStoreFile = getFilePath(SMART_STORE_INFO_FILE_NAME);

	QByteArray bytes;
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, smartStoreFile);
	if (bytes.isEmpty()) {
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live load smart store info failed. reason: %s", parseError.errorString().toUtf8().constData());
		return;
	}

	QJsonObject obj = doc.object();
	smartStoreInfo.storeId = JSON_getString(obj, storeId);
	smartStoreInfo.storeName = JSON_getString(obj, storeName);
	smartStoreInfo.accessToken = JSON_getString(obj, accessToken);
}

void PLSNaverShoppingLIVEDataManager::saveSmartStoreInfo()
{
	QJsonObject obj{JSON_mkObject(storeId, smartStoreInfo.storeId), JSON_mkObject(storeName, smartStoreInfo.storeName), JSON_mkObject(accessToken, smartStoreInfo.accessToken)};

	QString recentProductsFile = getFilePath(SMART_STORE_INFO_FILE_NAME);

	QJsonDocument doc(obj);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), recentProductsFile)) {
		PLS_ERROR(NAVERSHOPPINGLIVE_DATAMANAGER, "naver shopping live save smart store info failed.");
	}
}

void PLSNaverShoppingLIVEDataManager::tokenExpiredClear()
{
	clearRecentProductNos();

	clearSearchKeywords();

	clearLatestUseTab(false);
	clearLatestStoreId(false);
	saveOtherInfos();

	clearSmartStoreInfo();

	clearThumbnailPixmaps();
}

qint64 PLSNaverShoppingLIVEDataManager::getItemId()
{
	if ((lastItemId <= 0) || (lastItemId >= INT64_MAX)) {
		lastItemId = 0;
	}
	return lastItemId++;
}
