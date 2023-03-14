#include "PLSPlatformApi.h"

#include <string>
#include <algorithm>
#include <functional>
#include <iterator>
#include <QApplication>
#include <QString>

#include "PLSAction.h"
#include "frontend-api.h"
#include "alert-view.hpp"
#include "ChannelConst.h"
#include "window-basic-main.hpp"
#include "PLSLiveInfoDialogs.h"
#include "prism/PLSPlatformPrism.h"
#include "log.h"
#include "log/log.h"
#include "twitch/PLSLiveInfoTwitch.h"
#include "PLSHttpApi\PLSHttpHelper.h"
#include "channels/ChannelsDataApi/ChannelConst.h"
#include "channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "pls-net-url.hpp"
#include "navertv/PLSPlatformNaverTV.h"
#include "band/PLSPlatformBand.h"
#include "PLSLiveEndDialog.h"
#include "qwidget.h"
#include "main-view.hpp"
#include "youtube/PLSAPIYoutube.h"
#include "PLSServerStreamHandler.hpp"
#include "window-basic-main.hpp"
#include "ResolutionGuidePage.h"

#define PRISM_MAX_OUT_Y 1080

using namespace std;

extern const QString translatePlatformName(const QString &platformName);

PLSPlatformApi *PLSPlatformApi::instance()
{
	static PLSPlatformApi *_instance = nullptr;

	if (nullptr == _instance) {
		mosqpp::lib_init();

		_instance = new PLSPlatformApi();
		_instance->moveToThread(qApp->thread()); //zhangdewen force move to main thread
		prism_frontend_web_invoked = PLSPlatformApi::invokedByWeb;
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
			delete _instance;
			_instance = nullptr;
		});
	}

	return _instance;
}

PLSPlatformApi::PLSPlatformApi() : platformListMutex(QMutex::Recursive)
{
	PLS_INFO(MODULE_PlatformService, "%p %s", this, __FUNCTION__);

	m_timerMQTT.setInterval(30000);
	m_timerMQTT.setSingleShot(true);
}

PLSPlatformApi::~PLSPlatformApi()
{
	PLS_INFO(MODULE_PlatformService, "%p %s", this, __FUNCTION__);

	obs_frontend_remove_event_callback(onFrontendEvent, this);

	for (auto item : platformList) {
		delete item;
	}
	platformList.clear();
}

bool PLSPlatformApi::initialize()
{
	obs_frontend_add_event_callback(onFrontendEvent, this);
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
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelCreateError, this, &PLSPlatformApi::onRemoveChannel);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigAllClear, this, &PLSPlatformApi::onClearChannel);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::broadcastGo, this, [=]() {
		emit enterLivePrepareState(true);
		onPrepareLive();
	});
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::stopBroadcastGo, this, &PLSPlatformApi::onPrepareFinish);
	connect(&m_timerMQTT, &QTimer::timeout, this, [] { pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QTStr("MQTT.Timeout")); });
	//Notice, onWebRequest is send from Browser thread, not main thread
	connect(this, &PLSPlatformApi::onWebRequest, this, &PLSPlatformApi::doWebRequest);
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

	PLS_LIVE_INFO(MODULE_PlatformService, "save straming info name=%s, StreamUrl=%s", platform.c_str(), server.c_str());
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

	QMutexLocker locker(&platformListMutex);
	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [](const auto v) { return v->isActive(); });

	return lst;
}

bool PLSPlatformApi::isPlatformActived(PLSServiceType serviceType) const
{
	QMutexLocker locker(&platformListMutex);
	return any_of(platformList.begin(), platformList.end(), [=](auto item) { return item->isActive() && item->getServiceType() == serviceType; });
}

void PLSPlatformApi::checkDirectPush()
{
	auto platformActived = getActivePlatforms();
	if (m_bPrismLive && platformActived.size() == 1) {
		auto platform = platformActived.front();
		QString streamUrl = platform->getStreamUrl();
		if (PLSGpopData::instance()->isPresetRTMP(streamUrl)) {
			m_bPrismLive = false;
		}
	}
}

PLSPlatformTwitch *PLSPlatformApi::getPlatformTwitch()
{
	return dynamic_cast<PLSPlatformTwitch *>(getPlatformByType(PLSServiceType::ST_TWITCH));
}

PLSPlatformYoutube *PLSPlatformApi::getPlatformYoutube(bool froceCreate)
{
	return dynamic_cast<PLSPlatformYoutube *>(getPlatformByType(PLSServiceType::ST_YOUTUBE, froceCreate));
}

PLSPlatformFacebook *PLSPlatformApi::getPlatformFacebook()
{
	return dynamic_cast<PLSPlatformFacebook *>(getPlatformByType(PLSServiceType::ST_FACEBOOK));
}

PLSPlatformVLive *PLSPlatformApi::getPlatformVLiveActive()
{
	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_VLIVE && platform->isActive()) {
			return dynamic_cast<PLSPlatformVLive *>(platform);
		}
	}
	return nullptr;
}

PLSPlatformAfreecaTV *PLSPlatformApi::getPlatformAfreecaTV()
{
	return dynamic_cast<PLSPlatformAfreecaTV *>(getPlatformByType(PLSServiceType::ST_AFREECATV));
}

list<PLSPlatformNaverShoppingLIVE *> PLSPlatformApi::getPlatformNaverShoppingLIVE()
{
	list<PLSPlatformNaverShoppingLIVE *> naverShoppingLIVEs;

	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_NAVER_SHOPPING_LIVE) {
			naverShoppingLIVEs.push_back(dynamic_cast<PLSPlatformNaverShoppingLIVE *>(platform));
		}
	}

	return naverShoppingLIVEs;
}

PLSPlatformNaverShoppingLIVE *PLSPlatformApi::getPlatformNaverShoppingLIVEActive()
{
	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_NAVER_SHOPPING_LIVE && platform->isActive()) {
			return dynamic_cast<PLSPlatformNaverShoppingLIVE *>(platform);
		}
	}
	return nullptr;
}

list<PLSPlatformNaverTV *> PLSPlatformApi::getPlatformNaverTV()
{
	list<PLSPlatformNaverTV *> naverTVList;

	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_NAVERTV) {
			naverTVList.push_back(dynamic_cast<PLSPlatformNaverTV *>(platform));
		}
	}

	return naverTVList;
}
PLSPlatformNaverTV *PLSPlatformApi::getPlatformNaverTVActive()
{
	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_NAVERTV && platform->isActive()) {
			return dynamic_cast<PLSPlatformNaverTV *>(platform);
		}
	}
	return nullptr;
}

void PLSPlatformApi::onActive(const QString &which)
{
	m_bDuringActivate = true;

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".empty: %s", which.toStdString().c_str());
		return;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_platformName).toString();

	if (!isValidChannel(info)) {
		PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".InvalidChannel: %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	auto platform = getPlatformById(which, info);
	if (nullptr != platform) {
		if (!platform->isActive()) {
			platform->onActive();
		}
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
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %p %s", platform, which.toStdString().c_str());
		if (platform->isActive()) {
			platform->onInactive();
		}
	}
}

