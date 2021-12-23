#include <Windows.h>
#include "PLSMotionNetwork.h"
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include "pls-app.hpp"
#include "PLSHttpApi/PLSHmacNetworkReplyBuilder.h"
#include "PLSMotionFileManager.h"
#include "PLSMotionImageListView.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "log/log.h"
#include "json-data-handler.hpp"
#include "pls-common-define.hpp"
#include "unzip.h"
#include "PLSMotionItemView.h"
#include "frontend-api.h"
#include "pls-net-url.hpp"

#define MODULE_MOTION_SERVICE "MotionService"
#define PRISM_GROUP_ID "PRISM"
#define FREE_GROUP_ID "FREE"
#define PRISM_RESOURCE_TYPE "PRISM_RESOURCE_TYPE"
#define PRISM_THUMBNAIL_TYPE "PRISM_THUMBNAIL_TYPE"
#define PRISM_STATIC_IMAGE_TYPE "PRISM_STATIC_IMAGE_TYPE"
#define VIRTUAL_BG_JSON "PRISMLiveStudio/virtual/virtual_bg.json"
#define DOWNLOAD_CACHE_JSON "PRISMLiveStudio/virtual/download_cache.json"

template<typename F> F getFunc(F &&f)
{
	return f;
}

static void destroyQThread(QThread *thread)
{
	thread->quit();
	thread->wait();

	delete thread;
}

static void sourceAutoRestore(const MotionData &md)
{
	std::vector<OBSSource> sources;
	pls_get_all_source(sources, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, "item_id", [itemId = md.itemId.toUtf8()](const char *value) { return itemId == value; });
	for (OBSSource source : sources) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, "force_auto_restore", true);
		obs_source_update(source, settings);
		obs_data_release(settings);
	}
}

PLSMotionDownloadFile::Url::Url(const QString &url_, QString &filePath_) : url(url_), filePath(filePath_) {}

PLSMotionDownloadFile::PLSMotionDownloadFile(QNetworkAccessManager *namanager_, const QString &group_, const MotionData &md_) : namanager(namanager_), group(group_), md(md_) {}

PLSMotionDownloadFile::~PLSMotionDownloadFile() {}

void PLSMotionDownloadFile::start()
{
	QMetaObject::invokeMethod(this, &PLSMotionDownloadFile::onStart, Qt::QueuedConnection);
}

void PLSMotionDownloadFile::abort()
{
	aborting = true;
	stopTimer();

	if (reply) {
		reply->abort();
	}
}

void PLSMotionDownloadFile::downloadSuccess()
{
	stopTimer();
	emit finished(true, group, md);
	delete this;
}

void PLSMotionDownloadFile::downloadFailed()
{
	stopTimer();

	if (!aborting) {
		PLS_ERROR(MODULE_MOTION_SERVICE, "download virtual background resource file failed, item id: %s", md.itemId.toUtf8().constData());
	}

	md.resourceState = md.staticImgState = md.foregroundState = md.foregroundStaticImgState = md.thumbnailState = MotionState::DOWNLOAD_FAILED;
	emit finished(false, group, md);
	delete this;
}

void PLSMotionDownloadFile::startTimer(int timeout)
{
	stopTimer();
	timer = new QTimer();
	connect(timer, &QTimer::timeout, this, &PLSMotionDownloadFile::onTimeout);
	timer->setSingleShot(true);
	timer->start(timeout);
}

void PLSMotionDownloadFile::stopTimer()
{
	if (timer) {
		timer->stop();
		delete timer;
		timer = nullptr;
	}
}

void PLSMotionDownloadFile::onStart()
{
	urls.clear();
	urls << std::shared_ptr<Url>(new Url(md.resourceUrl, resourcePath)) << std::shared_ptr<Url>(new Url(md.thumbnailUrl, thumbnailPath));
	onDownload();
}

