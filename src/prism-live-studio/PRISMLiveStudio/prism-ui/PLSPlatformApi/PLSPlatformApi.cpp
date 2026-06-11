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
#include "ChannelCommonFunctions.h"
#include "pls/pls-source.h"
#include "pls/pls-dual-output.h"
#include <sstream>
#include "PLSLoginDataHandler.h"

constexpr int PRISM_MAX_OUT_Y = 1080;

using namespace std;
using namespace common;
extern QString translatePlatformName(const QString &platformName);

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

const QString LIVE_ABORT_STATUS_CODE_KEY = "statusCode";
const QString LIVE_ABORT_ERROR_CODE_KEY = "errorCode";
const QString LIVE_ABORT_REQUEST_URL_KEY = "requestURL";
const QString LIVE_ABORT_JSON_TEXT_KEY = "jsonText";

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
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelCreateError, this, [this](const QString &channelUUID) { onRemoveChannel(channelUUID, {}); }, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigOperationChannelDone, this, &PLSPlatformApi::onAllChannelRefreshDone, Qt::QueuedConnection);
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
	//Notice, onWebRequest is send from Browser thread, not main thread
	connect(this, &PLSPlatformApi::onWebRequest, this, &PLSPlatformApi::doWebRequest);
	connect(this, &PLSPlatformApi::livePrepared, this, [this](bool value) {
		if (value == false) {
			emit enterLivePrepareState(false);
		}
	});
	connect(this, &PLSPlatformApi::liveStarted, this, [this]() { emit enterLivePrepareState(false); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::startFailed, this, [this]() { setLiveStatus(LiveStatus::Normal); }, Qt::QueuedConnection);

	return true;
}

QString FindProtocol(const string &server)
{
	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_property_t *services = obs_properties_get(props, "service");

	OBSDataAutoRelease settings = obs_data_create();

	obs_data_set_string(settings, "service", server.data());
	obs_property_modified(services, settings);

	obs_properties_destroy(props);

	const char *protocol = obs_data_get_string(settings, "protocol");
	if (protocol && *protocol)
		return QT_UTF8(protocol);

	return QString("RTMP");
}

void PLSPlatformApi::saveStreamSettings(string platform, string server, const string_view &key, bool bVerticalOutput, const QString &rtmpUserId, const QString &rtmpUserPassword)
{
	auto originPlatform = platform;
	PLS_LIVE_INFO(MODULE_PlatformService, "save streaming before info name=%s, StreamUrl=%s", platform.c_str(), server.c_str());
	PLS_INFO_KR(MODULE_PlatformService, "save streaming before info name=%s, StreamUrl=%s, StreamKey=%s", platform.c_str(), server.c_str(), key.data());
	if (isLiving()) {
		return;
	}
	auto serviceData = PLSBasic::instance()->LoadServiceData();
	if (!pls_is_dual_output_on() && !platform.empty() && platform != "Prism" && serviceData) {
		OBSDataAutoRelease settings = obs_data_get_obj(serviceData, "settings");
		const char *service = obs_data_get_string(settings, "service");
		string strService = service;
		const char *chServer = obs_data_get_string(settings, "server");
		PLS_INFO(MODULE_PlatformService, "LoadServiceData  service=%s, chServer=%s", service, chServer);
		string strServer = chServer;
		if (platform == "Twitch") {
			bool bWHIP = isTwitchWHIP();
			if (bWHIP) {
				platform = "WHIP";
				server = PLSSyncServerManager::instance()->getTwitchWhipServer().toStdString();
			} else {
				if (strServer != "auto" && strServer != "ServerAuto") {
					server = chServer;
				}
			}
		} else {
			transform(strService.begin(), strService.end(), strService.begin(), ::tolower);
			string tmpStr = platform;
			transform(tmpStr.begin(), tmpStr.end(), tmpStr.begin(), ::tolower);
			if (strService.find(tmpStr) != std::string::npos) {
				platform = service;
				if (strServer != "auto" && strServer != "ServerAuto") {
					server = chServer;
				}
			}
			if (platform == "YouTube") {
				if (strService != "YouTube - HLS") {
					platform = "YouTube - RTMPS";
				} else {
					platform = strService;
				}
			}
		}
	}

	auto activiedPlatforms = getActivePlatforms();
	if (!serviceData) {
		PLS_INFO(MODULE_PlatformService, "LoadServiceData  serviceData is null");
		if (activiedPlatforms.size() == 1 && platform == "YouTube") {
			platform = "YouTube - RTMPS";
		}
		if (!pls_is_dual_output_on() && activiedPlatforms.size() == 1 && platform == "Twitch") {
			platform = "WHIP";
			server = PLSSyncServerManager::instance()->getTwitchWhipServer().toStdString();
		}
	}

	PLS_LIVE_INFO(MODULE_PlatformService, "save streaming after info name=%s, StreamUrl=%s", platform.c_str(), server.c_str());
	PLS_INFO_KR(MODULE_PlatformService, "save streaming after info name=%s, StreamUrl=%s, StreamKey=%s", platform.c_str(), server.c_str(), key.data());
	OBSData settings = obs_data_create();
	obs_data_release(settings);

	auto serviceId = platform.empty() ? "rtmp_custom" : "rtmp_common";

	if (1 == activiedPlatforms.size()) {
		if (auto pPlatform = activiedPlatforms.front(); pPlatform->getChannelType() >= ChannelData::ChannelDataType::CustomType && pPlatform->getChannelName() == TWITCH) {
			platform = "Twitch";
			serviceId = "rtmp_common";
		}
	}

	if (!platform.empty()) {
		obs_data_set_string(settings, "service", platform.data());
		m_platFormUrlMap.insert(originPlatform.data(), server.data());
		if ("WHIP" != platform) {
			obs_data_set_string(settings, "protocol", QT_TO_UTF8(FindProtocol(platform)));
		}
	}

	obs_data_set_string(settings, "server", server.data());
	if (platform == "WHIP") {
		serviceId = "whip_custom";
		obs_data_set_string(settings, "bearer_token", key.data());
	} else {
		obs_data_set_string(settings, "key", key.data());
	}

	if (rtmpUserId.length() > 0 && rtmpUserPassword.length() > 0) {
		obs_data_set_bool(settings, "use_auth", true);
		obs_data_set_string(settings, "username", QT_TO_UTF8(rtmpUserId));
		obs_data_set_string(settings, "password", QT_TO_UTF8(rtmpUserPassword));
	}

	saveStreamSettings(serviceId, settings, bVerticalOutput);
}

