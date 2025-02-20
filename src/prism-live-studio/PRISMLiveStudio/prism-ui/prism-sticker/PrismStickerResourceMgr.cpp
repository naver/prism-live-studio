#include "PrismStickerResourceMgr.h"
#include "PLSStickerDataHandler.h"
#include "liblog.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include <qdir.h>

using namespace pls::rsm;
constexpr auto module_name = "CategoryPrismSticker";
constexpr auto USE_SENSE_TIME_ID = "MASK";
const size_t MAX_STICKER_USE_COUNT = 21;

#define DO_LOG_INFO(format, ...) pls_info(false, module_name, __FILE__, __LINE__, "[PrismStickerResource] " format, ##__VA_ARGS__)

static bool hasMaskProperty(const Item &item)
{
	auto itemId = item.attr("itemId").toString();
	auto idSplit = itemId.split("_");
	if (!idSplit.empty() && idSplit[0] == USE_SENSE_TIME_ID) {
		return true;
	}

	return false;
}

static std::optional<std::pair<QString, QString>> getStickerConfigInfo(const pls::rsm::Item &item)
{
	std::optional<std::pair<QString, QString>> info;
	if (auto rules = item.urlAndHowSaves(); !rules.empty()) {
		auto rule = rules.front();
		QFileInfo fileInfo(rule.savedFilePath());
		QDir dstDir = fileInfo.dir();

		QString baseName = QFileInfo(rule.url().fileName()).baseName();
		auto path = dstDir.absolutePath() + "/" + baseName + "/";
		auto configFile = path + baseName + ".json";
		info.emplace(path, configFile);
	}

	return info;
}

QString CategoryPrismSticker::categoryId(IResourceManager *) const
{
	return PLS_RSM_CID_REACTION;
}

void CategoryPrismSticker::jsonDownloaded(pls::rsm::IResourceManager *, const pls::rsm::DownloadResult &result)
{
	DO_LOG_INFO("Json downloaded. result: %s", result.isOk() ? "OK" : "Failed");
	emit finishDownloadJson(result.isOk(), result.timeout());
}

void CategoryPrismSticker::jsonLoaded(pls::rsm::IResourceManager *mgr, pls::rsm::Category category)
{
	DO_LOG_INFO("%s json loaded.", qUtf8Printable(category.categoryId()));
	emit finishLoadJson();
}

bool CategoryPrismSticker::groupManualDownload(IResourceManager *mgr, Group group) const
{
	return true;
}

bool CategoryPrismSticker::itemManualDownload(IResourceManager *mgr, Item item) const
{
	return true;
}

bool CategoryPrismSticker::itemNeedLoad(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const
{
	return !hasMaskProperty(item);
}

bool CategoryPrismSticker::checkItem(IResourceManager *mgr, Item item) const
{
	auto info = getStickerConfigInfo(item);
	if (!info.has_value()) {
		return false;
	}

	QString path = info.value().first;
	QString configFile = info.value().second;
	QDir dir(path);
	if (!dir.exists())
		return false;

	QFile file(configFile);
	if (!file.exists())
		return false;

	auto group = item.groups();
	auto groupId = group.front().groupId();
	if (group.empty() || groupId.isEmpty())
		return false;

	auto wrapper = PLSStickerDataHandler::CreateStickerParamWrapper(groupId);
	if (!wrapper->Serialize(configFile))
		return false;

	for (const auto &config : wrapper->m_config) {
		auto remuxedFile = path + config.resourceDirectory + ".mp4";
		auto resourcePath = path + config.resourceDirectory;
		if (!QFile::exists(remuxedFile))
			return false;

		QString imageFile = PLSStickerDataHandler::getTargetImagePath(resourcePath, groupId, item.itemId(), Orientation::landscape == config.orientation);
		if (!QFile::exists(imageFile)) {
			PLS_WARN(moduleName(), "Failed to get prism sticker target image, the sticker will be abnormal when loop is off.");
		}
	}

	return true;
}

void CategoryPrismSticker::getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const
{
	urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					 .names({QStringLiteral("resourceUrl")}) //
					 .fileName(item.itemId())
					 .needDecompress(true)
					 .decompress([](const auto &urlAndHowSave, const auto &filePath) {
						 QDir dstDir = QFileInfo(filePath).dir();
						 auto dstDirPath = dstDir.absolutePath();
						 return pls::rsm::unzip(filePath, dstDirPath, false);
					 }));
}

void CategoryPrismSticker::itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<DownloadResult> &results) const
{
	if (ok) {
		// do remux in download thread
		PLSStickerDataHandler::RemuxItemResource(item);
	}
	bool timeout = results.empty() ? false : results.front().timeout();
	DO_LOG_INFO("Downloaded prism sticker: %s. result: %s, timeout: %s", qUtf8Printable(item.itemId()), ok ? "OK" : "Failed", timeout ? "yes" : "no");
	emit finishDownloadItem(item, ok, timeout);
}

size_t CategoryPrismSticker::useMaxCount(pls::rsm::IResourceManager *mgr) const
{
	return MAX_STICKER_USE_COUNT;
}
