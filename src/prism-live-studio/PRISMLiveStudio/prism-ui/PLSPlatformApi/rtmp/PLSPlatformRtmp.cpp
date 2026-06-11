#include "PLSPlatformRtmp.h"

#include "../PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"

PLSServiceType PLSPlatformRtmp::getServiceType() const
{
	return PLSServiceType::ST_CUSTOM;
}

void PLSPlatformRtmp::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	setStreamServer(getInitData().value(ChannelData::g_channelRtmpUrl).toString().toStdString());
	setStreamKey(getInitData().value(ChannelData::g_streamKey).toString().toStdString());

	prepareLiveCallback(true);
}

void PLSPlatformRtmp::onAlLiveStarted(bool value)
{
	if (value && getChannelName() == FACEBOOK) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Facebook.OnBroadcast"));
	} else if (value && getChannelName() == BAND) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("Live.Check.Band.Rtmp.Living.Notice"));
	}
}

QString PLSPlatformRtmp::getShareUrlEnc()
{
	return mySharedData().m_strStreamServer.c_str();
}

QJsonObject PLSPlatformRtmp::getLiveStartParams()
{
	QJsonObject platform;

	platform["id"] = mySharedData().m_mapInitData[ChannelData::g_rtmpUserID].toString();
	platform["password"] = mySharedData().m_mapInitData[ChannelData::g_password].toString();
	platform["simulcastChannel"] = mySharedData().m_mapInitData[ChannelData::g_nickName].toString();

	return platform;
}
