#include "CategoryVirtualTemplate.h"
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
#include "pls-common-define.hpp"
#include "PLSMotionItemView.h"
#include "frontend-api.h"
#include "pls-net-url.hpp"
#include "utils-api.h"
#include "libhttp-client.h"
#include "pls-gpop-data-struct.hpp"

using namespace common;
constexpr auto MODULE_MOTION_SERVICE = "MotionService";
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

bool CategoryVirtualTemplate::groupNeedDownload(pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const
{
	if (group.groupId() == PRISM_STR || group.groupId() == FREE_STR)
		return true;
	return false;
}

void CategoryVirtualTemplate::getCustomGroupExtras(qsizetype &pos, bool &archive, pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const
{
	ICategory::getCustomGroupExtras(pos, archive, mgr, group);
	archive = true;
}

void CategoryVirtualTemplate::getCustomItemExtras(qsizetype &pos, bool &archive, pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const
{
	ICategory::getCustomItemExtras(pos, archive, mgr, item);
	archive = true;
	pos = 0;
}

void CategoryVirtualTemplate::getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const
{
	urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave() //
					 .names({MOTION_RESOURCE_URL_KEY})
					 .fileName(pls::rsm::FileName::FromUrl)
					 .needDecompress(true)
					 .decompress([](const auto &, const auto &filePath) {
						 QDir dstDir = QFileInfo(filePath).dir();
						 return pls::rsm::unzip(filePath, dstDir.absolutePath(), false);
					 }));
	urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave() //
					 .names({MOTION_THUMBNAIL_URL_KEY})
					 .fileName(pls::rsm::FileName::FromUrl));
}

void CategoryVirtualTemplate::itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<pls::rsm::DownloadResult> &results) const
{
	MotionData data(item);
	if (ok) {
		pls_async_call_mt([data]() { PLSMotionFileManager::instance()->updateThumbnailPixmap(data.itemId, QPixmap(data.thumbnailPath)); });
		emit resourceDownloadFinished(data);
	}

	PLS_INFO(moduleName(), "endDownloadItem virtual_bg %s %s", item.itemId().toUtf8().constData(), ok ? "ok" : "failed");
}

void CategoryVirtualTemplate::groupDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Group group, bool ok, const std::list<pls::rsm::DownloadResult> &results) const
{
	emit groupListFinishedSignal();
	PLS_INFO(moduleName(), "groupDownloaded virtual_bg %s %s", group.groupId().toUtf8().constData(), ok ? "ok" : "failed");
}

bool CategoryVirtualTemplate::groupNeedLoad(pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const
{
	if (group.groupId() == PRISM_STR || group.groupId() == FREE_STR)
		return true;
	return false;
}

bool CategoryVirtualTemplate::checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const
{
	MotionData data(item);
	return data.checkResourceCached();
}

void CategoryVirtualTemplate::allDownload(pls::rsm::IResourceManager *mgr, bool ok)
{
	emit allDownloadFinished(ok);
}

size_t CategoryVirtualTemplate::useMaxCount(pls::rsm::IResourceManager *mgr) const
{
	return MAX_RECENT_COUNT;
}

QList<MotionData> CategoryVirtualTemplate::getGroupList(const QString &groupId) const
{
	if (groupId == RECENT_STR) {
		return getRecentList();
	}

	for (const auto &group : getGroups()) {
		if (group.groupId() != groupId) {
			continue;
		}
		std::list<pls::rsm::Item> items = group.items();
		QList<MotionData> motionItems;
		for (const auto &item : items) {
			MotionData data(item);
			motionItems.push_back(data);
		}
		return motionItems;
	}
	return {};
}

void CategoryVirtualTemplate::removeAllCustomGroups()
{
	auto groups = getGroups();
	for (auto group : groups) {
		if (!group.isCustom()) {
			continue;
		}
		removeCustomItems(group.groupId());
	}
}

QList<MotionData> CategoryVirtualTemplate::getPrismList() const
{
	return getGroupList(PRISM_STR);
}

QList<MotionData> CategoryVirtualTemplate::getRecentList() const
{
	QList<MotionData> list;
	auto items = CategoryVirtualTemplateInstance->getUsedItems(RECENT_STR);
	for (const auto &item : items) {
		MotionData motionData(item);
		motionData.canDelete = true;
		motionData.groupId = RECENT_STR;
		list.append(motionData);
	}
	return list;
}

QList<MotionData> CategoryVirtualTemplate::getMyList()
{
	return getGroupList(MY_STR);
}

MotionData CategoryVirtualTemplate::getMotionDataById(const QString &itemId)
{
	std::list<pls::rsm::Item> items;
	for (const auto &group : getGroups()) {
		std::list<pls::rsm::Item> items = group.items();
		for (const auto &item : items) {
			MotionData data(item);
			if (data.itemId == itemId) {
				return data;
			}
		}
	}
	return MotionData();
}

bool CategoryVirtualTemplate::isPathEqual(const MotionData &md1, const MotionData &md2) const
{
	if ((md1.itemId == md2.itemId) && (md1.resourcePath == md2.resourcePath) && (md1.staticImgPath == md2.staticImgPath) && (md1.thumbnailPath == md2.thumbnailPath)) {
		return true;
	}
	return false;
}

bool CategoryVirtualTemplate::groupDownloadRequestFinished(const QString &group)
{
	pls::rsm::State state = getGroupState(group);
	return (state == pls::rsm::State::Ok || state == pls::rsm::State::Failed);
}

QList<MotionData> CategoryVirtualTemplate::getFreeList() const
{
	return getGroupList(FREE_STR);
}