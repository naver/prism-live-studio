#include "PLSPlatformApi.h"

#include <string>
#include <algorithm>
#include <functional>
#include <iterator>
#include <QCoreApplication>
#include <QString>

#include "log.h"
#include "frontend-api.h"
#include "alert-view.hpp"
#include "window-basic-main.hpp"
#include "PLSLiveInfoDialogs.h"
#include "twitch/PLSLiveInfoTwitch.h"
#include "PLSHttpApi\PLSHttpHelper.h"
#include "channels/ChannelsDataApi/ChannelConst.h"
#include "channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "pls-net-url.hpp"
#include <QJsonDocument>

#define PRISM_MAX_OUT_Y 1080

using namespace std;

PLSPlatformApi *PLSPlatformApi::instance()
{
	static PLSPlatformApi *_instance = nullptr;

	if (nullptr == _instance) {
		_instance = new PLSPlatformApi();
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { delete _instance; });
	}

	return _instance;
}

PLSPlatformApi::PLSPlatformApi()
{
}

PLSPlatformApi::~PLSPlatformApi() {}

bool PLSPlatformApi::initialize()
{
	obs_frontend_add_event_callback(onFrontendEvent, this);

	for (auto &item : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		auto uuid = item.value(ChannelData::g_channelUUID).toString();

		if (isValidChannel(item)) {
			onActive(uuid);
		}
	}

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelActiveChanged, this, [=](const QString &channelUUID, bool enable) {
		if (enable) {
			onActive(channelUUID);
		} else {
			onInactive(channelUUID);
		}
	});

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, &PLSPlatformApi::onAddChannel);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelModified, this, &PLSPlatformApi::onUpdateChannel);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this, &PLSPlatformApi::onRemoveChannel);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigAllClear, this, &PLSPlatformApi::onClearChannel);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::broadcastGo, this, [=]() {
		emit enterLivePrepareState(true);
		onPrepareLive();
	});
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::stopBroadcastGo, this, &PLSPlatformApi::onPrepareFinish);
	connect(this, &PLSPlatformApi::livePrepared, this, [=](bool value) {
		if (value == false) {
			emit enterLivePrepareState(false);
		}
	});
	connect(this, &PLSPlatformApi::liveStarted, this, [=]() { emit enterLivePrepareState(false); });

	return true;
}

void PLSPlatformApi::saveStreamSettings(OBSData settings) const
{
	const char *service_id = "rtmp_common";

	obs_service_t *oldService = obs_frontend_get_streaming_service();
	OBSData hotkeyData = obs_hotkeys_save_service(oldService);
	obs_data_release(hotkeyData);

	OBSService newService = obs_service_create(service_id, "default_service", settings, hotkeyData);
	obs_service_release(newService);

	if (!newService)
		return;

	obs_frontend_set_streaming_service(newService);
	obs_frontend_save_streaming_service();
}

void PLSPlatformApi::saveStreamSettings(const string &platform, const string &server, const string &key) const
{
	if (isLiving()) {
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s, %s, %s", platform.c_str(), server.c_str(), key.c_str());

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_string(settings, "service", platform.data());
	obs_data_set_string(settings, "server", server.data());
	obs_data_set_string(settings, "key", key.data());

	saveStreamSettings(settings);
}

list<PLSPlatformBase *> PLSPlatformApi::getActivePlatforms() const
{
	list<PLSPlatformBase *> lst;

	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [](const auto v) { return v->isActive(); });

	return lst;
}

PLSPlatformTwitch *PLSPlatformApi::getPlatformTwitch()
{
	return dynamic_cast<PLSPlatformTwitch *>(getPlatformByType(PLSServiceType::ST_TWITCH));
}

PLSPlatformYoutube *PLSPlatformApi::getPlatformYoutube()
{
	return dynamic_cast<PLSPlatformYoutube *>(getPlatformByType(PLSServiceType::ST_YOUTUBE));
}

PLSPlatformFacebook *PLSPlatformApi::getPlatformFacebook()
{
	return dynamic_cast<PLSPlatformFacebook *>(getPlatformByType(PLSServiceType::ST_FACEBOOK));
}

void PLSPlatformApi::onActive(const QString &which)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s", which.toStdString().c_str());
	m_bDuringActivate = true;

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".empty: %s", which.toStdString().c_str());
		return;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_channelName).toString();

	if (!isValidChannel(info)) {
		PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".InvalidChannel: %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	auto platform = getPlatformById(which, info);
	if (nullptr != platform) {
		platform->onActive();
	}
}

void PLSPlatformApi::onInactive(const QString &which)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s", which.toStdString().c_str());
	m_bDuringDeactivate = true;

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".empty: %s", which.toStdString().c_str());
		return;
	}

	auto platform = getPlatformById(which, info);
	if (nullptr != platform) {
		platform->onInactive();
	}
}

