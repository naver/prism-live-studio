#include "PLSPlatformApi.h"

#include <string>
#include <algorithm>
#include <functional>
#include <iterator>
#include <QApplication>
#include <QString>

#include "PLSAction.h"
#include "frontend-api.h"
#include "PLSAlertView.h"
#include "pls-channel-const.h"
#include "window-basic-main.hpp"
#include "PLSLiveInfoDialogs.h"
#include "PLSPlatformPrism.h"
#include "log/log.h"
#include "pls-channel-const.h"
#include "PLSChannelDataAPI.h"
#include "pls-net-url.hpp"
#include "PLSPlatformNaverTV.h"
#include "PLSLiveEndDialog.h"
#include "PLSAPIYoutube.h"
#include "PLSServerStreamHandler.hpp"
#include "window-basic-main.hpp"
#include "ResolutionGuidePage.h"
#include "utils-api.h"
#include "PLSChatHelper.h"
#include "PLSNaverShoppingUseTerm.h"
#include "qt-wrappers.hpp"
#include "PLSChannelsVirualAPI.h"
#include "pls-gpop-data.hpp"
#include "obs.h"
#include "PLSApp.h"
#include "pls-frontend-api.h"
#include "PLSSyncServerManager.hpp"

constexpr int PRISM_MAX_OUT_Y = 1080;

using namespace std;
using namespace common;
extern QString translatePlatformName(const QString &platformName);
extern QString getCategoryString(int dataType);

const QString ANALOG_IS_SUCCESS_KEY = "isSuccess";
const QString ANALOG_FAIL_CODE_KEY = "failCode";
const QString ANALOG_FAIL_REASON_KEY = "failReason";
const QString ANALOG_LIVERECORD_SCENE_COUNT_KEY = "sceneCount";
const QString ANALOG_LIVERECORD_SOURCE_COUNT_KEY = "sourceCount";

const auto PROPERTY_LIST_SELECTED_KEY = QString("_selected_name");
const auto CAMERA_DEVICE_ID = "video_device_id";
const auto CUSTOM_AUDIO_DEVICE_ID = "audio_device_id";
const auto USE_CUSTOM_AUDIO = "use_custom_audio_device";
const auto GENERAL_PLATFORM = "General_Platform";

PLSPlatformApi *PLSPlatformApi::instance()
{
	static PLSPlatformApi *_instance = nullptr;

	if (nullptr == _instance) {
		mosqpp::lib_init();
		_instance = pls_new<PLSPlatformApi>();
		_instance->moveToThread(qApp->thread()); //zhangdewen force move to main thread
		pls_frontend_set_web_invoked_cb(PLSPlatformApi::invokedByWeb);
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { pls_delete(_instance, nullptr); });
	}
	return _instance;
}

PLSPlatformApi::PLSPlatformApi()
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
		pls_delete(item);
	}
	platformList.clear();
	stopMqtt();
}

bool PLSPlatformApi::initialize()
{
	obs_frontend_add_event_callback(onFrontendEvent, this);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::channelActiveChanged, this,
		[this](const QString &channelUUID, bool enable) {
			if (enable) {
				onActive(channelUUID);
			} else {
				onInactive(channelUUID);
			}
			updatePlatformViewerCount();
			emit platformActiveDone();
		},
		Qt::QueuedConnection);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, &PLSPlatformApi::onAddChannel, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelModified, this, &PLSPlatformApi::onUpdateChannel, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this, &PLSPlatformApi::onRemoveChannel, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelCreateError, this, &PLSPlatformApi::onRemoveChannel, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigAllChannelRefreshDone, this, &PLSPlatformApi::onAllChannelRefreshDone, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigAllClear, this, &PLSPlatformApi::onClearChannel, Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::broadcastGo, this,
		[this]() {
			//The main page setting button is disabled
			emit enterLivePrepareState(true);
			onPrepareLive();
		},
		Qt::QueuedConnection);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::stopBroadcastGo, this, &PLSPlatformApi::onPrepareFinish, Qt::QueuedConnection);
	connect(&m_timerMQTT, &QTimer::timeout, this, [] {
		auto activiedPlatforms = PLS_PLATFORM_ACTIVIED;
		bool singleYoutube = activiedPlatforms.size() == 1 && activiedPlatforms.front()->getServiceType() == PLSServiceType::ST_YOUTUBE;
		if (singleYoutube) {
			return;
		}

		bool containPlatformExceptBandOrRtmp = false;
		for (auto platform : activiedPlatforms) {
			if (platform->getServiceType() != PLSServiceType::ST_BAND && platform->getServiceType() != PLSServiceType::ST_CUSTOM) {
				containPlatformExceptBandOrRtmp = true;
				break;
			}
		}
		if (containPlatformExceptBandOrRtmp) {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QTStr("MQTT.Timeout"));
		}
	});
	//Notice, onWebRequest is send from Browser thread, not main thread
	connect(this, &PLSPlatformApi::onWebRequest, this, &PLSPlatformApi::doWebRequest);
	connect(this, &PLSPlatformApi::livePrepared, this, [this](bool value) {
		if (value == false) {
			emit enterLivePrepareState(false);
		}
	});
	connect(this, &PLSPlatformApi::liveStarted, this, [this]() { emit enterLivePrepareState(false); });

	return true;
}

void PLSPlatformApi::saveStreamSettings(string platform, string server, const string_view &key) const
{
	if (isLiving()) {
		return;
	}
	auto serviceData = PLSBasic::instance()->LoadServiceData();
	if (!platform.empty() && platform != "Prism" && serviceData) {
		OBSDataAutoRelease settings = obs_data_get_obj(serviceData, "settings");
		const char *service = obs_data_get_string(settings, "service");
		string strService = service;
		transform(strService.begin(), strService.end(), strService.begin(), ::tolower);
		string tmpStr = platform;
		transform(tmpStr.begin(), tmpStr.end(), tmpStr.begin(), ::tolower);
		if (strService.find(tmpStr) != std::string::npos) {
			platform = service;
			const char *chServer = obs_data_get_string(settings, "server");
			string strServer = chServer;
			if (strServer != "ServerAuto") {
				server = chServer;
			}
		}
	}

	if (!serviceData) {
		auto activiedPlatforms = getActivePlatforms();
		if (activiedPlatforms.size() == 1 && platform == "YouTube") {
			platform = "YouTube - RTMPS";
		}
	}

	PLS_LIVE_INFO(MODULE_PlatformService, "save straming info name=%s, StreamUrl=%s", platform.c_str(), server.c_str());
	PLS_INFO_KR(MODULE_PlatformService, "save straming info name=%s, StreamUrl=%s, StreamKey=%s", platform.c_str(), server.c_str(), key.data());
	OBSData settings = obs_data_create();
	obs_data_release(settings);

	auto serviceId = platform.empty() ? "rtmp_custom" : "rtmp_common";

	if (!platform.empty()) {
		obs_data_set_string(settings, "service", platform.data());
	}
	obs_data_set_string(settings, "server", server.data());
	obs_data_set_string(settings, "key", key.data());

	saveStreamSettings(serviceId, settings);
}

