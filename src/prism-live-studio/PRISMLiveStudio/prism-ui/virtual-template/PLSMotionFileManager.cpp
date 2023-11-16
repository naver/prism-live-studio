#include "PLSMotionFileManager.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <quuid.h>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QCoreApplication>
#include "json-data-handler.hpp"
#include "frontend-internal.hpp"
#include <pls/media-info.h>
#include "liblog.h"
#include "PLSMotionItemView.h"
#include "PLSMotionNetwork.h"
#include "pls-common-define.hpp"
#include "PLSVirtualBgManager.h"
#include "utils-api.h"
#include <sys/stat.h>
#include <map>
using namespace common;
constexpr const char *MOTION_FILE_MANAGER = "MotionFileManager";
constexpr const char *VIRTUAL_SYNC_JSON_FILE = "PRISMLiveStudio/virtual/virtual_bg.json";
constexpr const char *VIRTUAL_SYNC_JSON_FILE_DEFAULT = "/data/prism-studio/virtual_bg/virtual_bg.json";
constexpr const char *VIRTUAL_USER_PATH = "PRISMLiveStudio/virtual/";
constexpr const char *VIRTUAL_USER_CACHE_JSON_FILE = "PRISMLiveStudio/virtual/cache.json";
constexpr const char *CATEGORY_INDEX_FILE = "PRISMLiveStudio/virtual/categoryIndex.ini";

#define MOTION_ID_KEY QStringLiteral("motionID")
#define MOTION_TYPE_KEY QStringLiteral("motionType")
#define MOTION_TITLE_KEY QStringLiteral("motionTitle")
#define MOTION_CAN_DELETE_KEY QStringLiteral("canDelete")
#define RESOURCE_PATH_KEY QStringLiteral("resourcePath")
#define THUMNAIL_PATH_KEY QStringLiteral("thumnailPath")
#define STATIC_IMAGE_PATH_KEY QStringLiteral("staticImgPath")
#define FOREBACKGROUND_PATH_KEY QStringLiteral("foregroundPath")
#define FOREBACKGROUND_STATIC_IMAGE_PATH_KEY QStringLiteral("foregroundStaticImgPath")

const int MAX_RECENT_COUNT = 30;

void sourceClearBackgroundTemplate(const QStringList &itemIds)
{
	std::vector<OBSSource> sources;
	pls_get_all_source(sources, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, "item_id", [itemIds](const char *value) { return itemIds.contains(QString::fromUtf8(value)); });
	for (OBSSource source : sources) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "item_id", "");
		obs_data_set_int(settings, "item_type", 0);
		obs_data_set_string(settings, "file_path", "");
		obs_data_set_string(settings, "image_file_path", "");
		obs_data_set_string(settings, "thumbnail_file_path", "");
		obs_source_update(source, settings);
		obs_data_release(settings);
	}
}

static bool isFileInDir(const QDir &dir, const QString &filePath)
{
	QFileInfo fi(filePath);
	QString absoluteFilePath = fi.absoluteFilePath();
	QString dirFilePath = dir.absoluteFilePath(fi.fileName());
	return dirFilePath == absoluteFilePath;
}
template<typename IsLast> int PLSAddMyResourcesProcessor::process(MotionData &md, QObject *sourceUi, const QString &file, IsLast isLast)
{
	MotionType type = MotionType::UNKOWN;

	QFileInfo fileInfo(file);
	if (!fileInfo.isFile()) {
		return FileNotFountError;
	}

	bool isGif = false;
	QString suffix = fileInfo.suffix().toLower();
	if (PLSMotionFileManager::instance()->isStaticImageTypeFile(suffix)) {
		type = MotionType::STATIC;
	} else if (PLSMotionFileManager::instance()->isVideoTypeFile(suffix, isGif)) {
		type = MotionType::MOTION;
	} else {
		return FileFormatError;
	}

	//open file failed
	media_info_t mi;
	if (!mi_open(&mi, file.toUtf8().constData(), MI_OPEN_DIRECTLY)) {
		return OpenFailedError;
	}

	if (isGif) { // check if there are too few frames
		const qint64 GIF_FOR_STATIC_MAX_FRAME_COUNT = 3;
		if (mi_get_int(&mi, "frame_count") <= GIF_FOR_STATIC_MAX_FRAME_COUNT) { // less 3 frames, as static image
			type = MotionType::STATIC;
		}
	} else if (type == MotionType::STATIC) {
		const qint64 GIF_FOR_STATIC_MAX_FRAME_COUNT = 3;
		if (mi_get_int(&mi, "frame_count") > GIF_FOR_STATIC_MAX_FRAME_COUNT) {
			type = MotionType::MOTION;
		}
	}

	//check resolution is valid
	qint64 width = mi_get_int(&mi, "width");
	qint64 height = mi_get_int(&mi, "height");
	if ((width * height) >= MAX_RESOLUTION_SIZE) {
		mi_free(&mi);
		return MaxResolutionError;
	}

	auto firstFrame = (mi_frame_t *)mi_get_obj(&mi, "first_frame_obj");
	if (!firstFrame) {
		mi_free(&mi);
		return GetMotionFirstFrameFailedError;
	}

	QString itemId = QUuid::createUuid().toString(QUuid::Id128);
	QString thumbnailPath;
	if (!saveImages(thumbnailPath, QImage((const uchar *)firstFrame->data, static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBX8888),
			QSize(static_cast<int>(width), static_cast<int>(height)), itemId, file)) {
		mi_free(&mi);
		return SaveImageFailedError;
	}

	mi_free(&mi);

	md.canDelete = true;
	md.title = fileInfo.fileName();
	md.dataType = DataType::MYLIST;
	md.itemId = itemId;
	md.type = type;
	md.resourcePath = file;
	md.thumbnailPath = md.staticImgPath = thumbnailPath;
	md.resourceState = md.thumbnailState = md.staticImgState = MotionState::LOCAL_FILE;
	md.foregroundState = md.foregroundStaticImgState = MotionState::NO_FILE;
	PLSMotionFileManager::instance()->updateThumbnailPixmap(itemId, QPixmap(thumbnailPath));
	emit addResourceFinished(sourceUi, md, isLast());

	return NoError;
}

