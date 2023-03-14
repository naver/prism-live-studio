#include "PLSNaverTVDataHandler.h"

#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"
#include "PLSPlatformNaverTV.h"
#include "PLSChannelDataAPI.h"

static QVariantMap makeErrorInfo(const QVariantMap &srcInfo)
{
	QVariantMap info = srcInfo;
	info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
	return info;
}

PLSNaverTVDataHandler::PLSNaverTVDataHandler() {}
PLSNaverTVDataHandler::~PLSNaverTVDataHandler() {}

QString PLSNaverTVDataHandler::getPlatformName()
{
	return NAVER_TV;
}

bool PLSNaverTVDataHandler::isMultiChildren()
{
	return true;
}

bool PLSNaverTVDataHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback callback)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	if (channelUUID.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s %s NaverTV refresh failed, channel's UUID is empty.", PrepareInfoPrefix, __FUNCTION__);
		callback({makeErrorInfo(srcInfo)});
		return false;
	}

	PLSPlatformNaverTV *navertv = dynamic_cast<PLSPlatformNaverTV *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!navertv) {
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s %s NaverTV refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		callback({makeErrorInfo(srcInfo)});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s channelUUID(%s) NaverTV platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), navertv);
	QMetaObject::invokeMethod(navertv, [=]() { navertv->getAuth(getPlatformName(), srcInfo, callback, PLSCHANNELS_API->isResetNeed()); });
	return true;
}