void PLSPlatformApi::setLiveStatus(LiveStatus value)
{
	m_liveStatus = value;
	if (LiveStatus::PrepareLive == value) {
		PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START);
	} else if (LiveStatus::Normal == value) {
		if (!isRecording() && PLSBasic::Get()) {
			PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END);
		}
	}
}

void PLSPlatformApi::onPrepareLive()
{
	setLiveStatus(LiveStatus::PrepareLive);

	//PRISM/LiuHaibin/20210906/Pre-check encoders
	bool encoderAvailable = PLSBasic::Get()->CheckStreamEncoder();
	if (!encoderAvailable) {
		assert(false);
		prepareLiveCallback(false);
		return;
	}

	m_bApiPrepared = false;
	m_bApiStarted = false;

	m_bPrismLive = true;
	uint32_t out_cx = config_get_uint(PLSBasic::Get()->Config(), "Video", "OutputCX");
	uint32_t out_cy = config_get_uint(PLSBasic::Get()->Config(), "Video", "OutputCY");
	//check output resolution is greater 1080p or whether contain vlive channel,if has then set not prism living
	if (min(out_cx, out_cy) > PRISM_MAX_OUT_Y || PLSPlatformApi::isContainVliveChannel()) {
		PLS_INFO(MODULE_PlatformService, "%s %s output resolution greater 1080p(%dx%d) or contains vlive channel", PrepareInfoPrefix, __FUNCTION__, out_cx, out_cy);
		m_bPrismLive = false;
	}

	QSet<QString> setChannelsFromDashboard;
	for (auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		const auto channelUUID = info.value(ChannelData::g_channelUUID).toString();
		//const auto channelType = info.value(ChannelData::g_data_type).toInt();
		const auto channelName = info.value(ChannelData::g_platformName).toString();
		//const auto displayOrder = info.value(ChannelData::g_displayOrder).toInt();
		if (isValidChannel(info)) {
			setChannelsFromDashboard.insert(channelUUID);
			auto platform = getPlatformById(channelUUID, info);
			if (nullptr != platform && !platform->isActive()) {
				onActive(channelUUID);
			}
		}
	}

	{
		QMutexLocker locker(&platformListMutex);
		platformList.sort([](PLSPlatformBase *lValue, PLSPlatformBase *rValue) { return lValue->getChannelOrder() < rValue->getChannelOrder(); });
	}

	uuidOnStarted.clear();
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto &info : platformActived) {
		if (!setChannelsFromDashboard.contains(info->getChannelUUID())) {
			onInactive(info->getChannelUUID());
		}
	}
	platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto &item : platformActived) {
		uuidOnStarted.push_back(item->getChannelUUID());
		PLS_INFO(MODULE_PlatformService, "%s %s ActivedPlatform: %p,uuid=%s,name=%s", PrepareInfoPrefix, __FUNCTION__, item, item->getChannelUUID().toStdString().c_str(),
			 item->getNameForChannelType());
	}

	if (platformActived.empty()) {
		PLS_INFO(MODULE_PlatformService, "%s %s ActiveChannel list is Empty", PrepareInfoPrefix, __FUNCTION__);
		prepareLiveCallback(false);
		return;
	}

	if (m_bPrismLive && pls_get_gcc_data() != HTTP_GCC_KR && platformActived.size() == 1) {
		PLS_INFO(MODULE_PlatformService, "%s %s pls_get_gcc_data() is not KR", PrepareInfoPrefix, __FUNCTION__);
		m_bPrismLive = false;
	}

	if (m_bPrismLive) {
		auto _actived = PLS_PLATFORM_ACTIVIED;
		auto platform = _actived.front();
		if (platform->getServiceType() == PLSServiceType::ST_NAVER_SHOPPING_LIVE) {
			PLS_INFO(MODULE_PlatformService, "%s %s contains NaverShopping", PrepareInfoPrefix, __FUNCTION__);
			m_bPrismLive = false;
		}
	}

	if (!m_bPrismLive) {
		if (PLS_PLATFORM_ACTIVIED.size() > 1) {
			auto alertResult = QDialogButtonBox::Cancel;
			if (isRecording()) {
				PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Live.Check.Internal.Greater.1080.Recording"));
			} else {
				alertResult = PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Live.Check.Multicast.Greater.1080"));
			}
			PLS_INFO(MODULE_PlatformService, "%s %s present 1080p alert isRecording(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(isRecording()));
			prepareLiveCallback(false);
			if (QDialogButtonBox::Ok == alertResult) {
				ResolutionGuidePage::setVisibleOfGuide(PLSBasic::Get());
			}
			return;
		}
	}

	QString strError;
	if (!PLSServerStreamHandler::instance()->isSupportedResolutionFPS(strError)) {
		auto platformActived = PLS_PLATFORM_ACTIVIED;
		bool containvLive = any_of(platformActived.begin(), platformActived.end(), [](PLSPlatformBase *pPlatform) { return PLSServiceType::ST_VLIVE == pPlatform->getServiceType(); });
		auto alertResult = QDialogButtonBox::Cancel;
		if (isRecording()) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), strError);
		} else {
			if (containvLive) {
				alertResult = PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Live.Check.Internal.VLive.Fps.Error"));
			} else {
				alertResult = PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), strError);
			}
		}
		PLS_INFO(MODULE_PlatformService, "%s %s resolution(%s) and fps(%s) not support, isRecording(%s)", PrepareInfoPrefix, __FUNCTION__,
			 PLSServerStreamHandler::instance()->getOutputResolution().toStdString().c_str(), PLSServerStreamHandler::instance()->getOutputFps().toStdString().c_str(),
			 BOOL2STR(isRecording()));
		prepareLiveCallback(false);
		if (QDialogButtonBox::Ok == alertResult) {
			ResolutionGuidePage::setVisibleOfGuide(PLSBasic::Get());
		}
		return;
	}

	if (PLS_PLATFORM_ACTIVIED.size() > 1) {
		auto iMultiplePlatformMaxBitrate = PLSGpopData::instance()->getMultiplePlatformMaxBitrate();
		if (auto videoBitrate = getOutputBitrate(); videoBitrate > iMultiplePlatformMaxBitrate) {
			PLS_INFO(MODULE_PlatformService, "%s %s max bitrate error, current bitrate: %d, max bitrate: %d", PrepareInfoPrefix, __FUNCTION__, videoBitrate, iMultiplePlatformMaxBitrate);
			auto alertResult = PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Broadcast.Bitrate.Too.High").arg(iMultiplePlatformMaxBitrate));
			prepareLiveCallback(false);
			if (QDialogButtonBox::Ok == alertResult) {
				PLSBasic::Get()->showSettingVideo();
			}
			return;
		}
	}

	if (!pls_get_network_state()) {
		PLS_INFO(MODULE_PlatformService, "%s %s current network environment diabled", PrepareInfoPrefix, __FUNCTION__);
		PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("login.check.note.network"));
		prepareLiveCallback(false);
		return;
	}

	m_iTotalSteps = 0;
	for (auto item : platformActived) {
		if (PLSServiceType::ST_RTMP != item->getServiceType()) {
			item->setCurrStep(++m_iTotalSteps);
		}

		item->setChannelLiveSeq(0);
		item->setMqttStatus(PLSPlatformMqttStatus::PMS_NONE);
		item->setMqttFirstBroadcastOn(false);
		item->setTokenRequestStatus(PLSTokenRequestStatus::PLS_GOOD);
	}

	//set watermark display option
	QString guide;
	bool enabled;
	bool selected;
	PLSServerStreamHandler::instance()->getWatermarkInfo(guide, enabled, selected);
	config_set_bool(App()->GlobalConfig(), "General", "Watermark", selected);
	obs_watermark_set_enabled(selected);

	//check watermark is valid
	if (!PLSServerStreamHandler::instance()->isValidWatermark() || !PLSServerStreamHandler::instance()->isValidOutro()) {
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("watermark.resource.is.not.existed.tip"));
		prepareLiveCallback(false);
		return;
	}

	//call the first platform onPrepareLive method
	platformActived.front()->onPrepareLive(true);
}

