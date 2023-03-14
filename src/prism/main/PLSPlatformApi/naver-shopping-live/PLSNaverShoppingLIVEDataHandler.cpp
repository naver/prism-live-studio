#include "PLSNaverShoppingLIVEDataHandler.h"

#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSChannelDataAPI.h"

static QVariantMap makeErrorInfo(const QVariantMap &srcInfo)
{
	QVariantMap info = srcInfo;
	info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
	return info;
}

PLSNaverShoppingLIVEDataHandler::PLSNaverShoppingLIVEDataHandler() {}

PLSNaverShoppingLIVEDataHandler::~PLSNaverShoppingLIVEDataHandler() {}

QString PLSNaverShoppingLIVEDataHandler::getPlatformName()
{
	return NAVER_SHOPPING_LIVE;
}

bool PLSNaverShoppingLIVEDataHandler::isMultiChildren()
{
	return false;
}

bool PLSNaverShoppingLIVEDataHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback callback)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	if (channelUUID.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "%s %s Naver Shopping LIVE refresh failed, channel's UUID is empty.", PrepareInfoPrefix, __FUNCTION__);
		callback({makeErrorInfo(srcInfo)});
		return false;
	}

	PLSPlatformNaverShoppingLIVE *naverShoppingLIVE = dynamic_cast<PLSPlatformNaverShoppingLIVE *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!naverShoppingLIVE) {
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "%s %s Naver Shopping LIVE refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		callback({makeErrorInfo(srcInfo)});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "%s %s channelUUID(%s) NaverShopping platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(),
		 naverShoppingLIVE);
	naverShoppingLIVE->setInitData(srcInfo);
	QMetaObject::invokeMethod(naverShoppingLIVE, [=]() { naverShoppingLIVE->getUserInfo(getPlatformName(), srcInfo, callback, PLSCHANNELS_API->isResetNeed()); });
	return true;
}