void PLSMotionDownloadFile::onDownload()
{
	bytesReceived = 0;
	bytesTotal = 0;

	if (urls.isEmpty()) {
		return;
	}

	std::shared_ptr<Url> url = urls.takeFirst();

	url->filePath = PLSMotionFileManager::instance()->getFilePathByURL(md, url->url);
	if (QFile::exists(url->filePath)) {
		QFile::remove(url->filePath);
	}

	QDir fileDir = QFileInfo(url->filePath).dir();
	if (!fileDir.exists()) {
		fileDir.mkpath(fileDir.absolutePath());
	}

	if (file.isOpen()) {
		file.flush();
		file.close();
	}

	file.setFileName(url->filePath);
	if (!file.open(QFile::WriteOnly)) {
		PLS_ERROR(MODULE_MOTION_SERVICE, "create virtual background resource %s file failed, reason: %s", url->url.toUtf8().constData(), file.errorString().toUtf8().constData());
		downloadFailed();
		return;
	}

	this->lastWriteTime = std::chrono::steady_clock::now();
	reply = PLSNetworkReplyBuilder(url->url).get(namanager);
	QObject::connect(reply, &QNetworkReply::downloadProgress, this, &PLSMotionDownloadFile::onDownloadProgress);
	QObject::connect(reply, &QNetworkReply::readyRead, this, &PLSMotionDownloadFile::onWriteData);
	QObject::connect(reply, &QNetworkReply::finished, this, [=]() { onDownloadFinished(); });
	startTimer(PRISM_NET_REQUEST_TIMEOUT);
}

void PLSMotionDownloadFile::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	this->lastWriteTime = std::chrono::steady_clock::now();
	this->bytesReceived = bytesReceived;
	this->bytesTotal = bytesTotal;
}

void PLSMotionDownloadFile::onWriteData()
{
	this->lastWriteTime = std::chrono::steady_clock::now();
	file.write(reply->readAll());
	file.flush();
}

void PLSMotionDownloadFile::onDownloadFinished()
{
	this->lastWriteTime = std::chrono::steady_clock::now();

	bool ok = true;
	if (reply) {
		ok = reply->error() == QNetworkReply::NoError;
		reply->deleteLater();
		reply = nullptr;
	}

	if (file.isOpen()) {
		file.flush();
		file.close();
	}

	if (!(ok && (bytesTotal > 0) && (bytesReceived == bytesTotal))) {
		downloadFailed();
	} else if (urls.isEmpty()) {
		onUncompress();
	} else {
		onDownload();
	}
}

static QString findImageByPrefix(const QStringList &filePaths, const QString &prefix)
{
	for (const QString &filePath : filePaths) {
		QFileInfo fi(filePath);
		if (fi.fileName().startsWith(prefix)) {
			return filePath;
		}
	}
	return QString();
}