void PLSPlatformApi::onLiveStarted()
{
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
	if (m_liveStatus >= LiveStatus::PrepareFinish) {
		return;
	}
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
	if (m_liveStatus < LiveStatus::ToStart || m_liveStatus >= LiveStatus::LiveStoped) {
		PLS_WARN(MODULE_PlatformService, __FUNCTION__, ".Unexpected status, %d", m_liveStatus);
		return;
	}

	setLiveStatus(LiveStatus::LiveStoped);

	if (nullptr != prism_frontend_dispatch_js_event) {
		const char *pszTypeEnd = "{\"type\": \"end\"}";
		prism_frontend_dispatch_js_event("prism_events", pszTypeEnd);
	}
	if (nullptr != m_pMQTT) {
		PLSMosquitto::stopInstance(m_pMQTT);
		m_pMQTT = nullptr;
		m_timerMQTT.stop();
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
		if (!m_bReplayBuffer)
			emit outputStopped();
	}
}

void PLSPlatformApi::onReplayBufferStarted()
{
	m_bReplayBuffer = true;
}

void PLSPlatformApi::onReplayBufferStoped()
{
	m_bReplayBuffer = false;

	if (LiveStatus::Normal == m_liveStatus && !m_bRecording) {
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
		sendWebPrismPlatformClose(platform);
	}
	emit channelDeactive(platform->getChannelUUID(), value);
}

void PLSPlatformApi::prepareLiveCallback(bool value)
{
	if (LiveStatus::PrepareLive != m_liveStatus) {
		return;
	}

	m_bApiPrepared = value;
	if (value) {
		PLS_INFO(MODULE_PlatformService, "%s %s value(%s) setLiveStatus(ToStart)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
		setLiveStatus(LiveStatus::ToStart);
	} else {
		PLS_INFO(MODULE_PlatformService, "%s %s value(%s) setLiveStatus(Normal)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
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
	if (value) {
		sendWebPrismInit();

		if (!PLSCHANNELS_API->isRehearsaling()) {
			m_pMQTT = PLSMosquitto::createInstance();
			connect(m_pMQTT, &PLSMosquitto::onMessage, this, &PLSPlatformApi::onMqttMessage);
			auto bStatistic = m_bPrismLive;
			if (!bStatistic) {
				bStatistic = any_of(platformActived.begin(), platformActived.end(), [](PLSPlatformBase *pPlatform) {
					switch (pPlatform->getServiceType()) {
					case PLSServiceType::ST_AFREECATV:
					case PLSServiceType::ST_NAVER_SHOPPING_LIVE:
						return true;
					default:
						return false;
					}
				});
			}
			PLSMosquitto::startIntance(m_pMQTT, PLS_PLATFORM_PRSIM->getVideoSeq(), bStatistic);

			if (bStatistic) {
				m_timerMQTT.start();
			}
		}
	}

	for (const auto &uuid : getUuidOnStarted()) {
		const auto &mSourceData = PLSCHANNELS_API->getChanelInfoRef(uuid);
		if (mSourceData.contains(ChannelData::g_viewers)) {
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_viewers, QString("0"));
		}
		if (mSourceData.contains(ChannelData::g_likes)) {
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_likes, QString("0"));
		}
		if (mSourceData.contains(ChannelData::g_comments)) {
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_comments, QString("0"));
		}
		if (mSourceData.contains(ChannelData::g_totalViewers)) {
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_totalViewers, QString("0"));
		}
	}

	if (value) {
		QMetaObject::invokeMethod(this, &PLSPlatformApi::showStartMessageIfNeeded, Qt::QueuedConnection);
	}

	emit liveStarted(value);
}

void PLSPlatformApi::prepareFinishCallback()
{
	if (LiveStatus::PrepareFinish != m_liveStatus) {
		return;
	}
	setLiveStatus(LiveStatus::ToStop);

	emit liveToStop();
}

void PLSPlatformApi::liveStoppedCallback()
{
	if (LiveStatus::LiveStoped != m_liveStatus) {
		return;
	}

	onLiveEnded();
}

void PLSPlatformApi::liveEndedCallback()
{
	if (LiveStatus::LiveEnded != m_liveStatus) {
		return;
	}
	emit enterLivePrepareState(false);
	emit liveEnded(isApiPrepared(), isApiStarted());
	emit liveEndedForUi();
	setLiveStatus(LiveStatus::Normal);

	if (!m_bRecording && !m_bReplayBuffer) {
		emit outputStopped();
	}
}

void PLSPlatformApi::onAddChannel(const QString &which)
{
	//PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %s", which.toStdString().c_str());

	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".empty: %s", which.toStdString().c_str());
		return;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_platformName).toString();

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
	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_INFO(MODULE_PlatformService, "%s %s empty %s", PrepareInfoPrefix, __FUNCTION__, which.toStdString().c_str());
		return;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_platformName).toString();
	if (!isValidChannel(info)) {
		PLS_INFO(MODULE_PlatformService, "%s %s InvalidChannel: %d %s %s", PrepareInfoPrefix, __FUNCTION__, channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

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
		const auto channelName = info.value(ChannelData::g_platformName).toString();

		if (!isValidChannel(info)) {
			PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".InvalidChannel: %d %s %s", channelType, channelName.toStdString().c_str(), which.toStdString().c_str());
			return;
		}
	}

	auto platform = getPlatformById(which, {}, true);
	if (nullptr != platform) {
		platform->setActive(false);
		emit channelRemoved(platform->getInitData());
		if (!platform->isSingleChannel()) {
			QMutexLocker locker(&platformListMutex);
			platformList.remove(platform);
			delete platform;
		}
	}
}

