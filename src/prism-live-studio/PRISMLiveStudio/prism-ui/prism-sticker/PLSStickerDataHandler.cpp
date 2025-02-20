#include "PLSStickerDataHandler.h"
#include "media-io/media-remux.h"
#include "pls-common-define.hpp"
#include "frontend-api/frontend-api.h"
#include "liblog.h"
#include "utils-api.h"
#include "util/platform.h"
#include "log/module_names.h"
#include "giphy/GiphyDefine.h"
#include "PLSFileDownloader.h"
#include <pls/media-info.h>
#include <pls/pls-source.h>
#include "PLSSyncServerManager.hpp"
#include "PrismStickerResourceMgr.h"
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

using DownloadTasks = std::map<QString, std::vector<obs_weak_source_t *>>;

static bool checkCallback(void *data, obs_source_t *source)
{
	const char *id = obs_source_get_id(source);
	auto tasks = (DownloadTasks *)(data);
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

			if (auto iter = tasks->find(resourceId);iter != tasks->end()) {
				iter->second.push_back(obs_source_get_weak_source(source));
			}else{
				std::vector<obs_weak_source_t *> data;
				data.emplace_back(obs_source_get_weak_source(source));
				tasks->emplace(resourceId, data);
			}
		}
	}
	return true;
}

void updateSourceSettings(const pls::rsm::Item &item, obs_source_t *source) 
{
	auto result = PLSStickerDataHandler::RemuxItemResource(item);
	if (!result.success) {
		PLS_WARN(MAIN_PRISM_STICKER, "%s: Failed to remux file: %s", __FUNCTION__, qUtf8Printable(item.itemId()));
		return;
	}
	PLS_INFO(MAIN_PRISM_STICKER, "re-download and uncompress sticker resource:'%s' successfully!", qUtf8Printable(item.itemId()));
	if (source) {
		OBSData settings = pls_get_source_private_setting(source);
		obs_data_set_string(settings, "landscapeVideo", qUtf8Printable(result.landscapeVideoFile));
		obs_data_set_string(settings, "landscapeImage", qUtf8Printable(result.landscapeImage));
		obs_data_set_string(settings, "portraitVideo", qUtf8Printable(result.portraitVideo));
		obs_data_set_string(settings, "portraitImage", qUtf8Printable(result.portraitImage));
		pls_source_set_private_data(source, settings);
	}
}

void onDownloadedItem(const pls::rsm::Item &item, DownloadTasks &tasks) 
{
	auto id = item.itemId();
	if (auto iter = tasks.find(id); iter != tasks.end()) {
		for (auto source_weak: iter->second) {
			auto source = OBSGetStrongRef(source_weak);
			if (source) {
				updateSourceSettings(item, source);
			}
		}
		tasks.erase(iter);
	}
}

bool PLSStickerDataHandler::CheckStickerSource()
{
	return true;
}

bool PLSStickerDataHandler::MediaRemux(const QString &filePath, const QString &outputFileName, uint fps)
{
	return mi_remux_do(qUtf8Printable(filePath), qUtf8Printable(outputFileName), fps);
}

bool PLSStickerDataHandler::UnCompress(const QString &srcFile, const QString &dstPath, QString &error)
{
	auto fileName = srcFile.mid(srcFile.lastIndexOf('/') + 1);
	auto prefix = fileName.left(fileName.indexOf("."));
	QDir dir(dstPath);
	bool ret = dir.rename(prefix, prefix + "_temp");
	if (pls::rsm::unzip(srcFile, dstPath, true, &error)) {
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
	if (!pls_read_data(data, fileName))
		return false;
	obj = QJsonDocument::fromJson(data).object();
	return true;
}

std::shared_ptr<StickerParamWrapper> PLSStickerDataHandler::CreateStickerParamWrapper(const QString &categoryId)
{
	std::shared_ptr<StickerParamWrapper> wrapper;
	if (categoryId == "RandomTouch")
		wrapper = std::make_shared<RandowTouch3DParamWrapper>();
	else
		wrapper = std::make_shared<Touch2DStickerParamWrapper>();
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
	if (pls_read_data(data, cacheFile)) {
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
	return pls_write_json(file, cacheObj);
}

bool PLSStickerDataHandler::ClearPrismStickerData()
{
	SetClearDataFlag(true);
	CategoryPrismSticker::instance()->removeAllUsedItems(RECENT_USED_GROUP_ID, false);
	return true;
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

StickerHandleResult PLSStickerDataHandler::RemuxItemResource(const pls::rsm::Item &item)
{
	StickerHandleResult result;
	result.data = StickerData(item);
	auto rules = item.urlAndHowSaves();
	if (rules.empty()) {
		assert(false);
		result.breakFlow = true;
		return result;
	}
		
	auto urlAndHowSave = rules.front();
	QFileInfo fileInfo(urlAndHowSave.savedFilePath());
	QDir dstDir = fileInfo.dir();

	QString baseName = QFileInfo(urlAndHowSave.url().fileName()).baseName();
	auto path = dstDir.absolutePath() + "/" + baseName + "/";
	auto configFile = path + baseName + ".json";

	auto groups = item.groups();
	if (groups.empty()) {
		assert(false);
		result.breakFlow = true;
		return result;
	}

	auto groupId = groups.front().groupId();
	auto wrapper = PLSStickerDataHandler::CreateStickerParamWrapper(groupId);
	if (!wrapper->Serialize(configFile)) {
		result.breakFlow = true;
		return result;
	}

	bool ok = true;
	for (const auto &config : wrapper->m_config) {
		auto remuxedFile = path + config.resourceDirectory + ".mp4";
		auto resourcePath = path + config.resourceDirectory;
		if (!QFile::exists(remuxedFile) && !PLSStickerDataHandler::MediaRemux(resourcePath, remuxedFile, config.fps)) {
			ok = false;
			PLS_WARN(MAIN_PRISM_STICKER, "Failed to remux file:%s.", qUtf8Printable(downloader::getFileName(remuxedFile)));
			continue;
		}

		QString imageFile = PLSStickerDataHandler::getTargetImagePath(resourcePath, groupId, item.itemId(), Orientation::landscape == config.orientation);

		if (Orientation::landscape == config.orientation) {
			result.landscapeVideoFile = remuxedFile;
			result.landscapeImage = imageFile; // TODO
		} else {
			result.portraitVideo = remuxedFile;
			result.portraitImage = imageFile; // TODO
		}
	}
	result.success = ok;
	result.AdjustParam();
	return result;
}
