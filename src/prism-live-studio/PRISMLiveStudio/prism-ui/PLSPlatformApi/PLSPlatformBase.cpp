#pragma once

#include "PLSPlatformBase.hpp"
#include "PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"
#include "prism/PLSPlatformPrism.h"
#include "PLSServerStreamHandler.hpp"
#include "PLSAlertView.h"
#include "pls/pls-dual-output.h"

using namespace std;

const std::array<const char *, PLATFORM_SIZE> NamesForChannelType =
	pls_make_array<const char *>(CUSTOM_RTMP, TWITCH, YOUTUBE, FACEBOOK, VLIVE, NAVER_TV, BAND, AFREECATV, NAVER_SHOPPING_LIVE, TWITTER, CHZZK, NCB2B);
const std::array<const char *, PLATFORM_SIZE> NamesForSettingId =
	pls_make_array<const char *>("", "Twitch", "YouTube", "Facebook Live", "Vlive", "NaverTv", "BAND", "AFREECATV", NAVER_SHOPPING_LIVE, TWITTER, CHZZK, NCB2B);
const std::array<const char *, PLATFORM_SIZE> NamesForLiveStart =
	pls_make_array<const char *>("CUSTOM", "TWITCH", "YOUTUBE", "FACEBOOK", "VLIVE", "NAVERTV", "BAND", "AFREECATV", "SHOPPINGLIVE", "TWITTER", "CHZZK", NCP_LIVE_START_NAME);

const char *const KeyConfigLiveInfo = "LiveInfo";
const char *const KeyTwitchServer = "TwitchServer";

QString PLSPlatformBase::getChannelUUID() const
{
	return mySharedData().m_mapInitData.value(ChannelData::g_channelUUID).toString();
}

QString PLSPlatformBase::getChannelToken() const
{
	return mySharedData().m_mapInitData.value(ChannelData::g_channelToken).toString();
}

QString PLSPlatformBase::getChannelRefreshToken() const
{
	return mySharedData().m_mapInitData[ChannelData::g_refreshToken].toString();
}

ChannelData::ChannelDataType PLSPlatformBase::getChannelType() const
{
	return static_cast<ChannelData::ChannelDataType>(mySharedData().m_mapInitData[ChannelData::g_data_type].toInt());
}

QString PLSPlatformBase::getChannelName() const
{
	return mySharedData().m_mapInitData[ChannelData::g_channelName].toString();
}

QString PLSPlatformBase::getPlatFormName() const
{
	return mySharedData().m_mapInitData[ChannelData::g_fixPlatformName].toString();
}

int PLSPlatformBase::getChannelOrder() const
{
	return mySharedData().m_mapInitData[ChannelData::g_displayOrder].toInt();
}

QString PLSPlatformBase::getChannelCookie() const
{
	return mySharedData().m_mapInitData[ChannelData::g_channelCookie].toString();
}

bool PLSPlatformBase::isValid() const
{
	return PLSCHANNELS_API->getChannelStatus(getChannelUUID()) == ChannelData::Valid;
}

bool PLSPlatformBase::isExpired() const
{
	return PLSCHANNELS_API->getChannelStatus(getChannelUUID()) == ChannelData::Expired;
}

const QVariantMap &PLSPlatformBase::getInitData()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	for (auto iter = info.cbegin(); iter != info.cend(); ++iter) {
		mySharedData().m_mapInitData[iter.key()] = iter.value();
	}

	return mySharedData().m_mapInitData;
}

void PLSPlatformBase::activateCallback(bool value) const
{
	PLS_PLATFORM_API->activateCallback(this, value);
}

void PLSPlatformBase::deactivateCallback(bool value)
{
	PLS_PLATFORM_PRSIM->onInactive(this, value);
}