void PLSPlatformApi::onClearChannel()
{
	QMutexLocker locker(&platformListMutex);
	for (auto iter = platformList.begin(); iter != platformList.end();) {
		auto platform = *iter;
		platform->setActive(false);

		if (!platform->isSingleChannel()) {
			iter = platformList.erase(iter);
			delete platform;
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

		assert(false);
		return PLSServiceType::ST_RTMP;
	}

	const auto type = info.value(ChannelData::g_data_type).toInt();
	const auto name = info.value(ChannelData::g_platformName).toString();

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
	case PLSServiceType::ST_NAVERTV:
		platform = new PLSPlatformNaverTV();
		break;
	case PLSServiceType::ST_VLIVE:
		platform = new PLSPlatformVLive();
		break;
	case PLSServiceType::ST_BAND:
		platform = new PLSPlatformBand();
		break;
	case PLSServiceType::ST_AFREECATV:
		platform = new PLSPlatformAfreecaTV();
		break;
	case PLSServiceType::ST_NAVER_SHOPPING_LIVE:
		platform = new PLSPlatformNaverShoppingLIVE();
		break;
	default:
		assert(false);
		break;
	}

	if (nullptr != platform) {
		platform->moveToThread(this->thread());
		QMutexLocker locker(&platformListMutex);
		platformList.push_back(platform);
	} else {
		assert(false);
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ ".null %d", type);
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformByType(PLSServiceType type, bool froceCreate)
{
	PLSPlatformBase *platform = nullptr;

	{
		QMutexLocker locker(&platformListMutex);
		for (auto item : platformList) {
			if (type == item->getServiceType()) {
				platform = item;
				break;
			}
		}
	}

	if (nullptr == platform && froceCreate) {
		platform = buildPlatform(type);
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformByTypeNotFroceCreate(const QString &uuid)
{
	PLSPlatformBase *platform = nullptr;
	QMutexLocker locker(&platformListMutex);
	for (auto item : platformList) {
		if (uuid == item->getChannelUUID()) {
			platform = item;
			break;
		}
	}
	return platform;
}

list<PLSPlatformBase *> PLSPlatformApi::getPlatformsByType(PLSServiceType type)
{
	list<PLSPlatformBase *> lst;

	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == type) {
			lst.push_back(platform);
		}
	}

	return lst;
}

PLSPlatformBase *PLSPlatformApi::getPlatformById(const QString &which, const QVariantMap &info, bool bRemove)
{
	const auto channelUUID = info.value(ChannelData::g_channelUUID).toString().toUtf8();
	//const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_platformName).toString().toUtf8();
	//const auto displayOrder = info.value(ChannelData::g_displayOrder).toInt();

	PLSPlatformBase *platform = nullptr;
	auto serviceType = PLSServiceType::ST_RTMP;
	if (!info.isEmpty()) {
		serviceType = getServiceType(info);
	}

	{
		QMutexLocker locker(&platformListMutex);
		for (auto item : platformList) {
			if (which == item->getChannelUUID() || (item->isSingleChannel() && item->getServiceType() == serviceType)) {
				platform = item;
				break;
			}
		}
	}

	if (nullptr == platform && !info.isEmpty() && !bRemove) {
		platform = buildPlatform(serviceType);
	}

	if (nullptr != platform && !info.isEmpty()) {
		platform->setInitData(info);
	}

	if (nullptr == platform && !bRemove) {
		assert(false);
		PLS_WARN(MODULE_PlatformService, __FUNCTION__ ".null: %s", which.toStdString().c_str());
	}
	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformByName(const QString &name)
{

	PLSPlatformBase *platform = nullptr;

	QMutexLocker locker(&platformListMutex);
	for (auto item : platformList) {
		if (name == item->getNameForLiveStart()) {
			platform = item;
			break;
		}
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getActivePlatformByName(const QString &name)
{

	PLSPlatformBase *platform = nullptr;

	QMutexLocker locker(&platformListMutex);
	for (auto item : platformList) {
		if (item->isActive() && name == item->getNameForLiveStart()) {
			platform = item;
			break;
		}
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformBySimulcastSeq(int simulcastSeq)
{
	PLSPlatformBase *platform = nullptr;

	QMutexLocker locker(&platformListMutex);
	for (auto item : platformList) {
		if (item->getChannelLiveSeq() == simulcastSeq) {
			platform = item;
			break;
		}
	}

	return platform;
}

bool PLSPlatformApi::isContainVliveChannel()
{
	bool conatinVlive = false;
	for (auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		const auto channelType = info.value(ChannelData::g_data_type).toInt();
		const auto channelName = info.value(ChannelData::g_platformName).toString();

		if (channelType == ChannelData::ChannelType && channelName == VLIVE) {
			conatinVlive = true;
			break;
		}
	}
	return conatinVlive;
}

bool PLSPlatformApi::isValidChannel(const QVariantMap &info)
{
	if (info.isEmpty()) {
		throw false;
	}

	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_platformName).toString();

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
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": %d %d %d", m_bReplayBuffer, m_bRecording, m_liveStatus);
	m_bStopForExit = true;

	if (m_bRecording || m_bReplayBuffer || (LiveStatus::LiveStarted <= m_liveStatus && m_liveStatus < LiveStatus::LiveStoped)) {
		QEventLoop loop;
		connect(this, &PLSPlatformApi::outputStopped, &loop, &QEventLoop::quit);

		if (LiveStatus::LiveStarted <= m_liveStatus && m_liveStatus < LiveStatus::LiveStoped) {
			PLS_LIVE_INFO(MODULE_PlatformService, "live abort because obs exit event stop broadcast");
			PLSCHANNELS_API->toStopBroadcast();
		}

		if (m_bReplayBuffer) {
			PLSBasic::Get()->StopReplayBuffer();
		}

		if (m_bRecording) {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ ": toStopRecord");
			PLSCHANNELS_API->toStopRecord();
		}

		QTimer::singleShot(15000, &loop, &QEventLoop::quit);

		loop.exec();
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".end: %d %d %d", m_bReplayBuffer, m_bRecording, m_liveStatus);
	}
}

void PLSPlatformApi::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
	PLSPlatformApi *self = reinterpret_cast<PLSPlatformApi *>(private_data);

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		PLS_LIVE_INFO(MODULE_PlatformService, "receive obs start streaming callback message");
		self->onLiveStarted();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		PLS_LIVE_INFO(MODULE_PlatformService, "receive obs stop streaming callback message");
		self->onLiveStopped();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		PLS_LIVE_INFO(MODULE_PlatformService, "receive obs start recording callback message");
		self->onRecordingStarted();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		PLS_LIVE_INFO(MODULE_PlatformService, "receive obs stop recording callback message");
		self->onRecordingStoped();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		PLS_INFO(MODULE_PlatformService, "%s receive obs start replaybuffer callback message", LiveInfoPrefix);
		self->onReplayBufferStarted();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		PLS_INFO(MODULE_PlatformService, "%s receive obs stop replaybuffer callback message", LiveInfoPrefix);
		self->onReplayBufferStoped();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		self->ensureStopOutput();
		break;
	default:
		break;
	}
}

QJsonObject PLSPlatformApi::getWebPrismInit(bool onlyYoutube)
{
	QJsonObject data;

	QJsonArray platforms;
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto item : platformActived) {
		if (onlyYoutube && PLSServiceType::ST_YOUTUBE != item->getServiceType()) {
			continue;
		}
		if (PLSServiceType::ST_RTMP == item->getServiceType()) {
			continue;
		}
		if (PLSServiceType::ST_BAND == item->getServiceType()) {
			continue;
		}
		platforms.append(item->getWebChatParams());
	}
	data.insert("hasRtmp", false);
	data.insert("videoSeq", PLS_PLATFORM_PRSIM->getVideoSeq());
	if (platforms.size() > 0) {
		data.insert("platforms", platforms);
	}

	QJsonObject root;
	root.insert("type", "init");
	root.insert("data", data);
	return root;
}

