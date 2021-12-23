#include "PLSStickerResHandler.h"

PLSStickerResHandler::PLSStickerResHandler(const QStringList &resUrls, QObject *parent) : PLSResourceHandler(resUrls, parent) {}

PLSStickerResHandler::~PLSStickerResHandler() {}

void PLSStickerResHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString & /*resPath*/)
{
	switch (downMode) {
	case DownLoadResourceMode::All: {

		PLS_INFO(MAINFILTER_MODULE, "Start download PRISM sticker resource");
		QMutexLocker locker(&cache_mutex);
		auto cacheFile = pls_get_user_path(PRISM_STICKER_DOWNLOAD_CACHE_FILE);
		cachesVersion.clear();
		// Get cache version.
		QByteArray cache;
		if (PLSJsonDataHandler::getJsonArrayFromFile(cache, cacheFile)) {
			QJsonObject obj = QJsonDocument::fromJson(cache).object();
			if (!obj.isEmpty()) {
				for (auto iter = obj.begin(); iter != obj.end(); ++iter) {
					auto objInfo = iter.value().toObject();
					CacheData info;
					info.id = iter.key();
					info.version = objInfo.value("version").toInt();
					cachesVersion.insert(iter.key(), info);
				}
			}
		}

		// Save it to local file.
		auto fileName = pls_get_user_path(PRISM_STICKER_JSON_FILE);
		if (!PLSJsonDataHandler::saveJsonFile(data, fileName)) {
			PLS_WARN(MAIN_PRISM_STICKER, "save reaction.json file failed.");
			return;
		}

		auto checkeVersion = [=](const QJsonArray &array) {
			auto iter = array.constBegin();
			while (iter != array.constEnd()) {
				auto item = iter->toObject();
				bool hasUseSenseTime = item.value("properties").toObject().value("reaction").toObject().value("itemInfo").toObject().value("hasUseSenseTime").toBool();
				if (hasUseSenseTime) {
					iter++;
					continue;
				}
				qint64 newVersion = item.value("version").toInt(-1);
				auto itemId = item.value("itemId").toString();
				bool findVersion = (cachesVersion.find(itemId) != cachesVersion.end());

				QUrl resourceUrl(item.value("resourceUrl").toString());
				auto resourcePath = pls_get_user_path(PRISM_STICKER_USER_PATH) + resourceUrl.fileName().left(resourceUrl.fileName().indexOf("."));

				bool needDelete = true;
				if (findVersion) {
					if (cachesVersion[itemId].version == newVersion) {
						needDelete = false;
					}
				}

				if (needDelete) {
					// Remove old resource files.
					QDir dir(resourcePath);
					dir.removeRecursively();
				}
				iter++;
			}
		};

		// Check version one by one.
		auto obj = QJsonDocument::fromJson(data).object();
		if (!obj.isEmpty()) {
			if (obj.contains("group") && obj.value("group").isArray()) {
				auto groups = obj.value("group").toArray();
				auto iter = groups.constBegin();
				while (iter != groups.constEnd()) {
					auto group = iter->toObject();
					auto items = group.value("items").toArray();
					if (items.size() > 0) {
						checkeVersion(items);
					}
					iter++;
				}
			}
		}
		break;
	}
	default:
		break;
	}
}

bool PLSStickerResHandler::saveResource(const QByteArray &data, const QString &filePath)
{
	QFile file(filePath);
	if (!file.open(QFile::WriteOnly)) {
		PLS_ERROR(MAIN_PRISM_STICKER, "save file to: %s failed, reason: %s", qUtf8Printable(file.fileName()), qUtf8Printable(file.errorString()));
		return false;
	}
	file.write(data);
	file.close();
	return true;
}

void PLSStickerResHandler::updateDownloadCache(const QString &id, qint64 newVerison)
{
	CacheData data;
	data.id = id;
	data.version = newVerison;
	cache_mutex.lock();
	cachesVersion[id] = data;
	cache_mutex.unlock();
}

bool PLSStickerResHandler::CacheDataToLocalJson()
{
	QFile file(pls_get_user_path(PRISM_STICKER_DOWNLOAD_CACHE_FILE));
	if (!file.open(QIODevice::WriteOnly))
		return false;

	QJsonObject cacheObj;
	cache_mutex.lock();
	auto iter = cachesVersion.begin();
	while (iter != cachesVersion.end()) {
		CacheData data = iter.value();
		QJsonObject info;
		info.insert("id", data.id);
		info.insert("version", data.version);
		cacheObj.insert(iter.key(), info);
		iter++;
	}
	cache_mutex.unlock();

	QByteArray data = QJsonDocument(cacheObj).toJson();
	file.write(data);
	file.close();

	return true;
}