void PLSPlatformBase::prepareLiveCallback(bool value)
{
	//The state of the onPrepare callback function does not match
	if (LiveStatus::PrepareLive != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}

	//Set the live broadcast preparation status of the current channel
	m_bApiPreparedStatus = value ? PLSPlatformLiveStartedStatus::PLS_SUCCESS : PLSPlatformLiveStartedStatus::PLS_FAILED;
	PLS_INFO(MODULE_PlatformService, "%s  %s call PLSPlatformBase prepareLiveCallback method, channel name is %s , apiPrepared value is %d", PrepareInfoPrefix, getNameForChannelType(),
		 getChannelName().toStdString().c_str(), static_cast<int>(getApiPrepared()));

	//If the current channel is not ready, this preparation process is directly interrupted
	if (!value) {
		PLS_PLATFORM_API->prepareLiveCallback(false);
		return;
	}

	list<PLSPlatformBase *> platforms = pls_is_dual_output_on() && isVerticalOutput() ? PLS_PLATFORM_API->getVerticalPlatforms() : PLS_PLATFORM_API->getHorizontalPlatforms();

	if (getServiceType() == PLSServiceType::ST_CUSTOM) {
		QString rtmpId = mySharedData().m_mapInitData[ChannelData::g_rtmpUserID].toString();
		QString rtmpPassword = mySharedData().m_mapInitData[ChannelData::g_password].toString();
		PLS_PLATFORM_API->saveStreamSettings(getNameForSettingId(), getStreamServer(), getStreamKey(), isVerticalOutput(), rtmpId, rtmpPassword);
	} else {
		PLS_PLATFORM_API->saveStreamSettings(getNameForSettingId(), getStreamServer(), getStreamKey(), isVerticalOutput());
	}

	auto iter = find(platforms.begin(), platforms.end(), this);

	if (iter != platforms.end() && ++iter != platforms.end()) {
		(*iter)->onPrepareLive(value);
	} else {
		if (pls_is_dual_output_on() && isHorizontalOutput()) {
			PLS_PLATFORM_API->getVerticalPlatforms().front()->onPrepareLive(true);
		} else {
			PLS_PLATFORM_API->prepareLiveCallback(value);
		}
	}
}

void PLSPlatformBase::liveStartedCallback(bool value)
{
	if (LiveStatus::LiveStarted != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}
	m_bApiStartedStatus = value ? PLSPlatformLiveStartedStatus::PLS_SUCCESS : PLSPlatformLiveStartedStatus::PLS_FAILED;
	PLS_PLATFORM_API->checkAllPlatformLiveStarted();
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
	stopMaxLiveTimer();
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

void PLSPlatformBase::startMaxLiveTimer(int minutes, const QStringList &intervalList)
{
	if (minutes < 0) {
		PLS_INFO(MODULE_PlatformService, "mqtt max live time status: not need max live timer, minutes is %d , channel name is %s", minutes, getNameForChannelType());
		return;
	}
	m_intervalList = intervalList;
	m_maxLiveMinutes = minutes;
	m_maxLiveleftMinute = 0;
	if (m_intervalList.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, "mqtt max live time status: setup max live timer, maxLiveMinutes is %d , channel name is %s", m_maxLiveMinutes, getNameForChannelType());
		startMaxLiveTimer(m_maxLiveMinutes * 60 * 1000);
	} else {
		m_maxLiveleftMinute = m_intervalList.takeFirst().toInt();
		PLS_INFO(MODULE_PlatformService, "mqtt max live time status: setup max live timer, maxLiveMinutes is %d , m_maxLiveleftMinute is %d , channel name is %s", m_maxLiveMinutes,
			 m_maxLiveleftMinute, getNameForChannelType());
		startMaxLiveTimer((m_maxLiveMinutes - m_maxLiveleftMinute) * 60 * 1000);
	}
}