void PLSPlatformApi::saveStreamSettings(const char *serviceId, OBSData settings) const
{
	obs_service_t *oldService = obs_frontend_get_streaming_service();
	OBSData hotkeyData = obs_hotkeys_save_service(oldService);
	obs_data_release(hotkeyData);

	OBSService newService = obs_service_create(serviceId, "default_service", settings, hotkeyData);
	obs_service_release(newService);

	if (!newService)
		return;

	obs_frontend_set_streaming_service(newService);
}

list<PLSPlatformBase *> PLSPlatformApi::getActivePlatforms() const
{
	list<PLSPlatformBase *> lst;

	QMutexLocker locker(&platformListMutex);
	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [](const auto v) { return v->isActive(); });

	return lst;
}

std::list<PLSPlatformBase *> PLSPlatformApi::getActiveValidPlatforms() const
{
	list<PLSPlatformBase *> lst;

	QMutexLocker locker(&platformListMutex);
	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [](const auto v) { return v->isActive() && v->isValid(); });
	return lst;
}

bool PLSPlatformApi::isPlatformActived(PLSServiceType serviceType) const
{
	QMutexLocker locker(&platformListMutex);
	return any_of(platformList.begin(), platformList.end(), [=](auto item) { return item->isActive() && item->getServiceType() == serviceType; });
}

bool PLSPlatformApi::isPlatformExisted(PLSServiceType serviceType) const
{
	QMutexLocker locker(&platformListMutex);
	return any_of(platformList.begin(), platformList.end(), [=](auto item) { return item->getServiceType() == serviceType; });
}

PLSPlatformBase *PLSPlatformApi::getExistedPlatformByType(PLSServiceType type)
{
	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == type) {
			return platform;
		}
	}
	return nullptr;
}

PLSPlatformBase *PLSPlatformApi::getExistedActivePlatformByType(PLSServiceType type)
{
	PLSPlatformBase *platform = nullptr;
	QMutexLocker locker(&platformListMutex);
	for (auto item : PLS_PLATFORM_ACTIVIED) {
		if (item->getServiceType() == type) {
			platform = item;
		}
	}
	return platform;
}

list<PLSPlatformBase *> PLSPlatformApi::getExistedPlatformsByType(PLSServiceType type)
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

PLSPlatformBase *PLSPlatformApi::getExistedPlatformByLiveStartName(const QString &name)
{
	PLSPlatformBase *platform = nullptr;
	QMutexLocker locker(&platformListMutex);
	for (auto item : platformList) {
		if (name.contains(item->getNameForLiveStart(), Qt::CaseInsensitive)) {
			platform = item;
			break;
		}
	}
	return platform;
}

PLSPlatformBase *PLSPlatformApi::getExistedActivePlatformByLiveStartName(const QString &name)
{
	PLSPlatformBase *platform = nullptr;
	QMutexLocker locker(&platformListMutex);
	for (auto item : platformList) {
		if (item->isActive() && name.contains(item->getNameForLiveStart(), Qt::CaseInsensitive)) {
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

PLSPlatformRtmp *PLSPlatformApi::getPlatformRtmp()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_CUSTOM);
	if (platform) {
		return dynamic_cast<PLSPlatformRtmp *>(platform);
	}
	return nullptr;
}

PLSPlatformRtmp *PLSPlatformApi::getPlatformRtmpActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_CUSTOM);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformRtmp *>(platform);
	}
	return nullptr;
}

PLSPlatformTwitch *PLSPlatformApi::getPlatformTwitch()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_TWITCH);
	if (platform) {
		return dynamic_cast<PLSPlatformTwitch *>(platform);
	}
	return nullptr;
}

PLSPlatformTwitch *PLSPlatformApi::getPlatformTwitchActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_TWITCH);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformTwitch *>(platform);
	}
	return nullptr;
}

PLSPlatformBand *PLSPlatformApi::getPlatformBand()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_BAND);
	if (platform) {
		return dynamic_cast<PLSPlatformBand *>(platform);
	}
	return nullptr;
}

PLSPlatformBand *PLSPlatformApi::getPlatformBandActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_BAND);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformBand *>(platform);
	}
	return nullptr;
}

PLSPlatformYoutube *PLSPlatformApi::getPlatformYoutube()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_YOUTUBE);
	if (platform) {
		return dynamic_cast<PLSPlatformYoutube *>(platform);
	}
	return nullptr;
}

PLSPlatformYoutube *PLSPlatformApi::getPlatformYoutubeActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_YOUTUBE);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformYoutube *>(platform);
	}
	return nullptr;
}

PLSPlatformFacebook *PLSPlatformApi::getPlatformFacebook()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_FACEBOOK);
	if (platform) {
		return dynamic_cast<PLSPlatformFacebook *>(platform);
	}
	return nullptr;
}

PLSPlatformFacebook *PLSPlatformApi::getPlatformFacebookActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_FACEBOOK);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformFacebook *>(platform);
	}
	return nullptr;
}

PLSPlatformAfreecaTV *PLSPlatformApi::getPlatformAfreecaTV()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_AFREECATV);
	if (platform) {
		return dynamic_cast<PLSPlatformAfreecaTV *>(platform);
	}
	return nullptr;
}

PLSPlatformAfreecaTV *PLSPlatformApi::getPlatformAfreecaTVEActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_AFREECATV);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformAfreecaTV *>(platform);
	}
	return nullptr;
}

PLSPlatformNaverShoppingLIVE *PLSPlatformApi::getPlatformNaverShoppingLIVE()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_NAVER_SHOPPING_LIVE);
	if (platform) {
		return dynamic_cast<PLSPlatformNaverShoppingLIVE *>(platform);
	}
	return nullptr;
}

PLSPlatformNaverShoppingLIVE *PLSPlatformApi::getPlatformNaverShoppingLIVEActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_NAVER_SHOPPING_LIVE);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformNaverShoppingLIVE *>(platform);
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
	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI active channel get channel info is empty, channel uuid is %s", which.toStdString().c_str());
		return;
	}

	const auto channelName = info.value(ChannelData::g_platformName).toString();
	if (!isValidChannel(info)) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI active channel get channel info is invalid, channel type is %d, channel name is %s, channel uuid is %s",
			  info.value(ChannelData::g_data_type).toInt(), channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, "PlatformAPI active platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
		 channelName.toStdString().c_str(), which.toStdString().c_str());
	auto platform = getPlatformById(which, info);
	if (nullptr != platform && !platform->isActive()) {
		platform->onActive();
	}
}

