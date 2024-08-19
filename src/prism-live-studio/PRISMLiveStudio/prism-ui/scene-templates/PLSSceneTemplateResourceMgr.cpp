#include "PLSSceneTemplateResourceMgr.h"

#include <QApplication>
#include <QDir>

#include "PLSCommonConst.h"
#include "frontend-api.h"
#include "pls-net-url.hpp"
#include "libhttp-client.h"
#include "PLSResCommonFuns.h"
#include "PLSResourceManager.h"
#include "pls-common-define.hpp"
#include "log/log.h"

using namespace common;
constexpr auto DOWNLOAD_SCENE_TEMPLATE_TIMEOUT_MS = 180000;

constexpr auto Config_Json = "config.json";
constexpr auto Main_Scene_Image_Name = "MainSceneImage";
constexpr auto Main_Scene_Video_Name = "MainSceneVideo";
constexpr auto Main_Scene_Thumbnail_1_Name = "MainSceneThumbnail_1";
constexpr auto Main_Scene_Thumbnail_2_Name = "MainSceneThumbnail_2";
constexpr auto Detail_Scene_Thumbnail_Name = "DetailSceneThumbnail_";

using namespace std;

PLSSceneTemplateResourceMgr &PLSSceneTemplateResourceMgr::instance()
{
	static PLSSceneTemplateResourceMgr *_instance = nullptr;
	if (nullptr == _instance) {
		_instance = new PLSSceneTemplateResourceMgr;
	}

	return *_instance;
}

PLSSceneTemplateResourceMgr::PLSSceneTemplateResourceMgr(QObject *parent) : QObject(parent) {}

PLSSceneTemplateResourceMgr::~PLSSceneTemplateResourceMgr()
{
	PLS_WARN(SCENE_TEMPLATE, "%p: invoke %s", this, __FUNCTION__);
}

const SceneTemplateList &PLSSceneTemplateResourceMgr::getSceneTemplateList() const
{
	return m_listSceneTemplate;
}

const QList<SceneTemplateItem> &PLSSceneTemplateResourceMgr::getSceneTemplateGroup(const QString &groupId) const
{
	static QList<SceneTemplateItem> temp = {};

	for (auto &group : m_listSceneTemplate.groups) {
		if (groupId == group.groupId) {
			return group.items;
		}
	}
	return temp;
}

QList<SceneTemplateItem> PLSSceneTemplateResourceMgr::getSceneTemplateValidGroup(const QString &groupId) const
{
	auto temp = getSceneTemplateGroup(groupId);
	temp.removeIf([this](auto &item) { return !isItemValid(item); });

	return temp;
}

const SceneTemplateItem &PLSSceneTemplateResourceMgr::getSceneTemplateItem(const QString &itemId) const
{
	static SceneTemplateItem temp = {};

	for (auto &group : m_listSceneTemplate.groups) {
		for (auto &item : group.items) {
			if (itemId == item.itemId) {
				return item;
			}
		}
	}
	return temp;
}

QStringList PLSSceneTemplateResourceMgr::getGroupIdList()
{
	QStringList list;
	for (auto &group : m_listSceneTemplate.groups) {
		list.append(group.groupId);
	}
	return list;
}

bool PLSSceneTemplateResourceMgr::isListFinished() const
{
	return m_bListFinished;
}

bool PLSSceneTemplateResourceMgr::isItemsFinished() const
{
	return m_bItemsFinished;
}

bool PLSSceneTemplateResourceMgr::isDownloadingFinished() const
{
	return 0 == m_iDownloadingCount;
}

void PLSSceneTemplateResourceMgr::downloadList(const QString &categoryPath)
{
	downloadList();
}

void PLSSceneTemplateResourceMgr::downloadList()
{
}

void PLSSceneTemplateResourceMgr::downloadItems()
{
	if (0 != m_iDownloadingCount) {
		return;
	}

	m_bItemsFinished = false;
	if (parseJson()) {
		for (auto &group : m_listSceneTemplate.groups) {
			for (auto &item : group.items) {
				findResource(item);
				if (m_iListVersion == m_listSceneTemplate.version && m_mapItemVersion[item.itemId] == item.version && isItemValid(item)) {
					continue;
				}

				downloadItem(item);
			}
		}
		m_iListVersion = m_listSceneTemplate.version;
	}

	if (0 == m_iDownloadingCount) {
		PLS_INFO(SCENE_TEMPLATE, "items finished");
		m_bItemsFinished = true;
	}
	emit onItemsFinished();
}