void PLSPlatformApi::setLiveStatus(LiveStatus value)
{
	m_liveStatus = value;

	if (LiveStatus::PrepareLive == value) {
		PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START);
	} else if (LiveStatus::Normal == value) {
		if (!isRecording()) {
			PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END);
		}
	}
}

void PLSPlatformApi::onPrepareLive()
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	setLiveStatus(LiveStatus::PrepareLive);

	m_bApiPrepared = false;
	m_bApiStarted = false;

	m_bPrismLive = true;
	uint32_t out_cy = config_get_uint(PLSBasic::Get()->Config(), "Video", "OutputCY");
	if (out_cy > PRISM_MAX_OUT_Y) {
		m_bPrismLive = false;
	}

	for (auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		const auto channelUUID = info.value(ChannelData::g_channelUUID).toString();
		const auto channelType = info.value(ChannelData::g_data_type).toInt();
		const auto channelName = info.value(ChannelData::g_channelName).toString();
		const auto displayOrder = info.value(ChannelData::g_displayOrder).toInt();

		if (isValidChannel(info)) {
			auto platform = getPlatformById(channelUUID, info);
			if (!platform->isActive()) {
				onActive(channelUUID);
			}

			PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".SelectedChannels: %s %d %s", channelUUID.toStdString().c_str(), channelType, channelName.toStdString().c_str());
		} else {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".InvalidChannels: %s %d %s", channelUUID.toStdString().c_str(), channelType, channelName.toStdString().c_str());
		}
	}

	platformList.sort([](PLSPlatformBase *lValue, PLSPlatformBase *rValue) { return lValue->getChannelOrder() < rValue->getChannelOrder(); });

	uuidOnStarted.clear();
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto &info : platformActived) {
		uuidOnStarted.push_back(info->getChannelUUID());
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".ActivedPlatform: %s %s", info->getChannelUUID().toStdString().c_str(), info->getNameForChannelType());
	}

	if (platformActived.empty()) {
		prepareLiveCallback(false);
		return;
	}

	if (!m_bPrismLive) {
		bool bInternalPlatform = false;
		for (auto item : platformActived) {
			if (PLSServiceType::ST_NAVERTV == item->getServiceType() || PLSServiceType::ST_VLIVE == item->getServiceType()) {
				bInternalPlatform = true;
				break;
			} else if (PLSServiceType::ST_RTMP == item->getServiceType()) {
				auto channelName = item->getChannelName();
				if (channelName == NamesForChannelType[static_cast<int>(PLSServiceType::ST_NAVERTV)] ||
				    channelName == NamesForChannelType[static_cast<int>(PLSServiceType::ST_VLIVE)]) {
					bInternalPlatform = true;
					break;
				}
			}
		}

		if (bInternalPlatform) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.Internal.Greater.1080"));
			prepareLiveCallback(false);
			PLSBasic::Get()->showSettingVideo();
			return;
		}

		if (PLS_PLATFORM_ACTIVIED.size() > 1) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.Multicast.Greater.1080"));
			prepareLiveCallback(false);
			PLSBasic::Get()->showSettingVideo();
			return;
		}
	}

	if (!pls_network_environment_reachable()) {
		PLSAlertView::warning(PLSBasic::Get(), QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.Network.Error"));
		prepareLiveCallback(false);
		return;
	}

	m_iTotalSteps = 0;
	for (auto item : platformActived) {
		if (PLSServiceType::ST_RTMP != item->getServiceType()) {
			item->setCurrStep(++m_iTotalSteps);
		}

		item->setChannelLiveSeq(0);
		item->setTokenRequestStatus(PLSTokenRequestStatus::PLS_GOOD);
	}

	platformActived.front()->onPrepareLive(true);
}

void PLSPlatformApi::onLiveStarted()
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	setLiveStatus(LiveStatus::LiveStarted);

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		liveStartedCallback(false);
		return;
	}

	platformActived.front()->onLiveStarted(true);
}

void PLSPlatformApi::onPrepareFinish()
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	setLiveStatus(LiveStatus::PrepareFinish);

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		prepareFinishCallback();
		return;
	}

	platformActived.front()->onPrepareFinish();
}

void PLSPlatformApi::onLiveStopped()
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	setLiveStatus(LiveStatus::LiveStoped);

	if (isPrismLive()) {
		prism_frontend_dispatch_js_event("prism_events", "{\"type\": \"end\"}");
	}

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		liveStoppedCallback();
		return;
	}

	platformActived.front()->onLiveStopped();
}

void PLSPlatformApi::onLiveEnded()
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	setLiveStatus(LiveStatus::LiveEnded);

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		liveEndedCallback();
		return;
	}

	platformActived.front()->onLiveEnded();
}

void PLSPlatformApi::onRecordingStarted()
{
	m_bRecording = true;

	PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START);
}

