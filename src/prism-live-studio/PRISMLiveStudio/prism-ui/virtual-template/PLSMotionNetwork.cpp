#include "PLSMotionNetwork.h"
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include "obs-app.hpp"
#include "PLSMotionFileManager.h"
#include "PLSMotionImageListView.h"
#include "log/log.h"
#include "json-data-handler.hpp"
#include "pls-common-define.hpp"
#include "PLSMotionItemView.h"
#include "frontend-api.h"
#include "pls-net-url.hpp"
#include "utils-api.h"
#include "libhttp-client.h"
#include "pls-gpop-data-struct.hpp"
#include "PLSResCommonFuns.h"

using namespace common;
constexpr auto MODULE_MOTION_SERVICE = "MotionService";
constexpr auto PRISM_GROUP_ID = "PRISM";
constexpr auto FREE_GROUP_ID = "FREE";
constexpr auto VIRTUAL_BG_JSON = "PRISMLiveStudio/virtual/virtual_bg.json";
constexpr auto DOWNLOAD_CACHE_JSON = "PRISMLiveStudio/virtual/download_cache.json";

const int DOWNLOAD_VIRTUAL_BACKGROUND_TIMEOUT_MS = 180000;

static void sourceAutoRestore(const MotionData &md)
{
	pls_check_app_exiting();

	std::vector<OBSSource> sources;
	pls_get_all_source(sources, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, "item_id", [itemId = md.itemId.toUtf8()](const char *value) { return itemId == value; });
	for (OBSSource source : sources) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_bool(settings, "force_auto_restore", true);
		obs_source_update(source, settings);
		obs_data_release(settings);
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

PLSMotionNetwork::PLSMotionNetwork(QObject *parent) : QObject(parent)
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

void PLSMotionNetwork::downloadResource(const QString &categoryJsonFilePath) const
{
	auto categoryPath = categoryJsonFilePath;
	if (!QFileInfo(categoryPath).exists()) {
		PLS_INFO(RESOURCE_DOWNLOAD, "local category json not exist, will use app data dir.");
		QDir appDir(pls_get_dll_dir("libresources-download"));
		categoryPath = appDir.absoluteFilePath(QString("data/prism-studio/user/category.json"));
	}
	QJsonArray array;
	pls_read_json(array, categoryPath);
	for (auto val : array) {
		QJsonObject obj = val.toObject();
		if (obj[QStringLiteral("categoryId")].toString() == QStringLiteral("virtual_bg")) {
			downloadResource(obj);
			return;
		}
	}

	downloadResource(QJsonObject{});
}

void PLSMotionNetwork::downloadResource(const QJsonObject &obj) const
{
	QString url = obj["resourceUrl"].toString();
	if (url.isEmpty()) {
		PLSMotionNetwork::instance()->downloadPrismListRequest(true);
		PLSMotionNetwork::instance()->downloadFreeListRequest(true);
		return;
	}

	pls::http::request(pls::http::Request()                    //
				   .method(pls::http::Method::Get) //
				   .hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
				   .forDownload(true)
				   .saveFilePath(pls_get_user_path(VIRTUAL_BG_JSON))
				   .workInMainThread()
				   .timeout(DOWNLOAD_VIRTUAL_BACKGROUND_TIMEOUT_MS)
				   .result([](const pls::http::Reply &reply) {
					   if (!reply.isOk()) {
						   PLS_ERROR(MODULE_MOTION_SERVICE, "download virtual_bg.json failed");
					   }

					   //download the resources
					   PLSMotionNetwork::instance()->downloadPrismListRequest(true);
					   PLSMotionNetwork::instance()->downloadFreeListRequest(true);
				   }));
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

QList<MotionData> PLSMotionNetwork::getPrismCacheList() const
{
	return m_cacheList.value(PRISM_GROUP_ID);
}

QList<MotionData> PLSMotionNetwork::getFreeCacheList() const
{
	return m_cacheList.value(FREE_GROUP_ID);
}

bool PLSMotionNetwork::prismRequestFinished() const
{
	return m_prismRequestFinished;
}

bool PLSMotionNetwork::freeRequestFinished() const
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

int PLSMotionNetwork::GetResourceNotCachedCount()
{
	int count = 0;
	for (MotionData &motion : m_cacheList[PRISM_GROUP_ID]) {
		bool cached = checkResourceCached(motion, PRISM_GROUP_ID);
		if (!cached)
			count++;
	}
	for (MotionData &motion : m_cacheList[FREE_GROUP_ID]) {
		bool cached = checkResourceCached(motion, FREE_GROUP_ID);
		if (!cached)
			count++;
	}
	return count;
}

void PLSMotionNetwork::downloadListRequest(const QVariantList &list, const QString &group)
{
	QList<MotionData> &motionList = m_cacheList[group];
	for (const QVariant &it : list) {
		QJsonObject jsonObject = it.toJsonObject();
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
			downloadResourceFile(group, data);
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

void PLSMotionNetwork::downloadResourcePrismFile(const MotionData &md, bool update, bool reDownload, const std::function<void(const MotionData &md)> &ok,
						 const std::function<void(const MotionData &md)> &fail)
{
	downloadResourceFile(PRISM_GROUP_ID, md, update, reDownload, ok, fail);
}
void PLSMotionNetwork::downloadResourceFreeFile(const MotionData &md, bool update, bool reDownload, const std::function<void(const MotionData &md)> &ok,
						const std::function<void(const MotionData &md)> &fail)
{
	downloadResourceFile(FREE_GROUP_ID, md, update, reDownload, ok, fail);
}
void PLSMotionNetwork::downloadResourceFile(const QString &group, const MotionData &md, bool update, bool reDownload, const std::function<void(const MotionData &md)> &ok,
					    const std::function<void(const MotionData &md)> &fail)
{
	if (isDownloading(group, md.itemId)) {
		return;
	}

	setDownloading(group, md.itemId);
	pls::http::requests(pls::http::Requests()
				    .workInGroup(pls::http::RS_DOWNLOAD_WORKER_GROUP)
				    .add(pls::http::Request()         //
						 .forDownload(true)   //
						 .url(md.resourceUrl) //
						 .withLog()           //
						 .saveFilePath(PLSMotionFileManager::instance()->getFilePathByURL(md, md.resourceUrl))
						 .timeout(DOWNLOAD_VIRTUAL_BACKGROUND_TIMEOUT_MS))
				    .add(pls::http::Request()          //
						 .forDownload(true)    //
						 .url(md.thumbnailUrl) //
						 .withLog()            //
						 .saveFilePath(PLSMotionFileManager::instance()->getFilePathByURL(md, md.thumbnailUrl))
						 .timeout(DOWNLOAD_VIRTUAL_BACKGROUND_TIMEOUT_MS))
				    .receiver(this)
				    .results([this, ok, fail, group, md, update, reDownload](const pls::http::Replies &replies) {
					    MotionData smd = md;
					    if (!replies.isOk() || !uncompress(smd, replies.reply(0).downloadFilePath(), replies.reply(1).downloadFilePath())) {
						    PLS_ERROR(MODULE_MOTION_SERVICE, "download virtual background resource file failed, item id: %s", md.itemId.toUtf8().constData());
						    smd.resourceState = smd.staticImgState = smd.foregroundState = smd.foregroundStaticImgState = smd.thumbnailState = MotionState::DOWNLOAD_FAILED;
						    pls_async_call(this, [this, fail, group, smd, update, reDownload]() {
							    downloadResourceFileFinished(false, group, smd, update, reDownload);
							    pls_invoke_safe(fail, smd);
						    });
					    } else {
						    pls_async_call(this, [this, ok, group, smd, update, reDownload]() {
							    downloadResourceFileFinished(true, group, smd, update, reDownload);
							    pls_invoke_safe(ok, smd);
						    });
					    }
				    }));
}

void PLSMotionNetwork::reDownloadResourceFile(MotionData &md, bool update, const std::function<void(const MotionData &md)> &ok, const std::function<void(const MotionData &md)> &fail)
{
	QString group;
	if (md.dataType == DataType::PRISM) {
		group = PRISM_GROUP_ID;
		if (bool cached = checkResourceCached(md, group); cached) {
			emit prismResourceDownloadFinished(md, update);
			return;
		}
	} else if (md.dataType == DataType::FREE) {
		group = FREE_GROUP_ID;
		if (bool cached = checkResourceCached(md, group); cached) {
			emit freeResourceDownloadFinished(md, update);
			return;
		}
	}

	downloadResourceFile(group, md, update, true, ok, fail);
}

bool PLSMotionNetwork::uncompress(MotionData &md, const QString &resourcePath, const QString &thumbnailPath) const
{
	QDir dstDir = QFileInfo(resourcePath).dir();
	PLSResCommonFuns::unZip(dstDir.absolutePath(), resourcePath, QFileInfo(resourcePath).fileName(), false);

	QStringList files;
	QDir dir(resourcePath.mid(0, resourcePath.lastIndexOf('.')));
	for (const QFileInfo &fi : dir.entryInfoList(QDir::Files, QDir::Time)) {
		files << fi.filePath();
	}

	if (md.type == MotionType::STATIC) {
		checkStatic(md, files);
	} else if (md.type == MotionType::MOTION) {
		checkMotion(md, files);
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
		return true;
	}
	return false;
}

void PLSMotionNetwork::checkStatic(MotionData &md, const QStringList &files) const
{
	if (QString imagePath = findImageByPrefix(files, "background_static"); !imagePath.isEmpty() && QFile::exists(imagePath)) {
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
}

void PLSMotionNetwork::checkMotion(MotionData &md, const QStringList &files) const
{
	if (QString videoPath = findImageByPrefix(files, "background_motion"); !videoPath.isEmpty() && QFile::exists(videoPath)) {
		md.resourcePath = videoPath;
		md.resourceState = MotionState::LOCAL_FILE;
	} else {
		md.resourceState = MotionState::NO_FILE;
		md.resourcePath.clear();
	}

	if (QString imagePath = findImageByPrefix(files, "background_static"); !imagePath.isEmpty() && QFile::exists(imagePath)) {
		md.staticImgPath = imagePath;
		md.staticImgState = MotionState::LOCAL_FILE;
	} else {
		md.staticImgState = MotionState::NO_FILE;
		md.staticImgPath.clear();
	}

	if (QString foregroundVideoPath = findImageByPrefix(files, "foreground_motion"); !foregroundVideoPath.isEmpty() && QFile::exists(foregroundVideoPath)) {
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

void PLSMotionNetwork::downloadResourceFileFinished(bool, const QString &group, const MotionData &md, bool update, bool reDownload)
{
	auto &cache_mds = m_cacheList[group];
	for (qsizetype i = 0, count = cache_mds.count(); i < count; ++i) {
		if (cache_mds[i].itemId == md.itemId) {
			cache_mds[i] = md;
			break;
		}
	}

	auto &download_mds = m_downloadCache[group];
	for (qsizetype i = 0, count = download_mds.count(); i < count; ++i) {
		if (download_mds[i].itemId == md.itemId) {
			download_mds[i] = md;
			break;
		}
	}

	saveCacheList();

	//check download file whether finished or not
	if ((md.backgroundIsLocalFile() || md.foregroundIsLocalFile()) && md.thumbnailIsLocalFile()) {
		if (group == PRISM_GROUP_ID) {
			emit prismResourceDownloadFinished(md, update);
		} else if (group == FREE_GROUP_ID) {
			emit freeResourceDownloadFinished(md, update);
		}
	}

	checkListRequestState(group);

	if (!reDownload)
		sourceAutoRestore(md);
}

void PLSMotionNetwork::checkListRequestState(const QString &groupType)
{
	QList<MotionData> list = m_cacheList.value(groupType);
	for (const MotionData &md : list) {
		if (md.isUnknown()) {
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

bool PLSMotionNetwork::isDownloading(const QString &group, const QString &itemId) const
{
	for (const auto &md : m_cacheList[group]) {
		if (md.itemId == itemId) {
			return md.resourceState == MotionState::DOWNLOADING;
		}
	}
	return false;
}
void PLSMotionNetwork::setDownloading(const QString &group, const QString &itemId)
{
	for (auto &md : m_cacheList[group]) {
		if (md.itemId == itemId) {
			md.resourceState = MotionState::DOWNLOADING;
			return;
		}
	}
}

void PLSMotionNetwork::loadList()
{
	const PLSMotionFileManager *manager = PLSMotionFileManager::instance();
	loadListRequest(manager->getPrismList(), PRISM_GROUP_ID);
	loadListRequest(manager->getFreeList(), FREE_GROUP_ID);
}

void PLSMotionNetwork::loadListRequest(const QVariantList &list, const QString &group)
{
	QList<MotionData> &motionList = m_cacheList[group];
	for (const QVariant &it : list) {
		QJsonObject jsonObject = it.toJsonObject();

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