void PLSPlatformApi::saveStreamSettings(const char *serviceId, OBSData settings, bool bVerticalOutput) const
{
	obs_service_t *oldService = obs_frontend_get_streaming_service();
	OBSData hotkeyData = obs_hotkeys_save_service(oldService);
	obs_data_release(hotkeyData);

	OBSService newService = obs_service_create(serviceId, "default_service", settings, hotkeyData);
	obs_service_release(newService);

	if (!newService)
		return;

	if (bVerticalOutput && pls_is_dual_output_on()) {
		PLSBasic::instance()->setVerticalService(newService);
	} else {
		obs_frontend_set_streaming_service(newService);
		obs_frontend_save_streaming_service();
	}
}

void PLSPlatformApi::setPlatformUrl()
{
	auto platforms = getActivePlatforms();
	if (platforms.size() != 1) {
		return;
	}
	auto platformBase = platforms.front();
	if (platformBase->getServiceType() == PLSServiceType::ST_CUSTOM) {
		return;
	}

	QString platformNameKey = platformBase->getNameForSettingId();
	QString server = m_platFormUrlMap.value(platformNameKey);
	if (!server.isEmpty()) {
		platformBase->setStreamServer(server.toStdString());
	}
}

list<PLSPlatformBase *> PLSPlatformApi::getActivePlatforms() const
{
	list<PLSPlatformBase *> lst;

	QMutexLocker locker(&platformListMutex);
	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [](const auto v) { return v->isActive(); });

	return lst;
}

list<PLSPlatformBase *> PLSPlatformApi::getHorizontalPlatforms() const
{
	list<PLSPlatformBase *> lst;

	QMutexLocker locker(&platformListMutex);
	bool is_dual_output = pls_is_dual_output_on();
	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [is_dual_output](const auto v) { return v->isActive() && (!is_dual_output || v->isHorizontalOutput()); });

	return lst;
}

list<PLSPlatformBase *> PLSPlatformApi::getVerticalPlatforms() const
{
	list<PLSPlatformBase *> lst;

	QMutexLocker locker(&platformListMutex);
	copy_if(platformList.begin(), platformList.end(), back_inserter(lst), [](const auto v) { return v->isActive() && v->isVerticalOutput(); });

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

bool PLSPlatformApi::isTwitchWHIP()
{
	if (pls_is_dual_output_on()) {
		return false;
	}

	auto allPlatforms = getAllPlatforms();
	auto activePlatforms = getActivePlatforms();
	auto twitchPlatform = getPlatformTwitch();
	if (twitchPlatform) {
		if (allPlatforms.size() == 1 || (activePlatforms.size() == 1 && twitchPlatform->isActive())) {
			auto serviceData = PLSBasic::instance()->LoadServiceData();
			if (serviceData) {
				OBSDataAutoRelease settings = obs_data_get_obj(serviceData, "settings");
				const char *service = obs_data_get_string(settings, "service");
				QString strService = service;
				PLS_INFO(MODULE_PlatformService, "get service is %s", service);
				if (isLiving() && strService != "WHIP") {
					PLS_INFO(MODULE_PlatformService, "living service is not whip");
					return false;
				}
				if (!strService.contains("Twitch", Qt::CaseInsensitive)) {
					PLS_INFO(MODULE_PlatformService, "service not contain twitch ,judge this is whip");
					return true;
				}
			} else {
				PLS_INFO(MODULE_PlatformService, "serviceData is empty, judge this is whip");
				return true;
			}
		}
	}
	PLS_INFO(MODULE_PlatformService, "not contain twitch platform, judge this is not whip");
	return false;
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

PLSPlatformChzzk *PLSPlatformApi::getPlatformChzzk()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_CHZZK);
	if (platform) {
		return dynamic_cast<PLSPlatformChzzk *>(platform);
	}
	return nullptr;
}

PLSPlatformChzzk *PLSPlatformApi::getPlatformChzzkActive()
{
	auto platform = getExistedPlatformByType(PLSServiceType::ST_CHZZK);
	if (platform && platform->isActive()) {
		return dynamic_cast<PLSPlatformChzzk *>(platform);
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

list<PLSPlatformNCB2B *> PLSPlatformApi::getPlatformNCB2B()
{
	list<PLSPlatformNCB2B *> plist;
	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_NCB2B) {
			plist.push_back(dynamic_cast<PLSPlatformNCB2B *>(platform));
		}
	}
	return plist;
}