void PLSMotionDownloadFile::onUncompress()
{
	HZIP zip = OpenZip(resourcePath.toStdWString().c_str(), 0);
	if (zip == 0) {
		downloadFailed();
		return;
	}

	QDir dstDir = QFileInfo(resourcePath).dir();
	ZRESULT result = SetUnzipBaseDir(zip, dstDir.absolutePath().toStdWString().c_str());
	if (result != ZR_OK) {
		CloseZip(zip);
		downloadFailed();
		return;
	}

	ZIPENTRY zipEntry;
	result = GetZipItem(zip, -1, &zipEntry);
	if (result != ZR_OK) {
		CloseZip(zip);
		downloadFailed();
		return;
	}

	QStringList files;
	int numitems = zipEntry.index;
	for (int i = 0; i < numitems; i++) {
		if ((GetZipItem(zip, i, &zipEntry) == ZR_OK) && (UnzipItem(zip, i, zipEntry.name) == ZR_OK)) {
			files.append(dstDir.absoluteFilePath(QString::fromWCharArray(zipEntry.name)));
		}
	}

	CloseZip(zip);

	if (md.type == MotionType::STATIC) {
		QString imagePath = findImageByPrefix(files, "background_static");
		if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
			md.resourcePath = md.staticImgPath = imagePath;
			md.resourceState = md.staticImgState = MotionState::LOCAL_FILE;
		} else {
			md.resourceState = md.staticImgState = MotionState::NO_FILE;
			md.resourcePath.clear();
			md.staticImgPath.clear();
		}

		QString foregroundImagePath = findImageByPrefix(files, "foreground_static");
		if (!foregroundImagePath.isEmpty() && QFile::exists(foregroundImagePath)) {
			md.foregroundPath = md.foregroundStaticImgPath = foregroundImagePath;
			md.foregroundState = md.foregroundStaticImgState = MotionState::LOCAL_FILE;
		} else {
			md.foregroundState = md.foregroundStaticImgState = MotionState::NO_FILE;
			md.foregroundPath.clear();
			md.foregroundStaticImgPath.clear();
		}
	} else if (md.type == MotionType::MOTION) {
		QString videoPath = findImageByPrefix(files, "background_motion");
		if (!videoPath.isEmpty() && QFile::exists(videoPath)) {
			md.resourcePath = videoPath;
			md.resourceState = MotionState::LOCAL_FILE;
		} else {
			md.resourceState = MotionState::NO_FILE;
			md.resourcePath.clear();
		}

		QString imagePath = findImageByPrefix(files, "background_static");
		if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
			md.staticImgPath = imagePath;
			md.staticImgState = MotionState::LOCAL_FILE;
		} else {
			md.staticImgState = MotionState::NO_FILE;
			md.staticImgPath.clear();
		}

		QString foregroundVideoPath = findImageByPrefix(files, "foreground_motion");
		if (!foregroundVideoPath.isEmpty() && QFile::exists(foregroundVideoPath)) {
			md.foregroundPath = foregroundVideoPath;
			md.foregroundState = MotionState::LOCAL_FILE;
		} else {
			md.foregroundState = MotionState::NO_FILE;
			md.foregroundPath.clear();
		}

		QString foregroundImagePath = findImageByPrefix(files, "foreground_static");
		if (!foregroundImagePath.isEmpty() && QFile::exists(foregroundImagePath)) {
			md.foregroundStaticImgPath = foregroundImagePath;
			md.foregroundStaticImgState = MotionState::LOCAL_FILE;
		} else {
			md.foregroundStaticImgState = MotionState::NO_FILE;
			md.foregroundStaticImgPath.clear();
		}
	}

	if (!thumbnailPath.isEmpty() && QFile::exists(thumbnailPath)) {
		md.thumbnailPath = thumbnailPath;
		md.thumbnailState = MotionState::LOCAL_FILE;
		PLSMotionFileManager::instance()->updateThumbnailPixmap(md.itemId, QPixmap(thumbnailPath));
	} else {
		md.thumbnailState = MotionState::NO_FILE;
		md.thumbnailPath.clear();
	}

	if (md.backgroundIsLocalFile() && md.thumbnailIsLocalFile()) { // foreground is optional
		downloadSuccess();
	} else {
		downloadFailed();
	}
}

void PLSMotionDownloadFile::onTimeout()
{
	if (reply) {
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastWriteTime).count() > PRISM_NET_REQUEST_TIMEOUT) {
			reply->abort();
		} else {
			startTimer(PRISM_NET_REQUEST_TIMEOUT);
		}
	}
}

PLSMotionDownloadThread::PLSMotionDownloadThread()
{
	namanager = new QNetworkAccessManager();
	namanager->moveToThread(this);
	connect(
		namanager, &QObject::destroyed, this, [this]() { destroyQThread(this); }, Qt::QueuedConnection);
}

PLSMotionDownloadThread::~PLSMotionDownloadThread() {}

bool PLSMotionDownloadThread::downloadResourceFile(const QString &group, const MotionData &md)
{
	if (totalCount >= MaxTotalCount) {
		return false;
	}

	++totalCount;
	PLSMotionDownloadFile *mdf = new PLSMotionDownloadFile(namanager, group, md);
	mdf->moveToThread(this);
	connect(mdf, &PLSMotionDownloadFile::finished, this, &PLSMotionDownloadThread::downloadResourceFileFinished, Qt::QueuedConnection);
	connect(this, &PLSMotionDownloadThread::aborting, mdf, &PLSMotionDownloadFile::abort, Qt::QueuedConnection);
	mdf->start();
	return true;
}

