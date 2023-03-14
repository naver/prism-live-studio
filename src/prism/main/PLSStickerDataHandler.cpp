#include "pls/media-info.h"
#include "PLSStickerDataHandler.h"
#include "media-io/media-remux.h"
#include "pls-common-define.hpp"
#include "frontend-api/frontend-api.h"
#include "json-data-handler.hpp"
#include "liblog.h"
#include "util/platform.h"
#include "log/module_names.h"
#include "GiphyDefine.h"
#include "PLSHttpApi/PLSFileDownloader.h"
#include <QFile>
#include <QDir>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <Windows.h>
#include <stdio.h>
#include "unzip.h"

#define UNIVERSAL_DOWNLOAD 0
bool PLSStickerDataHandler::clearData = false;

// PRISM Sticker handler
PLSStickerDataHandler::PLSStickerDataHandler(QObject *parent) : QObject(parent) {}

PLSStickerDataHandler::~PLSStickerDataHandler() {}

bool PLSStickerDataHandler::HandleDownloadedFile(const QString &fileName)
{
	QFile file(fileName);
	if (file.exists()) {
		auto targetPath = pls_get_user_path(PRISM_STICKER_USER_PATH);
		QString error;
		if (UnCompress(fileName, targetPath, error)) {
			// Delete zip files.
			if (!file.remove())
				PLS_INFO(MAIN_PRISM_STICKER, "Remove: %s failed", qUtf8Printable(fileName));
			return true;
		}
		PLS_WARN(MAIN_PRISM_STICKER, "HandleDownloadedFile: %s", qUtf8Printable(error));
	}
	return false;
}

bool PLSStickerDataHandler::CheckStickerSource()
{
	struct DownloadTask {
		uint64_t source_ptr = 0ull;
		StickerData data;
	};
	auto callback = [](void *data, obs_source_t *source) {
		const char *id = obs_source_get_id(source);
		std::vector<DownloadTask> &tasks = *reinterpret_cast<std::vector<DownloadTask> *>(data);
		if (id && *id) {
			if (0 != strcmp(id, PRISM_STICKER_SOURCE_ID))
				return true;
			obs_data_t *settings = obs_source_get_private_settings(source);
			const char *url = obs_data_get_string(settings, "resourceUrl");
			const char *id = obs_data_get_string(settings, "resourceId");
			const char *category = obs_data_get_string(settings, "category");
			const char *landscapeVideo = obs_data_get_string(settings, "landscapeVideo");
			const char *portraitVideo = obs_data_get_string(settings, "landscapeVideo");
			//const char *landscapeImage = obs_data_get_string(settings, "landscapeImage");
			//const char *portraitImage = obs_data_get_string(settings, "portraitImage");
			qint64 version = obs_data_get_int(settings, "version");
			obs_data_release(settings);
			if (!url || !id)
				return true;
			if (!os_is_file_exist(landscapeVideo) || !os_is_file_exist(portraitVideo) /*|| !os_is_file_exist(landscapeImage) || !os_is_file_exist(portraitImage)*/) {
				StickerData data;
				data.id = id;
				data.category = category;
				data.resourceUrl = url;
				data.version = version;

				DownloadTask task;
				task.data = data;
				task.source_ptr = reinterpret_cast<uint64_t>(source);
				tasks.emplace_back(task);
			}
		}
		return true;
	};
	std::vector<DownloadTask> tasks;
	obs_enum_sources(callback, &tasks);
	if (tasks.size() > 0) {
		PLS_INFO(MAIN_PRISM_STICKER, "There are %d sticker resource file missed, download it agin", tasks.size());
		for (const DownloadTask &task : tasks) {
			DownloadTaskData dowloadTask;
			dowloadTask.url = task.data.resourceUrl;
			dowloadTask.needRetry = true;
			dowloadTask.version = task.data.version;
			dowloadTask.outputFileName = task.data.id;
			dowloadTask.outputPath = pls_get_user_path(PRISM_STICKER_USER_PATH);
			dowloadTask.callback = [=](const TaskResponData &data) {
				if (data.resultType == ResultStatus::ERROR_OCCUR)
					return;
				auto zipFile = PLSStickerDataHandler::GetStickerResourceFile(task.data);
				QString error;
				QString targetPath = PLSStickerDataHandler::GetStickerResourceParentDir(task.data);
				if (!PLSStickerDataHandler::UnCompress(zipFile, targetPath, error))
					return;
				QFile file(zipFile);
				file.remove();
				auto path = PLSStickerDataHandler::GetStickerResourcePath(task.data);
				auto configFile = path + PLSStickerDataHandler::GetStickerConfigJsonFileName(task.data);
				StickerParamWrapper *wrapper = PLSStickerDataHandler::CreateStickerParamWrapper(task.data.category);
				if (!wrapper->Serialize(configFile)) {
					delete wrapper;
					wrapper = nullptr;
					return;
				}

				StickerHandleResult result;
				for (const auto &config : wrapper->m_config) {
					auto remuxedFile = path + config.resourceDirectory + ".mp4";
					auto resourcePath = path + config.resourceDirectory;
					PLSStickerDataHandler::MediaRemux(resourcePath, remuxedFile, config.fps);
				}

				PLS_INFO(MAIN_PRISM_STICKER, "re-download and uncompress sticker resource:'%s' successfully!", qUtf8Printable(task.data.id));
				OBSSource source = pls_get_source_by_pointer_address((void *)task.source_ptr);
				OBSData settings = pls_get_source_setting(source);
				obs_source_update(source, settings);

				delete wrapper;
				wrapper = nullptr;
			};
			PLSFileDownloader::instance()->Get(dowloadTask);
		}
	}

	return true;
}