PLSPlatformNCB2B *PLSPlatformApi::getPlatformNCB2BActive()
{
	QMutexLocker locker(&platformListMutex);
	for (auto platform : platformList) {
		if (platform->getServiceType() == PLSServiceType::ST_NCB2B && platform->isActive()) {
			return dynamic_cast<PLSPlatformNCB2B *>(platform);
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

	const auto channelName = info.value(ChannelData::g_channelName).toString();
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

	const auto channelName = info.value(ChannelData::g_channelName).toString();
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

	const auto channelName = info.value(ChannelData::g_channelName).toString();
	if (!isValidChannel(info)) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI add channel get channel info is invalid, channel type is %d, channel name is %s, channel uuid is %s",
			  info.value(ChannelData::g_data_type).toInt(), channelName.toStdString().c_str(), channelUUID.toStdString().c_str());
		return;
	}

	PLS_INFO(MODULE_PlatformService, "PlatformAPI add platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
		 channelName.toStdString().c_str(), channelUUID.toStdString().c_str());

	getPlatformById(channelUUID, info);
}

void PLSPlatformApi::onRemoveChannel(const QString &channelUUID, const QVariantMap &unDeletedChannelData)
{
	auto platform = getExistedPlatformById(channelUUID);
	if (nullptr != platform) {
		if (platform->isActive() && isExclusiveChannel(unDeletedChannelData)) {
			PLSBasic::instance()->changeOutputCount(-1);
		}
		platform->setActive(false);

		const QVariantMap &info = platform->getInitData();
		const auto channelName = info.value(ChannelData::g_channelName).toString();
		PLS_INFO(MODULE_PlatformService, "PlatformAPI remove platform success, channel type is %d, channel name is %s , channel uuid is %s", info.value(ChannelData::g_data_type).toInt(),
			 channelName.toStdString().c_str(), channelUUID.toStdString().c_str());
		emit channelRemoved(info);

		QMutexLocker locker(&platformListMutex);
		platformList.remove(platform);
		pls_delete(platform);
		PLS_INFO(MODULE_PlatformService, "PlatformAPI current remove platformList count is %d", platformList.size());
	}
	updatePlatformViewerCount();
}

void PLSPlatformApi::onUpdateChannel(const QString &which)
{
	QVariantMap info = PLSCHANNELS_API->getChannelInfo(which);
	if (info.empty()) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI update channel get channel info is empty, channel uuid is %s", which.toStdString().c_str());
		return;
	}

	const auto channelName = info.value(ChannelData::g_channelName).toString();
	if (!isValidChannel(info)) {
		PLS_ERROR(MODULE_PlatformService, "PlatformAPI update channel get channel info is invalid, channel type is %d, channel name is %s, channel uuid is %s",
			  info.value(ChannelData::g_data_type).toInt(), channelName.toStdString().c_str(), which.toStdString().c_str());
		return;
	}

	auto platform = getPlatformById(which, info);
	if (nullptr != platform && platform->isActive()) {
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
	PLSBasic::instance()->changeOutputCount(1);

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
	m_endLiveType = EndLiveType::NONE_TYPE;
	m_ignoreRequestBroadcastEnd = false;
	m_platFormUrlMap.clear();

	//Get the output resolution and fps in the setting
	auto out_cx = config_get_uint(PLSBasic::Get()->Config(), "Video", "OutputCX");
	auto out_cy = config_get_uint(PLSBasic::Get()->Config(), "Video", "OutputCY");

	//Sort platform pointer objects
	sortPlatforms();

	//Get the list of platforms activated when the live broadcast starts
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.empty()) {
		PLS_INFO(MODULE_PlatformService, "%s %s ActiveChannel list is Empty", PrepareInfoPrefix, __FUNCTION__);
		prepareLiveCallback(false);
		return;
	}

	//Get a list of platform UUIDs that are activated when the live broadcast starts
	uuidOnStarted.clear();
	for (auto &item : platformActived) {
		uuidOnStarted.push_back(item->getChannelUUID());
		PLS_INFO(MODULE_PlatformService, "%s Actived Platform channel uuid is %s , channel type name is %s , channel name is %s", PrepareInfoPrefix,
			 item->getChannelUUID().toStdString().c_str(), item->getNameForChannelType(), item->getChannelName().toStdString().c_str());
	}

	//reset live active platform info
	resetPlatformsLivingInfo();

	auto bDualOutput = pls_is_dual_output_on();
	auto platformsHorizontal = getHorizontalPlatforms();
	auto platformVertical = getVerticalPlatforms();

	if (bDualOutput) {
		m_bPrismLive[DualOutputType::Horizontal] = platformsHorizontal.size() > 1;
		m_bPrismLive[DualOutputType::Vertical] = platformVertical.size() > 1;

		if (platformsHorizontal.empty() || platformVertical.empty()) {
			pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("DualOutput.Streaming.No.Platform"));
			prepareLiveCallback(false);
			return;
		}
	} else {
		m_bPrismLive[DualOutputType::Horizontal] = platformActived.size() > 1;
	}

	m_listFailedPlatform.clear();

	m_bStopStream[DualOutputType::Horizontal] = false;
	m_bStopStream[DualOutputType::Vertical] = false;

	auto strDualOutput = pls_bool_2_string(bDualOutput);
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, {{"dualOutput", strDualOutput}}, "Is dualOutput mode: %s", strDualOutput);

	platformActived.front()->onPrepareLive(true);
}

