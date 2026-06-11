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
#include "PLSErrorHandler.h"

QString PLSAfreecaTVDataHandler::getPlatformName()
{
	return AFREECATV;
}

bool PLSAfreecaTVDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	auto platform = dynamic_cast<PLSPlatformAfreecaTV *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!platform) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "%s %s afreecatv refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		PLSErrorHandler::ExtraData otherData;
		otherData.errPhase = PLSErrPhaseLogin;
		otherData.urlEn = "tryToUpdate SOOP";
		auto retData = PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::CHANNEL_AFREECATV_LOGIN_ERROR, AFREECATV, "", otherData);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		info[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
		info[ChannelData::g_errorString] = retData.alertMsg;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_AFREECATV, "%s %s channelUUID(%s) afreecatv platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), platform);
	platform->setInitData(srcInfo);
	QMetaObject::invokeMethod(platform, [platform, srcInfo, finishedCall]() {
		platform->reInitLiveInfo(PLSCHANNELS_API->isResetNeed());
		platform->requestChannelInfo(srcInfo, finishedCall);
	});
	return true;
}