bool PLSAddMyResourcesProcessor::saveImages(QString &thumbnailPath, const QImage &image, const QSize &imageSize, const QString &itemId, const QString &filePath) const
{
	double imageWHA = double(imageSize.width()) / double(imageSize.height());
	double thumbnailWHA = double(THUMBNAIL_WIDTH) / double(THUMBNAIL_HEIGHT);
	QImage thumbnail = (imageWHA < thumbnailWHA) ? image.scaledToWidth(THUMBNAIL_WIDTH) : image.scaledToWidth(THUMBNAIL_HEIGHT);
	thumbnailPath = pls_get_user_path(VIRTUAL_USER_PATH + itemId + "_thunmb.png");
	if (!thumbnail.save(thumbnailPath)) {
		PLS_ERROR(MOTION_FILE_MANAGER, "save thumbnail image failed. src = %s, dst = %s", filePath.toUtf8().constData(), thumbnailPath.toUtf8().constData());
		return false;
	}
	return true;
}

void PLSAddMyResourcesThread::process()
{
	while (running()) {
		std::map<QObject *, int> errors;

		DataType data;
		if (pop(data) && running()) {
			MotionData md;

			do {
				errors[data.first] |= processor()->process(md, data.first, data.second, [this, sourceUi = data.first]() { return isLast(sourceUi); });
			} while (pop(data) && running());
		}

		if (running() && !errors.empty()) {
			for (const auto &error : errors) {
				processor()->addResourcesFinished(error.first, error.second);
			}
		}
	}
}

void PLSResourcesThumbnailProcessor::process(PLSResourcesThumbnailProcessFinished *finished, const MotionData &md, bool properties) const
{
	QPixmap normalPixmap;
	QPixmap selectedPixmap;
	PLSMotionFileManager::instance()->getThumbnailPixmap(normalPixmap, selectedPixmap, md, properties);
	finished->processThumbnailFinished(thread(), md.itemId, normalPixmap, selectedPixmap);
}

void PLSResourcesThumbnailThread::process()
{
	while (running()) {
		DataType resource;
		if (pop(resource) && running()) {
			processor()->process(std::get<0>(resource), std::get<1>(resource), std::get<2>(resource));
		}
	}
}

void PLSDeleteMyResourcesProcessor::process(const QObject *, const MotionData &data) const
{
	QDir dir(pls_get_user_path(VIRTUAL_USER_PATH));
	if (isFileInDir(dir, data.thumbnailPath)) {
		PLS_DEBUG(MOTION_FILE_MANAGER, "remove thumbnail file: %s", data.thumbnailPath.toUtf8().constData());
		QFile::remove(data.thumbnailPath);
	}
	MotionFileManage->removeThumbnailPixmap(data.itemId);
}

void PLSDeleteResourcesThread::process()
{
	while (running()) {
		DataType data;
		while (pop(data) && running()) {
			processor()->process(data.first, data.second);
		}
	}
}

PLSMotionFileManager *PLSMotionFileManager::instance()
{
	static PLSMotionFileManager manager;
	return &manager;
}

PLSMotionFileManager::PLSMotionFileManager(QObject *parent) : QObject(parent)
{
	PLSMotionItemView::addBatchCache((getPrismList().count() + getFreeList().count()) * 2);
	PLSMotionItemView::addBatchCache((MAX_RECENT_COUNT + 20) * 2);

	addMyResourcesThread = pls_new<PLSAddMyResourcesThread>();
	connect(addMyResourcesThread->processor(), &PLSAddMyResourcesProcessor::addResourceFinished, this, &PLSMotionFileManager::addResourceFinished, Qt::QueuedConnection);
	connect(addMyResourcesThread->processor(), &PLSAddMyResourcesProcessor::addResourcesFinished, this, &PLSMotionFileManager::addResourcesFinished, Qt::QueuedConnection);
	addMyResourcesThread->startThread();

	resourcesThumbnailThread = pls_new<PLSResourcesThumbnailThread>();
	resourcesThumbnailThread->startThread();

	deleteAllMyResourcesThread = pls_new<PLSDeleteResourcesThread>();
	deleteAllMyResourcesThread->startThread();

	chooseFileDir = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();

	//check jsonObject is valid
	QJsonObject jsonObject;
	if (!getLocalMotionObject(jsonObject))
		return;

	//get virtual recent list data
	QJsonArray virtualArray = jsonObject.value(VIRTUAL_BACKGROUND_RECENT_LIST).toArray();
	getMotionListByJsonArray(virtualArray, m_virtualRecentList, DataType::VIRTUAL_RECENT);

//get virtual list category index
#if 0
	m_virtualCategoryIndex = jsonObject.value(VIRTUAL_CATEGORY_INDEX).toString();
#endif

	//get property list data
	QJsonArray propertyArray = jsonObject.value(PROPERTY_RECENT_LIST).toArray();
	getMotionListByJsonArray(propertyArray, m_propertyRecentList, DataType::PROP_RECENT);

//get property list category index
#if 0
	m_propertyCategoryIndex = jsonObject.value(PROPERTY_CATEGORY_INDEX).toString();
#endif

	//get my list data
	QJsonArray myListArray = jsonObject.value(MY_FILE_LIST).toArray();
	getMotionListByJsonArray(myListArray, m_myList, DataType::MYLIST);

	PLSMotionItemView::addBatchCache(m_myList.size() * 2);

	//init download thread
	QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
		saveMotionList();

		pls_delete(addMyResourcesThread, nullptr);
		pls_delete(resourcesThumbnailThread, nullptr);
		pls_delete(deleteAllMyResourcesThread, nullptr);
	});
}

