#include "PLSLiveInfoDialogs.h"

#include "PLSPlatformApi.h"
#include "twitch/PLSLiveInfoTwitch.h"
#include "youtube/PLSLiveInfoYoutube.h"
#include "ChannelConst.h"

int pls_exec_live_Info(const QVariantMap &info, QWidget *parent)
{
	auto channelName = info.value(ChannelData::g_channelName).toString();
	auto channelUUID = info.value(ChannelData::g_channelUUID).toString();

	if (TWITCH == channelName) {
		return pls_exec_live_Info_twitch(channelUUID, info, parent);
	} else if (YOUTUBE == channelName) {
		return pls_exec_live_Info_youtube(channelUUID, info, parent);
	}

	return QDialog::Rejected;
}

int pls_exec_live_Info_twitch(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	PLSLiveInfoTwitch liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_youtube(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	PLSLiveInfoYoutube liveInfo(pPlatform, parent);
	return liveInfo.exec();
}