void PLSPlatformApi::showMultiplePlatformGreater1080pAlert()
{
	auto alertResult = QDialogButtonBox::Cancel;
	if (isRecording() || isVirtualCameraActive()) {
		pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Live.Check.Internal.Greater.1080.Recording"));
	} else {
		alertResult = pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Live.Check.Multicast.Greater.1080"));
	}
	PLS_INFO(MODULE_PlatformService, "%s %s present 1080p alert isRecording(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(isRecording()));
	prepareLiveCallback(false);
	if (QDialogButtonBox::Ok == alertResult) {
		ResolutionGuidePage::setVisibleOfGuide(PLSBasic::Get());
	}
}

void PLSPlatformApi::sortPlatforms()
{
	QMutexLocker locker(&platformListMutex);
	platformList.sort([](const PLSPlatformBase *lValue, const PLSPlatformBase *rValue) {
		if (pls_is_dual_output_on()) {
			if (lValue->isHorizontalOutput() != rValue->isHorizontalOutput()) {
				return lValue->isHorizontalOutput();
			}
			return lValue->getChannelOrder() < rValue->getChannelOrder();
		} else {
			return lValue->getChannelOrder() < rValue->getChannelOrder();
		}
	});
}

bool PLSPlatformApi::checkNetworkConnected()
{
	if (!pls_get_network_state()) {
		PLS_INFO(MODULE_PlatformService, "%s %s current network environment diabled", PrepareInfoPrefix, __FUNCTION__);
		PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("login.check.note.network"));
		prepareLiveCallback(false);
		return false;
	}
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
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	if (platformActived.size() == 1) {
		auto type = platformActived.front()->getChannelType();
		auto platName = platformActived.front()->getPlatFormName();
		bool bEnable = config_get_bool(App()->GetUserConfig(), "General", "Watermark");
		if (platName == NCB2B && (type == ChannelData::ChannelDataType::ChannelType)) {
			if ((bEnable && !PLSServerStreamHandler::instance()->isValidWatermark(platName)) || !PLSServerStreamHandler::instance()->isValidOutro(platName)) {
				PLSLoginDataHandler::instance()->reDownloadWaterMark();
				pls_alert_error_message(nullptr, QTStr("Alert.Title"), QTStr("Ncb2b.Watermark.Resource.Not.Existed"));
				return false;
			}
		}
	}
	return true;
}

bool PLSPlatformApi::checkOutputBitrateValid()
{
	auto iMultiplePlatformMaxBitrate = PLSSyncServerManager::instance()->getMultiplePlatformMaxBitrate();
	auto videoBitrate = getOutputBitrate();
	if (videoBitrate > iMultiplePlatformMaxBitrate) {
		PLS_INFO(MODULE_PlatformService, "%s %s max bitrate error, current bitrate: %d, max bitrate: %d", PrepareInfoPrefix, __FUNCTION__, videoBitrate, iMultiplePlatformMaxBitrate);
		auto alertResult = pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("Broadcast.Bitrate.Too.High").arg(iMultiplePlatformMaxBitrate));
		prepareLiveCallback(false);
		if (QDialogButtonBox::Ok == alertResult) {
			//PLSBasic::Get()->showSettingVideo();
		}
		return false;
	}
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
	bool isSpecialPlatform = platform->getServiceType() == PLSServiceType::ST_BAND || platform->getServiceType() == PLSServiceType::ST_NAVER_SHOPPING_LIVE ||
				 platform->getServiceType() == PLSServiceType::ST_NCB2B;
	auto channelName = platform->getInitData().value(ChannelData::g_channelName).toString();
	auto displayPlatformName = translatePlatformName(channelName);
	PLS_INFO(MODULE_PlatformService, "mqtt max live time status: notifyLiveLeftMinutes channel name is %s , maxLiveTime is %d , leftMinutes is %d", platform->getNameForChannelType(), maxLiveTime,
		 leftMinutes);
	if (isSpecialPlatform) {
		if (leftMinutes > 0) {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE,
					  QTStr("MQTT.Max.Live.Time.Band.NaverShopping.Left.Minute").arg(displayPlatformName).arg(maxLiveTime / 60).arg(leftMinutes));
			return;
		}
		if (getActivePlatforms().size() == 1) {
			stopMqtt();
			PLS_PLATFORM_API->stopStreaming(QString("end live because %1 live reach max live time").arg(displayPlatformName), EndLiveType::MQTT_END_LIVE);
			pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("MQTT.Max.Live.Time.Band.NaverShopping.End.Live").arg(displayPlatformName));
			return;
		}
		PLSCHANNELS_API->setChannelStatus(platform->getChannelUUID(), ChannelData::Error);
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Max.Live.Time.Band.NaverShopping.End.Live").arg(displayPlatformName));
	} else {
		if (leftMinutes == 0) {
			stopMqtt();
			PLS_PLATFORM_API->stopStreaming(QString("end live because %1 live reach max live time").arg(displayPlatformName));
			pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("MQTT.Max.Live.Time.General.Platform.End.Live"));
			return;
		}
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Max.Live.Time.General.Platform.Left.Minute").arg(leftMinutes));
	}
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
	if (LiveStatus::LiveEnded != m_liveStatus && LiveStatus::Normal != m_liveStatus) {
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
	const auto platformName = info.value(ChannelData::g_fixPlatformName).toString();
	if (type == ChannelData::ChannelType) {
		for (int i = 1; i < PLATFORM_SIZE; ++i) {
			if (platformName == NamesForChannelType[i]) {
				return static_cast<PLSServiceType>(i);
			}
		}
	} else if (type >= ChannelData::CustomType) {
		return PLSServiceType::ST_CUSTOM;
	}
	PLS_ERROR(MODULE_PlatformService, "PlatformAPI get platform type failed, unmatched channel type is %1, channel name is %2", type, platformName.toStdString().c_str());
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
	case PLSServiceType::ST_CHZZK:
		platform = pls_new<PLSPlatformChzzk>();
		break;
	case PLSServiceType::ST_NCB2B:
		platform = pls_new<PLSPlatformNCB2B>();
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
	const auto channelName = info.value(ChannelData::g_channelName).toString();
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

	//call platform onlive started method
	for (PLSPlatformBase *pPlatform : PLS_PLATFORM_ACTIVIED) {
		pPlatform->onLiveStarted();
		if (!pPlatform->getIsAllowPushStream()) {
			PLSCHANNELS_API->setChannelStatus(pPlatform->getChannelUUID(), ChannelData::Error);
			PLSCHANNELS_API->setChannelUserStatus(pPlatform->getChannelUUID(), ChannelData::Disabled);
		}
	}

	//Clear the number of views, likes, and comments on each platform
	clearLiveStatisticsInfo();

	//Register for MQTT messages without Rehearsal live broadcast
	if (!PLSCHANNELS_API->isRehearsaling()) {
		PLS_INFO(MODULE_PlatformService, "start up platformApi living mqtt");
		doStartGpopMaxTimeLiveTimer();
	}

	//Refresh chat UI of chat view
	checkAllPlatformLiveStarted();

	for (auto pPlatform : m_listFailedPlatform) {
		PLSCHANNELS_API->setChannelStatus(pPlatform->getChannelUUID(), ChannelData::Error);
	}

	for (auto &item : PLS_PLATFORM_PRSIM->getApiErrorList()) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QString("%1: %2").arg(joinPlatformNames(item.first), item.second.alertMsg));
	}
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

	const char *pszTypeEnd = "{\"type\": \"end\"}";
	pls_frontend_call_dispatch_js_event_cb("prism_events", pszTypeEnd);
	stopMqtt();

	//stop the generic platform timer
	stopGeneralMaxTimeLiveTimer();

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
		PLSBasic::Get()->m_bForceStop = true;

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
			QMetaObject::invokeMethod(this, [] { PLSBasic::Get()->StopVirtualCam(); }, Qt::QueuedConnection);
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