void PLSPlatformApi::onInactive(const QString &which)
{
	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI inactive channel get channel info is empty, channel uuid is %s", which.toStdString().c_str());
		return;
	}

	const auto channelName = info.value(ChannelData::g_platformName).toString();
	if (!isValidChannel(info)) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI inactive channel get channel info is invalid, channel type is %d, channel name is %s, channel uuid is %s",
			  info.value(ChannelData::g_data_type).toInt(), channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, "PlatformAPI inactive platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
		 channelName.toStdString().c_str(), which.toStdString().c_str());
	auto platform = getPlatformById(which, info);
	if (nullptr != platform && platform->isActive()) {
		platform->onInactive();
	}
}

void PLSPlatformApi::onClearChannel()
{
	QMutexLocker locker(&platformListMutex);
	for (auto iter = platformList.begin(); iter != platformList.end();) {
		auto platform = *iter;
		platform->setActive(false);
		iter = platformList.erase(iter);
		pls_delete(platform);
		++iter;
	}
}

void PLSPlatformApi::onAddChannel(const QString &channelUUID)
{
	QVariantMap info = PLSCHANNELS_API->getChannelInfo(channelUUID);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI add channel get channel info is empty, channel uuid is %s", channelUUID.toStdString().c_str());
		return;
	}

	const auto channelName = info.value(ChannelData::g_platformName).toString();
	if (!isValidChannel(info)) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI add channel get channel info is invalid, channel type is %d, channel name is %s, channel uuid is %s",
			  info.value(ChannelData::g_data_type).toInt(), channelName.toStdString().c_str(), channelUUID.toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, "PlatformAPI add platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
		 channelName.toStdString().c_str(), channelUUID.toStdString().c_str());

	getPlatformById(channelUUID, info);
}

void PLSPlatformApi::onRemoveChannel(const QString &channelUUID)
{
	auto platform = getExistedPlatformById(channelUUID);
	if (nullptr != platform) {
		platform->setActive(false);

		const QVariantMap &info = platform->getInitData();
		const auto channelName = info.value(ChannelData::g_platformName).toString();
		PLS_INFO(MODULE_PlatformService, "PlatformAPI remove platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
			 channelName.toStdString().c_str(), channelUUID.toStdString().c_str());
		emit channelRemoved(info);

		QMutexLocker locker(&platformListMutex);
		platformList.remove(platform);
		pls_delete(platform);
		PLS_INFO(MODULE_PlatformService, "PlatformAPI current remove platformList count is %d", platformList.size());
	}
}

void PLSPlatformApi::onUpdateChannel(const QString &which)
{
	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI update channel get channel info is empty, channel uuid is %s", which.toStdString().c_str());
		return;
	}

	const auto channelName = info.value(ChannelData::g_platformName).toString();
	if (!isValidChannel(info)) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI update channel get channel info is invalid, channel type is %d, channel name is %s, channel uuid is %s",
			  info.value(ChannelData::g_data_type).toInt(), channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, "PlatformAPI update platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
		 channelName.toStdString().c_str(), which.toStdString().c_str());
	auto platform = getPlatformById(which, info);
	if (nullptr != platform) {
		platform->setInitData(info);
	}
}

void PLSPlatformApi::setLiveStatus(LiveStatus value)
{
	m_liveStatus = value;
	if (LiveStatus::LiveStarted == value) {
		PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START);
	} else if (LiveStatus::Normal == value && !isRecording() && PLSBasic::Get()) {
		PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END);
	}
}

void PLSPlatformApi::onPrepareLive()
{

	//Set the current live status to PrepareLive
	setLiveStatus(LiveStatus::PrepareLive);

	//PRISM/LiuHaibin/20210906/Pre-check encoders
	if (!PLSBasic::instance()->CheckStreamEncoder()) {
		prepareLiveCallback(false);
		return;
	}

	//Initialize PLSPlatformAPI live broadcast status
	m_bApiPrepared = PLSPlatformLiveStartedStatus::PLS_NONE;
	m_bApiStarted = PLSPlatformLiveStartedStatus::PLS_NONE;
	m_endLiveReason.clear();
	m_bPrismLive = true;
	m_ignoreRequestBroadcastEnd = false;
	m_isConnectedMQTT = false;

	//Get the list of platforms activated when the live broadcast starts
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	
	//reset live active platform info
	resetPlatformsLivingInfo();

	//call the first platform onPrepareLive method
	platformActived.front()->onPrepareLive(true);
}

void PLSPlatformApi::showMultiplePlatformGreater1080pAlert()
{
	
}

void PLSPlatformApi::sortPlatforms()
{
	QMutexLocker locker(&platformListMutex);
	platformList.sort([](const PLSPlatformBase *lValue, const PLSPlatformBase *rValue) { return lValue->getChannelOrder() < rValue->getChannelOrder(); });
}

bool PLSPlatformApi::checkNetworkConnected()
{
	return true;
}

void PLSPlatformApi::resetPlatformsLivingInfo()
{
	m_iTotalSteps = 0;
	for (const auto &item : PLS_PLATFORM_ACTIVIED) {
		if (PLSServiceType::ST_CUSTOM != item->getServiceType()) {
			++m_iTotalSteps;
			item->setCurrStep(m_iTotalSteps);
		}

		item->setChannelLiveSeq(0);
		item->setApiPrepared(PLSPlatformLiveStartedStatus::PLS_NONE);
		item->setApiStarted(PLSPlatformLiveStartedStatus::PLS_NONE);
		item->setTokenRequestStatus(PLSTokenRequestStatus::PLS_GOOD);
		item->setMaxLiveTimerStatus(PLSMaxLiveTimerStatus::PLS_TIMER_NONE);
		item->setAllowPushStream(true);
	}
}

bool PLSPlatformApi::checkWaterMarkAndOutroResource()
{
	//check watermark is valid
	return true;
}

bool PLSPlatformApi::checkOutputBitrateValid()
{
	return true;
}

void PLSPlatformApi::onPrepareFinish()
{
	if (m_liveStatus >= LiveStatus::PrepareFinish) {
		return;
	}

	setLiveStatus(LiveStatus::PrepareFinish);
	if (m_endLiveReason.isEmpty()) {
		m_endLiveReason = "Click finish button";
	}
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		prepareFinishCallback();
		return;
	}
	platformActived.front()->onPrepareFinish();
}

void PLSPlatformApi::activateCallback(const PLSPlatformBase *platform, bool value) const
{
	//If the value is false, it means that the activation of the channel fails. At this time, the channel should become unselected.
	if (!value) {
		PLSCHANNELS_API->setChannelUserStatus(platform->getChannelUUID(), ChannelData::Disabled);
	}
}

