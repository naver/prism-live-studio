#include "PLSAfreecaTVDataHandler.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include <QMetaMethod>
#include "../PLSPlatformApi.h"
#include "PLSAPIAfreecaTV.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformAfreecaTV.h"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
//#include "PLSPlatformVLive.h"

PLSAfreecaTVDataHandler::PLSAfreecaTVDataHandler() {}
PLSAfreecaTVDataHandler::~PLSAfreecaTVDataHandler() {}

QString PLSAfreecaTVDataHandler::getPlatformName()
{
	return AFREECATV;
}

bool PLSAfreecaTVDataHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback finishedCall)
{
	auto *platform = PLS_PLATFORM_AFREECATV;
	if (!platform) {
		PLS_ERROR(MODULE_PlatformService, "%s %s afreecatv refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	PLS_INFO(MODULE_PlatformService, "%s %s channelUUID(%s) afreecatv platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), platform);
	platform->setInitData(srcInfo);
	QMetaObject::invokeMethod(platform, [=]() {
		platform->reInitLiveInfo(PLSCHANNELS_API->isResetNeed());
		platform->requestChannelInfo(srcInfo, finishedCall);
	});
	return true;
}
