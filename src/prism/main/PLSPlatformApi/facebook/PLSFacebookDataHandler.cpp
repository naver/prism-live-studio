#include "PLSFacebookDataHandler.h"
#include "PLSChannelDataAPI.h"
#include "../PLSPlatformApi.h"

PLSFacebookDataHandler::PLSFacebookDataHandler() {}

PLSFacebookDataHandler::~PLSFacebookDataHandler() {}

QString PLSFacebookDataHandler::getPlatformName()
{
	return FACEBOOK;
}

bool PLSFacebookDataHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	PLSPlatformFacebook *facebook = dynamic_cast<PLSPlatformFacebook *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!facebook) {
		QVariantMap info = srcInfo;
		PLS_ERROR(MODULE_PLATFORM_FACEBOOK, "%s %s Facebook refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_FACEBOOK, "%s %s channelUUID(%s) Facebook platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), facebook);
	facebook->setInitData(srcInfo);
	QMetaObject::invokeMethod(facebook, [=]() {
		facebook->initInfo(srcInfo);
		auto userInfoFinished = [=](PLSAPIFacebookType) {
			QVariantMap info = facebook->getSrcInfo();
			finishedCall(QList<QVariantMap>{info});
			return true;
		};
		facebook->getLongLivedUserAccessToken(userInfoFinished);
	});
	return true;
}