void PLSPlatformApi::sendWebPrismInit(bool onlyYoutube)
{
	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(getWebPrismInit(onlyYoutube)).toJson().constData());
	}
}

void PLSPlatformApi::sendWebChat(QString value)
{
	QJsonArray filteredValue;
	QString platformName;
	for (auto pltchatv : QJsonDocument::fromJson(value.toUtf8()).array()) {
		auto pltchat = pltchatv.toObject();
		auto platform = getActivePlatformByName(pltchat["livePlatform"].toString());
		if (platform && platform->isMqttChatCanShow(pltchat)) {
			filteredValue.append(pltchat);
			platformName.append(QString(" %1").arg(pltchat["livePlatform"].toString()));
		}
	}

	if (filteredValue.isEmpty()) {
		return;
	}

	PLS_LIVE_INFO(MODULE_PlatformService, "mqtt-status:%s receive chat message", platformName.toStdString().c_str());
	QJsonObject data;
	data.insert("message", QString::fromUtf8(QJsonDocument(filteredValue).toJson()));

	QJsonObject root;
	root.insert("type", "chat");
	root.insert("data", data);

	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(root).toJson().constData());
	}
}

void PLSPlatformApi::forwardWebMessage(const QJsonObject &data)
{
	QJsonObject root;
	root.insert("type", "broadcast");
	root.insert("data", data);

	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(root).toJson().constData());
	}
}

void PLSPlatformApi::forwardWebMessagePrivateChanged(PLSPlatformBase *platform, bool isPrivate)
{
	QJsonObject root;
	root.insert("type", "permission");
	QJsonObject dataObj;
	dataObj["isPrivate"] = isPrivate;
	dataObj["platform"] = platform->getNameForLiveStart();
	root.insert("data", dataObj);

	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(root).toJson().constData());
	}
}

void PLSPlatformApi::sendWebPrismToken(PLSPlatformBase *platform)
{
	QJsonObject data;
	data.insert("platform", QString::fromStdString(platform->getNameForLiveStart()));
	data.insert("token", platform->getChannelToken());

	QJsonObject root;
	root.insert("type", "token");
	root.insert("data", data);

	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(root).toJson().constData());
	}
}

void PLSPlatformApi::sendWebPrismPlatformClose(PLSPlatformBase *platform)
{
	QJsonObject data;
	data.insert("platform", QString::fromStdString(platform->getNameForLiveStart()));

	QJsonObject root;
	root.insert("type", "platform_close");
	root.insert("data", data);

	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(root).toJson().constData());
	}
}

void PLSPlatformApi::sendWebChatTabShown(const QString &channelName, bool isAllTab)
{
	QString platName = QString();
	auto platformActived = PLS_PLATFORM_ALL;
	for (auto item : platformActived) {
		if (PLSServiceType::ST_RTMP == item->getServiceType()) {
			continue;
		}
		if (channelName != item->getChannelName()) {
			continue;
		}
		platName = item->getNameForLiveStart();
		break;
	}

	if (isAllTab) {
		platName = "ALL";
	}
	if (platName.isEmpty()) {
		return;
	}

	QJsonObject data;
	data.insert("platform", platName);

	QJsonObject root;
	root.insert("type", "showTab");
	root.insert("data", data);

	if (nullptr != prism_frontend_dispatch_js_event) {
		prism_frontend_dispatch_js_event("prism_events", QJsonDocument(root).toJson().constData());
	}
}

void PLSPlatformApi::doStatRequest(const QJsonObject &data)
{
	auto keyYoutube = "youtube";
	auto keyTwitch = "twitch";
	auto keyNaverTV = "naverTv";
	auto keyAfreecaTV = "afreecatv";
	const static QString keyShoppingLive = "shoppingLive";
	if (data.contains(keyYoutube)) {
		auto youtube = data[keyYoutube].toObject();
		auto likeCount = youtube["likeCount"].toInt();
		auto viewCount = youtube["viewCount"].toInt();
		auto platformService = getPlatformYoutube();
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_viewers, viewCount);
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_likes, likeCount);
	} else if (data.contains(keyTwitch)) {
		auto twitch = data[keyTwitch].toObject();
		auto viewCount = twitch["viewCount"].toInt();

		auto platformService = getPlatformTwitch();
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_viewers, viewCount);
	} else if (data.contains(keyNaverTV)) {
		auto naverTV = data[keyNaverTV].toObject();
		qint64 viewCount = (qint64)naverTV["viewCount"].toDouble(); // accuViewCount

		if (auto platform = getPlatformNaverTVActive(); platform) {
			PLSCHANNELS_API->setValueOfChannel(platform->getChannelUUID(), ChannelData::g_viewers, QString::number(viewCount));
		}
	} else if (data.contains(keyAfreecaTV)) {
		auto platform = data[keyAfreecaTV].toObject();
		auto viewCount = platform["viewCount"].toInt();
		auto totalViewCount = platform["accuViewCount"].toInt();

		auto platformService = getPlatformAfreecaTV();
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_viewers, viewCount);
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_totalViewers, totalViewCount);
	} else if (data.contains(keyShoppingLive)) {
		auto platform = data[keyShoppingLive].toObject();
		auto likeCount = platform["likeCount"].toInt();
		auto viewCount = platform["viewCount"].toInt();
		auto platformService = getPlatformNaverShoppingLIVEActive();
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_viewers, viewCount);
		PLSCHANNELS_API->setValueOfChannel(platformService->getChannelUUID(), ChannelData::g_likes, likeCount);
	}
}

void PLSPlatformApi::requestWebChat(const QJsonObject &data)
{
	auto text = data["message"].toString();
	auto sendPlatform = data["platform"].toString();

	QJsonArray messages;
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto item : platformActived) {
		if (!item->isSendChatToMqtt()) {
			continue;
		}
		if (PLSServiceType::ST_YOUTUBE == item->getServiceType()) {
			auto youtube = dynamic_cast<PLSPlatformYoutube *>(item);
			if (!youtube || youtube->isPrivateStatus()) {
				continue;
			}
		}
		auto itemName = item->getNameForLiveStart();
		if (!sendPlatform.isEmpty() && sendPlatform.compare(itemName, Qt::CaseInsensitive)) {
			continue;
		}

		QJsonObject message;
		message["platform"] = itemName;
		message["simulcastSeq"] = item->getChannelLiveSeq();
		message["message"] = text;

		auto params = item->getMqttChatParams();
		for (auto iter = params.constBegin(); iter != params.constEnd(); ++iter) {
			message.insert(iter.key(), iter.value());
		}

		messages.append(message);
	}

	QJsonObject root;
	root["messages"] = messages;

	auto url = QString("%1/chat/%2/write").arg(PRISM_API_BASE.arg(PRISM_SSL)).arg(PLS_PLATFORM_PRSIM->getVideoSeq());
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(code)
		Q_UNUSED(data)
		Q_UNUSED(context)
		PLS_LIVE_INFO(MODULE_PlatformService, "request send chat url success", url.toStdString().c_str());
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_LIVE_ERROR(MODULE_PlatformService, "request send chat url error: %d-%d", code, error);
	};

	PLSHmacNetworkReplyBuilder builder(url);
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setCookie(pls_get_prism_cookie()).setJsonObject(root);
	PLS_HTTP_HELPER->connectFinished(builder.put(), this, _onSucceed, _onFail);
	PLS_LIVE_INFO(MODULE_PlatformService, "request send chat url:%s", url.toStdString().c_str());
}

