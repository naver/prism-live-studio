#include "PLSPlatformRtmp.h"

#include "../PLSPlatformApi.h"

PLSPlatformRtmp::PLSPlatformRtmp() {}

PLSServiceType PLSPlatformRtmp::getServiceType() const
{
	return PLSServiceType::ST_RTMP;
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
	}
}
