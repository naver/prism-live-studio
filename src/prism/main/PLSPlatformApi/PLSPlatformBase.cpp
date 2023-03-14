#pragma once

#include "PLSPlatformBase.hpp"
#include "PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"
#include "prism/PLSPlatformPrism.h"
#include "PLSServerStreamHandler.hpp"
#include "alert-view.hpp"

const char *const NamesForChannelType[] = {CUSTOM_RTMP, TWITCH, YOUTUBE, FACEBOOK, VLIVE, NAVER_TV, BAND, AFREECATV, NAVER_SHOPPING_LIVE};
const char *const NamesForSettingId[] = {"rtmp_custom", "Twitch", "YouTube / YouTube Gaming", "Facebook Live", "Vlive", "NaverTv", "BAND", "AFREECATV", NAVER_SHOPPING_LIVE};
const char *const NamesForLiveStart[] = {"CUSTOM", "TWITCH", "YOUTUBE", "FACEBOOK", "VLIVE", "NAVERTV", "BAND", "AFREECATV", "SHOPPINGLIVE"};

const char *const KeyConfigLiveInfo = "LiveInfo";
const char *const KeyTwitchServer = "TwitchServer";

const QString PLSPlatformBase::getChannelUUID() const
{
	return m_mapInitData[ChannelData::g_channelUUID].toString();
}

const QString PLSPlatformBase::getChannelToken() const
{
	return m_mapInitData[ChannelData::g_channelToken].toString();
}

const QString PLSPlatformBase::getChannelRefreshToken() const
{
	return m_mapInitData[ChannelData::g_refreshToken].toString();
}

const ChannelData::ChannelDataType PLSPlatformBase::getChannelType() const
{
	return static_cast<ChannelData::ChannelDataType>(m_mapInitData[ChannelData::g_data_type].toInt());
}

const QString PLSPlatformBase::getChannelName() const
{
	return m_mapInitData[ChannelData::g_platformName].toString();
}

const int PLSPlatformBase::getChannelOrder() const
{
	return m_mapInitData[ChannelData::g_displayOrder].toInt();
}

const QString PLSPlatformBase::getChannelCookie() const
{
	return m_mapInitData[ChannelData::g_channelCookie].toString();
}

const QVariantMap &PLSPlatformBase::getInitData()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	for (auto iter = info.cbegin(); iter != info.cend(); ++iter) {
		m_mapInitData[iter.key()] = iter.value();
	}

	return m_mapInitData;
}

void PLSPlatformBase::activateCallback(bool value)
{
	if (!PLS_PLATFORM_API->isDuringActivate()) {
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".not: %p %d type=%d, name=%s, uuid=%s", this, value, getChannelType(), getChannelName().toStdString().c_str(),
			 getChannelUUID().toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %p %d type=%d, name=%s, uuid=%s", this, value, getChannelType(), getChannelName().toStdString().c_str(),
		 getChannelUUID().toStdString().c_str());

	PLS_PLATFORM_PRSIM->onActive(this, value);
}
void PLSPlatformBase::deactivateCallback(bool value)
{
	if (!PLS_PLATFORM_API->isDuringDeactivate()) {
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".not: %p %d type=%d, name=%s, uuid=%s", this, value, getChannelType(), getChannelName().toStdString().c_str(),
			 getChannelUUID().toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %p %d type=%d, name=%s, uuid=%s", this, value, getChannelType(), getChannelName().toStdString().c_str(),
		 getChannelUUID().toStdString().c_str());

	PLS_PLATFORM_PRSIM->onInactive(this, value);
}

void PLSPlatformBase::prepareLiveCallback(bool value)
{
	if (LiveStatus::PrepareLive != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}

	if (value && !PLSServerStreamHandler::instance()->isValidWatermark()) {
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("watermark.resource.is.not.existed.tip"));
		PLS_PLATFORM_PRSIM->onPrepareLive(false);
		return;
	}

	m_bApiPrepared = value;
	if (value) {
		PLS_PLATFORM_API->saveStreamSettings(getNameForSettingId(), getStreamServer(), getStreamKey());
	}

	auto platforms = PLS_PLATFORM_API->getActivePlatforms();
	auto iter = find(platforms.begin(), platforms.end(), this);

	if (iter != platforms.end() && ++iter != platforms.end()) {
		(*iter)->onPrepareLive(value);
	} else {
		PLS_PLATFORM_PRSIM->onPrepareLive(value);
	}
}

