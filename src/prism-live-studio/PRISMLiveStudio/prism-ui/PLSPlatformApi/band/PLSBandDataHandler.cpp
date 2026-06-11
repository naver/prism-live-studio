#include "PLSBandDataHandler.h"

#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"

PLSBandDataHandler::~PLSBandDataHandler()
{
	qDebug() << "PlatformAPI PLSBandDataHandler release";
}

QString PLSBandDataHandler::getPlatformName()
{
	return BAND;
}

bool PLSBandDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	PLS_INFO(MODULE_PlatformService, "PlatformAPI tryToUpdate band platform, channel type is %d , channel uuid is %s", srcInfo.value(ChannelData::g_data_type).toInt(),
		 channelUUID.toStdString().c_str());
	if (auto band = dynamic_cast<PLSPlatformBand *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo)); !band) {
		PLS_ERROR(MODULE_PLATFORM_BAND, "%s %s Band refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);

		PLSErrorHandler::ExtraData otherData;
		otherData.urlEn = QStringLiteral("band platform not exit");
		otherData.errPhase = PLSErrPhaseLogin;
		auto retData = PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::COMMON_CHANNEL_LOGIN_FAIL, BAND, "", otherData);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		info[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
		info[ChannelData::g_errorString] = retData.alertMsg;
		finishedCall(m_bandInfos);
		return false;

	} else {
		band->clearBandInfos();
		band->setInitData(srcInfo);
		PLS_INFO(MODULE_PLATFORM_BAND, "%s %s channelUUID(%s) Band platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), band);
		QMetaObject::invokeMethod(band, [band, srcInfo, finishedCall]() {
			band->initLiveInfo(PLSCHANNELS_API->isResetNeed());
			if (getInfo(srcInfo, ChannelData::g_channelToken).isEmpty()) {
				band->getBandTokenInfo(srcInfo, finishedCall);
			} else {
				band->getBandCategoryInfo(srcInfo, finishedCall);
			}
		});
	}

	return true;
}