void PLSPlatformApi::onRecordingStoped()
{
	m_bRecording = false;

	if (LiveStatus::Normal == m_liveStatus) {
		PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END);
		emit outputStopped();
	}
}

void PLSPlatformApi::activateCallback(PLSPlatformBase *platform, bool value)
{
	if (!m_bDuringActivate) {
		return;
	}

	m_bDuringActivate = false;
	emit channelActive(platform->getChannelUUID(), value);
}

void PLSPlatformApi::deactivateCallback(PLSPlatformBase *platform, bool value)
{
	if (!m_bDuringDeactivate) {
		return;
	}

	m_bDuringDeactivate = false;
	if (value && PLSServiceType::ST_RTMP != platform->getServiceType()) {
	}
	emit channelDeactive(platform->getChannelUUID(), value);
}

void PLSPlatformApi::prepareLiveCallback(bool value)
{
	if (LiveStatus::PrepareLive != m_liveStatus) {
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": Result=%s", BOOL2STR(value));

	m_bApiPrepared = value;
	if (value) {
		setLiveStatus(LiveStatus::ToStart);
	} else {
		m_bApiStarted = false;
		setLiveStatus(LiveStatus::Normal);
	}

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto info : platformActived) {
		info->onAllPrepareLive(value);
	}

	if (!value) {
		emit liveEnded(false, false);
	}

	emit livePrepared(value);
}

void PLSPlatformApi::liveStartedCallback(bool value)
{
	if (LiveStatus::LiveStarted != m_liveStatus) {
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": Result=%s", BOOL2STR(value));

	m_bApiStarted = value;
	setLiveStatus(LiveStatus::Living);

	bool bInteractChannel = false;
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto info : platformActived) {
		if (PLSServiceType::ST_RTMP != info->getServiceType()) {
			bInteractChannel = true;
		}
		info->onAlLiveStarted(value);
	}

	if (value && isPrismLive()) {

	}

	emit liveStarted(value);
}

void PLSPlatformApi::prepareFinishCallback()
{
	if (LiveStatus::PrepareFinish != m_liveStatus) {
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	setLiveStatus(LiveStatus::ToStop);

	emit liveToStop();
}

void PLSPlatformApi::liveStoppedCallback()
{
	if (LiveStatus::LiveStoped != m_liveStatus) {
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	onLiveEnded();
}

void PLSPlatformApi::liveEndedCallback()
{
	if (LiveStatus::LiveEnded != m_liveStatus) {
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	emit enterLivePrepareState(false);
	emit liveEnded(isApiPrepared(), isApiStarted());
	emit liveEndedForUi();

	setLiveStatus(LiveStatus::Normal);

	if (!m_bRecording) {
		emit outputStopped();
	}
}

void PLSPlatformApi::onAddChannel(const QString &which)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s", which.toStdString().c_str());

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".empty: %s", which.toStdString().c_str());
		return;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_channelName).toString();

	if (!isValidChannel(info)) {
		PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".InvalidChannel: %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	auto platform = getPlatformById(which, info);
	if (nullptr != platform) {
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
}

void PLSPlatformApi::onUpdateChannel(const QString &which)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s", which.toStdString().c_str());

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".empty: %s", which.toStdString().c_str());
		return;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_channelName).toString();

	if (!isValidChannel(info)) {
		PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".InvalidChannel: %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());

	auto platform = getPlatformById(which, info);
	if (nullptr != platform) {
		platform->setInitData(info);
	}
}

void PLSPlatformApi::onRemoveChannel(const QString &which)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s", which.toStdString().c_str());

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (!info.isEmpty()) {
		const auto channelType = info.value(ChannelData::g_data_type).toInt();
		const auto channelName = info.value(ChannelData::g_channelName).toString();

		if (!isValidChannel(info)) {
			PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".InvalidChannel: %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
			return;
		}
	}

	auto platform = getPlatformById(which, {});
	if (nullptr != platform) {
		platform->setActive(false);
		emit channelRemoved(platform->getInitData());
		if (PLSServiceType::ST_RTMP == platform->getServiceType()) {
			platformList.remove(platform);
			delete platform;
		}
	}
}

void PLSPlatformApi::onClearChannel()
{
	for (auto iter = platformList.begin(); iter != platformList.end();) {
		auto platform = *iter;
		platform->setActive(false);

		if (PLSServiceType::ST_RTMP == platform->getServiceType()) {
			delete platform;
			iter = platformList.erase(iter);
		} else {
			++iter;
		}
	}
}

