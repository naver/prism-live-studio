/*
* @file		PLSPlatformRtmp.h
* @brief	for rtmp, no additional http api request
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include "../PLSPlatformBase.hpp"

class PLSPlatformRtmp : public PLSPlatformBase {
public:
	PLSServiceType getServiceType() const override;

	void onPrepareLive(bool value) override;
	void onAlLiveStarted(bool) override;
	QString getShareUrlEnc() override;

	QJsonObject getLiveStartParams() override;
};