void PLSSceneTemplateResourceMgr::downloadItem(SceneTemplateItem &item)
{
	if (item.resourceUrl.isEmpty()) {
		PLS_WARN(SCENE_TEMPLATE, "item: %s resourceUrl is empty", qUtf8Printable(item.itemId));
		return;
	}

	if (m_setDownloadingItem.contains(item.itemId)) {
		m_setDownloadingItem[item.itemId].append(&item);
		return;
	} else {
		m_setDownloadingItem.insert(item.itemId, QList<SceneTemplateItem *>());
	}

	PLS_INFO(SCENE_TEMPLATE, "To download item: %s", qUtf8Printable(item.itemId));

	++m_iDownloadingCount;
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .hmacUrl(item.resourceUrl, PLS_PC_HMAC_KEY.toUtf8())
				   .forDownload(true)
				   .saveFilePath(QDir(pls_get_user_path(SCENE_TEMPLATE_DIR)).absoluteFilePath(item.itemId) + "/" + item.resourceUrl.split("/").last())
				   .timeout(DOWNLOAD_SCENE_TEMPLATE_TIMEOUT_MS)
				   .receiver(this)
				   .workInNewThread()
				   .allowAbort(true)
				   .result([self = QPointer(this), &item](const pls::http::Reply &reply) {
					   auto itemId = item.itemId;
					   PLS_INFO(SCENE_TEMPLATE, "enter download item: %s finished", qUtf8Printable(itemId));

					   bool bSuccess = false;
					   if (reply.isOk()) {
						   PLS_INFO(SCENE_TEMPLATE, "download item: %s successful", qUtf8Printable(itemId));
						   if (PLSResCommonFuns::unZip(QDir(pls_get_user_path(SCENE_TEMPLATE_DIR)).absoluteFilePath(itemId), reply.downloadFilePath(),
									       QFileInfo(reply.downloadFilePath()).fileName(), false)) {
							   PLS_INFO(SCENE_TEMPLATE, "unzip item: %s successful", qUtf8Printable(itemId));

							   bSuccess = true;
						   } else {
							   PLS_WARN(SCENE_TEMPLATE, "unzip item: %s failed", qUtf8Printable(itemId));
						   }
					   } else {
						   PLS_WARN(SCENE_TEMPLATE, "download item: %s failed", qUtf8Printable(itemId));
					   }

					   if (self) {
						   pls_async_call_mt(self.data(), [self, bSuccess, &item] { self->onItemDownloaded(item, bSuccess); });
					   } else {
						   PLS_INFO(SCENE_TEMPLATE, "PLSSceneTemplateResourceMgr have been deleted, don't invoke onItemDownloaded");
					   }

					   PLS_INFO(SCENE_TEMPLATE, "leave download item: %s finished", qUtf8Printable(itemId));
				   }));
}

void PLSSceneTemplateResourceMgr::onItemDownloaded(SceneTemplateItem &item, bool bSuccess)
{
	PLS_INFO(SCENE_TEMPLATE, "%s: %s download %d", __FUNCTION__, qUtf8Printable(item.itemId), bSuccess);

	if (bSuccess) {
		m_mapItemVersion[item.itemId] = item.version;
	}

	auto listItems = m_setDownloadingItem[item.itemId];
	m_setDownloadingItem.remove(item.itemId);

	findResource(item);
	if (isItemValid(item)) {
		emit onItemFinished(item);
		for (auto pItem : listItems) {
			findResource(*pItem);
			emit onItemFinished(*pItem);
		}
	}

	if (0 == --m_iDownloadingCount) {
		PLS_INFO(SCENE_TEMPLATE, "items finished");

		m_bItemsFinished = true;
		emit onItemsFinished();
	}
}

bool PLSSceneTemplateResourceMgr::parseJson()
{
	if (!m_listSceneTemplate.groups.isEmpty()) {
		return true;
	}

	bool bResult = false;
	QJsonObject root;
	if (pls_read_json(root, m_strJsonPath)) {
		SceneTemplateList listSceneTemplate = {};

		listSceneTemplate.version = root["version"].toInt();

		auto plsVersion = QVersionNumber(pls_get_prism_version_major(), pls_get_prism_version_minor(), pls_get_prism_version_patch());

		auto groupArray = root["group"].toArray();
		for (auto groupItem : groupArray) {
			SceneTemplateGroup dataGroup = {};
			dataGroup.groupId = groupItem.toObject()["groupId"].toString();

			auto itemsArray = groupItem.toObject()["items"].toArray();
			for (auto item : itemsArray) {
				auto itemObject = item.toObject();
				auto itemProperties = itemObject["properties"].toObject();
				SceneTemplateItem dataItem = {};

				dataItem.groupId = dataGroup.groupId;
				dataItem.itemId = itemObject["itemId"].toString();

				auto versionLimit = itemProperties["versionLimit"].toString();
				if (!versionLimit.isEmpty()) {
					auto bMatch = pls_check_version(versionLimit.toUtf8(), plsVersion);
					if (!bMatch.has_value()) {
						PLS_WARN(SCENE_TEMPLATE, "item: %s, version: %s is a wrong format", qUtf8Printable(dataItem.itemId), qUtf8Printable(versionLimit));
						continue;
					} else if (!bMatch.value()) {
						continue;
					}
				}

				dataItem.resourcePath = QDir(pls_get_user_path(SCENE_TEMPLATE_DIR)).absoluteFilePath(dataItem.itemId);
				dataItem.version = itemObject["version"].toInt();
				dataItem.scenesNumber = itemProperties["scenesNumber"].toInt();
				dataItem.width = itemProperties["width"].toInt();
				dataItem.height = itemProperties["height"].toInt();
				dataItem.title = itemProperties["title"].toString();
				dataItem.resourceUrl = itemObject["resourceUrl"].toString();
				dataGroup.items.append(dataItem);
			}

			listSceneTemplate.groups.append(dataGroup);
		}

		bResult = true;
		m_listSceneTemplate = listSceneTemplate;
	} else {
		PLS_WARN(SCENE_TEMPLATE, "Can't read json file");

		bResult = false;
	}

	m_bListFinished = true;
	emit onListFinished(m_listSceneTemplate);

	return bResult;
}