bool PLSMotionFileManager::createMotionJsonFile() const
{
	//create userPath folder
	QString userPath = getUserPath(VIRTUAL_USER_PATH);
	QDir dir(userPath);
	if (!dir.exists())
		dir.mkpath(userPath);

	//create json file
	QString userFileName = getUserPath(VIRTUAL_USER_CACHE_JSON_FILE);
	QFile file(userFileName);
	if (!file.open(QIODevice::ReadWrite)) {
		return false;
	}
	QByteArray json = file.readAll();
	if (json.isEmpty()) {
		QJsonObject obj;
		QJsonArray propertyRecentList;
		QJsonArray myFileList;
		QJsonArray virtualRecentList;
		obj.insert(PROPERTY_RECENT_LIST, propertyRecentList);
		obj.insert(VIRTUAL_BACKGROUND_RECENT_LIST, virtualRecentList);
		obj.insert(MY_FILE_LIST, myFileList);
		QJsonDocument document;
		document.setObject(obj);
		file.write(document.toJson(QJsonDocument::Indented));
	}
	file.close();
	return true;
}

bool PLSMotionFileManager::getLocalMotionObject(QJsonObject &object) const
{
	//create file if file not exist
	if (!createMotionJsonFile())
		return false;

	//get json file data
	QString userFileName = getUserPath(VIRTUAL_USER_CACHE_JSON_FILE);
	QFile file(userFileName);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
		return false;
	}

	//read json file form local file
	QJsonParseError parseError;
	QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		file.close();
		return false;
	}

	//get json object
	object = document.object();
	file.close();
	return true;
}

QVariantList PLSMotionFileManager::getPrismList() const
{
	return getListForKey("PRISM");
}

QVariantList PLSMotionFileManager::getFreeList() const
{
	return getListForKey("FREE");
}

bool PLSMotionFileManager::isDownloadFileExist(const QString &filePath) const
{
#if defined(Q_OS_WIN)
	struct _stat64 s;
	if (_wstat64(filePath.toStdWString().c_str(), &s)) {
		return false;
	} else if (s.st_mode & S_IFREG) {
		return true;
	}
	return false;
#else
	return pls_file_is_existed(filePath);
#endif
}

bool PLSMotionFileManager::isValidMotionData(const MotionData &data, bool onlyCheckValues) const
{
	if (data.itemId.isEmpty()) {
		return false;
	} else if (data.type == MotionType::UNKOWN) {
		return false;
	} else if (data.title.isEmpty()) {
		return false;
	} else if (!isDownloadFileExist(data.resourcePath)) {
		return false;
	} else if (!isDownloadFileExist(data.thumbnailPath)) {
		return false;
	} else if (!data.staticImgPath.isEmpty() && (data.resourcePath != data.staticImgPath) && (data.thumbnailPath != data.staticImgPath) && !isDownloadFileExist(data.staticImgPath)) {
		return false;
	} else if (onlyCheckValues) {
		return true;
	} else if (PLSMotionNetwork::instance()->isPrismOrFree(data)) {
		return true;
	} else if (isMy(data.itemId)) {
		return true;
	}
	return false;
}

bool PLSMotionFileManager::insertMotionData(const MotionData &data, const QString &key)
{
	//local motion data can delete
	MotionData saveData = data;
	saveData.canDelete = true;

	//If property recent list view count greater 30
	if (key == PROPERTY_RECENT_LIST) {
		removeRepeatedMotionData(data, m_propertyRecentList);
		saveData.dataType = DataType::PROP_RECENT;
		m_propertyRecentList.insert(0, saveData);
		if (m_propertyRecentList.count() > MAX_RECENT_COUNT) {
			m_propertyRecentList.removeLast();
		}
	} else if (key == VIRTUAL_BACKGROUND_RECENT_LIST) {
		removeRepeatedMotionData(data, m_virtualRecentList);
		saveData.dataType = DataType::VIRTUAL_RECENT;
		m_virtualRecentList.insert(0, saveData);
		if (m_virtualRecentList.count() > MAX_RECENT_COUNT) {
			m_virtualRecentList.removeLast();
		}
	} else if (key == MY_FILE_LIST) {
		removeRepeatedMotionData(data, m_myList);
		saveData.dataType = DataType::MYLIST;
		m_myList.insert(0, saveData);
	}
	return true;
}