void PLSPlatformApi::deactivateCallback(const PLSPlatformBase *platform, bool value) const
{
	if (value && PLSServiceType::ST_CUSTOM != platform->getServiceType()) {
		sendWebPrismPlatformClose(platform);
	}

	if (!value) {
		PLSCHANNELS_API->setChannelUserStatus(platform->getChannelUUID(), ChannelData::Enabled);
	}
}

void PLSPlatformApi::prepareLiveCallback(bool value)
{
	//The state of the onPrepare callback function does not match
	if (LiveStatus::PrepareLive != m_liveStatus) {
		return;
	}

	m_bApiPrepared = value ? PLSPlatformLiveStartedStatus::PLS_SUCCESS : PLSPlatformLiveStartedStatus::PLS_FAILED;
	PLS_INFO(MODULE_PlatformService, "%s PLSPlatformApi call prepareLiveCallback method, value is %s", PrepareInfoPrefix, BOOL2STR(value));
	if (value) {
		setLiveStatus(LiveStatus::ToStart);
	} else {
		setLiveStatus(LiveStatus::Normal);
	}

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto info : platformActived) {
		info->onAllPrepareLive(value);
		PLS_INFO(MODULE_PlatformService, "%s %s getApiPrepared() value is %d , channelLiveSeq is %d", PrepareInfoPrefix, info->getNameForChannelType(),
			 static_cast<int>(info->getApiPrepared()), info->getChannelLiveSeq());
	}

	//send stop live signal (apiPrepared=false, apiStarted=false)
	if (!value) {
		emit liveEnded(false, false);
	}

	//Restore the disabled state of the setting button on the main page
	emit livePrepared(value);
}

void PLSPlatformApi::checkAllPlatformLiveStarted()
{
	if (LiveStatus::LiveStarted != m_liveStatus) {
		return;
	}

	//If the number of currently live broadcast platforms is 0, end the live broadcast
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		PLS_INFO(MODULE_PlatformService, "live started callback current active platform is empty");
		liveStartedCallback(false);
		return;
	}

	//Refresh chat UI of chat view
	sendWebPrismInit();

	//Check if the live broadcast has started on all platforms
	for (const PLSPlatformBase *pPlatform : platformActived) {
		if (pPlatform->getApiStarted() == PLSPlatformLiveStartedStatus::PLS_NONE) {
			return;
		}
	}

	//Single-platform live broadcast, if the platform is successful, it is success, and failure is failure
	if (platformActived.size() == 1) {
		PLSPlatformLiveStartedStatus liveStartedApi = platformActived.front()->getApiStarted();
		if (liveStartedApi == PLSPlatformLiveStartedStatus::PLS_FAILED) {
			liveStartedCallback(false);
		} else if (liveStartedApi == PLSPlatformLiveStartedStatus::PLS_SUCCESS) {
			liveStartedCallback(true);
		}
		return;
	}

	//Multi-platform live broadcast only means success if one platform is successful
	bool containsSuccess = false;
	for (const PLSPlatformBase *pPlatform : platformActived) {
		if (pPlatform->getApiStarted() == PLSPlatformLiveStartedStatus::PLS_SUCCESS) {
			containsSuccess = true;
			break;
		}
	}

	//Notify all platforms to request live broadcast status results
	liveStartedCallback(containsSuccess);
}

void PLSPlatformApi::notifyLiveLeftMinutes(PLSPlatformBase *platform, int maxLiveTime, uint leftMinutes)
{
	
}

void PLSPlatformApi::liveStartedCallback(bool value)
{
	//Indicates whether the request is successful on all platforms
	m_bApiStarted = value ? PLSPlatformLiveStartedStatus::PLS_SUCCESS : PLSPlatformLiveStartedStatus::PLS_FAILED;
	//Notify all live broadcast platforms of the current live broadcast
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto info : platformActived) {
		info->onAlLiveStarted(value);
		if (!value)
			info->stopMaxLiveTimer();
		if (value) {
			const char *platformName = info->getNameForChannelType();
			PLS_LOGEX(PLS_LOG_INFO, MODULE_PlatformService, {{"liveStartService", platformName}}, "%s start living.", platformName);
		}
	}
	//All live broadcasts on all platforms are considered to be live broadcasts.
	if (value) {
		setLiveStatus(LiveStatus::Living);
		int count = static_cast<int>(platformActived.size());
		PLS_LOGEX(PLS_LOG_INFO, MODULE_PlatformService, {{"streamType", count > 1 ? "multiplePlatform" : "singlePlatform"}, {"liveStatus", "Start"}}, "start living");
	}

	//The toast will be cleared if the live broadcast fails on all platforms, but the endPage will not be displayed.
	if (!value) {
		//clear toast logic
		showEndView(false, false);
	}

	emit liveStarted(value);
}

void PLSPlatformApi::sendWebPrismInit() const
{
	pls_frontend_call_dispatch_js_event_cb("prism_events", QJsonDocument(getWebPrismInit()).toJson().constData());
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
	emit liveEnded(m_bApiPrepared == PLSPlatformLiveStartedStatus::PLS_SUCCESS ? true : false, m_bApiStarted == PLSPlatformLiveStartedStatus::PLS_SUCCESS ? true : false);
	emit liveEndedForUi();
	setLiveStatus(LiveStatus::Normal);

	if (!m_bRecording && !m_bReplayBuffer && !m_bVirtualCamera) {
		emit outputStopped();
	}
}

PLSServiceType PLSPlatformApi::getServiceType(const QVariantMap &info) const
{
	if (info.isEmpty()) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI get platform type failed, info is empty");
		assert(false);
		return PLSServiceType::ST_CUSTOM;
	}

	const auto type = info.value(ChannelData::g_data_type).toInt();
	const auto name = info.value(ChannelData::g_platformName).toString();

	if (type == ChannelData::ChannelType) {
		for (int i = 1; i < PLATFORM_SIZE; ++i) {
			if (name == NamesForChannelType[i]) {
				return static_cast<PLSServiceType>(i);
			}
		}
	} else if (type >= ChannelData::CustomType) {
		return PLSServiceType::ST_CUSTOM;
	}
	PLS_ERROR(MODULE_PlatformService, "PlatformAPI get platform type failed, unmatched channel type is %1, channel name is %2", type, name.toStdString().c_str());
	return PLSServiceType::ST_CUSTOM;
}