void PLSPlatformApi::stopMqtt(DualOutputType outputType)
{
}

const QString &PLSPlatformApi::getLiveEndReason() const
{
	return m_endLiveReason;
}

const EndLiveType PLSPlatformApi::getLiveEndType() const
{
	return m_endLiveType;
}

void PLSPlatformApi::setLiveEndReason(const QString &reason, EndLiveType endLiveType)
{
	m_endLiveType = endLiveType;
	if (endLiveType == EndLiveType::OTHER_TYPE) {
		m_endLiveReason = reason;
	} else if (endLiveType == EndLiveType::MQTT_END_LIVE) {
		m_endLiveReason = "MQTT message " + reason;
	} else if (endLiveType == EndLiveType::OBS_END_LIVE) {
		m_endLiveReason = "OBS message " + reason;
	}
}

void PLSPlatformApi::stopStreaming(const QString &reason, EndLiveType endLiveType, DualOutputType outputType)
{
	setLiveEndReason(reason, endLiveType);

	if (pls_is_dual_output_on()) {
		switch (outputType) {
		case DualOutputType::Horizontal:
			m_bStopStream[outputType] = true;
			if (m_bStopStream[DualOutputType::Vertical] || 0 == PLS_PLATFORM_PRSIM->getVideoSeq(DualOutputType::Vertical)) {
				outputType = DualOutputType::All;
			}
			break;
		case DualOutputType::Vertical:
			m_bStopStream[outputType] = true;
			if (m_bStopStream[DualOutputType::Horizontal] || 0 == PLS_PLATFORM_PRSIM->getVideoSeq(DualOutputType::Horizontal)) {
				outputType = DualOutputType::All;
			}
			break;
		default:
			break;
		}

		if (DualOutputType::All != outputType) {
			for (auto pPlatform : DualOutputType::Vertical == outputType ? getVerticalPlatforms() : getHorizontalPlatforms()) {
				PLSCHANNELS_API->setChannelStatus(pPlatform->getChannelUUID(), ChannelData::Error);
			}
		}
	} else {
		outputType = DualOutputType::All;
	}

	PLSCHANNELS_API->toStopBroadcast(outputType);
}

QString getLiveControlType()
{
	//live from: prism/remote-control/stream-deck/output-time
	auto type = pls_get_current_live_control_type();
	QString strType = "unknown";
	switch (type) {
	case ControlSrcType::None:
		strType = "Prism";
		break;
	case ControlSrcType::RemoteControl:
		strType = "remoteControl";
		break;
	case ControlSrcType::OutPutTimer:
		strType = "outputTimer";
		break;
	case ControlSrcType::StreamDeck:
		strType = "streamDeck";
		break;
	default:
		strType = "unknown";
		break;
	}
	return strType;
}

void getMoreVideoSettings(QString &res, QString &colorspace, bool isVertical)
{
	res = "none";
	colorspace = "none";

	struct obs_video_info ovi;
	if (isVertical) {
		if (!pls_get_vertical_video_info(&ovi)) {
			assert(false);
			return;
		}
	} else if (!obs_get_video_info(&ovi)) {
		assert(false);
		return;
	}

	res = QString("%1x%2").arg(ovi.output_width).arg(ovi.output_height);

	switch (ovi.colorspace) {
	case VIDEO_CS_DEFAULT:
		colorspace = "default";
		break;
	case VIDEO_CS_601:
		colorspace = "601";
		break;
	case VIDEO_CS_709:
		colorspace = "709";
		break;
	case VIDEO_CS_SRGB:
		colorspace = "srgb";
		break;
	case VIDEO_CS_2100_PQ:
		colorspace = "2100_pq";
		break;
	case VIDEO_CS_2100_HLG:
		colorspace = "2100_hlg";
		break;
	default:
		assert(false);
		colorspace = "undefined";
		break;
	}
}