void PLSMotionDownloadThread::abort()
{
	QEventLoop el;
	connect(this, &QObject::destroyed, &el, &QEventLoop::quit);
	emit aborting();
	el.exec();
}

void PLSMotionDownloadThread::downloadResourceFileFinished(bool ok, const QString &group, const MotionData &md)
{
	++finishedCount;
	emit finished(ok, group, md);

	if (finishedCount == totalCount) {
		namanager->deleteLater();
	}
}

PLSMotionNetwork::PLSMotionNetwork(QObject *parent) : QObject(parent), m_prismRequestFinished(false), m_freeRequestFinished(false)
{
	loadCacheList();
	loadList();
}

PLSMotionNetwork::~PLSMotionNetwork()
{
	saveCacheList();
}

PLSMotionNetwork *PLSMotionNetwork::instance()
{
	static PLSMotionNetwork syncServerManager;
	return &syncServerManager;
}

void PLSMotionNetwork::downloadResource(const QJsonObject &obj)
{
	QString url = obj["resourceUrl"].toString();
	if (url.isEmpty()) {
		PLSMotionNetwork::instance()->downloadPrismListRequest(true);
		PLSMotionNetwork::instance()->downloadFreeListRequest(true);
		return;
	}

	auto okCallback = [=](QNetworkReply *, int, QByteArray data, void *) {
		PLSJsonDataHandler::saveJsonFile(data, pls_get_user_path(VIRTUAL_BG_JSON));

		//download the resources
		PLSMotionNetwork::instance()->downloadPrismListRequest(true);
		PLSMotionNetwork::instance()->downloadFreeListRequest(true);
	};
	auto failCallback = [=](QNetworkReply *, int, QByteArray data, QNetworkReply::NetworkError, void *) {
		PLS_ERROR(MODULE_MOTION_SERVICE, "download virtual_bg.json failed, resp: %s", data.constData());

		//download the resources use locacle cache
		PLSMotionNetwork::instance()->downloadPrismListRequest(true);
		PLSMotionNetwork::instance()->downloadFreeListRequest(true);
	};

	PLSHmacNetworkReplyBuilder builder(url, HmacType::HT_PRISM);
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	PLSHttpHelper::connectFinished(builder.get(), this, okCallback, failCallback);
}

void PLSMotionNetwork::downloadPrismListRequest(bool force)
{
	if (m_prismRequestFinished || force) {
		m_prismRequestFinished = false;
		m_cacheList.remove(PRISM_GROUP_ID);
		emit prismListStartedSignal();
		QVariantList list = PLSMotionFileManager::instance()->getPrismList();
		downloadListRequest(list, PRISM_GROUP_ID);
		checkListRequestState(PRISM_GROUP_ID);
	}
}

void PLSMotionNetwork::downloadFreeListRequest(bool force)
{
	if (m_freeRequestFinished || force) {
		m_freeRequestFinished = false;
		m_cacheList.remove(FREE_GROUP_ID);
		emit freeListStartedSignal();
		QVariantList list = PLSMotionFileManager::instance()->getFreeList();
		downloadListRequest(list, FREE_GROUP_ID);
		checkListRequestState(FREE_GROUP_ID);
	}
}

QList<MotionData> PLSMotionNetwork::getPrismCacheList()
{
	return m_cacheList.value(PRISM_GROUP_ID);
}

QList<MotionData> PLSMotionNetwork::getFreeCacheList()
{
	return m_cacheList.value(FREE_GROUP_ID);
}

bool PLSMotionNetwork::prismRequestFinished()
{
	return m_prismRequestFinished;
}

bool PLSMotionNetwork::freeRequestFinished()
{
	return m_freeRequestFinished;
}