PLSPlatformBase *PLSPlatformApi::buildPlatform(PLSServiceType type)
{
	PLSPlatformBase *platform = nullptr;

	switch (type) {
	case PLSServiceType::ST_CUSTOM:
		platform = pls_new<PLSPlatformRtmp>();
		break;
	case PLSServiceType::ST_TWITCH:
		platform = pls_new<PLSPlatformTwitch>();
		break;
	case PLSServiceType::ST_YOUTUBE:
		platform = pls_new<PLSPlatformYoutube>();
		break;
	case PLSServiceType::ST_FACEBOOK:
		platform = pls_new<PLSPlatformFacebook>();
		break;
	case PLSServiceType::ST_NAVERTV:
		platform = pls_new<PLSPlatformNaverTV>();
		break;
	case PLSServiceType::ST_VLIVE:
		assert(false);
		break;
	case PLSServiceType::ST_BAND:
		platform = pls_new<PLSPlatformBand>();
		break;
	case PLSServiceType::ST_AFREECATV:
		platform = pls_new<PLSPlatformAfreecaTV>();
		break;
	case PLSServiceType::ST_NAVER_SHOPPING_LIVE:
		platform = pls_new<PLSPlatformNaverShoppingLIVE>();
		break;
	case PLSServiceType::ST_TWITTER:
		break;
	default:
		assert(false);
		break;
	}

	if (nullptr != platform) {
		platform->moveToThread(this->thread());
		QMutexLocker locker(&platformListMutex);
		platformList.push_back(platform);
		PLS_INFO(MODULE_PlatformService, "PlatformAPI current build platformList count is %d", platformList.size());
	} else {
		assert(false);
		PLS_ERROR(MODULE_PlatformService, "buildPlatform .null %d", type);
	}

	return platform;
}

PLSPlatformBase *PLSPlatformApi::getExistedPlatformById(const QString &channelUUID)
{
	PLSPlatformBase *platform = nullptr;
	auto isMatched = [&channelUUID](const PLSPlatformBase *item) { return channelUUID == item->getChannelUUID(); };
	platform = findMatchedPlatform(isMatched);
	return platform;
}

PLSPlatformBase *PLSPlatformApi::getPlatformById(const QString &channelUUID, const QVariantMap &info)
{

	auto serviceType = PLSServiceType::ST_CUSTOM;
	if (!info.isEmpty()) {
		serviceType = getServiceType(info);
	}

	//Get platform pointer by channel uuid
	PLSPlatformBase *platform = getExistedPlatformById(channelUUID);
	if (nullptr == platform && !info.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, "PlatformAPI build platform success, channel type is %d , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
			 channelUUID.toStdString().c_str());
		platform = buildPlatform(serviceType);
	}

	if (nullptr != platform && !info.isEmpty()) {
		platform->setInitData(info);
	}

	if (nullptr == platform) {
		assert(false);
		PLS_WARN(MODULE_PlatformService, "getPlatformById .null: %s", channelUUID.toStdString().c_str());
	}

	return platform;
}

void PLSPlatformApi::onAllChannelRefreshDone()
{
	platformList.remove_if([](const auto &platform) {
		bool isExisted = PLSCHANNELS_API->isChannelInfoExists(platform->getChannelUUID());
		if (!isExisted) {
			PLS_INFO(MODULE_PlatformService, "PlatformAPI remove invalid platform pointer, channel uuid is %s", platform->getChannelUUID().toStdString().c_str());
		}
		return !isExisted;
	});
	PLS_INFO(MODULE_PlatformService, "PlatformAPI all channel refresh done,  platformList count is %d", platformList.size());
	updatePlatformViewerCount();
}

bool PLSPlatformApi::isValidChannel(const QVariantMap &info)
{
	if (info.isEmpty()) {
		return false;
	}
	const auto channelType = info.value(ChannelData::g_data_type).toInt();
	const auto channelName = info.value(ChannelData::g_platformName).toString();
	if (channelType == ChannelData::ChannelType || channelType >= ChannelData::CustomType) {
		return true;
	}
	PLS_INFO(MODULE_PlatformService, "isValidChannel : false %d %s", channelType, channelName.toStdString().c_str());

	return false;
}

void PLSPlatformApi::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
	auto *self = static_cast<PLSPlatformApi *>(private_data);

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		PLS_INFO(MODULE_PlatformService, "receive obs start streaming callback message");
		self->onLiveStarted();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		PLS_INFO(MODULE_PlatformService, "receive obs stop streaming callback message");
		self->onLiveStopped();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		PLS_INFO(MODULE_PlatformService, "receive obs start recording callback message");
		self->onRecordingStarted();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		PLS_INFO(MODULE_PlatformService, "receive obs stop recording callback message");
		self->onRecordingStoped();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		PLS_INFO(MODULE_PlatformService, "%s receive obs start replaybuffer callback message", LiveInfoPrefix);
		self->onReplayBufferStarted();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		PLS_INFO(MODULE_PlatformService, "%s receive obs stop replaybuffer callback message", LiveInfoPrefix);
		self->onReplayBufferStoped();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
		PLS_INFO(MODULE_PlatformService, "receive obs start virtual camera callback message");
		self->onVirtualCameraStarted();
		emit self->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		PLS_INFO(MODULE_PlatformService, "receive obs stop virtual camera callback message");
		self->onVirtualCameraStopped();
		emit self->outputStateChanged();
		break;
	default:
		break;
	}
}

void PLSPlatformApi::onLiveStarted()
{
	PLS_INFO(MODULE_PlatformService, "set platformApi live status is livestarted");
	setLiveStatus(LiveStatus::LiveStarted);

	//If the number of currently live broadcast platforms is 0, end the live broadcast
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		PLS_INFO(MODULE_PlatformService, "receive obs push stream message, but the current active platform list is empty");
		liveStartedCallback(false);
		return;
	}
	
	//Clear the number of views, likes, and comments on each platform
	clearLiveStatisticsInfo();
}

void PLSPlatformApi::clearLiveStatisticsInfo() const
{
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
}

void PLSPlatformApi::onLiveStopped()
{
	if (m_liveStatus < LiveStatus::ToStart || m_liveStatus >= LiveStatus::LiveStoped) {
		PLS_WARN(MODULE_PlatformService, "onLiveStopped Unexpected status, %d", m_liveStatus);
		return;
	}

	setLiveStatus(LiveStatus::LiveStoped);
	
	//call each platform to stop the push method
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
	PLS_PLATFORM_API->sendRecordAnalog(true);
	PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START);
}

void PLSPlatformApi::onRecordingStoped()
{
	m_bRecording = false;

	if (LiveStatus::Normal == m_liveStatus) {
		PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END);
		if (!m_bReplayBuffer && !m_bVirtualCamera)
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

	if (LiveStatus::Normal == m_liveStatus && !m_bRecording && !m_bVirtualCamera) {
		emit outputStopped();
	}
}

void PLSPlatformApi::onVirtualCameraStarted()
{
	m_bVirtualCamera = true;
}