void PLSMotionFileManager::removeRepeatedMotionData(const MotionData &data, QList<MotionData> &list) const
{
	for (int i = 0, size = list.size(); i < size; i++) {
		MotionData motionData = list[i];
		if (motionData.itemId == data.itemId) {
			list.removeAt(i);
			break;
		}
	}
}

bool PLSMotionFileManager::deleteMotionData(QObject *sourceUi, const QString &itemId, const QString &key, bool isVbUsed, bool isSourceUsed)
{
	if (key == PROPERTY_RECENT_LIST) {
		deleteMotionListItem(itemId, m_propertyRecentList, false);
	} else if (key == VIRTUAL_BACKGROUND_RECENT_LIST) {
		deleteMotionListItem(itemId, m_virtualRecentList, false);
	} else if (key == MY_FILE_LIST) {
		deleteMotionListItem(itemId, m_propertyRecentList, false);
		deleteMotionListItem(itemId, m_virtualRecentList, false);
		deleteMotionListItem(itemId, m_myList, true);
		removeThumbnailPixmap(itemId);
		emit deleteResourceFinished(sourceUi, itemId, isVbUsed, isSourceUsed);
		sourceClearBackgroundTemplate({itemId});
		PLSVirtualBgManager::sourceIsDeleted(itemId);
	} else {
		return false;
	}

	saveMotionList();
	return true;
}

bool PLSMotionFileManager::copyFileToPrismPath(const QFileInfo &fileInfo, QString &fileUuid, QString &destPath) const
{
	QFile readFile(fileInfo.filePath());
	if (!readFile.open(QFile::ReadOnly)) {
		return false;
	}

	//get uuid string
	QUuid u = QUuid::createUuid();
	fileUuid = u.toString();
	destPath = getUserPath(VIRTUAL_USER_PATH + fileUuid + "." + fileInfo.suffix());
	QFile writeFile(destPath);
	if (!writeFile.open(QFile::WriteOnly)) {
		return false;
	}
	writeFile.write(readFile.readAll());
	return true;
}

QString PLSMotionFileManager::getPrismResourcePathByUUid(const QString &uuid, const QString &suffix) const
{
	return getUserPath(VIRTUAL_USER_PATH + uuid + "." + suffix);
}

QString PLSMotionFileManager::getPrismThumbnailPathByUUid(const QString &uuid, const QString &suffix) const
{
	return getUserPath(VIRTUAL_USER_PATH + uuid + "_thunmb" + "." + suffix);
}

QString PLSMotionFileManager::getPrismStaticImgPathByUUid(const QString &uuid, const QString &suffix) const
{
	return getUserPath(VIRTUAL_USER_PATH + uuid + "_static" + "." + suffix);
}

bool PLSMotionFileManager::isVideoTypeFile(const QString &suffix, bool &isGif) const
{
	isGif = suffix == "gif";
	if (isGif) {
		return true;
	}

	QStringList videoExtList;
	videoExtList << "mp4"
		     << "ts"
		     << "mov"
		     << "flv"
		     << "mkv"
		     << "avi"
		     << "webm";
	return videoExtList.contains(suffix);
}

bool PLSMotionFileManager::isStaticImageTypeFile(const QString &suffix) const
{
	QStringList imageExtList;
	imageExtList << "bmp"
		     << "tga"
		     << "png"
		     << "jpg"
		     << "psd"
		     << "jfif";
	return imageExtList.contains(suffix);
}

MotionType PLSMotionFileManager::motionTypeByString(const QString &type) const
{
	if (type == "motion") {
		return MotionType::MOTION;
	} else if (type == "static") {
		return MotionType::STATIC;
	}
	return MotionType::UNKOWN;
}

QString PLSMotionFileManager::getUserPath(const QString &subPath) const
{
	return pls_get_user_path(subPath);
}

QString PLSMotionFileManager::getFileNameByURL(const QString &url) const
{
	return url.split("/").last();
}

QString PLSMotionFileManager::getFilePathByURL(const MotionData &data, const QString &url) const
{
	return getUserPath(VIRTUAL_USER_PATH) + data.itemId + "/" + getFileNameByURL(url);
}

QVariantList PLSMotionFileManager::getListForKey(const QString &key) const
{
	QString path = getUserPath(VIRTUAL_SYNC_JSON_FILE);
	if (!QFile::exists(path)) {
		path = QApplication::applicationDirPath() + VIRTUAL_SYNC_JSON_FILE_DEFAULT;
	}

	//get local watermark file data
	QByteArray array;
	PLSJsonDataHandler::getJsonArrayFromFile(array, path);

	//get the sync server schemas data
	QVariantList list;
	PLSJsonDataHandler::getValuesFromByteArray(array, name2str(group), list);

	//get prism item list
	QVariantList prismList;
	for (auto item : list) {
		QJsonObject jsonObject = item.toJsonObject();
		QString groupId = jsonObject.value(name2str(groupId)).toString();
		if (groupId == key) {
			PLSJsonDataHandler::getValues(jsonObject, name2str(items), prismList);
		}
	}
	return prismList;
}