void PLSPlatformApi::doNoticeLong(const QJsonObject &data)
{
	if (getPlatformNaverShoppingLIVEActive()) {
		return;
	}

	const auto leftMinutes = data["leftMinutes"].toInt();
	PLS_LIVE_INFO(MODULE_PlatformService, "Notice long leftMinutes:%d", leftMinutes);
	if (leftMinutes == 0) {
		if (isLiving()) {
			PLS_LIVE_INFO(MODULE_PlatformService, "FinishedBy Prism because mqtt NOTICE_LONG_BROADCAST left minute 0");
			PLSCHANNELS_API->toStopBroadcast();
		}
		PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("MQTT.LongBroadcast.End"));
	} else if (leftMinutes == 1) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.LongBroadcast.1Minute"));
	} else if (leftMinutes <= 10) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.LongBroadcast.Less.10Minuts").arg(leftMinutes));
	} else if (leftMinutes <= 60) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.LongBroadcast.Less.1Hour"));
	}
}

void PLSPlatformApi::showMqttError(PLSPlatformBase *platform, PLSPlatformMqttStatus status)
{
	const auto channelName = platform->getInitData().value(ChannelData::g_platformName).toString();
	auto displayPlatformName = translatePlatformName(channelName);
	PLS_LIVE_INFO(MODULE_PlatformService, "%s show mqtt error,status:%d", platform->getNameForChannelType(), status);
	switch (status) {
	case PLSPlatformMqttStatus::PMS_CANNOT_FIND_SERVER:
	case PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_SERVER:
		if (PLSServiceType::ST_FACEBOOK == platform->getServiceType()) {
			if (getActivePlatforms().size() > 1) {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QTStr("LiveInfo.live.error.stoped.byRemote").arg(displayPlatformName));
			} else {
				PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("LiveInfo.live.error.stoped.byRemote").arg(displayPlatformName));
			}
		} else {
			if (PLSServiceType::ST_RTMP != platform->getServiceType() || getActivePlatforms().size() == 1) {
				PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("LiveInfo.live.error.start.other").arg(displayPlatformName));
			} else {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QTStr("LiveInfo.live.error.stoped.byRemote").arg(displayPlatformName));
			}
		}
		break;
	case PLSPlatformMqttStatus::PMS_CANNOT_AUTH_TO_SERVER:
	case PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_PATH:
		if (PLSServiceType::ST_RTMP != platform->getServiceType() || getActivePlatforms().size() == 1) {
			PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("MQTT.RTMP.Url.Error").arg(displayPlatformName));
		} else {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QTStr("LiveInfo.live.error.stoped.byRemote").arg(displayPlatformName));
		}
		break;
	}

	if (!isLiving()) {
		return;
	}
	PLSCHANNELS_API->setChannelStatus(platform->getChannelUUID(), ChannelData::Error);
}

void PLSPlatformApi::doStatusRequest(const QJsonObject &data)
{
	auto keyRequestAccessToken = "REQUEST_ACCESS_TOKEN";
	auto keyBroadcastEnd = "REQUEST_BROADCAST_END";
	auto keyBroadcastStatus = "BROADCAST_STATUS";
	auto keySimulcastUnstable = "SIMULCAST_UNSTABLE";
	auto keyNoticeLongBroadcast = "NOTICE_LONG_BROADCAST";
	auto keyLiveFinishedByPlatform = "LIVE_FINISHED_BY_PLATFORM";
	auto keyAfreecatvDisableChat = "AFREECATV_LIVE_CHAT_DISABLED";

	const auto statusType = data["statusType"].toString();
	if (keyNoticeLongBroadcast == statusType) {
		doNoticeLong(data["notice"].toObject());
	} else if (keyLiveFinishedByPlatform == statusType) {
		auto simulcastSeq = data["reason"].toObject().toVariantMap()["simulcastSeq"].toInt();
		auto platform = getPlatformBySimulcastSeq(simulcastSeq);
		if (platform != nullptr) {
			auto isBand = platform->getServiceType() == PLSServiceType::ST_BAND;
			auto channelName = platform->getInitData().value(ChannelData::g_platformName).toString();
			auto displayPlatformName = translatePlatformName(channelName);
			if (getActivePlatforms().size() == 1) {
				PLSMosquitto::stopInstance(m_pMQTT);
				m_pMQTT = nullptr;

				PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"),
						      isBand ? QTStr("Live.Check.Band.Broardcast.Time.Limit") : QTStr("LiveInfo.live.error.stoped.byRemote").arg(displayPlatformName));
				if (isLiving()) {
					PLS_LIVE_INFO(MODULE_PlatformService, "FinishedBy Prism because mqtt %s LIVE_FINISHED_BY_PLATFORM", channelName.toStdString().c_str());
					PLSCHANNELS_API->toStopBroadcast();
				}
			} else {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR,
						  isBand ? QTStr("Live.Check.Band.Broardcast.Time.Limit") : QTStr("LiveInfo.live.error.stoped.byRemote").arg(displayPlatformName));
				PLSCHANNELS_API->setChannelStatus(platform->getChannelUUID(), ChannelData::Error);
			}
		}
	} else if (keyAfreecatvDisableChat == statusType) {
		PLS_LIVE_INFO(MODULE_PlatformService, "afreecatv disabled chat");
		auto platformService = getPlatformAfreecaTV();
		platformService->setIsChatDisabled(true);
		forwardWebMessagePrivateChanged(platformService, platformService->getIsChatDisabled());
	} else {
		const auto statusList = data["statusList"].toArray();
		for (const auto &item : statusList) {
			const auto status = item.toObject();
			const auto simulcastSeq = status["simulcastSeq"].toInt();
			const auto livePlatform = status["livePlatform"].toString();
			const auto broadcastStatus = status["broadcastStatus"].toString();

			auto platform = getPlatformBySimulcastSeq(simulcastSeq);
			if (nullptr == platform) {
				continue;
			}

			if (!isLiving()) {
				break;
			}

			if (keyBroadcastEnd == statusType) {
				platform->setMqttStatus(broadcastStatus);
				//const auto currMqttStatus = platform->getMqttStatus();
				PLSMosquitto::stopInstance(m_pMQTT);
				m_pMQTT = nullptr;
				if (getActivePlatforms().size() > 1) {
					PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("MQTT.Broadcast.End"));
				} else {
					PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("main.message.simulcast.each.disconnected"));
				}

				if (isLiving()) {
					PLS_LIVE_ABORT_INFO(MODULE_PlatformService, "live abort because mqtt REQUEST_BROADCAST_END", "live abort because %s mqtt REQUEST_BROADCAST_END",
							    platform->getNameForChannelType());
					PLSCHANNELS_API->toStopBroadcast();
				}

				break;
			} else if (keyRequestAccessToken == statusType) {
				PLS_LIVE_INFO(MODULE_PlatformService, "%s REQUEST_ACCESS_TOKEN", platform->getNameForChannelType());
				PLS_PLATFORM_PRSIM->mqttRequestRefreshToken(platform, nullptr);
			} else if (keyBroadcastStatus == statusType) {
				PLS_LIVE_INFO(MODULE_PlatformService, "%s BROADCAST_STATUS:%s", platform->getNameForChannelType(), broadcastStatus.toStdString().c_str());
				switch (PLSPlatformBase::getMqttStatus(broadcastStatus)) {
				case PLSPlatformMqttStatus::PMS_ON_BROADCAST:
					platform->setMqttStatus(broadcastStatus);
					break;
				case PLSPlatformMqttStatus::PMS_END_BROADCAST:
					break;
				case PLSPlatformMqttStatus::PMS_CONNECTING_TO_SERVER:
					break;
				case PLSPlatformMqttStatus::PMS_CANNOT_FIND_SERVER:
				case PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_SERVER:
				case PLSPlatformMqttStatus::PMS_CANNOT_AUTH_TO_SERVER:
				case PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_PATH:
					break;
				case PLSPlatformMqttStatus::PMS_WAITING_TO_BROADCAST:
					break;
				default:
					break;
				}
			} else if (keySimulcastUnstable == statusType) {
				const auto lastMqttStatus = platform->getMqttStatus();
				platform->setMqttStatus(broadcastStatus);
				const auto currMqttStatus = platform->getMqttStatus();

				if (lastMqttStatus == currMqttStatus) {
					continue;
				}

				switch (currMqttStatus) {
				case PLSPlatformMqttStatus::PMS_ON_BROADCAST:
					break;
				case PLSPlatformMqttStatus::PMS_END_BROADCAST:
					break;
				case PLSPlatformMqttStatus::PMS_CONNECTING_TO_SERVER:
					break;
				case PLSPlatformMqttStatus::PMS_CANNOT_FIND_SERVER:
				case PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_SERVER:
				case PLSPlatformMqttStatus::PMS_CANNOT_AUTH_TO_SERVER:
				case PLSPlatformMqttStatus::PMS_CANNOT_CONNECT_TO_PATH:
					showMqttError(platform, currMqttStatus);
					break;
				case PLSPlatformMqttStatus::PMS_WAITING_TO_BROADCAST:
					break;
				default:
					break;
				}
			}
		}
	}
}