void PLSPlatformApi::onVirtualCameraStopped()
{
	m_bVirtualCamera = false;
	if (LiveStatus::Normal == m_liveStatus && !m_bRecording && !m_bReplayBuffer) {
		emit outputStopped();
	}
}

void PLSPlatformApi::ensureStopOutput()
{
	PLS_INFO(MODULE_PlatformService, "ensureStopOutput : %d %d %d %d", m_bReplayBuffer, m_bRecording, m_liveStatus, m_bVirtualCamera);
	m_bStopForExit = true;

	if (m_bRecording || m_bReplayBuffer || (LiveStatus::LiveStarted <= m_liveStatus && m_liveStatus < LiveStatus::LiveStoped) || m_bVirtualCamera) {
		QEventLoop loop;
		connect(this, &PLSPlatformApi::outputStopped, &loop, &QEventLoop::quit);

		if (LiveStatus::LiveStarted <= m_liveStatus && m_liveStatus < LiveStatus::LiveStoped) {
			PLS_INFO(MODULE_PlatformService, "obs exit event causes stopping the push stream");
			PLS_PLATFORM_API->sendLiveAnalog(true);
			PLS_PLATFORM_API->stopStreaming("end live because obs exit event causes stopping the push stream");
		}

		if (m_bReplayBuffer) {
			PLSBasic::Get()->StopReplayBuffer();
		}

		if (m_bVirtualCamera) {
#ifdef __APPLE__
			QMetaObject::invokeMethod(
				this, [] { PLSBasic::Get()->StopVirtualCam(); }, Qt::QueuedConnection);
#else
			PLSBasic::Get()->StopVirtualCam();
#endif
		}

		if (m_bRecording) {
			PLS_INFO(MODULE_PlatformService, "ensureStopOutput : toStopRecord");
			PLSCHANNELS_API->toStopRecord();
		}

		QTimer::singleShot(15000, &loop, &QEventLoop::quit);

		loop.exec();
		PLS_INFO(MODULE_PlatformService, "ensureStopOutput .end: %d %d %d", m_bReplayBuffer, m_bRecording, m_liveStatus);
	}
}

QJsonObject PLSPlatformApi::getWebPrismInit() const
{

	return QJsonObject();
}

void PLSPlatformApi::forwardWebMessagePrivateChanged(const PLSPlatformBase *platform, bool isPrivate) const
{
	
}

void PLSPlatformApi::sendWebChatTabShown(const QString &channelName, bool isAllTab) const
{
	
}

void PLSPlatformApi::stopMqtt()
{
	
}

const QString &PLSPlatformApi::getLiveEndReason() const
{
	return m_endLiveReason;
}

void PLSPlatformApi::setLiveEndReason(const QString &reason, EndLiveType endLiveType)
{
	
}

void PLSPlatformApi::stopStreaming(const QString &reason, EndLiveType endLiveType)
{
	PLSCHANNELS_API->toStopBroadcast();
}

void PLSPlatformApi::createAnalogInfo(QVariantMap &uploadVariantMap) const
{
	
}

void PLSPlatformApi::sendLiveAnalog(bool success, const QString &reason, int code) const
{
	
}

void PLSPlatformApi::sendLiveAnalog(const QVariantMap &info) const
{
	
}

void PLSPlatformApi::sendRecordAnalog(bool success, const QString &reason, int code) const
{
	
}

void PLSPlatformApi::sendRecordAnalog(const QVariantMap &info) const
{
	
}

void PLSPlatformApi::sendAnalog(AnalogType type, const QVariantMap &info) const
{
	
}

void PLSPlatformApi::sendBeautyAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendGiphyAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendTouchStickerAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendVirtualBgAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendDrawPenAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendSourceAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendFilterAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendBgmAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendVirtualCamAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendClockWidgetAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendBgTemplateAnalog(OBSData privious, OBSData current) const
{
	
}

void PLSPlatformApi::sendAudioVisualizerAnalog(OBSData privious, OBSData current) const
{
	
}

void PLSPlatformApi::sendCameraDeviceAnalog(OBSData privious, OBSData current) const
{
	
}

void PLSPlatformApi::sendAnalogOnUserConfirm(OBSSource source, OBSData privious, OBSData current) const
{
	
}

void PLSPlatformApi::sendCodecAnalog(const QVariantMap& info) const {
}

