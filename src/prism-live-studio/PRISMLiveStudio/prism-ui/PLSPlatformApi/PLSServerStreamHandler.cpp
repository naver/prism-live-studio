
#include "PLSServerStreamHandler.hpp"
#include "PLSChannelDataAPI.h"
//#include "pls-app.hpp"
#include "PLSPlatformApi/PLSPlatformApi.h"
#include "PLSSyncServerManager.hpp"
#include "common/PLSAPICommon.h"
#include "pls-common-define.hpp"
#include "PLSPlatformApi/prism/PLSPlatformPrism.h"
#include "ChannelCommonFunctions.h"
#include "pls-gpop-data.hpp"
#include "pls/pls-dual-output.h"

using namespace common;

/*********Thumbnail parameter**********/
constexpr auto THUMBNAIL_WIDTH = 1280;
constexpr auto THUMBNAIL_HEIGHT = 720;

struct ServerStreamGlobalVars {
	static quint32 _width;
	static quint32 _height;
	static char *image_data;
};

uint32_t ServerStreamGlobalVars::_width = 0;
uint32_t ServerStreamGlobalVars::_height = 0;
char *ServerStreamGlobalVars::image_data = nullptr;

/**********multiple form request ***********/
#define VIDEO_TITLE QStringLiteral("video.title")
#define VIDEO_CHANNEL QStringLiteral("video.channel")
#define VIDEO_VIDEOSEQ QStringLiteral("video.videoSeq")
#define THUMBNAIL_FILE QStringLiteral("thumbnailFile")
#define THUMBNAIL_FILE_NAME QStringLiteral("thumbnail.png")
#define HEADER_MINE_APPLICATION QStringLiteral("application/octet-stream")

PLSServerStreamHandler *PLSServerStreamHandler::instance()
{
	static PLSServerStreamHandler syncServerManager;
	return &syncServerManager;
}

PLSServerStreamHandler::~PLSServerStreamHandler()
{
	if (ServerStreamGlobalVars::image_data) {
		pls_free(ServerStreamGlobalVars::image_data);
	}
}

PLSServerStreamHandler::PLSServerStreamHandler(QObject *parent) : QObject(parent)
{
	main = qobject_cast<PLSBasic *>(App()->GetMainWindow());
}

QString PLSServerStreamHandler::getOutputResolution(bool bVertical) const
{
	return QString("%1x%2").arg(config_get_uint(main->Config(), "Video", bVertical ? "OutputCXV" : "OutputCX")).arg(config_get_uint(main->Config(), "Video", bVertical ? "OutputCYV" : "OutputCY"));
}

QString PLSServerStreamHandler::getOutputFps() const
{
	if (uint64_t fpsType = config_get_uint(main->Config(), "Video", "FPSType"); fpsType == 0) {
		const char *val_str = config_get_string(main->Config(), "Video", "FPSCommon");
		return QString(val_str);
	} else if (fpsType == 1) {
		uint64_t val = config_get_uint(main->Config(), "Video", "FPSInt");
		return QString("%1").arg(val);
	} else if (fpsType == 2) {
		uint64_t num = config_get_uint(main->Config(), "Video", "FPSNum");
		uint64_t den = config_get_uint(main->Config(), "Video", "FPSDen");
		return QString("%1").arg(num / den);
	}
	return QString();
}