bool PLSStickerDataHandler::MediaRemux(const QString &filePath, const QString &outputFileName, uint fps)
{
	return mi_remux_do(qUtf8Printable(filePath), qUtf8Printable(outputFileName), fps);
}

bool PLSStickerDataHandler::UnCompress(const QString &srcFile, const QString &dstPath, QString &error)
{
	auto fileName = srcFile.mid(srcFile.lastIndexOf('/') + 1);
	QFileInfo fileInfo(srcFile);
	if (!fileInfo.exists()) {
		error = QString("zip file name = %1 dose not exist").arg(fileName);
		return false;
	}

	auto fileSize = fileInfo.size();
	HZIP zip = OpenZip(srcFile.toStdWString().c_str(), 0);
	if (zip == 0) {
		error = QString("zip file name = %1 open zip error, size = %2 bytes").arg(fileName).arg(fileSize);
		return false;
	}

	// Remove directory first.
	auto prefix = fileName.left(fileName.indexOf("."));
	QDir dir(dstPath + prefix);
	dir.removeRecursively();

	ZRESULT result = SetUnzipBaseDir(zip, QString(dstPath).toStdWString().c_str());
	if (result != ZR_OK) {
		CloseZip(zip);
		error = QString("zip file name = %1 set unzip base error, size = %2 bytes").arg(fileName).arg(fileSize);
		return false;
	}

	ZIPENTRY zipEntry;
	result = GetZipItem(zip, -1, &zipEntry);
	if (result != ZR_OK) {
		CloseZip(zip);
		error = QString("zip file name = %1 get zip item error, size = %2 bytes").arg(fileName).arg(fileSize);
		return false;
	}

	int numitems = zipEntry.index;
	for (int i = 0; i < numitems; i++) {
		result = GetZipItem(zip, i, &zipEntry);
		result = UnzipItem(zip, i, zipEntry.name);
		if (result != ZR_OK) {
			continue;
		}
	}
	CloseZip(zip);
	return true;
}

bool PLSStickerDataHandler::ParseStickerParamJson(const QString &fileName, QJsonObject &obj)
{
	QByteArray data;
	if (!PLSJsonDataHandler::getJsonArrayFromFile(data, fileName))
		return false;
	obj = QJsonDocument::fromJson(data).object();
	return true;
}

StickerParamWrapper *PLSStickerDataHandler::CreateStickerParamWrapper(const QString &categoryId)
{
	StickerParamWrapper *wrapper = nullptr;
	if (categoryId == "RandomTouch")
		wrapper = new RandowTouch3DParamWrapper;
	else
		wrapper = new Touch2DStickerParamWrapper;
	return wrapper;
}

QString PLSStickerDataHandler::GetStickerConfigJsonFileName(const StickerData &data)
{
	QUrl resource(data.resourceUrl);
	QString name = resource.fileName();
	name = name.left(name.indexOf(".")) + ".json";
	return name;
}

QString PLSStickerDataHandler::GetStickerResourcePath(const StickerData &data)
{
	QUrl resource(data.resourceUrl);
	QString filePath = resource.fileName();
	filePath = pls_get_user_path(PRISM_STICKER_USER_PATH) + data.id + "/" + filePath.left(filePath.indexOf(".")) + "/";
	return filePath;
}

QString PLSStickerDataHandler::GetStickerResourceFile(const StickerData &data)
{
	QUrl resource(data.resourceUrl);
	QFileInfo info(resource.path());
	QString fileName = pls_get_user_path(PRISM_STICKER_USER_PATH) + data.id + "." + info.suffix();
	return fileName;
}

QString PLSStickerDataHandler::GetStickerResourceParentDir(const StickerData &data)
{
	QString fileName = pls_get_user_path(PRISM_STICKER_USER_PATH) + data.id;
	return fileName;
}

bool PLSStickerDataHandler::ReadDownloadCacheLocal(QJsonObject &cache)
{
	auto cacheFile = pls_get_user_path(PRISM_STICKER_DOWNLOAD_CACHE_FILE);
	// Get cache version.
	QByteArray data;
	if (PLSJsonDataHandler::getJsonArrayFromFile(data, cacheFile)) {
		cache = QJsonDocument::fromJson(data).object();
		return true;
	}
	return false;
}

bool PLSStickerDataHandler::WriteDownloadCache(const QString &key, qint64 version, QJsonObject &cacheObj)
{
	QJsonObject obj;
	obj.insert("id", key);
	obj.insert("version", version);
	cacheObj.insert(key, obj);
	return true;
}

bool PLSStickerDataHandler::WriteDownloadCacheToLocal(const QJsonObject &cacheObj)
{
	QString file(pls_get_user_path(PRISM_STICKER_DOWNLOAD_CACHE_FILE));
	QJsonDocument doc = QJsonDocument(cacheObj);
	return PLSJsonDataHandler::saveJsonFile(doc.toJson(), file);
}

bool PLSStickerDataHandler::ClearPrismStickerData()
{
	SetClearDataFlag(true);
	return QFile::remove(pls_get_user_path(QString(PRISM_STICKER_RECENT_JSON_FILE)));
}

void PLSStickerDataHandler::SetClearDataFlag(bool flag)
{
	clearData = flag;
}

bool PLSStickerDataHandler::GetClearDataFlag()
{
	return clearData;
}