//this key is mark for update schedule list task
const QString updateScheduleListTask = "updateScheduleListTask";
//delay 200ms updating for call more times
void PLSPlatformApi::updateAllScheduleList()
{
	//if task is working ,count is greater than 0
	if (currentTaskCount(updateScheduleListTask) > 0) {
		return;
	}
	static QTimer *timer = nullptr;
	auto runUpdate = [this](PLSPlatformBase *platfrom) {
		connect(platfrom, &PLSPlatformBase::scheduleListUpdateFinished, this, &PLSPlatformApi::onUpdateScheduleListFinished, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
		platfrom->toStartGetScheduleList();
	};

	auto updateAll = [this, runUpdate]() {
		this->loadingWidzardCheck();
		QMutexLocker locker(&platformListMutex);
		if (platformList.empty()) {
			emit allScheduleListUpdated();
			return;
		}

		setTaskCount(updateScheduleListTask, (int)platformList.size());
		std::for_each(platformList.cbegin(), platformList.cend(), runUpdate);
	};
	if (timer == nullptr) {
		timer = new QTimer(this);
		timer->setSingleShot(true);
		timer->setInterval(200);

		connect(timer, &QTimer::timeout, this, updateAll, Qt::QueuedConnection);
	}
	QMetaObject::invokeMethod(
		timer, []() { timer->start(); }, Qt::QueuedConnection);
}

void PLSPlatformApi::loadingWidzardCheck(bool isCheck)
{
	static QTimer *timer = nullptr;
	if (timer == nullptr) {
		timer = new QTimer(this);
		timer->setInterval(15000);
		timer->setSingleShot(true);
		connect(
			timer, &QTimer::timeout, this,
			[this]() {
				if (currentTaskCount(updateScheduleListTask) > 0) {
					PLS_INFO(MODULE_PlatformService, "force finished update schedule list for time out");
					resetTaskCount(updateScheduleListTask);
					emit allScheduleListUpdated();
				}
			},
			Qt::QueuedConnection);
	}
	if (isCheck) {
		timer->start();
	} else {
		timer->stop();
	}
}

QVariantList PLSPlatformApi::getAllScheduleList() const
{
	QMutexLocker locker(&platformListMutex);
	QVariantList ret;
	auto getList = [&ret](PLSPlatformBase *platform) {
		if (platform->isValid()) {
			ret.append(platform->getLastScheduleList());
		}
	};
	std::for_each(platformList.cbegin(), platformList.cend(), getList);
	return ret;
}

QVariantList PLSPlatformApi::getAllLastErrors() const
{
	QMutexLocker locker(&platformListMutex);
	QVariantList ret;
	auto getList = [&ret](PLSPlatformBase *platform) {
		if (!platform->isValid()) {
			return;
		}
		auto error = platform->getLastError();
		if (error.isEmpty()) {
			return;
		}
		ret.append(error);
	};
	std::for_each(platformList.cbegin(), platformList.cend(), getList);
	return ret;
}

void PLSPlatformApi::onUpdateScheduleListFinished()
{
	auto platform = dynamic_cast<PLSPlatformBase *>(sender());
	if (platform == nullptr) {
		return;
	}
	decreaseCount(updateScheduleListTask);
	if (currentTaskCount(updateScheduleListTask) > 0) {
		return;
	}
	emit allScheduleListUpdated();
}

void PLSPlatformApi::setTaskCount(const QString &taskKey, int taskCount)
{
	m_taskWaiting.insert(taskKey, taskCount);
}

int PLSPlatformApi::currentTaskCount(const QString &taskKey) const
{
	return m_taskWaiting.value(taskKey);
}

void PLSPlatformApi::decreaseCount(const QString &taskKey, int vol)
{
	auto it = m_taskWaiting.find(taskKey);
	if (it != m_taskWaiting.end()) {
		auto lastValue = it.value() - vol;
		if (lastValue < 0) {
			lastValue = 0;
		}
		it.value() = lastValue;
	}
}

void PLSPlatformApi::resetTaskCount(const QString &taskKey)
{
	m_taskWaiting.remove(taskKey);
}

void PLSPlatformApi::doStatRequest(const QJsonObject &data)
{
	
}

void PLSPlatformApi::doMqttStatForPlatform(const PLSPlatformBase *base, const QJsonObject &data) const
{
	
}

void PLSPlatformApi::doStatusRequest(const QJsonObject &data)
{
	
}

void PLSPlatformApi::doStartGpopMaxTimeLiveTimer()
{
	
}

void PLSPlatformApi::startGeneralMaxTimeLiveTimer()
{
	
}

void PLSPlatformApi::stopGeneralMaxTimeLiveTimer()
{
	
}

void PLSPlatformApi::doNoticeLong(const QJsonObject &data) const
{
	
}

void PLSPlatformApi::doNaverShoppingMaxLiveTime(int leftMinutes) const
{
	
}

void PLSPlatformApi::doGeneralMaxLiveTime(int leftMinutes) const
{
	
}

void PLSPlatformApi::doLiveFnishedByPlatform(const QJsonObject &data)
{
	
}

void PLSPlatformApi::doOtherMqttStatusType(const QJsonObject &data, const QString &statusType)
{
	
}

void PLSPlatformApi::doMqttRequestBroadcastEnd(PLSPlatformBase *platform)
{
	
}

void PLSPlatformApi::doMqttRequestAccessToken(PLSPlatformBase *platform) const
{
	
}

void PLSPlatformApi::doMqttBroadcastStatus(const PLSPlatformBase *, PLSPlatformMqttStatus status) const
{
	
}

void PLSPlatformApi::doMqttSimulcastUnstable(PLSPlatformBase *platform, PLSPlatformMqttStatus status)
{
	
}

void PLSPlatformApi::doMqttSimulcastUnstableError(PLSPlatformBase *platform, PLSPlatformMqttStatus status)
{
	
}

void PLSPlatformApi::doMqttChatRequest(QString value)
{
	
}

void PLSPlatformApi::doWebTokenRequest(const QJsonObject &jsonData)
{
	
}

void PLSPlatformApi::sendWebPrismToken(const PLSPlatformBase *platform) const
{
	
}

void PLSPlatformApi::sendWebPrismPlatformClose(const PLSPlatformBase *platform) const
{
	
}

void PLSPlatformApi::doWebBroadcastMessage(const QJsonObject &data) const
{
	
}

void PLSPlatformApi::doWebPageLogsMessage(const QJsonObject &obj) const
{
	
}

void PLSPlatformApi::doWebSendChatRequest(const QJsonObject &data) const
{
	
}

const char *PLSPlatformApi::invokedByWeb(const char *data)
{
	static const string EMPTY = "{}";

	emit PLS_PLATFORM_API->onWebRequest(QString::fromUtf8(data));

	return EMPTY.c_str();
}

void PLSPlatformApi::doWebRequest(const QString &data)
{
	
}

void PLSPlatformApi::onMqttMessage(const QString topic, const QString content)
{
	
}

void PLSPlatformApi::showEndViewByType(PLSEndPageType pageType) const
{
	auto mainwindw = PLSBasic::Get();
	QString startLog("show live end view");
	switch (pageType) {
	case PLSEndPageType::PLSRecordPage:
		startLog += " - record";
		break;
	case PLSEndPageType::PLSRehearsalPage:
		startLog += " - rehearsal";
		break;
	case PLSEndPageType::PLSLivingPage:
		startLog += " - live";
		break;
	default:
		break;
	}

	PLS_LIVE_INFO(MODULE_PlatformService, "%s", qUtf8Printable(startLog));
	PLSLiveEndDialog dialog(pageType, mainwindw);
	connect(mainwindw, &PLSBasic::mainClosing, &dialog, [&dialog] {
		PLS_LIVE_INFO(MODULE_PlatformService, "PLSEnd Dialog disbind and force to close");
		dialog.setParent(nullptr);
		dialog.close();
	});
	dialog.exec();
	PLS_INFO(MODULE_PlatformService, "PLSEnd Dialog closed");
}

void PLSPlatformApi::showEndView_Record(bool isShowDialog, bool isLivingAndRecording, bool isRehearsal)
{
	if (m_isIgnoreNextRecordShow) {
		//ignore this end page
		PLS_INFO(END_MODULE, "Show end with parameter ignore this record end page");
		m_isIgnoreNextRecordShow = false;
	} else if (isLivingAndRecording) {
		//record stopped, but live still streaming
		if (!isRehearsal) {
			// not rehearsal mode
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("main.message.error.recordwhenbroadcasting"));
		} else {
			// rehearsal mode
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("Live.Toast.Rehaersal.Record.Ended"));
		}
	} else {
		pls_toast_clear();
		//only record and live all stopped, to clear toast.
		if (PLSBasic::Get() && PLSBasic::instance()->getMainView() != nullptr && PLSBasic::instance()->getMainView()->isVisible() && isShowDialog) {
			showEndViewByType(PLSEndPageType::PLSRecordPage);
			if (pls_get_app_exiting()) {
				return;
			}
		}
	}
	PLSCHANNELS_API->setIsClickToStopRecord(false);
	emit liveEndPageShowComplected(true);
}