PLSServiceType PLSPlatformApi::getServiceType(const QVariantMap &info) const
{
	if (info.isEmpty()) {
		auto text = QString(__FUNCTION__ " info is empty");
		PLS_ERROR(MODULE_PlatformService, text.toStdString().c_str());
		PLSAlertView::critical(PLSBasic::Get(), QTStr("Live.Check.Alert.Title"), text);
		//throw text.toStdString();

		return PLSServiceType::ST_RTMP;
	}

	const auto type = info.value(ChannelData::g_data_type).toInt();
	const auto name = info.value(ChannelData::g_channelName).toString();

	switch (type) {
	case ChannelData::RTMPType:
		return PLSServiceType::ST_RTMP;
	case ChannelData::ChannelType:
		for (int i = 1; i < PLATFORM_SIZE; ++i) {
			if (name == NamesForChannelType[i]) {
				return static_cast<PLSServiceType>(i);
			}
		}
	}

	auto text = QString(__FUNCTION__ " unmatched channel type: %1, %2").arg(type).arg(name);
	PLS_ERROR(MODULE_PlatformService, text.toStdString().c_str());
	PLSAlertView::critical(PLSBasic::Get(), QTStr("Live.Check.Alert.Title"), text);
	//throw text.toStdString();

	return PLSServiceType::ST_RTMP;
}

PLSPlatformBase *PLSPlatformApi::buildPlatform(PLSServiceType type)
{
	PLSPlatformBase *platform = nullptr;

	switch (type) {
	case PLSServiceType::ST_RTMP:
		platform = new PLSPlatformRtmp();
		break;
	case PLSServiceType::ST_TWITCH:
		platform = new PLSPlatformTwitch();
		break;
	case PLSServiceType::ST_YOUTUBE:
		platform = new PLSPlatformYoutube();
		break;
	case PLSServiceType::ST_FACEBOOK:
		platform = new PLSPlatformFacebook();
		break;
	default:
		assert(false);
		break;
	}

	if (nullptr != platform) {
		platformList.push_back(platform);
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformByType(PLSServiceType type)
{
	PLSPlatformBase *platform = nullptr;

	for (auto item : platformList) {
		if (type == item->getServiceType()) {
			platform = item;
			break;
		}
	}

	if (nullptr == platform) {
		platform = buildPlatform(type);
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformById(const QString &which, const QVariantMap &info)
{
	PLSPlatformBase *platform = nullptr;

	for (auto item : platformList) {
		if (which == item->getChannelUUID()) {
			platform = item;
			break;
		}
	}

	if (nullptr == platform && !info.isEmpty()) {
		auto serviceType = getServiceType(info);
		if (PLSServiceType::ST_RTMP == serviceType) {
			platform = buildPlatform(serviceType);
		} else {
			platform = getPlatformByType(getServiceType(info));
		}
	}

	if (nullptr != platform && !info.isEmpty()) {
		platform->setInitData(info);
	}

	if (nullptr == platform) {
		PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".null: %s", which.toStdString().c_str());
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformByName(const QString &name)
{

	PLSPlatformBase *platform = nullptr;

	for (auto item : platformList) {
		if (name == item->getNameForLiveStart()) {
			platform = item;
			break;
		}
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformBySimulcastSeq(int simulcastSeq)
{
	PLSPlatformBase *platform = nullptr;

	for (auto item : platformList) {
		if (item->getChannelLiveSeq() == simulcastSeq) {
			platform = item;
			break;
		}
	}

	return platform;
}

bool PLSPlatformApi::isValidChannel(const QVariantMap &info)
{
	if (info.isEmpty()) {
		throw false;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_channelName).toString();

	switch (channelType) {
	case ChannelData::RTMPType:
	case ChannelData::ChannelType:
		return true;
	default:
		break;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": false %d %s", channelType, channelName.toStdString().c_str());

	return false;
}

void PLSPlatformApi::ensureStopOutput()
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %d %d", m_bRecording, m_liveStatus);
	m_bStopForExit = true;

	if (m_bRecording || (LiveStatus::LiveStarted <= m_liveStatus && m_liveStatus < LiveStatus::LiveStoped)) {
		QEventLoop loop;
		connect(this, &PLSPlatformApi::outputStopped, &loop, &QEventLoop::quit);

		if (m_bRecording) {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": toStopRecord");
			PLSCHANNELS_API->toStopRecord();
		}
		if (LiveStatus::LiveStarted <= m_liveStatus && m_liveStatus < LiveStatus::LiveStoped) {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": toStopBroadcast");
			PLSCHANNELS_API->toStopBroadcast();
		}

		QTimer::singleShot(5000, &loop, &QEventLoop::quit);

		loop.exec();
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".end: %d %d", m_bRecording, m_liveStatus);
	}
}

void PLSPlatformApi::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
	PLSPlatformApi *self = reinterpret_cast<PLSPlatformApi *>(private_data);

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		self->onLiveStarted();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		self->onLiveStopped();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		self->onRecordingStarted();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		self->onRecordingStoped();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		self->ensureStopOutput();
		break;
	default:
		break;
	}
}