bool PLSMotionNetwork::isPrismOrFree(const MotionData &md) const
{
	if (md.dataType == DataType::PRISM || md.dataType == DataType::FREE) {
		return true;
	} else if (md.dataType == DataType::MYLIST) {
		return false;
	}

	for (auto iter = m_cacheList.begin(), enditer = m_cacheList.end(); iter != enditer; ++iter) {
		for (const MotionData &_md : iter.value()) {
			if (_md.itemId == md.itemId) {
				return true;
			}
		}
	}
	return false;
}

bool PLSMotionNetwork::isPathEqual(const MotionData &md1, const MotionData &md2) const
{
	if ((md1.itemId == md2.itemId) && (md1.resourcePath == md2.resourcePath) && (md1.staticImgPath == md2.staticImgPath) && (md1.thumbnailPath == md2.thumbnailPath)) {
		return true;
	}
	return false;
}

bool PLSMotionNetwork::findPrismOrFree(MotionData &md, const QString &itemId) const
{
	for (auto iter = m_cacheList.begin(), enditer = m_cacheList.end(); iter != enditer; ++iter) {
		for (const MotionData &_md : iter.value()) {
			if (_md.itemId == itemId) {
				md = _md;
				return true;
			}
		}
	}
	return false;
}

void PLSMotionNetwork::abort()
{
	for (QPointer<PLSMotionDownloadThread> downloadThread : m_downloadThreads) {
		if (downloadThread) {
			downloadThread->abort();
		}
	}
}

void PLSMotionNetwork::downloadListRequest(const QVariantList &list, const QString &group)
{
	PLSMotionDownloadThread *downloadThread = nullptr;

	QList<MotionData> &motionList = m_cacheList[group];
	for (int i = 0; i < list.size(); i++) {
		QJsonObject jsonObject = list.at(i).toJsonObject();

		MotionData data;
		data.itemId = jsonObject.value(name2str(itemId)).toString();
		data.title = jsonObject.value(name2str(title)).toString();
		data.version = jsonObject.value(name2str(version)).toInt();
		data.type = PLSMotionFileManager::instance()->motionTypeByString(jsonObject.value(name2str(type)).toString());
		data.dataType = (PRISM_GROUP_ID == group) ? data.dataType = DataType::PRISM : DataType::FREE;
		data.resourceUrl = jsonObject.value(name2str(resourceUrl)).toString();
		data.thumbnailUrl = jsonObject.value(name2str(thumbnailUrl)).toString();
		data.canDelete = false;

		bool cached = checkResourceCached(data, group);
		motionList.append(data);

		if (!cached) {
			downloadResourceFile(downloadThread, group, data);
		} else if (group == PRISM_GROUP_ID) {
			emit prismResourceDownloadFinished(data);
		} else if (group == FREE_GROUP_ID) {
			emit freeResourceDownloadFinished(data);
		}
	}
}

bool PLSMotionNetwork::checkResourceCached(MotionData &md, const QString &group)
{
	auto &mds = m_downloadCache[group];
	for (const MotionData &_md : mds) {
		if (_md.itemId == md.itemId) {
			if ((_md.version == md.version) && (_md.resourceUrl == md.resourceUrl) && (_md.thumbnailUrl == md.thumbnailUrl) &&
			    ((MotionData::pathIsValid(_md.resourcePath, _md.resourceState) && MotionData::pathIsValid(_md.staticImgPath, _md.staticImgState)) ||
			     (MotionData::pathIsValid(_md.foregroundPath, _md.foregroundState) && MotionData::pathIsValid(_md.foregroundStaticImgPath, _md.foregroundStaticImgState))) &&
			    MotionData::pathIsValid(_md.thumbnailPath, _md.thumbnailState)) {
				md = _md;
				return true;
			}
			return false;
		}
	}
	mds.append(md);
	return false;
}