void PLSMotionFileManager::getMotionListByJsonArray(const QJsonArray &array, QList<MotionData> &list, DataType dataType) const
{
	for (const QJsonValue &it : array) {
		//Check QObject is Valid
		QJsonObject json = it.toObject();
		struct MotionData motionData;
		motionData.itemId = json.value(MOTION_ID_KEY).toString();
		motionData.title = json.value(MOTION_TITLE_KEY).toString();
		motionData.type = (MotionType)json.value(MOTION_TYPE_KEY).toInt();
		motionData.canDelete = json.value(MOTION_CAN_DELETE_KEY).toBool();
		motionData.resourcePath = json.value(RESOURCE_PATH_KEY).toString();
		motionData.thumbnailPath = json.value(THUMNAIL_PATH_KEY).toString();
		motionData.staticImgPath = json.value(STATIC_IMAGE_PATH_KEY).toString();
		motionData.dataType = dataType;
		list.append(motionData);
	}
}

void PLSMotionFileManager::deleteMotionListItem(const QString &itemId, QList<MotionData> &list, bool deleteFile) const
{
	for (int i = 0, size = list.size(); i < size; i++) {
		const MotionData &md = list[i];
		if (md.itemId != itemId) {
			continue;
		}

		if (!deleteFile) {
			list.removeAt(i);
			return;
		}

		MotionData data = md;
		list.removeAt(i);

		QDir dir(pls_get_user_path(VIRTUAL_USER_PATH));
		if (isFileInDir(dir, data.thumbnailPath)) {
			PLS_DEBUG(MOTION_FILE_MANAGER, "remove thumbnail file: %s", data.thumbnailPath.toUtf8().constData());
			QFile::remove(data.thumbnailPath);
		}
		return;
	}
}

void PLSMotionFileManager::deleteLocalFile(const QString &path) const
{
	QFile::remove(path);
}

QList<MotionData> &PLSMotionFileManager::getRecentMotionList(const QString &recentKey)
{
	if (recentKey == PROPERTY_RECENT_LIST) {
		return m_propertyRecentList;
	}
	return m_virtualRecentList;
}

QList<MotionData> &PLSMotionFileManager::getMyMotionList()
{
	return m_myList;
}

void PLSMotionFileManager::saveMotionList() const
{
	QJsonObject jsonObject;
	saveMotionListToLocalPath(MY_FILE_LIST, jsonObject, m_myList);
	saveMotionListToLocalPath(PROPERTY_RECENT_LIST, jsonObject, m_propertyRecentList);
	saveMotionListToLocalPath(VIRTUAL_BACKGROUND_RECENT_LIST, jsonObject, m_virtualRecentList);
#if 0
	if (m_propertyCategoryIndex != nullptr) {
		jsonObject.insert(PROPERTY_CATEGORY_INDEX, m_propertyCategoryIndex);
	}
	if (m_virtualCategoryIndex != nullptr) {
		jsonObject.insert(VIRTUAL_CATEGORY_INDEX, m_virtualCategoryIndex);
	}
#endif

	//convert json object to json string
	QJsonDocument writeDocument;
	writeDocument.setObject(jsonObject);
	QByteArray json = writeDocument.toJson(QJsonDocument::Indented);

	//get json file data
	QString userFileName = getUserPath(VIRTUAL_USER_CACHE_JSON_FILE);
	QFile file(userFileName);
	file.open(QFile::WriteOnly | QFile::Truncate);
	file.write(json);
	file.close();
}

void PLSMotionFileManager::saveCategoryIndex(int index, const QString &key)
{
	QString string = QString("%1").arg(index);
	if (key == PROPERTY_CATEGORY_INDEX) {
		m_propertyCategoryIndex = string;
	} else if (key == VIRTUAL_CATEGORY_INDEX) {
		m_virtualCategoryIndex = string;
	}
}

QString PLSMotionFileManager::categoryIndex(const QString &key) const
{
	if (key == PROPERTY_CATEGORY_INDEX) {
		return m_propertyCategoryIndex;
	} else if (key == VIRTUAL_CATEGORY_INDEX) {
		return m_virtualCategoryIndex;
	}
	return QString();
}

void PLSMotionFileManager::addMyResources(QObject *sourceUi, const QStringList &files)
{
	if (files.isEmpty()) {
		return;
	}

	chooseFileDir = QFileInfo(files.first()).dir().absolutePath();

	PLSMotionItemView::addBatchCache(files.size() * 2, true);
	addMyResourcesThread->push(sourceUi, files);
}

void PLSMotionFileManager::deleteAllMyResources(QObject *sourceUi)
{
	emit deleteMyResources();
	QStringList idList;
	for (const MotionData &data : m_myList) {
		removeAt(m_propertyRecentList, data.itemId);
		removeAt(m_virtualRecentList, data.itemId);
		deleteAllMyResourcesThread->push(sourceUi, data);
		idList.append(data.itemId);
	}
	PLSVirtualBgManager::sourceIsDeleted(nullptr, idList);
	sourceClearBackgroundTemplate(idList);
	m_myList.clear();
	saveMotionList();
}

bool PLSMotionFileManager::copyList(QList<MotionData> &copied, const QList<MotionData> &dst, const QList<MotionData> &src, QList<MotionData> *removed) const
{
	for (const MotionData &md : src) {
		if (isValidMotionData(md)) {
			copied.append(md);
		} else if (removed) {
			removed->append(md);
		}
	}

	if (copied.count() != dst.count()) {
		return true;
	}

	for (int i = 0, count = copied.count(); i < count; ++i) {
		if (copied[i].itemId != dst[i].itemId) {
			return true;
		}
	}
	return false;
}

