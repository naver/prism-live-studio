#include "PLSSceneTemplateResourceMgr.h"

#include <QDir>

#include "log/log.h"

constexpr auto Config_Json = "config.json";
constexpr auto Main_Scene_Image_Name = "MainSceneImage";
constexpr auto Main_Scene_Video_Name = "MainSceneVideo";
constexpr auto Main_Scene_Thumbnail_1_Name = "MainSceneThumbnail_1";
constexpr auto Main_Scene_Thumbnail_2_Name = "MainSceneThumbnail_2";
constexpr auto Detail_Scene_Thumbnail_Name = "DetailSceneThumbnail_";

using namespace std;

void CategorySceneTemplate::jsonLoaded(pls::rsm::IResourceManager *mgr, pls::rsm::Category category)
{
	emit onJsonDownloaded();
}

void CategorySceneTemplate::getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const
{
	PLS_INFO(moduleName(), "%p-%s: item=%s", this, __FUNCTION__, qUtf8Printable(item.itemId()));

	urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave() //
					 .names({QStringLiteral("resourceUrl")})
					 .fileName(pls::rsm::FileName::FromUrl)
					 .needDecompress(true)
					 .decompress([](const auto &, const auto &filePath) {
						 QDir dstDir = QFileInfo(filePath).dir();
						 return pls::rsm::unzip(filePath, dstDir.absolutePath());
					 }));
}
void CategorySceneTemplate::itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<pls::rsm::DownloadResult> &results) const
{
	PLS_INFO(moduleName(), "%p-%s: item=%s, state=%s", this, __FUNCTION__, qUtf8Printable(item.itemId()), ok ? "ok" : "failed");

	if (ok) {
		SceneTemplateItem _item(item);

		findResource(_item);

		emit onItemDownloaded(_item);
	}
}

void CategorySceneTemplate::groupDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Group group, bool ok, const std::list<pls::rsm::DownloadResult> &results) const
{
	if (!ok) {
		emit onGroupDownloadFailed(group.groupId());
	}
}

bool CategorySceneTemplate::checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const
{
	SceneTemplateItem _item(item);

	findResource(_item);

	return checkItem(_item);
}

bool CategorySceneTemplate::checkItem(const SceneTemplateItem &item) const
{
	auto configJsonPath = item.resource.configJsonPath();
	auto mainSceneImagePath = item.resource.mainSceneImagePath();
	auto mainSceneThumbnail_1 = item.resource.mainSceneThumbnail_1();
	auto mainSceneThumbnail_2 = item.resource.mainSceneThumbnail_2();

	return (!configJsonPath.isEmpty() && QFile::exists(configJsonPath)) && (!mainSceneImagePath.isEmpty() && QFile::exists(mainSceneImagePath)) &&
	       (!mainSceneThumbnail_1.isEmpty() && QFile::exists(mainSceneThumbnail_1)) && (!mainSceneThumbnail_2.isEmpty() && QFile::exists(mainSceneThumbnail_2));
}

void CategorySceneTemplate::allDownload(pls::rsm::IResourceManager *mgr, bool ok)
{
	PLS_INFO(moduleName(), "%p-%s: %d", this, __FUNCTION__, ok);
}

void CategorySceneTemplate::findResource(SceneTemplateItem &item) const
{
	if (!item.resource.mainSceneImagePath().isEmpty() && !item.resource.mainSceneThumbnail_1().isEmpty() && !item.resource.mainSceneThumbnail_2().isEmpty()) {
		return;
	}

	item.resource.mainSceneImagePath({});
	item.resource.mainSceneVideoPath({});
	item.resource.mainSceneThumbnail_1({});
	item.resource.mainSceneThumbnail_2({});
	item.resource.detailSceneThumbnailPathList({});

	auto configDir = pls_find_subdir_contains_spec_file(item.resourcePath(), Config_Json);
	if (configDir) {
		QStringList thumbnailList;
		QFileInfoList fileInfoList = QDir(configDir.value()).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
		for (auto &fileInfo : fileInfoList) {
			QString fileName = fileInfo.baseName();

			if (fileInfo.fileName() == Config_Json) {
				item.resource.configJsonPath(fileInfo.absoluteFilePath());
			} else if (fileName == Main_Scene_Image_Name) {
				item.resource.mainSceneImagePath(fileInfo.absoluteFilePath());
			} else if (fileName == Main_Scene_Video_Name) {
				item.resource.mainSceneVideoPath(fileInfo.absoluteFilePath());
			} else if (fileName == Main_Scene_Thumbnail_1_Name) {
				item.resource.mainSceneThumbnail_1(fileInfo.absoluteFilePath());
			} else if (fileName == Main_Scene_Thumbnail_2_Name) {
				item.resource.mainSceneThumbnail_2(fileInfo.absoluteFilePath());
			} else if (fileName.startsWith(Detail_Scene_Thumbnail_Name)) {
				thumbnailList.append(fileInfo.absoluteFilePath());
			}
		}
		item.resource.detailSceneThumbnailPathList(thumbnailList);
	}
}
