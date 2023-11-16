#include "PLSStickerDataHandler.h"
#include "media-io/media-remux.h"
#include "pls-common-define.hpp"
#include "frontend-api/frontend-api.h"
#include "json-data-handler.hpp"
#include "liblog.h"
#include "utils-api.h"
#include "util/platform.h"
#include "log/module_names.h"
#include "giphy/GiphyDefine.h"
#include "PLSFileDownloader.h"
#include <pls/media-info.h>
#include <pls/pls-source.h>
#include <PLSResCommonFuns.h>
#include "PLSSyncServerManager.hpp"
#include <QFile>
#include <QDir>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

#include <stdio.h>
//#include "unzip.h"

using namespace common;
bool PLSStickerDataHandler::clearData = false;

// PRISM Sticker handler
PLSStickerDataHandler::PLSStickerDataHandler(QObject *parent) : QObject(parent) {}

bool PLSStickerDataHandler::HandleDownloadedFile(const QString &fileName) const
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

struct DownloadTask {
	uint64_t source_ptr = 0ULL;
	StickerData data;
};

static bool checkCallback(void *data, obs_source_t *source)
{
	const char *id = obs_source_get_id(source);
	auto ref = (std::vector<DownloadTask> *)(data);
	std::vector<DownloadTask> &tasks = *ref;
	if (id && *id) {
		if (0 != strcmp(id, PRISM_STICKER_SOURCE_ID))
			return true;
		obs_data_t *settings = obs_source_get_private_settings(source);
		const char *url = obs_data_get_string(settings, "resourceUrl");
		const char *resourceId = obs_data_get_string(settings, "resourceId");
		const char *category = obs_data_get_string(settings, "category");
		const char *landscapeVideo = obs_data_get_string(settings, "landscapeVideo");
		const char *portraitVideo = obs_data_get_string(settings, "portraitVideo");
#if 0
			const char *landscapeImage = obs_data_get_string(settings, "landscapeImage");
			const char *portraitImage = obs_data_get_string(settings, "portraitImage");
#endif
		qint64 version = obs_data_get_int(settings, "version");
		obs_data_release(settings);
		if (!url || !resourceId)
			return true;
		if (!QFile::exists(landscapeVideo) || !QFile::exists(portraitVideo)) {
			StickerData st_data;
			st_data.id = resourceId;
			st_data.category = category;
			st_data.resourceUrl = url;
			st_data.version = version;

			DownloadTask task;
			task.data = st_data;
			task.source_ptr = (uint64_t)(source);
			tasks.emplace_back(task);
		}
	}
	return true;
}

void downloadCallback(const DownloadTask &task, const TaskResponData &)
{
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
		pls_delete(wrapper, nullptr);
		return;
	}

	StickerHandleResult result;
	for (const auto &config : wrapper->m_config) {
		auto remuxedFile = path + config.resourceDirectory + ".mp4";
		auto resourcePath = path + config.resourceDirectory;
		if (!PLSStickerDataHandler::MediaRemux(resourcePath, remuxedFile, config.fps)) {
			PLS_WARN(MAIN_PRISM_STICKER, "Failed to remux sticker [%s]", task.data.id.toUtf8().constData());
			continue;
		}

		QString imageFile = PLSStickerDataHandler::getTargetImagePath(resourcePath, task.data.category, task.data.id, Orientation::landscape == config.orientation);
		if (Orientation::landscape == config.orientation) {
			result.landscapeVideoFile = remuxedFile;
			result.landscapeImage = imageFile; // TODO
		} else {
			result.portraitVideo = remuxedFile;
			result.portraitImage = imageFile; // TODO
		}
	}

	result.AdjustParam();
	PLS_INFO(MAIN_PRISM_STICKER, "re-download and uncompress sticker resource:'%s' successfully!", qUtf8Printable(task.data.id));
	OBSSource source = pls_get_source_by_pointer_address((void *)task.source_ptr);
	if (source) {
		OBSData settings = pls_get_source_private_setting(source);
		obs_data_set_string(settings, "landscapeVideo", qUtf8Printable(result.landscapeVideoFile));
		obs_data_set_string(settings, "landscapeImage", qUtf8Printable(result.landscapeImage));
		obs_data_set_string(settings, "portraitVideo", qUtf8Printable(result.portraitVideo));
		obs_data_set_string(settings, "portraitImage", qUtf8Printable(result.portraitImage));
		pls_source_set_private_data(source, settings);
	}

	pls_delete(wrapper, nullptr);
}

bool PLSStickerDataHandler::CheckStickerSource()
{
	return true;
}

bool PLSStickerDataHandler::MediaRemux(const QString &filePath, const QString &outputFileName, uint fps)
{
	return mi_remux_do(qUtf8Printable(filePath), qUtf8Printable(outputFileName), fps);
}

bool PLSStickerDataHandler::UnCompress(const QString &srcFile, const QString &dstPath, QString &)
{
	auto fileName = srcFile.mid(srcFile.lastIndexOf('/') + 1);
	auto prefix = fileName.left(fileName.indexOf("."));
	QDir dir(dstPath);
	bool ret = dir.rename(prefix, prefix + "_temp");
	if (PLSResCommonFuns::unZip(dstPath, srcFile, fileName)) {
		if (ret && dir.cd(prefix + "_temp")) {
			dir.removeRecursively();
		}
		return true;
	} else if (ret) {
		dir.rename(prefix + "_temp", prefix);
	}
	return false;
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
		wrapper = pls_new<RandowTouch3DParamWrapper>();
	else
		wrapper = pls_new<Touch2DStickerParamWrapper>();
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
	auto doc = QJsonDocument(cacheObj);
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

QString PLSStickerDataHandler::getTargetImagePath(QString resourcePath, QString category, QString id, bool landscape)
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