void PLSPlatformBase::startMaxLiveTimer(int interval)
{
	PLS_INFO(MODULE_PlatformService, "mqtt max live time status: start max live timer, interval is %d , channel name is %s", interval, getNameForChannelType());
	stopMaxLiveTimer();
	m_maxTimeLenthTimer = pls_new<QTimer>();
	m_maxTimeLenthTimer->setSingleShot(true);
	QObject::connect(m_maxTimeLenthTimer, &QTimer::timeout, this, &PLSPlatformBase::reachMaxLivingTime);
	m_maxTimeLenthTimer->start(interval);
	m_maxLiveTimerStatus = PLSMaxLiveTimerStatus::PLS_TIMER_ING;
}

void PLSPlatformBase::stopMaxLiveTimer()
{
	if (m_maxTimeLenthTimer) {
		PLS_INFO(MODULE_PlatformService, "mqtt max live time status: stop max live timer, channel name is %s ", getNameForChannelType());
		m_maxTimeLenthTimer->stop();
		pls_delete(m_maxTimeLenthTimer, nullptr);
		m_maxLiveTimerStatus = PLSMaxLiveTimerStatus::PLS_TIMER_NONE;
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

bool PLSPlatformBase::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	if (PLS_PLATFORM_API->isPrismLive()) {
		return true;
	}
	return false;
}

QJsonObject PLSPlatformBase::getWebChatParams()
{
	QJsonObject platform;

	platform.insert("name", QString::fromStdString(getNameForLiveStart()));
	platform.insert("accessToken", getChannelToken());
	platform.insert("videoSeq",
			pls_is_dual_output_on() && isVerticalOutput() ? PLS_PLATFORM_PRSIM->getVideoSeq(DualOutputType::Vertical) : PLS_PLATFORM_PRSIM->getVideoSeq(DualOutputType::Horizontal));

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

//to get schedule list

void PLSPlatformBase::toStartGetScheduleList()
{
	mySharedData().m_lastError.clear();
	if (this->isValid()) {
		this->updateScheduleList();
		return;
	}
	emit scheduleListUpdateFinished();
}

void PLSPlatformBase::updateScheduleList()
{
	emit scheduleListUpdateFinished();
}

void PLSPlatformBase::convertScheduleListToMapList() {}

const QVariantList &PLSPlatformBase::getLastScheduleList()
{
	mySharedData().m_scheduleList.clear();
	if (isValid() && mySharedData().m_lastError.isEmpty()) {
		convertScheduleListToMapList();
		auto timeToString = [](QVariant &value) {
			auto varMap = value.toMap();
			auto time = varMap.value(ChannelData::g_timeStamp).toLongLong();
			auto timeO = QDateTime::fromSecsSinceEpoch(time);
			varMap.insert(ChannelData::g_createTime, timeO.toString());
			value = varMap;
		};
		auto &mySchedules = mySharedData().m_scheduleList;
		std::for_each(mySchedules.begin(), mySchedules.end(), timeToString);
	}
	return mySharedData().m_scheduleList;
}

const QVariantMap &PLSPlatformBase::getLastError()
{
	return mySharedData().m_lastError;
}

void PLSPlatformBase::reachMaxLivingTime()
{
	if (m_intervalList.isEmpty()) {
		stopMaxLiveTimer();
		PLS_INFO(MODULE_PlatformService, "mqtt max live time status: reach max live time, m_maxLiveMinutes is %d , channel name is %s", m_maxLiveMinutes, getNameForChannelType());
		PLS_PLATFORM_API->notifyLiveLeftMinutes(this, m_maxLiveMinutes, 0);
	} else {
		PLS_PLATFORM_API->notifyLiveLeftMinutes(this, m_maxLiveMinutes, m_maxLiveleftMinute);
		int maxTime = m_maxLiveleftMinute;
		m_maxLiveleftMinute = m_intervalList.takeFirst().toInt();
		PLS_INFO(MODULE_PlatformService, "mqtt max live time status: setup next max live timer, maxLiveMinutes is %d , m_maxLiveleftMinute is %d , channel name is %s", maxTime,
			 m_maxLiveleftMinute, getNameForChannelType());
		startMaxLiveTimer((maxTime - m_maxLiveleftMinute) * 60 * 1000);
	}
}