bool PLSMotionFileManager::copyListForPrism(QList<MotionData> &copied, const QList<MotionData> &dst, const QList<MotionData> &src)
{
	for (const MotionData &md : src) {
		if (isValidMotionData(md) || hasThumbnailPixmap(md.itemId)) {
			copied.append(md);
		}
	}

	if (copied.count() != dst.count()) {
		return true;
	}

	for (int i = 0, count = copied.count(); i < count; ++i) {
		if (copied[i].itemId != dst[i].itemId) {
			return true;
		}
	}
	return false;
}

bool PLSMotionFileManager::isRemovedChanged(QList<MotionData> &needNotified, QList<MotionData> &removed, QList<MotionData> &dst) const
{
	bool changed = false;
	for (int i = 0, count = removed.count(); i < count; ++i) {
		const MotionData &md = removed[i];
		if (!removeAt(dst, md.itemId)) {
			needNotified.append(md);
			changed = true;
		}
	}
	return changed;
}

void PLSMotionFileManager::notifyCheckedRemoved(const QList<MotionData> &removed)
{
	QStringList itemIds;
	for (const MotionData &md : removed) {
		PLS_DEBUG(MOTION_FILE_MANAGER, "notify check resource removed, itemId: %s", md.itemId.toUtf8().constData());
		bool isVbUsed = false;
		bool isSourceUsed = false;
		PLSVirtualBgManager::checkResourceIsUsed(md.itemId, isVbUsed, isSourceUsed);
		checkedRemoved(md, isVbUsed, isSourceUsed);
		itemIds.append(md.itemId);
	}

	sourceClearBackgroundTemplate(itemIds);
}

void PLSMotionFileManager::redownloadResource(MotionData &md, bool update, const std::function<void(const MotionData &md)> &ok, const std::function<void(const MotionData &md)> &fail) const
{
	PLSMotionNetwork::instance()->reDownloadResourceFile(md, update, ok, fail);
}

QSvgRenderer *PLSMotionFileManager::getMotionFlagSvg()
{
	return &motionFlagSvg;
}

void PLSMotionFileManager::loadMotionFlagSvg()
{
	motionFlagSvg.load(QStringLiteral(":/resource/images/virtual/img-vbg-motion.svg"));
}

static bool loadPixmap(QPixmap &pixmap, const MotionData &md)
{
	if (md.thumbnailPath.isEmpty() || !QFile::exists(md.thumbnailPath)) {
		return false;
	} else if (!pixmap.load(md.thumbnailPath)) {
		return false;
	} else if (pixmap.isNull() || pixmap.size().isEmpty()) {
		return false;
	}
	return true;
}

static QPixmap scaledCrop(const QPixmap &original, const QSize &size)
{
	if ((double(original.width()) / double(original.height())) >= (double(size.width()) / double(size.height()))) {
		auto width = (int)(double(original.width()) * double(size.height()) / double(original.height()));
		int height = size.height();
		QPixmap scaled = original.scaledToHeight(height);
		return scaled.copy((width - size.width()) / 2, (height - size.height()) / 2, size.width(), size.height());
	} else {
		int width = size.width();
		auto height = (int)(double(original.height()) * double(size.width()) / double(original.width()));
		QPixmap scaled = original.scaledToWidth(width);
		return scaled.copy((width - size.width()) / 2, (height - size.height()) / 2, size.width(), size.height());
	}
}

static QPixmap roundedPixmap(const QPixmap &pixmap, int radius, bool properties)
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

	painter.fillRect(rect, properties ? QColor(39, 39, 39) : QColor(30, 30, 31));
	painter.drawPixmap(rect, pixmap);

	return image;
}

static QPixmap selectedRoundedPixmap(const QSize &size, const QPixmap &crop, const QMargins &margin, int radius)
{
	QPixmap image(size * 4);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

	QRect rect = image.rect();
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::yellow);
	painter.drawRoundedRect(rect, radius * 4, radius * 4);

	rect -= margin * 4;
	painter.drawPixmap(rect, crop);

	return image;
}

