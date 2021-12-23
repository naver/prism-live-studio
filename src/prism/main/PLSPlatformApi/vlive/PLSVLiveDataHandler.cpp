#include "PLSVLiveDataHandler.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include <QMetaMethod>
#include "../PLSPlatformApi.h"
#include "PLSAPIVLive.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformVLive.h"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
//#include "PLSPlatformVLive.h"
#include "PLSChannelDataAPI.h"

PLSVLiveDataHandler::PLSVLiveDataHandler() {}

PLSVLiveDataHandler::~PLSVLiveDataHandler() {}

QString PLSVLiveDataHandler::getPlatformName()
{
	return VLIVE;
}

bool PLSVLiveDataHandler::isMultiChildren()
{
	return true;
}

bool PLSVLiveDataHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	if (channelUUID.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "%s %s Band refresh failed, channel's UUID is empty.", PrepareInfoPrefix, __FUNCTION__);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	PLSPlatformVLive *vlive = dynamic_cast<PLSPlatformVLive *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!vlive) {
		PLS_ERROR(MODULE_PlatformService, "%s %s VLive refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}

	PLS_INFO(MODULE_PlatformService, "%s %s channelUUID(%s) VLive platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), vlive);
	QMetaObject::invokeMethod(vlive, [=]() {
		auto vliveInfos = PLSCHANNELS_API->getChanelInfosByPlatformName(VLIVE, ChannelData::ChannelType);
		for (const auto &info : vliveInfos) {
			const auto &uuid = info.value(ChannelData::g_channelUUID).toString();
			auto _vlive = dynamic_cast<PLSPlatformVLive *>(PLS_PLATFORM_API->getPlatformByTypeNotFroceCreate(uuid));
			if (_vlive) {
				_vlive->reInitLiveInfo(PLSCHANNELS_API->isResetNeed(), true);
			}
		}

		auto _onNext = [=]() {
			auto _callback = [=](const InfosList &infos) {
				bool isDelete = !srcInfo.value(ChannelData::g_isUpdated).toBool();
				if (isDelete) {
					PLS_PLATFORM_API->onRemoveChannel(channelUUID);
				}
				finishedCall(infos);
			};
			vlive->requestChannelInfo(srcInfo, _callback);
		};

		auto _onCountries = [=]() {
			if (PLSVliveStatic::instance()->countries.isEmpty()) {
				vlive->getCountryCodes(_onNext);
			} else {
				_onNext();
			}
		};

		if (!PLSVliveStatic::instance()->isLoaded) {
			PLSVliveStatic::instance()->isLoaded = true;
			vlive->initVliveGcc(_onCountries);
		} else {
			_onCountries();
		}
	});
	return true;
}

void PLSVLiveDataHandler::updateDisplayInfo(InfosList &srcList)
{
	for (auto &info : srcList) {
		info[ChannelData::g_sortString] = info[ChannelData::g_nickName];
		const auto &origianlData = PLSCHANNELS_API->getChanelInfoRefBySubChannelID(info.value(ChannelData::g_subChannelId).toString(), VLIVE);
		auto profileData = PLSPlatformVLive::getChannelProfile(origianlData);
		PLSPlatformVLive::changeChannelDataWhenProfileSelect(info, profileData, pls_get_main_view());
	}
}