void PLSMotionNetwork::downloadResourceFile(PLSMotionDownloadThread *&downloadThread, const QString &group, const MotionData &md)
{
	if (!downloadThread) {
		downloadThread = new PLSMotionDownloadThread();
		m_downloadThreads.append(downloadThread);
		connect(downloadThread, &PLSMotionDownloadThread::finished, this, &PLSMotionNetwork::downloadResourceFileFinished, Qt::QueuedConnection);
		downloadThread->start();
	}

	if (downloadThread->downloadResourceFile(group, md)) {
		return;
	}

	downloadThread = nullptr;
	downloadResourceFile(downloadThread, group, md);
}

void PLSMotionNetwork::downloadResourceFileFinished(bool, const QString &group, const MotionData &md)
{
	{
		auto &mds = m_cacheList[group];
		for (auto i = 0, count = mds.count(); i < count; ++i) {
			if (mds[i].itemId == md.itemId) {
				mds[i] = md;
				break;
			}
		}
	}

	{
		auto &mds = m_downloadCache[group];
		for (auto i = 0, count = mds.count(); i < count; ++i) {
			if (mds[i].itemId == md.itemId) {
				mds[i] = md;
				break;
			}
		}
	}

	saveCacheList();

	//check download file whether finished or not
	if ((md.backgroundIsLocalFile() || md.foregroundIsLocalFile()) && md.thumbnailIsLocalFile()) {
		if (group == PRISM_GROUP_ID) {
			emit prismResourceDownloadFinished(md);
		} else if (group == FREE_GROUP_ID) {
			emit freeResourceDownloadFinished(md);
		}
	}

	checkListRequestState(group);

	sourceAutoRestore(md);
}

void PLSMotionNetwork::checkListRequestState(const QString &groupType)
{
	QList<MotionData> list = m_cacheList.value(groupType);
	for (int i = 0; i < list.size(); i++) {
		if (list.at(i).isUnknown()) {
			return;
		}
	}

	if (groupType == PRISM_GROUP_ID) {
		m_prismRequestFinished = true;
		emit prismListFinishedSignal();
	} else if (groupType == FREE_GROUP_ID) {
		m_freeRequestFinished = true;
		emit freeListFinishedSignal();
	}
}

void PLSMotionNetwork::loadList()
{
	PLSMotionFileManager *manager = PLSMotionFileManager::instance();
	loadListRequest(manager->getPrismList(), PRISM_GROUP_ID);
	loadListRequest(manager->getFreeList(), FREE_GROUP_ID);
}

void PLSMotionNetwork::loadListRequest(const QVariantList &list, const QString &group)
{
	QList<MotionData> &motionList = m_cacheList[group];
	for (int i = 0; i < list.size(); i++) {
		QJsonObject jsonObject = list.at(i).toJsonObject();

		MotionData data;
		data.itemId = jsonObject.value(name2str(itemId)).toString();
		data.title = jsonObject.value(name2str(title)).toString();
		data.version = jsonObject.value(name2str(version)).toInt();
		data.type = PLSMotionFileManager::instance()->motionTypeByString(jsonObject.value(name2str(type)).toString());
		data.dataType = (PRISM_GROUP_ID == group) ? data.dataType = DataType::PRISM : DataType::FREE;
		data.resourceUrl = jsonObject.value(name2str(resourceUrl)).toString();
		data.thumbnailUrl = jsonObject.value(name2str(thumbnailUrl)).toString();
		data.canDelete = false;

		checkResourceCached(data, group);
		motionList.append(data);
	}
}