const char *PLSPlatformApi::invokedByWeb(const char *data)
{
	static const string EMPTY = "{}";

	emit PLS_PLATFORM_API->onWebRequest(QString::fromUtf8(data));

	return EMPTY.c_str();
}

void PLSPlatformApi::doWebRequest(const QString data)
{
	static QString dataLast;
	if (data != dataLast) {
		dataLast = data;
	}

	auto doc = QJsonDocument::fromJson(data.toUtf8());
	if (doc.isObject()) {
		auto root = doc.object();

		auto type = root["type"].toString();

		QJsonObject jsonData;
		const char *KEY_DATA = "data";
		if (root[KEY_DATA].isObject()) {
			jsonData = root[KEY_DATA].toObject();
		} else if (root[KEY_DATA].isString()) {
			auto strData = root["data"].toString();
			jsonData = QJsonDocument::fromJson(strData.toUtf8()).object();
		}

		if ("token" == type) {
			auto platform = getPlatformByName(jsonData["platform"].toString());
			if (nullptr != platform) {
				PLS_PLATFORM_PRSIM->mqttRequestRefreshToken(platform, [=](bool value) {
					if (value) {
						sendWebPrismToken(platform);
					} else {
						sendWebPrismPlatformClose(platform);
					}
				});
			}
		} else if ("stat" == type) {
			//doStatRequest(jsonData);
		} else if ("status" == type) {
			//doStatusRequest(jsonData);
		} else if ("send" == type) {
			requestWebChat(jsonData);
		} else if ("broadcast" == type) {
			forwardWebMessage(jsonData);
		} else if ("onReady" == type) {
			if (m_liveStatus > LiveStatus::Normal && m_liveStatus < LiveStatus::PrepareFinish) {
				sendWebPrismInit();
			}
		} else if ("loaded" == type) {
			auto plats = jsonData["platform"].toArray();
			auto youtubeString = plats.size() <= 0 ? "" : plats[0].toString();
			auto platform = getPlatformByName(youtubeString.toUpper());
			if (nullptr == platform || platform->getServiceType() != PLSServiceType::ST_YOUTUBE) {
				return;
			}
			if (PLS_PLATFORM_API->getLiveStatus() > LiveStatus::Normal && PLS_PLATFORM_API->getLiveStatus() < LiveStatus::PrepareFinish) {
				PLS_PLATFORM_API->sendWebPrismInit(true);
			}
		} else if ("addChannel" == type) { // add chat source
			PLSBasic::Get()->AddSource(PRISM_CHAT_SOURCE_ID);
		} else if (0 == type.compare("webPageLogs", Qt::CaseInsensitive)) {
			printWebLog(jsonData);
		}
	}
}

void PLSPlatformApi::onMqttMessage(const QString topic, const QString content)
{
	if (m_timerMQTT.isActive()) {
		m_timerMQTT.stop();
	}

	if (nullptr == m_pMQTT) {
		return;
	}

	if (topic.endsWith("stat")) {
		if (m_strLastMqttStat == content) {
			return;
		}
		PLS_LIVE_INFO_KR(MODULE_PlatformService, "mqtt-stat: %s-%s", topic.toStdString().c_str(), content.toStdString().c_str());
		PLS_LIVE_INFO(MODULE_PlatformService, "mqtt-stat: %s", topic.toStdString().c_str());
		m_strLastMqttStat = content;
		auto jsonData = QJsonDocument::fromJson(content.toUtf8()).object();
		doStatRequest(jsonData);
	} else if (topic.endsWith("status")) {
		if (m_strLastMqttStatus == content) {
			return;
		}
		PLS_LIVE_INFO_KR(MODULE_PlatformService, "mqtt-status:%s-%s", topic.toStdString().c_str(), content.toStdString().c_str());
		PLS_LIVE_INFO(MODULE_PlatformService, "mqtt-status: %s", topic.toStdString().c_str());
		m_strLastMqttStatus = content;
		auto jsonData = QJsonDocument::fromJson(content.toUtf8()).object();
		doStatusRequest(jsonData);
	} else if (topic.endsWith("chat")) {
		sendWebChat(content);
	} else {
		//PLS_INFO(MODULE_PlatformService, "%s %s %s-%s", LiveInfoPrefix, __FUNCTION__, topic.toStdString().c_str(), content.toStdString().c_str());
	}
}