bool PLSMotionFileManager::hasThumbnailPixmap(const QString &itemId)
{
	QReadLocker readLocker(&thumbnailPixmapCacheRWLock);
	auto iter = thumbnailPixmapCache.find(itemId);
	if (iter == thumbnailPixmapCache.end()) {
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

bool PLSMotionFileManager::getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &selectedPixmap, const MotionData &md, bool properties)
{
	QSize size;
	QMargins margin;
	int radius = 0;
	getThumbnailPixmapSize(size, margin, radius, properties);
	if (!isThumbnailPixmapSizeValid(size) || (radius <= 0)) {
		normalPixmap = selectedPixmap = QPixmap();
		return false;
	}

	QPixmap original;
	if (getThumbnailPixmap(original, normalPixmap, selectedPixmap, md.itemId, size, properties)) {
		return true;
	}

	bool needLoadOriginal = original.isNull();
	if (needLoadOriginal && !loadPixmap(original, md)) {
		normalPixmap = selectedPixmap = QPixmap();
		return false;
	}

	QPixmap scaled = scaledCrop(original, size);
	normalPixmap = roundedPixmap(scaled, radius, properties);

	QPixmap crop = scaled.copy(scaled.rect() - margin);
	selectedPixmap = selectedRoundedPixmap(size, roundedPixmap(crop, radius, properties), margin, radius);

	QWriteLocker writeLocker(&thumbnailPixmapCacheRWLock);
	auto &pixmaps = thumbnailPixmapCache[md.itemId];
	if (needLoadOriginal) {
		std::get<0>(pixmaps) = original;
	}
	if (properties) {
		std::get<1>(pixmaps) = normalPixmap;
		std::get<2>(pixmaps) = selectedPixmap;
	} else {
		std::get<3>(pixmaps) = normalPixmap;
		std::get<4>(pixmaps) = selectedPixmap;
	}
	return true;
}

bool PLSMotionFileManager::getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &selectedPixmap, const QString &itemId, const QSize &size, bool properties)
{
	// itemId => (thumbnailPixmap, scaledThumbnailPixmapPropNormal, scaledThumbnailPixmapPropSelected, scaledThumbnailPixmapVbNormal, scaledThumbnailPixmapVbSelected)
	QReadLocker readLocker(&thumbnailPixmapCacheRWLock);
	auto iter = thumbnailPixmapCache.find(itemId);
	if (iter == thumbnailPixmapCache.end()) {
		return false;
	}

	auto &pixmaps = iter.value();
	const QPixmap &normal = properties ? std::get<1>(pixmaps) : std::get<3>(pixmaps);
	const QPixmap &selected = properties ? std::get<2>(pixmaps) : std::get<4>(pixmaps);
	if (!normal.isNull() && (normal.size() == size) && !selected.isNull() && (selected.size() == size)) {
		normalPixmap = normal;
		selectedPixmap = selected;
		return true;
	}
	return false;
}

bool PLSMotionFileManager::getThumbnailPixmap(QPixmap &original, QPixmap &normalPixmap, QPixmap &selectedPixmap, const QString &itemId, const QSize &size, bool properties)
{
	// itemId => (thumbnailPixmap, scaledThumbnailPixmapPropNormal, scaledThumbnailPixmapPropSelected, scaledThumbnailPixmapVbNormal, scaledThumbnailPixmapVbSelected)
	QReadLocker readLocker(&thumbnailPixmapCacheRWLock);
	auto iter = thumbnailPixmapCache.find(itemId);
	if (iter == thumbnailPixmapCache.end()) {
		return false;
	}

	auto &pixmaps = iter.value();
	original = std::get<0>(pixmaps);

	const QPixmap &normal = properties ? std::get<1>(pixmaps) : std::get<3>(pixmaps);
	const QPixmap &selected = properties ? std::get<2>(pixmaps) : std::get<4>(pixmaps);
	if (!normal.isNull() && (normal.size() == size) && !selected.isNull() && (selected.size() == size)) {
		normalPixmap = normal;
		selectedPixmap = selected;
		return true;
	}
	return false;
}

void PLSMotionFileManager::getThumbnailPixmapAsync(PLSResourcesThumbnailProcessFinished *itemView, const MotionData &md, const QSize &size, double dpi, bool properties)
{
	if (!itemView) {
		return;
	}

	int radius = 3;
	if (size.isEmpty() || (radius <= 0)) {
		itemView->processThumbnailFinished(thread(), md.itemId, QPixmap(), QPixmap());
		return;
	}

	QMargins margin = QMargins(2, 2, 2, 2);
	updateThumbnailPixmapSize(size, margin, radius, properties);

	QPixmap normalPixmap;
	QPixmap selectedPixmap;
	if (!getThumbnailPixmap(normalPixmap, selectedPixmap, md.itemId, size, properties)) {
		resourcesThumbnailThread->push(std::make_tuple(itemView, md, properties));
	} else {
		itemView->processThumbnailFinished(thread(), md.itemId, normalPixmap, selectedPixmap);
	}
}

void PLSMotionFileManager::updateThumbnailPixmap(const QString &itemId, const QPixmap &thumbnailPixmap)
{
	setThumbnailPixmap(itemId, thumbnailPixmap);
	if (thumbnailPixmap.isNull() || thumbnailPixmap.size().isEmpty()) {
		return;
	}

	QSize size;
	QMargins margin;
	int radius = 0;

	// Properties
	getThumbnailPixmapSize(size, margin, radius, true);
	if (isThumbnailPixmapSizeValid(size) && radius > 0) {
		QPixmap scaled = scaledCrop(thumbnailPixmap, size);
		QPixmap normal = roundedPixmap(scaled, radius, true);

		QPixmap crop = scaled.copy(scaled.rect() - margin);
		QPixmap selected = selectedRoundedPixmap(size, roundedPixmap(crop, radius, true), margin, radius);

		QWriteLocker writeLocker(&thumbnailPixmapCacheRWLock);
		auto &pixmaps = thumbnailPixmapCache[itemId];
		std::get<1>(pixmaps) = normal;
		std::get<2>(pixmaps) = selected;
	}

	// Virtual Background
	getThumbnailPixmapSize(size, margin, radius, false);
	if (isThumbnailPixmapSizeValid(size) && radius > 0) {
		QPixmap scaled = scaledCrop(thumbnailPixmap, size);
		QPixmap normal = roundedPixmap(scaled, radius, false);

		QPixmap crop = scaled.copy(scaled.rect() - margin);
		QPixmap selected = selectedRoundedPixmap(size, roundedPixmap(crop, radius, false), margin, radius);

		QWriteLocker writeLocker(&thumbnailPixmapCacheRWLock);
		auto &pixmaps = thumbnailPixmapCache[itemId];
		std::get<3>(pixmaps) = normal;
		std::get<4>(pixmaps) = selected;
	}
}