void PLSPlatformApi::createAnalogInfo(QVariantMap &uploadVariantMap) const
{
}

void PLSPlatformApi::sendLiveAnalog(bool success, const QString &reason, int code) const
{
}

bool isERTMPCodec(const char *codec)
{
	if (strcmp(codec, "av1") == 0)
		return true;
#ifdef ENABLE_HEVC
	if (strcmp(codec, "hevc") == 0)
		return true;
#endif
	return false;
}

static void createLiveCodecInfo(QVariantMap &uploadVariantMap)
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

void PLSPlatformApi::sendBgTemplateAnalog(OBSData privious, OBSData current) const
{
}

void PLSPlatformApi::sendAudioVisualizerAnalog(const char *id, OBSData privious, OBSData current) const
{
}

void PLSPlatformApi::sendCameraDeviceAnalog(OBSData privious, OBSData current) const
{
}

void PLSPlatformApi::sendAnalogOnUserConfirm(OBSSource source, OBSData privious, OBSData current) const
{
}

void PLSPlatformApi::sendCodecAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendSceneTemplateAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendPlatformOutputGuideAnalog(const QVariantMap &info) const
{
}

void PLSPlatformApi::sendNCB2BLogin(const QVariantMap &info) const
{
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
	QMetaObject::invokeMethod(timer, []() { timer->start(); }, Qt::QueuedConnection);
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

void PLSPlatformApi::doStatusRequest(const QJsonObject &data, DualOutputType outputType)
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

void PLSPlatformApi::doNCPMaxLiveTime(const QJsonObject &data)
{
}

void PLSPlatformApi::doGeneralMaxLiveTime(int leftMinutes) const
{
}

void PLSPlatformApi::doLiveFnishedByPlatform(const QJsonObject &data)
{
}

void PLSPlatformApi::doOtherMqttStatusType(const QJsonObject &data, const QString &statusType, DualOutputType outputType)
{
}

void PLSPlatformApi::doMqttRequestBroadcastEnd(PLSPlatformBase *platform, DualOutputType outputType, const QJsonObject &jsonObject)
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

void PLSPlatformApi::onMqttMessage(const QString &topic, const QString &content, DualOutputType outputType)
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

QString PLSPlatformApi::getLiveAbortReason(LiveAbortStage stage, const QVariantMap &info)
{
	QString liveAbortReason;
	switch (stage) {
	case LiveAbortStage::LiveStartRequestFailed:
		liveAbortReason = "live abort because live start call api error";
		break;
	case LiveAbortStage::LiveDirectStartRequestFailed:
		liveAbortReason = "live abort because live direct start call api error";
		break;
	case LiveAbortStage::DuplicatedStreamingActive:
		liveAbortReason = "live abort because duplicated streaming active";
		break;
	case LiveAbortStage::DisableOutputsRef:
		liveAbortReason = "live abort because disabled output ref";
		break;
	case LiveAbortStage::SetupStreamingFailed:
		liveAbortReason = "live abort because start setup push streaming failed";
		break;
	case LiveAbortStage::StartStreamingFailed:
		liveAbortReason = "live abort because start prepare push streaming failed";
		break;
	case LiveAbortStage::ObsForceStoppingStream: {
		int errorCode = info.value(LIVE_ABORT_ERROR_CODE_KEY).toInt();
		liveAbortReason = QString("live abort because obs error code:%1").arg(errorCode);
		break;
	}
	case LiveAbortStage::LiveFinishedByPlatform:
		liveAbortReason = "live abort because mqtt LIVE_FINISHED_BY_PLATFORM";
		break;
	case LiveAbortStage::RequestBroadcastEnd:
		liveAbortReason = "live abort because mqtt REQUEST_BROADCAST_END";
		break;
	case LiveAbortStage::SimulcastUnstableEndLive:
		liveAbortReason = "live abort because mqtt SIMULCAST_UNSTABLE";
		break;
	default:
		break;
	}
	return liveAbortReason;
}

QString PLSPlatformApi::getLiveAbortDetailReason(LiveAbortDetailStage stage, const QVariantMap &info)
{
	QString liveAbortDetailReason;
	switch (stage) {
	case LiveAbortDetailStage::LiveStartRequestNotJsonObject: {
		int statusCode = info.value(LIVE_ABORT_STATUS_CODE_KEY).toInt();
		QString requestURL = info.value(LIVE_ABORT_REQUEST_URL_KEY).toString();
		liveAbortDetailReason = QString("%1 response data is not json object, statusCode is %2").arg(requestURL).arg(statusCode);
		break;
	}
	case LiveAbortDetailStage::LiveStartRequestFailed: {
		int statusCode = info.value(LIVE_ABORT_STATUS_CODE_KEY).toInt();
		int errorCode = info.value(LIVE_ABORT_ERROR_CODE_KEY).toInt();
		QString requestURL = info.value(LIVE_ABORT_REQUEST_URL_KEY).toString();
		liveAbortDetailReason = QString("%1 request failed, statusCode is %2, errorCode is %3").arg(requestURL).arg(statusCode).arg(errorCode);
		break;
	}
	case LiveAbortDetailStage::LiveStartRequestTimeout: {
		int statusCode = info.value(LIVE_ABORT_STATUS_CODE_KEY).toInt();
		QString requestURL = info.value(LIVE_ABORT_REQUEST_URL_KEY).toString();
		liveAbortDetailReason = QString("%1 request 15s timeout, statusCode is %2").arg(requestURL).arg(statusCode);
		break;
	}
	case LiveAbortDetailStage::LiveStartRequestFailedWithJson: {
		QString requestURL = info.value(LIVE_ABORT_REQUEST_URL_KEY).toString();
		liveAbortDetailReason = QString("%1 response json is %2").arg(requestURL).arg(QString::fromUtf8(info.value(LIVE_ABORT_JSON_TEXT_KEY).toByteArray()));
		break;
	}
	case LiveAbortDetailStage::DuplicatedStreamingActive:
		liveAbortDetailReason = getLiveAbortReason(LiveAbortStage::DuplicatedStreamingActive);
		break;
	case LiveAbortDetailStage::DisableOutputsRef:
		liveAbortDetailReason = getLiveAbortReason(LiveAbortStage::DisableOutputsRef);
		break;
	case LiveAbortDetailStage::SetupStreamingFailed:
		liveAbortDetailReason = getLiveAbortReason(LiveAbortStage::SetupStreamingFailed);
		break;
	case LiveAbortDetailStage::StartStreamingFailed:
		liveAbortDetailReason = getLiveAbortReason(LiveAbortStage::StartStreamingFailed);
		break;
	case LiveAbortDetailStage::ObsForceStoppingStream:
		liveAbortDetailReason = getLiveAbortObsErrorDetailReason(info);
		break;
	case LiveAbortDetailStage::LiveFinishedByPlatform: {
		liveAbortDetailReason = QString("videoSeq is %1 , reason json is %2")
						.arg(QString::fromStdString(PLS_PLATFORM_PRSIM->getCharVideoSeq()))
						.arg(QString::fromUtf8(info.value(LIVE_ABORT_JSON_TEXT_KEY).toByteArray()));
		break;
	}
	case LiveAbortDetailStage::RequestBroadcastEnd: {
		liveAbortDetailReason = QString("videoSeq is %1 , reason json is %2")
						.arg(QString::fromStdString(PLS_PLATFORM_PRSIM->getCharVideoSeq()))
						.arg(QString::fromUtf8(info.value(LIVE_ABORT_JSON_TEXT_KEY).toByteArray()));
		break;
	}
	case LiveAbortDetailStage::SimulcastUnstableEndLive: {
		liveAbortDetailReason =
			QString("videoSeq is %1 , %2").arg(QString::fromStdString(PLS_PLATFORM_PRSIM->getCharVideoSeq())).arg(getLiveAbortReason(LiveAbortStage::SimulcastUnstableEndLive));
		break;
	}
	default:
		break;
	}
	return liveAbortDetailReason;
}

QString PLSPlatformApi::getLiveAbortPlatformName()
{
	QStringList platformNameList;
	for (auto item : PLS_PLATFORM_ACTIVIED) {
		if (!item->getIsAllowPushStream()) {
			continue;
		}
		const char *platformName = item->getNameForChannelType();
		platformNameList.append(platformName);
	}
	return platformNameList.join(",");
}

QString PLSPlatformApi::getLiveAbortObsErrorDetailReason(const QVariantMap &info)
{
	int errorCode = info.value(LIVE_ABORT_ERROR_CODE_KEY).toInt();

	QMap<int, QString> statMap;
	statMap.insert(OBS_OUTPUT_BAD_PATH, "OBS_OUTPUT_BAD_PATH");
	statMap.insert(OBS_OUTPUT_CONNECT_FAILED, "OBS_OUTPUT_CONNECT_FAILED");
	statMap.insert(OBS_OUTPUT_INVALID_STREAM, "OBS_OUTPUT_INVALID_STREAM");
	statMap.insert(OBS_OUTPUT_ERROR, "OBS_OUTPUT_ERROR");
	statMap.insert(OBS_OUTPUT_DISCONNECTED, "OBS_OUTPUT_DISCONNECTED");
	statMap.insert(OBS_OUTPUT_UNSUPPORTED, "OBS_OUTPUT_UNSUPPORTED");
	statMap.insert(OBS_OUTPUT_NO_SPACE, "OBS_OUTPUT_NO_SPACE");
	statMap.insert(OBS_OUTPUT_ENCODE_ERROR, "OBS_OUTPUT_ENCODE_ERROR");
	statMap.insert(OBS_OUTPUT_HDR_DISABLED, "OBS_OUTPUT_HDR_DISABLED");

	return QString("[%1] receive obs error code string is %2").arg(getLiveAbortPlatformName()).arg(statMap.value(errorCode));
}

void PLSPlatformApi::sendLiveAbortOperation(const QString &liveAbortReason, const QString &liveAbortDetailReason, int analogFailedType)
{

	// send analog request
	sendLiveAnalog(false, liveAbortDetailReason, analogFailedType);

	// send live abort info
	PLS_LIVE_ABORT_INFO(MODULE_PlatformService, liveAbortReason.toUtf8().constData(), "%s", liveAbortDetailReason.toUtf8().constData());

	// send platform abort info
	for (auto item : PLS_PLATFORM_ACTIVIED) {
		if (!item->getIsAllowPushStream()) {
			continue;
		}
		const char *platformName = item->getNameForChannelType();
		PLS_LOGEX(PLS_LOG_WARN, MODULE_PlatformService, {{"liveAbortService", platformName}, {"liveAbortType", liveAbortDetailReason.toUtf8().constData()}}, "%s abort living.", platformName);
	}
}

bool PLSPlatformApi::AllowsMultiTrack() const
{
	if (pls_is_dual_output_on()) {
		return false;
	}

	auto platforms = getActivePlatforms();
	if (platforms.size() == 1) {
		auto platform = platforms.front();
		if (PLSServiceType::ST_CUSTOM == platform->getServiceType()) {
			auto streamServer = platform->getStreamServerFromInitData().toLower();

			return streamServer.startsWith("srt://") || streamServer.startsWith("rist://");
		}
	}

	return false;
}

void PLSPlatformApi::showEndView(bool isRecord, bool isShowDialog)
{
	if (pls_get_app_exiting()) {
		PLS_INFO(MODULE_PlatformService, "Show end with ignored, because app is exiting.");
		return;
	}

	//User clicks stop recording button
	bool isClickToStopRecord = PLSCHANNELS_API->getIsClickToStopRecord();

	//When recording is stopped, the live broadcast is in progress
	//A recording is in progress when the stream is stopped
	bool isLivingAndRecording = false;
	if (isRecord && PLS_PLATFORM_API->isLiving()) {
		isLivingAndRecording = true;
	} else if (!isRecord && PLS_PLATFORM_API->isRecording()) {
		isLivingAndRecording = true;
	}

	//When streaming starts, recording starts
	bool recordWhenStreaming = config_get_bool(App()->GetUserConfig(), "BasicWindow", "RecordWhenStreaming");
	//when stopping live sometime later but not stop complected, then start record, the keepRecordingWhenStreamStops is true.
	bool keepRecordingWhenStreamStops = config_get_bool(App()->GetUserConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");

	//when stop streaming will stop record same time.
	bool isStreamingRecordStopAuto = recordWhenStreaming && !keepRecordingWhenStreamStops;
	bool isRehearsal = PLSCHANNELS_API->isRehearsaling();

	int liveState = PLSCHANNELS_API->currentBroadcastState();
	int recordState = PLSCHANNELS_API->currentReocrdState();
	//End the recording page, if the recording is set to end automatically and the user does not click the end recording button, the recording End page will not be displayed
	if (isStreamingRecordStopAuto && !isClickToStopRecord) {
		//living or in stopping living. (Living <= status <= LiveEnded)
		bool isLivingOrStopping = (int)LiveStatus::Living <= liveState && liveState <= (int)LiveStatus::LiveEnded;
		bool isRecordingOrStopping = (int)ChannelData::RecordStarted <= recordState && recordState <= (int)ChannelData::RecordStopped;
		if (isRecord && isLivingOrStopping) {
			m_isIgnoreNextRecordShow = true;
		} else if (!isRecord && isRecordingOrStopping) { //#4703, record enter stop after click server diconnected alert.
			m_isIgnoreNextRecordShow = true;
		}
	}

	PLS_INFO(END_MODULE, "Show end with parameter \n\
\tisLivingAndRecording:%s, \n\
\tisShowRecordEnd:%s, \n\
\tisClickToStopRecord:%s, \n\
\tisStreamingRecordStopAuto:%s, \n\
\tisIgnoreNextRecordShow:%s, \n\
\tisRehaersaling:%s, \n\
\tisShowDialog:%s, \n\
\tisRecord:%s, \n\
\tcurrentReocrdState:%d, \n\
\tcurrentBroadcastState:%d",
		 BOOL2STR(isLivingAndRecording), BOOL2STR(isRecord), BOOL2STR(isClickToStopRecord), BOOL2STR(isStreamingRecordStopAuto), BOOL2STR(m_isIgnoreNextRecordShow), BOOL2STR(isRehearsal),
		 BOOL2STR(isShowDialog), BOOL2STR(isRecord), recordState, liveState);

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
		const auto channelName = info.value(ChannelData::g_channelName).toString();
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

void PLSPlatformApi::addFailedPlatform(const std::list<PLSPlatformBase *> &listPlatform)
{
	for (auto pPlatform : listPlatform) {
		PLS_PLATFORM_PRSIM->stopLiveAndMqtt(pPlatform);
		m_listFailedPlatform.push_back(pPlatform);
	}
}

QString PLSPlatformApi::joinPlatformNames(const std::list<PLSPlatformBase *> &listPlatform)
{
	ostringstream oss;
	for (auto pPlatform : listPlatform) {
		if (!oss.str().empty()) {
			oss << ", ";
		}
		oss << pPlatform->getChannelName().toStdString();
	}

	return QString::fromStdString(oss.str());
}

bool PLSPlatformApi::isPrismLive() const
{
	if (pls_is_dual_output_on()) {
		return m_bPrismLive[DualOutputType::Horizontal] || m_bPrismLive[DualOutputType::Vertical];
	} else {
		return m_bPrismLive[DualOutputType::Horizontal];
	}
}

bool PLSPlatformApi::isPrismLive(const PLSPlatformBase *pPlatform) const
{
	if (pls_is_dual_output_on()) {
		return isPrismLive(pPlatform->isHorizontalOutput() ? DualOutputType::Horizontal : DualOutputType::Vertical);
	} else {
		return m_bPrismLive[DualOutputType::Horizontal];
	}
}

bool PLSPlatformApi::isPrismLive(DualOutputType dualOutputType) const
{
	return m_bPrismLive[dualOutputType];
}