static bool isSupportRehearsalShowEndPage(enum PLSServiceType type)
{
	if (type == PLSServiceType::ST_NAVER_SHOPPING_LIVE) {
		return true;
	}
	if (type == PLSServiceType::ST_YOUTUBE) {
		return true;
	}
	return false;
}

void PLSPlatformApi::showEndView_Live(bool isShowDialog, bool isLivingAndRecording, bool isRehearsal, bool isStreamingRecordStopAuto)
{
	if (isLivingAndRecording && !isStreamingRecordStopAuto) {
		//live ended, but still recording
		if (isRehearsal) {
			if (PLS_PLATFORM_ACTIVIED.empty()) {
				PLS_INFO(END_MODULE, "show rehearsal page active platform list is empty");
				return;
			}
			const PLSPlatformBase *platform = PLS_PLATFORM_ACTIVIED.front();
			if (isSupportRehearsalShowEndPage(platform->getServiceType())) {
				QString content = QString("%1\n%2").arg(tr("broadcast.endpage.rehearsal.title")).arg(tr("navershopping.liveinfo.rehearsal.endpage.content"));
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, content);
			} else {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("broadcast.end.rehearsal"));
			}
		} else {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("broadcast.end.live"));
		}
		emit liveEndPageShowComplected(false);
		return;
	}

	pls_toast_clear();

	//only record and live all stopped, to clear toast.
	if (PLSBasic::Get() && PLSBasic::instance()->getMainView() != nullptr && PLSBasic::instance()->getMainView()->isVisible() && isShowDialog) {
		if (PLS_PLATFORM_ACTIVIED.empty()) {
			PLS_INFO(END_MODULE, "show end page active platform list is empty");
			return;
		}
		if (!isRehearsal) {
			showEndViewByType(PLSEndPageType::PLSLivingPage);
		} else if (const PLSPlatformBase *platform = PLS_PLATFORM_ACTIVIED.front(); isSupportRehearsalShowEndPage(platform->getServiceType())) {
			showEndViewByType(PLSEndPageType::PLSRehearsalPage);
		}
		if (pls_get_app_exiting()) {
			return;
		}
	}
	emit liveEndPageShowComplected(false);
}

void PLSPlatformApi::showEndView(bool isRecord, bool isShowDialog)
{
	PLS_INFO(MODULE_PlatformService, "show live end view isRecord(%s)", BOOL2STR(isRecord));

	if (pls_get_app_exiting()) {
		PLS_INFO(MODULE_PlatformService, "show live end view ignored, because app is exiting.");
		return;
	}

	//User clicks stop recording button
	bool isClickToStopRecord = PLSCHANNELS_API->getIsClickToStopRecord();
	bool isRecordingButNotToStop = PLSCHANNELS_API->currentReocrdState() == ChannelData::RecordStarted || PLSCHANNELS_API->currentReocrdState() == ChannelData::RecordStarting;
	qDebug() << "showEndView  isRecordingButNotToStop  : " << PLSCHANNELS_API->currentReocrdState();

	//When recording is stopped, the live broadcast is in progress
	//A recording is in progress when the stream is stopped
	bool isLivingAndRecording = false;
	if (isRecord && PLS_PLATFORM_API->isLiving()) {
		isLivingAndRecording = true;
	} else if (!isRecord && PLS_PLATFORM_API->isRecording()) {
		isLivingAndRecording = true;
	}

	qDebug() << "showEndView  isLivingAndRecording  : " << isLivingAndRecording;
	qDebug() << "showEndView  isClickToStopRecord  : " << isClickToStopRecord;
	qDebug() << "showEndView  isRecord  : " << isRecord;

	//When streaming starts, recording starts
	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	//when stopping live sometime later but not stop complected, then start record, the keepRecordingWhenStreamStops is true.
	bool keepRecordingWhenStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");

	//when stop streaming will stop record same time.
	bool isStreamingRecordStopAuto = recordWhenStreaming && !keepRecordingWhenStreamStops;
	bool isRehearsal = PLSCHANNELS_API->isRehearsaling();

	//End the recording page, if the recording is set to end automatically and the user does not click the end recording button, the recording End page will not be displayed
	if (isStreamingRecordStopAuto && !isClickToStopRecord) {
		if (isRecord) {
			m_isIgnoreNextRecordShow = true;
		} else if (!isRecord && isLivingAndRecording && !isRecordingButNotToStop) {
			m_isIgnoreNextRecordShow = true;
		}
	}

	qDebug() << "showEndView  m_isIgnoreNextRecordShow  : " << m_isIgnoreNextRecordShow;

	PLS_INFO(
		END_MODULE,
		"Show end with parameter \n\tisLivingAndRecording:%s, \n\tisShowRecordEnd:%s, \n\tisClickToStopRecord:%s, \n\tisStreamingRecordStopAuto:%s, \n\tisIgnoreNextRecordShow:%s, \n\tisRehaersaling:%s, \n\tisRecordingButNotToStop:%s, \n\tisShowDialog:%s",
		BOOL2STR(isLivingAndRecording), BOOL2STR(isRecord), BOOL2STR(isClickToStopRecord), BOOL2STR(isStreamingRecordStopAuto), BOOL2STR(m_isIgnoreNextRecordShow), BOOL2STR(isRehearsal),
		BOOL2STR(isRecordingButNotToStop), BOOL2STR(isShowDialog));

	if (isRecord) {
		showEndView_Record(isShowDialog, isLivingAndRecording, isRehearsal);
	} else {
		showEndView_Live(isShowDialog, isLivingAndRecording, isRehearsal, isStreamingRecordStopAuto);
	}
}

void PLSPlatformApi::doChannelInitialized()
{
	for (const auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		const auto channelUUID = info.value(ChannelData::g_channelUUID).toString();
		const auto channelName = info.value(ChannelData::g_platformName).toString();
		if (isValidChannel(info)) {
			onActive(channelUUID);
		} else {
			PLS_INFO(MODULE_PlatformService, "doChannelInitialized .InvalidChannels: type=%d, name=%s, uuid=%s", info.value(ChannelData::g_data_type).toInt(),
				 channelName.toStdString().c_str(), channelUUID.toStdString().c_str());
		}
	}
}

extern OBSData GetDataFromJsonFile(const char *jsonFile);
int PLSPlatformApi::getOutputBitrate()
{
	auto config = PLSBasic::Get()->Config();

	auto mode = config_get_string(config, "Output", "Mode");
	auto advOut = astrcmpi(mode, "Advanced") == 0;

	if (advOut) {
		OBSData streamEncSettings = GetDataFromJsonFile("streamEncoder.json");
		return int(obs_data_get_int(streamEncSettings, "bitrate"));
	} else {
		return int(config_get_int(config, "SimpleOutput", "VBitrate"));
	}
}