bool PLSServerStreamHandler::isSupportedResolutionFPS(QString &outTipString) const
{
	bool result = true;
	QMap<QString, QString> map;
	map.insert(YOUTUBE, "youtube");
	map.insert(VLIVE, "vlive");
	map.insert(NAVER_TV, "navertv");
	map.insert(TWITCH, "twitch");
	map.insert(BAND, "band");
	map.insert(FACEBOOK, "facebook");
	map.insert(NOW, "now");
	map.insert(AFREECATV, "afreecatv");
	map.insert(NAVER_SHOPPING_LIVE, NAVER_SHOPPING_RESOLUTION_KEY);
	QList<QString> platformList;
	for (const PLSPlatformBase *pPlatform : PLS_PLATFORM_ACTIVIED) {
		QString channelName = pPlatform->getChannelName();
		//if the convert platform key failed, then iterator the next
		if (!map.contains(channelName)) {
			continue;
		}
		QString platformKey = map.value(channelName);
		ChannelData::ChannelDataType type = pPlatform->getChannelType();
		QVariantMap platformFPSMap;
		if (ChannelData::ChannelDataType::ChannelType == type) {
			platformFPSMap = PLSSyncServerManager::instance()->getLivePlatformResolutionFPSMap();
		} else if (type >= ChannelData::ChannelDataType::CustomType) {
			platformFPSMap = PLSSyncServerManager::instance()->getRtmpPlatformFPSMap();
		}

		if (platformFPSMap.count() == 0) {
			if (ChannelData::ChannelDataType::ChannelType == type) {
				PLS_WARN(MODULE_PlatformService, "local platform fps json file is empty");
			} else if (type >= ChannelData::ChannelDataType::CustomType) {
				PLS_WARN(MODULE_PlatformService, "local rtmp fps json file is empty");
			}
		}

		//if the platform limit list not contains the platform key
		if (!platformFPSMap.contains(platformKey)) {
			continue;
		}
		if (channelName == NAVER_SHOPPING_LIVE) {
			channelName = tr("navershopping.liveinfo.title");
		} else if (channelName == AFREECATV) {
			channelName = TR_AFREECATV;
		}
		checkChannelResolutionFpsValid(channelName, platformFPSMap, platformKey, result, platformList, pls_is_dual_output_on() && pPlatform->isVerticalOutput());
	}
	if (platformList.isEmpty()) {
		return result;
	}
	QString channelListName = platformList.at(0);
	for (int i = 1; i < platformList.size(); i++) {
		channelListName.append(QString(",%1").arg(platformList.at(i)));
	}

	outTipString = getResolutionAndFpsInvalidTip(channelListName);
	return result;
}

QString PLSServerStreamHandler::getResolutionAndFpsInvalidTip(const QString &channeName) const
{
	return QTStr("Platform.resolution.fps.failed").arg(channeName);
}

void PLSServerStreamHandler::checkChannelResolutionFpsValid(const QString &channelName, const QVariantMap &platformFPSMap, const QString &platformKey, bool &result, QList<QString> &platformList,
							    bool bVertical) const
{
	//if the limited platform list contains the current platform name
	QString outResolution = getOutputResolution(bVertical);
	QString outFps = getOutputFps();
	QMap resolutionFps = platformFPSMap.value(platformKey).toMap();
	if (resolutionFps.keys().contains(outResolution)) {
		QVariant fpsVariant = resolutionFps.value(outResolution);
		if (fpsVariant.typeId() == QMetaType::QString) {
			if (QString fps = fpsVariant.toString(); fps != outFps) {
				result = false;
				platformList.append(channelName);
			}
		} else if (fpsVariant.typeId() == QMetaType::QStringList) {
			QStringList fps = fpsVariant.toStringList();
			if (!fps.contains(outFps)) {
				result = false;
				platformList.append(channelName);
			}
		} else if (fpsVariant.typeId() == QMetaType::QVariantList) {
			QStringList fps;
			for (const auto &f : fpsVariant.toList())
				fps.append(f.toString());
			if (!fps.contains(outFps)) {
				result = false;
				platformList.append(channelName);
			}
		}

	} else {
		result = false;
		platformList.append(channelName);
	}
}

bool PLSServerStreamHandler::isValidWatermark(const QString &platFormName) const
{
	QString watermarkPath = PLSSyncServerManager::instance()->getWaterMarkResLocalPath(platFormName);
	if (!QFile::exists(watermarkPath)) {
		PLS_ERROR(MODULE_PlatformService, "watermark path not exist");
		return false;
	}
	return true;
}

bool PLSServerStreamHandler::isValidOutro(const QString &platFormName) const
{
	QVariantMap outroInfoMap = PLSSyncServerManager::instance()->getOutroResLocalPathAndText(platFormName);
	if (outroInfoMap.isEmpty()) {
		PLS_ERROR(MODULE_PlatformService, "getOutroResLocalPathAndText not exist");
		return false;
	}
	auto path = outroInfoMap.value(OUTRO_PATH).toString();
	if (!QFile::exists(path)) {
		PLS_ERROR(MODULE_PlatformService, "outro path not exist");
		return false;
	}
	auto text = outroInfoMap.value(OUTRO_TEXT).toString();
	if (text.isEmpty()) {
		PLS_ERROR(MODULE_PlatformService, "outro text is empty");
		return false;
	}
	return true;
}