void PLSMotionNetwork::loadCacheList()
{
	QByteArray bytes;
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, pls_get_user_path(DOWNLOAD_CACHE_JSON));
	if (bytes.isEmpty()) {
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		PLS_ERROR(MODULE_MOTION_SERVICE, "load resource download cache failed. reason: %s", parseError.errorString().toUtf8().constData());
		return;
	}

	QJsonObject cacheobj = doc.object();
	for (const QString &key : cacheobj.keys()) {
		QList<MotionData> mds;
		QJsonArray mdarr = cacheobj.value(key).toArray();
		for (int i = 0; i < mdarr.count(); ++i) {
			QJsonObject mdobj = mdarr[i].toObject();
			MotionData md;
			md.dataType = static_cast<DataType>(mdobj.value(name2str(dataType)).toInt());
			md.type = static_cast<MotionType>(mdobj.value(name2str(type)).toInt());
			md.itemId = mdobj.value(name2str(itemId)).toString();
			md.title = mdobj.value(name2str(title)).toString();
			md.version = mdobj.value(name2str(version)).toInt();
			md.resourceUrl = mdobj.value(name2str(resourceUrl)).toString();
			md.thumbnailUrl = mdobj.value(name2str(thumbnailUrl)).toString();
			md.resourceState = static_cast<MotionState>(mdobj.value(name2str(resourceState)).toInt());
			md.thumbnailState = static_cast<MotionState>(mdobj.value(name2str(thumbnailState)).toInt());
			md.staticImgState = static_cast<MotionState>(mdobj.value(name2str(staticImgState)).toInt());
			md.foregroundState = static_cast<MotionState>(mdobj.value(name2str(foregroundState)).toInt());
			md.foregroundStaticImgState = static_cast<MotionState>(mdobj.value(name2str(foregroundStaticImgState)).toInt());
			md.resourcePath = mdobj.value(name2str(resourcePath)).toString();
			md.staticImgPath = mdobj.value(name2str(staticImgPath)).toString();
			md.thumbnailPath = mdobj.value(name2str(thumbnailPath)).toString();
			md.foregroundPath = mdobj.value(name2str(foregroundPath)).toString();
			md.foregroundStaticImgPath = mdobj.value(name2str(foregroundStaticImgPath)).toString();
			md.canDelete = mdobj.value(name2str(canDelete)).toBool();
			mds.append(md);
		}
		m_downloadCache.insert(key, mds);
	}
}

void PLSMotionNetwork::saveCacheList()
{
	QJsonObject cacheobj;
	for (auto iter = m_downloadCache.begin(), enditer = m_downloadCache.end(); iter != enditer; ++iter) {
		QJsonArray mdarr;
		const QList<MotionData> &mds = iter.value();
		for (const MotionData &md : mds) {
			QJsonObject mdobj;
			mdobj[name2str(dataType)] = static_cast<int>(md.dataType);
			mdobj[name2str(type)] = static_cast<int>(md.type);
			mdobj[name2str(itemId)] = md.itemId;
			mdobj[name2str(title)] = md.title;
			mdobj[name2str(version)] = md.version;
			mdobj[name2str(resourceUrl)] = md.resourceUrl;
			mdobj[name2str(thumbnailUrl)] = md.thumbnailUrl;
			mdobj[name2str(resourceState)] = static_cast<int>(md.resourceState);
			mdobj[name2str(thumbnailState)] = static_cast<int>(md.thumbnailState);
			mdobj[name2str(staticImgState)] = static_cast<int>(md.staticImgState);
			mdobj[name2str(foregroundState)] = static_cast<int>(md.foregroundState);
			mdobj[name2str(foregroundStaticImgState)] = static_cast<int>(md.foregroundStaticImgState);
			mdobj[name2str(resourcePath)] = md.resourcePath;
			mdobj[name2str(staticImgPath)] = md.staticImgPath;
			mdobj[name2str(thumbnailPath)] = md.thumbnailPath;
			mdobj[name2str(foregroundPath)] = md.foregroundPath;
			mdobj[name2str(foregroundStaticImgPath)] = md.foregroundStaticImgPath;
			mdobj[name2str(canDelete)] = md.canDelete;
			mdarr.append(mdobj);
		}

		cacheobj[iter.key()] = mdarr;
	}

	QJsonDocument doc(cacheobj);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), pls_get_user_path(DOWNLOAD_CACHE_JSON))) {
		PLS_ERROR(MODULE_MOTION_SERVICE, "save resource download cache failed.");
	}
}