void PLSPlatformBase::liveStartedCallback(bool value)
{
	if (LiveStatus::LiveStarted != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}

	m_bApiStarted = true;
	auto platforms = PLS_PLATFORM_API->getActivePlatforms();
	auto iter = find(platforms.begin(), platforms.end(), this);

	if (iter != platforms.end() && ++iter != platforms.end()) {
		(*iter)->onLiveStarted(value);
	} else {
		PLS_PLATFORM_PRSIM->onLiveStarted(value);
	}
}

void PLSPlatformBase::prepareFinishCallback()
{
	if (LiveStatus::PrepareFinish != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}

	auto platforms = PLS_PLATFORM_API->getActivePlatforms();
	auto iter = find(platforms.begin(), platforms.end(), this);

	if (iter != platforms.end() && ++iter != platforms.end()) {
		(*iter)->onPrepareFinish();
	} else {
		PLS_PLATFORM_PRSIM->onPrepareFinish();
	}
}

void PLSPlatformBase::liveStoppedCallback()
{
	if (LiveStatus::LiveStoped != PLS_PLATFORM_API->getLiveStatus()) {
		onLiveEnded();
		return;
	}
	auto platforms = PLS_PLATFORM_API->getActivePlatforms();
	auto iter = find(platforms.begin(), platforms.end(), this);

	if (iter != platforms.end() && ++iter != platforms.end()) {
		(*iter)->onLiveStopped();
	} else {
		PLS_PLATFORM_PRSIM->onLiveStopped();
	}
}

void PLSPlatformBase::liveEndedCallback()
{
	if (LiveStatus::LiveEnded != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}
	auto platforms = PLS_PLATFORM_API->getActivePlatforms();
	auto iter = find(platforms.begin(), platforms.end(), this);

	if (iter != platforms.end() && ++iter != platforms.end()) {
		(*iter)->onLiveEnded();
	} else {
		PLS_PLATFORM_PRSIM->onLiveEnded();
	}
}

PLSPlatformMqttStatus PLSPlatformBase::getMqttStatus(const QString &szStatus)
{
	if ("ON_BROADCAST" == szStatus) {
		return PLSPlatformMqttStatus::PMS_ON_BROADCAST;
	} else if ("END_BROADCAST" == szStatus) {
		return PLSPlatformMqttStatus::PMS_END_BROADCAST;
	} else if ("CONNECTING_TO_SERVER" == szStatus) {
		return PLSPlatformMqttStatus::PMS_CONNECTING_TO_SERVER;
	} else if ("CANNOT_FIND_SERVER" == szStatus) {
		return PLSPlatformMqttStatus::PMS_CANNOT_FIND_SERVER;
	} else if ("CANNOT_CONNECT_TO_SERVER" == szStatus) {
		return PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_SERVER;
	} else if ("CANNOT_AUTH_TO_SERVER" == szStatus) {
		return PLSPlatformMqttStatus::PMS_CANNOT_AUTH_TO_SERVER;
	} else if ("CANNOT_CONNECT_TO_PATH" == szStatus) {
		return PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_PATH;
	} else if ("WAITING_TO_BROADCAST" == szStatus) {
		return PLSPlatformMqttStatus::PMS_WAITING_TO_BROADCAST;
	} else {
		return PLSPlatformMqttStatus::PMS_NONE;
	}
}

PLSPlatformBase &PLSPlatformBase::setMqttStatus(const QString &szStatus)
{
	setMqttStatus(getMqttStatus(szStatus));

	return *this;
}

PLSPlatformBase &PLSPlatformBase::setMqttStatus(const PLSPlatformMqttStatus value)
{
	if (PLSPlatformMqttStatus::PMS_ON_BROADCAST == value && !m_bMqttFirstBroadcastOn) {
		m_bMqttFirstBroadcastOn = true;
		onMqttBroadcastOn();
	}

	if (value != m_enumMqttStatus) {
		onMqttStatus(value);
	}

	m_enumMqttStatus = value;

	return *this;
}

QJsonObject PLSPlatformBase::getWebChatParams()
{
	QJsonObject platform;

	platform.insert("name", QString::fromStdString(getNameForLiveStart()));
	platform.insert("accessToken", getChannelToken());

	return platform;
}

QJsonObject PLSPlatformBase::getMqttChatParams()
{
	QJsonObject platform;

	platform.insert("accessToken", getChannelToken());

	return platform;
}

QJsonObject PLSPlatformBase::getLiveStartParams()
{
	QJsonObject platform;

	platform.insert("accessToken", getChannelToken());

	return platform;
}

bool PLSPlatformBase::isMqttChatCanShow(const QJsonObject &)
{
	return true;
}