void PLSPlatformApi::showEndView(bool isRecord, bool isShowDialog)
{
	PLS_INFO(MODULE_PlatformService, "show live end view isRecord(%s)", BOOL2STR(isRecord));
	bool isClickToStopRecord = PLSCHANNELS_API->getIsClickToStopRecord();
	bool isLivingAndRecording = false;
	//when stoping live sometime later but not stop complected, then start record, the keepRecordingWhenStreamStops is true.
	bool isRecordingButNotToStop = PLSCHANNELS_API->currentReocrdState() == ChannelData::RecordStarted || PLSCHANNELS_API->currentReocrdState() == ChannelData::RecordStarting;
	if (isRecord && PLS_PLATFORM_API->isLiving()) {
		isLivingAndRecording = true;
	} else if (!isRecord && PLS_PLATFORM_API->isRecording()) {
		isLivingAndRecording = true;
	}

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	//when stop streaming will stop record sametime.
	bool isStreamingRecordStopAuto = recordWhenStreaming && !keepRecordingWhenStreamStops;
	bool isRehearsal = PLSCHANNELS_API->isRehearsaling();
	//when live and record stop sametime, will ignore record end page show and toast.
	static bool isIgnoreNextRecordShow = false;
	if (isStreamingRecordStopAuto && !isClickToStopRecord) {
		if (isRecord) {
			isIgnoreNextRecordShow = true;
		} else if (!isRecord && isLivingAndRecording && !isRecordingButNotToStop) {
			isIgnoreNextRecordShow = true;
		}
	}

	PLS_INFO(
		END_MODULE,
		"Show end with parameter \n\tisLivingAndRecording:%s, \n\tisShowRecordEnd:%s, \n\tisClickToStopRecord:%s, \n\tisStreamingRecordStopAuto:%s, \n\tisIgnoreNextRecordShow:%s, \n\tisRehaersaling:%s, \n\tisRecordingButNotToStop:%s, \n\tisShowDialog:%s",
		BOOL2STR(isLivingAndRecording), BOOL2STR(isRecord), BOOL2STR(isClickToStopRecord), BOOL2STR(isStreamingRecordStopAuto), BOOL2STR(isIgnoreNextRecordShow), BOOL2STR(isRehearsal),
		BOOL2STR(isRecordingButNotToStop), BOOL2STR(isShowDialog));

	if (isRecord) {
		if (isIgnoreNextRecordShow) {
			//ignore this end page;
			PLS_INFO(END_MODULE, "Show end with parameter ignore this record end page");
			isIgnoreNextRecordShow = false;
		} else if (isLivingAndRecording) {
			//record stopped, but live still streaming
			if (!isRehearsal) {
				// not rehearsal mode
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("main.message.error.recordwhenbroadcasting"));
			} else {
				// rehaersal mode
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("Live.Toast.Rehaersal.Record.Ended"));
			}
		} else {
			pls_toast_clear();
			//only record and live all stoped, to clear toast.
			if (PLSBasic::Get() && PLSBasic::Get()->getMainView() != nullptr && PLSBasic::Get()->getMainView()->isVisible() && isShowDialog) {
				auto mainwindw = PLSBasic::Get();
				PLS_LIVE_INFO(MODULE_PlatformService, "show live end view");
				PLSLiveEndDialog dialog = PLSLiveEndDialog(isRecord, mainwindw);
				dialog.exec();
				PLS_LIVE_INFO(MODULE_PlatformService, "PLSEnd Dialog closed");
			}
		}
		PLSCHANNELS_API->setIsClickToStopRecord(false);
		emit liveEndPageShowComplected(isRecord);
		return;
	}

	if (isLivingAndRecording && !isStreamingRecordStopAuto) {
		//live ended, but still recording
		if (isRehearsal) {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("broadcast.end.rehearsal"));
		} else {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("broadcast.end.live"));
		}
	} else {
		pls_toast_clear();
		//only record and live all stopped, to clear toast.
		if (!isRehearsal) {
			if (PLSBasic::Get() && PLSBasic::Get()->getMainView() != nullptr && PLSBasic::Get()->getMainView()->isVisible() && isShowDialog) {
				auto mainwindw = PLSBasic::Get();
				PLS_LIVE_INFO(MODULE_PlatformService, "show live end view");
				PLSLiveEndDialog dialog = PLSLiveEndDialog(isRecord, mainwindw);
				dialog.exec();
				PLS_LIVE_INFO(MODULE_PlatformService, "PLSEnd Dialog closed");
			}
		}
	}
	emit liveEndPageShowComplected(isRecord);
}

void PLSPlatformApi::doChannelInitialized()
{
	for (auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		const auto channelUUID = info.value(ChannelData::g_channelUUID).toString();
		const auto channelType = info.value(ChannelData::g_data_type).toInt();
		const auto channelName = info.value(ChannelData::g_platformName).toString();
		//const auto displayOrder = info.value(ChannelData::g_displayOrder).toInt();

		if (isValidChannel(info)) {
			onActive(channelUUID);
		} else {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ ".InvalidChannels: type=%d, name=%s, uuid=%s", channelType, channelName.toStdString().c_str(), channelUUID.toStdString().c_str());
		}
	}
}

void PLSPlatformApi::showStartMessageIfNeeded()
{
	PLSPlatformYoutube::showAutoStartFalseAlertIfNeeded();
}

void PLSPlatformApi::printWebLog(const QJsonObject &obj)
{
	QString module = obj["module"].toString();
	QString level = obj["level"].toString();
	QString value = obj["value"].toString();
	QString valueKr = obj["valueKr"].toString();

	if (0 == level.compare("WARNING", Qt::CaseInsensitive)) {
		PLS_CHAT_WARN(module.toUtf8().data(), module.toUtf8().data(), "%s", value.toUtf8().data());
		PLS_CHAT_WARN_KR(module.toUtf8().data(), module.toUtf8().data(), "%s", valueKr.toUtf8().data());
	} else if (0 == level.compare("ERROR", Qt::CaseInsensitive)) {
		PLS_CHAT_ERROR(module.toUtf8().data(), module.toUtf8().data(), "%s", value.toUtf8().data());
		PLS_CHAT_ERROR_KR(module.toUtf8().data(), module.toUtf8().data(), "%s", valueKr.toUtf8().data());
	} else {
		PLS_CHAT_INFO(module.toUtf8().data(), module.toUtf8().data(), "%s", value.toUtf8().data());
		PLS_CHAT_INFO_KR(module.toUtf8().data(), module.toUtf8().data(), "%s", valueKr.toUtf8().data());
	}
}

OBSData GetDataFromJsonFile(const char *jsonFile);
int PLSPlatformApi::getOutputBitrate()
{
	auto config = PLSBasic::Get()->Config();

	auto mode = config_get_string(config, "Output", "Mode");
	auto advOut = astrcmpi(mode, "Advanced") == 0;

	if (advOut) {
		OBSData streamEncSettings = GetDataFromJsonFile("streamEncoder.json");
		return obs_data_get_int(streamEncSettings, "bitrate");
	} else {
		return config_get_int(config, "SimpleOutput", "VBitrate");
	}
}
