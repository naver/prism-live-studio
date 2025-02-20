#include "PLSFacebookDataHandler.h"
#include "PLSChannelDataAPI.h"
#include "../PLSPlatformApi.h"

QString PLSFacebookDataHandler::getPlatformName()
{
	return FACEBOOK;
}

bool PLSFacebookDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	auto facebook = dynamic_cast<PLSPlatformFacebook *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!facebook) {
		QVariantMap info = srcInfo;
		PLS_ERROR(MODULE_PLATFORM_FACEBOOK, "%s %s Facebook refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s channelUUID(%s) Facebook platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), facebook);
	facebook->setInitData(srcInfo);
	QMetaObject::invokeMethod(facebook, [facebook, srcInfo, finishedCall]() {
		facebook->setSrcInfo(srcInfo);
		auto userInfoFinished = [finishedCall, facebook](const PLSErrorHandler::RetData &) {
			QVariantMap info = facebook->getSrcInfo();
			finishedCall(QList<QVariantMap>{info});
			return true;
		};
		facebook->getLongLivedUserAccessToken(userInfoFinished);
	});
	return true;
}
