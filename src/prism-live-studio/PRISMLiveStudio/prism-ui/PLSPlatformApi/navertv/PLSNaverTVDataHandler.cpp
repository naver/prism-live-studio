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

QString PLSNaverTVDataHandler::getPlatformName()
{
	return NAVER_TV;
}

bool PLSNaverTVDataHandler::isMultiChildren()
{
	return true;
}

bool PLSNaverTVDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	if (channelUUID.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s %s NaverTV refresh failed, channel's UUID is empty.", PrepareInfoPrefix, __FUNCTION__);
		callback({makeErrorInfo(srcInfo)});
		return false;
	}
	PLS_INFO(MODULE_PlatformService, "PlatformAPI tryToUpdate platform success, channel type is %d , channel uuid is %s", srcInfo.value(ChannelData::g_data_type).toInt(),
		 channelUUID.toStdString().c_str());
	auto navertv = dynamic_cast<PLSPlatformNaverTV *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!navertv) {
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s %s NaverTV refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		callback({makeErrorInfo(srcInfo)});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s channelUUID(%s) NaverTV platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), navertv);
	QMetaObject::invokeMethod(navertv, [this, navertv, srcInfo, callback]() { navertv->getAuth(getPlatformName(), srcInfo, callback, PLSCHANNELS_API->isResetNeed()); });
	return true;
}