void PLSMotionFileManager::removeThumbnailPixmap(const QString &itemId)
{
	QWriteLocker writeLocker(&thumbnailPixmapCacheRWLock);
	// itemId => (thumbnailPixmap, scaledThumbnailPixmapPropNormal, scaledThumbnailPixmapPropSelected, scaledThumbnailPixmapVbNormal, scaledThumbnailPixmapVbSelected)
	thumbnailPixmapCache.remove(itemId);
}

void PLSMotionFileManager::updateThumbnailPixmapSize(const QSize &size, const QMargins &margin, int radius, bool properties)
{
	int index = properties ? 0 : 1; // // 0:Properties, 1:VirtualBackground
	QWriteLocker writeLocker(&thumbnailPixmapRWLock);
	thumbnailPixmapSize[index] = size;
	thumbnailPixmapMargin[index] = margin;
	thumbnailPixmapRadius[index] = radius;
}

void PLSMotionFileManager::getThumbnailPixmapSize(QSize &size, QMargins &margin, int &radius, bool properties)
{
	int index = properties ? 0 : 1; // // 0:Properties, 1:VirtualBackground
	QReadLocker readLocker(&thumbnailPixmapRWLock);
	size = thumbnailPixmapSize[index];
	margin = thumbnailPixmapMargin[index];
	radius = thumbnailPixmapRadius[index];
}

void PLSMotionFileManager::setThumbnailPixmap(const QString &itemId, const QPixmap &thumbnailPixmap)
{
	QWriteLocker writeLocker(&thumbnailPixmapCacheRWLock);
	// itemId => (thumbnailPixmap, scaledThumbnailPixmapPropNormal, scaledThumbnailPixmapPropSelected, scaledThumbnailPixmapVbNormal, scaledThumbnailPixmapVbSelected)
	thumbnailPixmapCache[itemId] = std::tuple<QPixmap, QPixmap, QPixmap, QPixmap, QPixmap>(thumbnailPixmap, QPixmap(), QPixmap(), QPixmap(), QPixmap());
}

bool PLSMotionFileManager::isMy(const QString &itemId) const
{
	return std::any_of(m_myList.begin(), m_myList.end(), [&itemId](const MotionData &md) { return md.itemId == itemId; });
}

bool PLSMotionFileManager::findMy(MotionData &md, const QString &itemId) const
{
	for (const MotionData &_md : m_myList) {
		if (_md.itemId == itemId) {
			md = _md;
			return true;
		}
	}
	return false;
}

bool PLSMotionFileManager::removeAt(QList<MotionData> &mds, const QString &itemId) const
{
	for (int i = 0, count = mds.count(); i < count; ++i) {
		if (mds[i].itemId == itemId) {
			mds.removeAt(i);
			return true;
		}
	}
	return false;
}

bool PLSMotionFileManager::findAndRemoveAt(MotionData &md, QList<MotionData> &mds, const QString &itemId) const
{
	for (int i = 0, count = mds.count(); i < count; ++i) {
		const auto &_md = mds[i];
		if (_md.itemId == itemId) {
			md = _md;
			mds.removeAt(i);
			return true;
		}
	}
	return false;
}

QString PLSMotionFileManager::getChooseFileDir() const
{
	return chooseFileDir;
}

void PLSMotionFileManager::logoutClear()
{
	QFile::remove(getUserPath(VIRTUAL_USER_CACHE_JSON_FILE));

	QDir dir(pls_get_user_path(VIRTUAL_USER_PATH));
	for (const MotionData &md : m_myList) {
		if (isFileInDir(dir, md.thumbnailPath)) {
			PLS_DEBUG(MOTION_FILE_MANAGER, "remove thumbnail file: %s", md.thumbnailPath.toUtf8().constData());
			QFile::remove(md.thumbnailPath);
		}
	}

	m_virtualRecentList.clear();
	m_propertyRecentList.clear();
	m_myList.clear();
}

void PLSMotionFileManager::saveMotionListToLocalPath(const QString &listTypeKey, QJsonObject &jsonObject, const QList<MotionData> &list) const
{
	//build new JsonObject
	QJsonArray jsonArray;
	for (const MotionData &data : list) {
		QJsonObject dataObject;
		dataObject.insert(MOTION_ID_KEY, data.itemId);
		dataObject.insert(MOTION_TITLE_KEY, data.title);
		dataObject.insert(MOTION_TYPE_KEY, (int)data.type);
		dataObject.insert(MOTION_CAN_DELETE_KEY, true);
		dataObject.insert(RESOURCE_PATH_KEY, data.resourcePath);
		dataObject.insert(THUMNAIL_PATH_KEY, data.thumbnailPath);
		dataObject.insert(STATIC_IMAGE_PATH_KEY, data.staticImgPath);
		dataObject.insert(FOREBACKGROUND_PATH_KEY, data.foregroundPath);
		dataObject.insert(FOREBACKGROUND_STATIC_IMAGE_PATH_KEY, data.foregroundStaticImgPath);
		jsonArray.append(dataObject);
	}
	jsonObject.insert(listTypeKey, jsonArray);
}