void PLSSceneTemplateResourceMgr::loadFileVersion(const QString &path)
{
	QJsonObject root;
	if (pls_read_json(root, path)) {
		m_iListVersion = root["version"].toInt();

		auto groupArray = root["group"].toArray();
		for (auto groupItem : groupArray) {
			auto itemsArray = groupItem.toObject()["items"].toArray();
			for (auto item : itemsArray) {
				auto itemObject = item.toObject();

				m_mapItemVersion[itemObject["itemId"].toString()] = itemObject["version"].toInt();
			}
		}
	}
}

void PLSSceneTemplateResourceMgr::findResource(SceneTemplateItem &item)
{
	if (!item.resource.MainSceneImagePath.isEmpty() && !item.resource.MainSceneThumbnail_1.isEmpty() && !item.resource.MainSceneThumbnail_2.isEmpty()) {
		return;
	}

	QDir dir(item.resourcePath);
	if (!dir.exists(Config_Json)) {
		dir = dir.filePath(QFileInfo(item.resourceUrl).baseName());
		if (dir.exists(Config_Json)) {
			item.resourcePath = dir.path();
		}
	}

	QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

	item.resource.MainSceneImagePath.clear();
	item.resource.MainSceneVideoPath.clear();
	item.resource.MainSceneThumbnail_1.clear();
	item.resource.MainSceneThumbnail_2.clear();
	item.resource.DetailSceneThumbnailPathList.clear();
	for (auto &fileInfo : fileInfoList) {
		QString fileName = fileInfo.baseName();

		if (fileName == Main_Scene_Image_Name) {
			item.resource.MainSceneImagePath = fileInfo.absoluteFilePath();
		} else if (fileName == Main_Scene_Video_Name) {
			item.resource.MainSceneVideoPath = fileInfo.absoluteFilePath();
		} else if (fileName == Main_Scene_Thumbnail_1_Name) {
			item.resource.MainSceneThumbnail_1 = fileInfo.absoluteFilePath();
		} else if (fileName == Main_Scene_Thumbnail_2_Name) {
			item.resource.MainSceneThumbnail_2 = fileInfo.absoluteFilePath();
		} else if (fileName.startsWith(Detail_Scene_Thumbnail_Name)) {
			item.resource.DetailSceneThumbnailPathList.append(fileInfo.absoluteFilePath());
		}
	}
}

bool PLSSceneTemplateResourceMgr::isListValid() const
{
	return !m_listSceneTemplate.groups.isEmpty();
}

bool PLSSceneTemplateResourceMgr::isItemValid(const SceneTemplateItem &item) const
{
	QDir dir(item.resourcePath);

	return (!item.resource.MainSceneImagePath.isEmpty() && dir.exists(item.resource.MainSceneImagePath)) &&
	       (!item.resource.MainSceneThumbnail_1.isEmpty() && dir.exists(item.resource.MainSceneThumbnail_1)) &&
	       (!item.resource.MainSceneThumbnail_2.isEmpty() && dir.exists(item.resource.MainSceneThumbnail_2)) && dir.exists(Config_Json);
}

bool PLSSceneTemplateResourceMgr::isItemsValid(const QString &groupId) const
{
	for (auto &group : m_listSceneTemplate.groups) {
		if (groupId != group.groupId) {
			continue;
		}
		for (auto &item : group.items) {
			if (m_iListVersion == m_listSceneTemplate.version && m_mapItemVersion[item.itemId] == item.version && isItemValid(item)) {
				return true;
			}
		}
	}

	return false;
}
