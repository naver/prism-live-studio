#include <Windows.h>

#include "PLSPlatformNaverShoppingLIVE.h"

#include <ctime>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "../../PLSHttpApi/PLSHmacNetworkReplyBuilder.h"
#include "../../PLSHttpApi/PLSHttpHelper.h"

#include "../../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../../channels/ChannelsDataApi/ChannelCommonFunctions.h"

#include "../common/PLSDateFormate.h"
#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"

#include "../PLSScheLiveNotice.h"
#include "prism/PLSPlatformPrism.h"
#include "log/log.h"

#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "../PLSSyncServerManager.hpp"
#include "PLSNaverShoppingTerm.h"
#include <platform.hpp>
#include <QFile>

#include "pls-app.hpp"
#include "main-view.hpp"
#include "ResolutionGuidePage.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "ui-config.h"
#include "../../PLSServerStreamHandler.hpp"
#include "../../channels/ChannelsDataApi/PLSChannelsVirualAPI.h"
#include "../../channels/ChannelsDataApi/ChannelConst.h"

static const QString IMAGE_FILE_NAME_PREFIX = "navershopping-";
static const int CHECK_STATUS_TIMESPAN = 5000;

#define USE_TERM_MESSAGE_INFO "UseTermMessageInfo"
#define USE_TERM_URL_INFO "UseTermURLInfo"
#define USE_TERM_SHOW_INFO "UseTermShowInfo"
#define USE_TERM_CLICK_CONFIRM_INFO "UseTermClickConfirmInfo"

#define PRECAUTION_MESSAGE_INFO "PrecautionMessageInfo"
#define PRECAUTION_URL_INFO "PrecautionUrlInfo"
#define PRECAUTION_SHOW_INFO "PrecautionShowInfo"

#define SUPPORT_RESOLUTION_MESSAGE_INFO "SupportReslolutionMessageInfo"
#define SUPPORT_RESOLUTION_SHOW_INFO "SupportReslolutionShowInfo"

#define CSTR_NAVER_SHOPPING_GROUP "navershoppingGroup"
#define CSTR_NAVER_SHOPPING_SERVICE_ID "serviceId"
#define CSTR_NAVER_SHOPPING_BROADCAST_ID "broadcasterId"
#define CSTR_NAVER_SHOPPING_PROFILE_IMAGE_URL "profileImageUrl"
#define CSTR_NAVER_SHOPPING_PROFILE_IMAGE_PATH "profileImagePath"
#define CSTR_NAVER_SHOPPING_NICKNAME "nickname"
#define CSTR_NAVER_SHOPPING_TOKEN "accessToken"

template<typename Callback> QTimer *startTimer(PLSPlatformNaverShoppingLIVE *platform, QTimer *&timer, int timeout, Callback callback, bool singleShot = true)
{
	deleteTimer(timer);
	timer = new QTimer(platform);
	timer->setSingleShot(singleShot);
	QObject::connect(timer, &QTimer::timeout, platform, callback);
	timer->start(timeout);
	return timer;
}

void deleteTimer(QTimer *&timer)
{
	if (timer) {
		timer->stop();
		delete timer;
		timer = nullptr;
	}
}

PLSPlatformNaverShoppingLIVE::PLSPlatformNaverShoppingLIVE()
{
	setSingleChannel(true);
	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [=](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_platformName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();
		QString token = info.value(ChannelData::g_channelToken).toString();
		if (dataType == ChannelData::ChannelType && platformName == NAVER_SHOPPING_LIVE && token.length() > 0) {
			clearLiveInfo();
			App()->clearNaverShoppingConfig();
			PLSNaverShoppingLIVEDataManager::instance()->tokenExpiredClear();
			PLSNaverShoppingLIVEAPI::logoutNaverShopping(this, token, this);
		}
	});

	loadUserInfo(m_userInfo);
	m_softwareUUid = pls_get_navershopping_deviceId();

	m_endCountdownTimer = new QTimer(this);
	m_endCountdownTimer->setInterval(10000);
	connect(m_endCountdownTimer, &QTimer::timeout, this, &PLSPlatformNaverShoppingLIVE::endTimeCountdownCalculate);
}

PLSPlatformNaverShoppingLIVE::~PLSPlatformNaverShoppingLIVE() {}

PLSServiceType PLSPlatformNaverShoppingLIVE::getServiceType() const
{
	return PLSServiceType::ST_NAVER_SHOPPING_LIVE;
}

void PLSPlatformNaverShoppingLIVE::onPrepareLive(bool value)
{
	if (!value) {
		PLSPlatformBase::onPrepareLive(value);
		return;
	}

	if (!checkSupportResolution(true)) {
		PLSPlatformBase::onPrepareLive(false);
		return;
	}
	PLS_INFO(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "%s %s show liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	value = pls_exec_live_Info_naver_shopping_live(this) == QDialog::Accepted;
	PLS_INFO(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "%s %s liveinfo closed value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	PLSPlatformBase::onPrepareLive(value);

	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	if (isRehearsal()) {
		latInfo.remove(ChannelData::g_viewers);
		latInfo.remove(ChannelData::g_likes);
	} else {
		latInfo.insert(ChannelData::g_viewers, "0");
		latInfo.insert(ChannelData::g_likes, "0");
	}
	PLSCHANNELS_API->setChannelInfos(latInfo, true);

	// notify golive button change status
	if (value && isRehearsal()) {
		PLSCHANNELS_API->rehearsalBegin();
	}
}

void PLSPlatformNaverShoppingLIVE::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}
	__super::setIsScheduleLive(!m_prepareLiveInfo.isNowLiving);

	m_endTimeData = PLSNShoppingLIVEEndTime();
	if (m_endCountdownTimer) {
		m_endCountdownTimer->start();
	}

	if (isRehearsal()) {
		setShareLink(m_prepareLiveInfo.broadcastEndUrl);
		return;
	}
	/*auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	latInfo.remove(ChannelData::g_displayLine2);
	PLSCHANNELS_API->setChannelInfos(latInfo, true);*/
	setShareLink(m_prepareLiveInfo.broadcastEndUrl);
	::startTimer(this, checkStatusTimer, CHECK_STATUS_TIMESPAN, &PLSPlatformNaverShoppingLIVE::onCheckStatus, false);
}

void PLSPlatformNaverShoppingLIVE::onAllPrepareLive(bool value)
{
	if (!value && m_callCreateLiveSuccess) {
		m_callCreateLiveSuccess = false;
		auto requestCallback = [=](bool) {

		};
		PLSNaverShoppingLIVEAPI::stopLiving(this, true, requestCallback, this, [](QObject *receiver) -> bool { return receiver != nullptr; });
	}
}

void PLSPlatformNaverShoppingLIVE::onLiveEnded()
{
	deleteTimer(checkStatusTimer);
	PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());
	m_prepareLiveInfo.broadcastEndUrl.clear();
	if (m_endCountdownTimer && m_endCountdownTimer->isActive()) {
		m_endCountdownTimer->stop();
	}
	auto requestCallback = [=](bool) {
		liveEndedCallback();
		if (!isRehearsal()) {
			clearLiveInfo();
		}
	};
	m_callCreateLiveSuccess = false;
	PLSNaverShoppingLIVEAPI::stopLiving(this, true, requestCallback, this, [](QObject *receiver) -> bool { return receiver != nullptr; });
}

void PLSPlatformNaverShoppingLIVE::onActive()
{
	PLSPlatformBase::onActive();

	if (m_naverShoppingTermAndNotesChecked) {
		QMetaObject::invokeMethod(this, &PLSPlatformNaverShoppingLIVE::onShowScheLiveNotice, Qt::QueuedConnection);
	}
}

QString PLSPlatformNaverShoppingLIVE::getShareUrl()
{
	return QString();
}

QJsonObject PLSPlatformNaverShoppingLIVE::getWebChatParams()
{
	QJsonObject object;
	object["X-LIVE-COMMERCE-AUTH"] = getAccessToken();
	object["broadcastId"] = m_livingInfo.id;
	object["X-LIVE-COMMERCE-DEVICE-ID"] = getSoftwareUUid();
	object["name"] = "navershopping";
	object["host"] = PRISM_SSL;
	object["isRehearsal"] = isRehearsal();
	object["hmac"] = PLS_PC_HMAC_KEY;
	bool isIIMS = m_livingInfo.broadcastType == "PLANNING";
	object["isIIMS"] = isIIMS;
	return object;
}

bool PLSPlatformNaverShoppingLIVE::isSendChatToMqtt() const
{
	return true;
}

QJsonObject PLSPlatformNaverShoppingLIVE::getLiveStartParams()
{
	QJsonObject platform = PLSPlatformBase::getLiveStartParams();
	platform["liveId"] = m_livingInfo.id;
	platform["authToken"] = m_userInfo.accessToken;
	platform["deviceId"] = "PRISMLiveStudio";
	platform["isRehearsal"] = (m_prepareLiveInfo.releaseLevel == "TEST");
	platform["scheduled"] = !m_prepareLiveInfo.isNowLiving;
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	return platform;
}

void PLSPlatformNaverShoppingLIVE::onInitDataChanged()
{
	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE init data changed");

	auto platforms = PLS_PLATFORM_API->getPlatformNaverShoppingLIVE();
	auto iter = std::find_if(platforms.begin(), platforms.end(), [](PLSPlatformNaverShoppingLIVE *platform) { return platform->isPrimary(); });
	if (iter == platforms.end()) {
		return;
	}

	auto primary = *iter;
	if (this != primary) {
		PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE synchronize user info");
	}
}

QString PLSPlatformNaverShoppingLIVE::getServiceLiveLink()
{
	return m_prepareLiveInfo.broadcastEndUrl;
}

void PLSPlatformNaverShoppingLIVE::getUserInfo(const QString channelName, const QVariantMap srcInfo, UpdateCallback finishedCall, bool isClearLiveInfo)
{
	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE get userInfo");

	for (auto *platform : PLS_PLATFORM_API->getPlatformNaverShoppingLIVE()) {
		platform->primary = false;
	}

	if (isClearLiveInfo) {
		for (auto platform : PLS_PLATFORM_API->getPlatformNaverShoppingLIVE()) {
			platform->clearLiveInfo();
		}
	}

	primary = true;

	auto requestCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo) {
		QVariantMap info = srcInfo;
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			setUserInfo(userInfo);
			info[ChannelData::g_platformName] = channelName;
			info[ChannelData::g_channelToken] = userInfo.accessToken;
			info[ChannelData::g_nickName] = userInfo.nickname;
			if (!userInfo.profileImagePath.isEmpty()) {
				info[ChannelData::g_userIconCachePath] = userInfo.profileImagePath;
			}
			info[ChannelData::g_shareUrl] = QString();
			info[ChannelData::g_viewers] = "0";
			info[ChannelData::g_viewersPix] = ChannelData::g_defaultViewerIcon;
			info[ChannelData::g_likes] = "0";
			info[ChannelData::g_likesPix] = ChannelData::g_naverTvLikeIcon;
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
			QMetaObject::invokeMethod(this, &PLSPlatformNaverShoppingLIVE::loginFinishedPopupAlert, Qt::QueuedConnection);
		} else if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingInvalidAccessToken) {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Expired;
		} else if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingNetworkError) {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::NetWorkNoStable;
		} else if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingNoLiveRight) {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorString] = QTStr("navershopping.no.live.right");
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::SpecializedError;
		} else if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingLoginFailed) {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorString] = QTStr("navershopping.login.fail");
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::SpecializedError;
		} else {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::UnknownError;
		}
		finishedCall({info});
	};
	PLSNaverShoppingLIVEAPI::refreshChannelToken(this, requestCallback, this, [](QObject *receiver) -> bool { return receiver != nullptr; });
}

void PLSPlatformNaverShoppingLIVE::loadUserInfo(PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo)
{
	auto config = App()->CookieConfig();
	userInfo.serviceId = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_SERVICE_ID));
	userInfo.broadcasterId = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_BROADCAST_ID));
	userInfo.profileImageUrl = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_URL));
	userInfo.profileImagePath = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_PATH));
	userInfo.nickname = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_NICKNAME));
	userInfo.accessToken = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_TOKEN));
}
void PLSPlatformNaverShoppingLIVE::saveUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo)
{
	auto config = App()->CookieConfig();
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_SERVICE_ID, userInfo.serviceId.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_BROADCAST_ID, userInfo.broadcasterId.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_URL, userInfo.profileImageUrl.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_PATH, userInfo.profileImagePath.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_NICKNAME, userInfo.nickname.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_TOKEN, userInfo.accessToken.toUtf8().constData());
	config_save(config);
}

void PLSPlatformNaverShoppingLIVE::showScheLiveNotice(const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo)
{
	if (!isActive() || isScheLiveNoticeShown) {
		return;
	}

	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE show schedule live notice");

	isScheLiveNoticeShown = true;

	PLSScheLiveNotice scheLiveNotice(this, scheduleInfo.title, scheduleInfo.startTimeUTC, App()->getMainView());
	scheLiveNotice.exec();
}

bool PLSPlatformNaverShoppingLIVE::isPrimary() const
{
	return primary;
}
QString PLSPlatformNaverShoppingLIVE::getSubChannelId() const
{
	return m_mapInitData[ChannelData::g_subChannelId].toString();
}
QString PLSPlatformNaverShoppingLIVE::getSubChannelName() const
{
	return m_mapInitData[ChannelData::g_nickName].toString();
}

bool PLSPlatformNaverShoppingLIVE::isRehearsal() const
{
	return PLSCHANNELS_API->isLiving() ? (m_livingInfo.releaseLevel == "TEST") : (m_prepareLiveInfo.releaseLevel == "TEST");
}

bool PLSPlatformNaverShoppingLIVE::isPlanningLive() const
{
	return m_livingInfo.broadcastType == "PLANNING" && m_livingInfo.releaseLevel == "REAL";
}

NaverShoppingAccountType PLSPlatformNaverShoppingLIVE::getAccountType()
{
	if (m_userInfo.serviceId == "SELECTIVE") {
		return NaverShoppingAccountType::NaverShoppingSelective;
	}
	return NaverShoppingAccountType::NaverShoppingSmartStore;
}

void PLSPlatformNaverShoppingLIVE::clearLiveInfo()
{
	m_livingInfo = PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo();
	m_prepareLiveInfo = PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo();
	PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_displayLine2, QString());
	PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());
}

QString PLSPlatformNaverShoppingLIVE::getAccessToken() const
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto accessToken = channelInfo[ChannelData::g_channelToken].toString();
	return accessToken;
}

void PLSPlatformNaverShoppingLIVE::setAccessToken(const QString &accessToken)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	channelInfo.insert(ChannelData::g_channelToken, accessToken);
	PLSCHANNELS_API->setChannelInfos(channelInfo, true);

	if (PLS_PLATFORM_API->isLiving()) {
		PLS_PLATFORM_API->sendWebPrismInit();
	}
}

const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &PLSPlatformNaverShoppingLIVE::getUserInfo()
{
	return m_userInfo;
}

const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &PLSPlatformNaverShoppingLIVE::getLivingInfo()
{
	return m_livingInfo;
}

const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &PLSPlatformNaverShoppingLIVE::getPrepareLiveInfo()
{
	return m_prepareLiveInfo;
}

const PLSNaverShoppingLIVEAPI::ScheduleInfo PLSPlatformNaverShoppingLIVE::getSelectedScheduleInfo(const QString &item_id)
{
	for (PLSNaverShoppingLIVEAPI::ScheduleInfo &info : m_scheduleList) {
		if (info.id == item_id) {
			return info;
		}
	}
	return PLSNaverShoppingLIVEAPI::ScheduleInfo();
}

const QList<PLSNaverShoppingLIVEAPI::LiveCategory> &PLSPlatformNaverShoppingLIVE::getCategoryList()
{
	return m_categoryList;
}

const QStringList &PLSPlatformNaverShoppingLIVE::getFirstCategoryTitleList()
{
	return m_firstCategoryTitleList;
}

const QStringList PLSPlatformNaverShoppingLIVE::getSecondCategoryTitleList(const QString &title)
{
	QStringList list;
	PLSNaverShoppingLIVEAPI::LiveCategory firstCategory = getFirstLiveCategory(title);
	for (PLSNaverShoppingLIVEAPI::LiveCategory category : firstCategory.children) {
		list.append(category.displayName);
	}
	return list;
}

const QString PLSPlatformNaverShoppingLIVE::getFirstCategoryTitle(const QString &id)
{
	for (PLSNaverShoppingLIVEAPI::LiveCategory firstCategory : m_categoryList) {
		if (firstCategory.id == id) {
			return firstCategory.displayName;
		}
	}
	return QString();
}

const PLSNaverShoppingLIVEAPI::LiveCategory PLSPlatformNaverShoppingLIVE::getFirstLiveCategory(const QString &title)
{
	for (PLSNaverShoppingLIVEAPI::LiveCategory firstCategory : m_categoryList) {
		if (firstCategory.displayName == title) {
			return firstCategory;
		}
	}
	return PLSNaverShoppingLIVEAPI::LiveCategory();
}

void PLSPlatformNaverShoppingLIVE::setUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo)
{
	m_userInfo = userInfo;
	saveUserInfo(m_userInfo);
}

void PLSPlatformNaverShoppingLIVE::setLivingInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)
{
	m_livingInfo = livingInfo;
}

void PLSPlatformNaverShoppingLIVE::setPrepareInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo)
{
	m_prepareLiveInfo = prepareInfo;
	if (prepareInfo.firstCategoryName.length() > 0) {
		QString category = prepareInfo.firstCategoryName;
		if (prepareInfo.secondCategoryName.length() > 0) {
			category = prepareInfo.firstCategoryName + QString(3, QChar(0x00B7)) + prepareInfo.secondCategoryName;
		}
		PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_displayLine2, category);
	}
}

void PLSPlatformNaverShoppingLIVE::setStreamUrlAndStreamKey()
{
	QString pushUrl = m_livingInfo.publishUrl;
	QStringList list = pushUrl.split("/");
	QString streamKey = list.last();
	list.removeLast();
	QString streamUrl = list.join("/");
	setStreamKey(streamKey.toStdString());
	setStreamServer(streamUrl.toStdString());
}

void PLSPlatformNaverShoppingLIVE::createLiving(std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> callback, QObject *receiver,
						PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid)
{
	m_callCreateLiveSuccess = false;
	__super::setTitle(m_prepareLiveInfo.title.toStdString());
	auto requestCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			m_callCreateLiveSuccess = true;
			setLivingInfo(livingInfo);
			setStreamUrlAndStreamKey();
			if (m_prepareLiveInfo.isNowLiving) {
				auto sharelinkCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &getLivingInfo) {
					if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
						m_prepareLiveInfo.broadcastEndUrl = getLivingInfo.broadcastEndUrl;
					}
					callback(apiType, m_livingInfo);
				};
				PLSNaverShoppingLIVEAPI::getLivingInfo(this, false, sharelinkCallback, receiver, receiverIsValid);
			} else {
				callback(apiType, m_livingInfo);
			}
			return;
		}
		callback(apiType, m_livingInfo);
	};
	if (m_prepareLiveInfo.isNowLiving) {
		QJsonObject body;
		body.insert("title", m_prepareLiveInfo.title);
		PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo userInfo = getUserInfo();
		body.insert("broadcasterId", userInfo.broadcasterId);
		body.insert("releaseLevel", m_prepareLiveInfo.releaseLevel);
		body.insert("serviceId", userInfo.serviceId);
		body.insert("standByImage", m_prepareLiveInfo.standByImageURL);
		body.insert("description", m_prepareLiveInfo.description);
		PLSNaverShoppingLIVEAPI::LiveCategory liveCategory = getFirstLiveCategory(m_prepareLiveInfo.firstCategoryName);
		if (liveCategory.children.count() > 0) {
			for (PLSNaverShoppingLIVEAPI::LiveCategory category : liveCategory.children) {
				if (category.displayName == m_prepareLiveInfo.secondCategoryName) {
					body.insert("displayCategoryId", category.id);
					break;
				}
			}
		} else {
			body.insert("displayCategoryId", liveCategory.id);
		}
		QJsonArray jsonArray;
		for (const auto &product : m_prepareLiveInfo.shoppingProducts) {
			QJsonObject jsonObject;
			jsonObject.insert("key", product.key);
			jsonObject.insert("represent", product.represent);
			jsonArray.append(jsonObject);
		}
		body.insert("shoppingProducts", jsonArray);
		PLSNaverShoppingLIVEAPI::createNowLiving(this, body, requestCallback, receiver, receiverIsValid);
	} else {
		PLSNaverShoppingLIVEAPI::createScheduleLiving(this, m_prepareLiveInfo.scheduleId, requestCallback, receiver, receiverIsValid);
	}
}

void PLSPlatformNaverShoppingLIVE::getLivingInfo(bool livePolling, std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> callback,
						 QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid)
{
	auto sharelinkCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			auto imageCallback = [=](bool ok, const QString &imagePath) {
				if (!ok) {
					callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo());
					return;
				}
				m_prepareLiveInfo.standByImagePath = imagePath;
				m_prepareLiveInfo.title = livingInfo.title;
				m_prepareLiveInfo.standByImageURL = livingInfo.standByImage;
				m_prepareLiveInfo.shoppingProducts = livingInfo.shoppingProducts;
				callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, livingInfo);
			};
			PLSNaverShoppingLIVEDataManager::instance()->downloadImage(this, livingInfo.standByImage, imageCallback, receiver, receiverIsValid);
			return;
		}
		callback(apiType, livingInfo);
	};
	PLSNaverShoppingLIVEAPI::getLivingInfo(this, livePolling, sharelinkCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::getCategoryList(PLSNaverShoppingLIVEAPI::GetCategoryListCallback callback, QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid)
{
	auto requestCallback = [=](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::LiveCategory> &categoryList) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			m_categoryList = categoryList;
			m_firstCategoryTitleList.clear();
			for (PLSNaverShoppingLIVEAPI::LiveCategory category : m_categoryList) {
				m_firstCategoryTitleList.append(category.displayName);
			}
		}
		callback(apiType, categoryList);
	};
	PLSNaverShoppingLIVEAPI::getCategoryList(this, requestCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::updateLiving(std::function<void(PLSAPINaverShoppingType apiType)> callback, const QString &id, QObject *receiver,
						PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid)
{
	QJsonObject body;
	body.insert("title", m_prepareLiveInfo.title);
	PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo userInfo = getUserInfo();
	body.insert("broadcasterId", userInfo.broadcasterId);
	body.insert("releaseLevel", PLSCHANNELS_API->isLiving() ? m_livingInfo.releaseLevel : m_prepareLiveInfo.releaseLevel);
	body.insert("serviceId", userInfo.serviceId);
	body.insert("standByImage", m_prepareLiveInfo.standByImageURL);
	QJsonArray jsonArray;
	for (const auto &product : m_prepareLiveInfo.shoppingProducts) {
		QJsonObject jsonObject;
		jsonObject.insert("key", product.key);
		jsonObject.insert("represent", product.represent);
		jsonArray.append(jsonObject);
	}
	body.insert("shoppingProducts", jsonArray);
	body.insert("description", m_prepareLiveInfo.description);
	PLSNaverShoppingLIVEAPI::LiveCategory liveCategory = getFirstLiveCategory(m_prepareLiveInfo.firstCategoryName);
	if (liveCategory.children.count() > 0) {
		for (PLSNaverShoppingLIVEAPI::LiveCategory category : liveCategory.children) {
			if (category.displayName == m_prepareLiveInfo.secondCategoryName) {
				body.insert("displayCategoryId", category.id);
				break;
			}
		}
	} else {
		body.insert("displayCategoryId", liveCategory.id);
	}
	if (!m_prepareLiveInfo.isNowLiving) {
		int hour = (m_prepareLiveInfo.ap == 1) ? (m_prepareLiveInfo.hour.toInt() + 12) : m_prepareLiveInfo.hour.toInt();
		QTime time(hour, m_prepareLiveInfo.minute.toInt());
		QDateTime dateTime(m_prepareLiveInfo.ymdDate, time);
		int koreanLocal = 9;
		uint koreanTimestamp = dateTime.toTime_t() - QDateTime::currentDateTime().offsetFromUtc() + koreanLocal * 3600;
		QDateTime koreanDateTime = QDateTime::fromSecsSinceEpoch(koreanTimestamp);
		koreanDateTime.setTimeSpec(Qt::UTC);
		QString expectedStartDate = koreanDateTime.toString("yyyy-MM-dd'T'HH:mm:ss.zzz");
		body.insert("expectedStartDate", expectedStartDate);
	}
	auto requestCallback = [=](PLSAPINaverShoppingType apiType) { callback(apiType); };
	PLSNaverShoppingLIVEAPI::updateNowLiving(this, id, body, requestCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::getScheduleList(PLSNaverShoppingLIVEAPI::GetScheduleListCallback callback, QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid)
{
	auto RequestCallback = [=](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList) {
		m_scheduleList = scheduleList;
		if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				apiType = PLSAPINaverShoppingType::PLSNaverShoppingScheduleListFailed;
				handleCommonApiType(apiType);
			}
			callback(apiType, m_scheduleList);
			return;
		}
		QList<QString> urls;
		for (auto &liveInfo : m_scheduleList) {
			if (!liveInfo.standByImage.isEmpty() && liveInfo.standByImage != "noImage") {
				urls.append(liveInfo.standByImage);
			}
		}
		auto imageCallback = [=](const QMap<QString, QString> &imagePaths) {
			for (int i = 0; i < m_scheduleList.count(); ++i) {
				m_scheduleList[i].standByImagePath = imagePaths.value(m_scheduleList[i].standByImage);
			}
			callback(apiType, m_scheduleList);
		};
		PLSNaverShoppingLIVEDataManager::instance()->downloadImages(this, urls, imageCallback, receiver, receiverIsValid);
	};
	PLSNaverShoppingLIVEAPI::getScheduleList(this, RequestCallback, receiver, receiverIsValid);
}

bool PLSPlatformNaverShoppingLIVE::isClickConfirmUseTerm()
{
	bool confirmed = config_get_bool(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_CLICK_CONFIRM_INFO);
	return confirmed;
}

void PLSPlatformNaverShoppingLIVE::loginFinishedPopupAlert()
{
	if (checkNaverShoppingTermOfAgree(false)) {
		checkNaverShoopingNotes(false);
		checkSupportResolution();
	} else {
		checkSupportResolution();
	}
	config_set_bool(App()->NaverShoppingConfig(), SUPPORT_RESOLUTION_MESSAGE_INFO, SUPPORT_RESOLUTION_SHOW_INFO, true);
	config_save(App()->NaverShoppingConfig());

	if (!m_naverShoppingTermAndNotesChecked) {
		m_naverShoppingTermAndNotesChecked = true;
		QMetaObject::invokeMethod(this, &PLSPlatformNaverShoppingLIVE::onShowScheLiveNotice, Qt::QueuedConnection);
	}
}

bool PLSPlatformNaverShoppingLIVE::checkNaverShoppingTermOfAgree(bool isGolive)
{
	//get term of agree url
	QMap<QString, QString> map = PLSGpopData::instance()->getNaverShoppingTermOfUse();
	QString verstr = QString("v%1.%2.%3").arg(PLS_RELEASE_CANDIDATE_MAJOR).arg(PLS_RELEASE_CANDIDATE_MINOR).arg(PLS_RELEASE_CANDIDATE_PATCH);
	QString newUrl = map.value(verstr);
	if (newUrl.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "NaverShopping Term of Agree URL is Empty");
		return false;
	}

	//read cache version info
	bool needPopup = true;
	bool show = config_get_bool(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_SHOW_INFO);
	if (isGolive) {
		const char *oldUrl = config_get_string(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_URL_INFO);
		//when click GoLive button,ok button not click, then need present this widget
		if (!isClickConfirmUseTerm()) {
			needPopup = true;
		} else if (oldUrl && strlen(oldUrl) > 0 && (QString::compare(oldUrl, newUrl.toUtf8().constData()) == 0)) {
			//when use term url is changed
			needPopup = false;
		}
	} else if (show) {
		needPopup = false;
	}

	//decide is show popup view
	if (needPopup) {
		config_remove_value(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_CLICK_CONFIRM_INFO);
		config_set_bool(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_SHOW_INFO, true);
		PLSNaverShoppingTerm term;
		term.setURL(newUrl);
		term.setMoreUrl("https://shoppinglive.naver.com/term");
		term.setMoreButtonHidden(false);
		term.setOKButtonTitle(tr("navershopping.use.term.agree.title"));
		term.setMoreLabelTitle(tr("navershopping.use.term.more.label"));
		bool isOk = (term.exec() == QDialog::Accepted);
		config_set_string(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_URL_INFO, newUrl.toUtf8().constData());
		if (isOk) {
			config_set_bool(App()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_CLICK_CONFIRM_INFO, true);
		}
		config_save(App()->NaverShoppingConfig());
	}
	return isClickConfirmUseTerm();
}

void PLSPlatformNaverShoppingLIVE::checkNaverShoopingNotes(bool isGolive)
{
	//get notes url
	QMap<QString, QString> map = PLSGpopData::instance()->getNaverShoppingNotes();
	QString ver = QString("v%1.%2.%3").arg(PLS_RELEASE_CANDIDATE_MAJOR).arg(PLS_RELEASE_CANDIDATE_MINOR).arg(PLS_RELEASE_CANDIDATE_PATCH);
	QString newUrl = map.value(ver);
	if (newUrl.isEmpty()) {
		return;
	}

	//read cache version info
	bool needPopup = true;
	bool show = config_get_bool(App()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_SHOW_INFO);
	if (isGolive) {
		const char *oldUrl = config_get_string(App()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_URL_INFO);
		if (oldUrl && strlen(oldUrl) > 0 && (QString::compare(oldUrl, newUrl.toUtf8().constData()) == 0)) {
			needPopup = false;
		}
	} else if (show) {
		needPopup = false;
	}

	//decide is show popup view
	if (needPopup) {
		config_set_bool(App()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_SHOW_INFO, true);
		PLSNaverShoppingTerm term;
		term.setURL(newUrl);
		term.setMoreButtonHidden(true);
		term.setCancelButtonHidden(true);
		term.setOKButtonTitle(tr("OK"));
		term.setMoreLabelTitle(tr("navershopping.note.more.label"));
		term.exec();
		config_set_string(App()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_URL_INFO, newUrl.toUtf8().constData());
		config_save(App()->NaverShoppingConfig());
	}
}

bool PLSPlatformNaverShoppingLIVE::checkSupportResolution(bool isGolive)
{
	bool needPopup = isPopupResolution(isGolive);
	if (!needPopup) {
		return true;
	}
	QString tipString;
	if (!isPortraitSupportResolution()) {
		QString resulution = ResolutionGuidePage::getPreferResolutionStringOfPlatform(NAVER_SHOPPING_LIVE);
		QString message = QTStr("navershopping.no.support.horizontal.mode.tip").arg(resulution);
		config_save(App()->NaverShoppingConfig());
		return showResolutionAlertView(message);
	} else if (!PLSServerStreamHandler::instance()->isSupportedResolutionFPS(tipString)) {
		QString resulution = ResolutionGuidePage::getPreferResolutionStringOfPlatform(NAVER_SHOPPING_LIVE);
		QString message = QTStr("navershopping.no.support.resolution.mode.tip").arg(resulution);
		config_save(App()->NaverShoppingConfig());
		return showResolutionAlertView(message);
	}
	return true;
}

bool PLSPlatformNaverShoppingLIVE::isPopupResolution(bool isGolive)
{
	QString tipString;
	bool needPopup = false;
	if (!isPortraitSupportResolution()) {
		needPopup = true;
	} else if (!PLSServerStreamHandler::instance()->isSupportedResolutionFPS(tipString)) {
		needPopup = true;
	}
	if (!isGolive) {
		bool show = config_get_bool(App()->NaverShoppingConfig(), SUPPORT_RESOLUTION_MESSAGE_INFO, SUPPORT_RESOLUTION_SHOW_INFO);
		if (show) {
			needPopup = false;
		}
	}
	return needPopup;
}

bool PLSPlatformNaverShoppingLIVE::isPortraitSupportResolution()
{
	PLSBasic *main = qobject_cast<PLSBasic *>(App()->GetMainWindow());
	uint64_t out_cx = 0, out_cy = 0;
	out_cx = config_get_uint(main->Config(), "Video", "OutputCX");
	out_cy = config_get_uint(main->Config(), "Video", "OutputCY");
	if (out_cx > out_cy) {
		return false;
	}
	return true;
}

bool PLSPlatformNaverShoppingLIVE::showResolutionAlertView(const QString &message)
{
	QMap<PLSAlertView::Button, QString> buttons;
	buttons.insert(PLSAlertView::Button::Ok, QTStr("navershopping.recommend.resolution.text"));
	buttons.insert(PLSAlertView::Button::No, QTStr("Cancel"));
	if (PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), message, buttons, PLSAlertView::Button::Ok) == PLSAlertView::Button::Ok) {
		ResolutionGuidePage::setUsingPlatformPreferResolution(NAVER_SHOPPING_LIVE);
		return true;
	}
	return false;
}

void PLSPlatformNaverShoppingLIVE::endTimeCountdownCalculate()
{

#ifdef DEBUG
	const static int s_timeAll = 12 * 60 - 1;
#else
	const static int s_timeAll = 120 * 60 - 1;
#endif
	const static int s_tiemLeft_1 = s_timeAll - 1 * 60;
	const static int s_tiemLeft_5 = s_timeAll - 5 * 60;
	const static int s_tiemLeft_10 = s_timeAll - 10 * 60;

	auto nowTime = std::chrono::steady_clock::now();
	auto liveTimes = std::chrono::duration_cast<std::chrono::seconds>(nowTime - m_endTimeData.startTime).count();

	if (liveTimes >= s_timeAll && !m_endTimeData.isMinShown_0) {
		m_endCountdownTimer->stop();
		m_endTimeData.isMinShown_0 = true;
		if (PLS_PLATFORM_API->isLiving()) {
			PLS_LIVE_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "FinishedBy navershopping because reach max live time");
			PLSCHANNELS_API->toStopBroadcast();
		}
		PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("navershopping.live.time.end.now"));
	} else if (liveTimes >= s_tiemLeft_10 && !m_endTimeData.isMinShown_10) {
		m_endTimeData.isMinShown_10 = true;
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("navershopping.live.time.end.soon").arg(10));
	} else if (liveTimes >= s_tiemLeft_5 && !m_endTimeData.isMinShown_5) {
		m_endTimeData.isMinShown_5 = true;
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("navershopping.live.time.end.soon").arg(5));
	} else if (liveTimes >= s_tiemLeft_1 && !m_endTimeData.isMinShown_1) {
		m_endTimeData.isMinShown_1 = true;
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("navershopping.live.time.end.soon").arg(1));
	}
}

QString PLSPlatformNaverShoppingLIVE::getOutputResolution()
{
	PLSBasic *main = qobject_cast<PLSBasic *>(App()->GetMainWindow());
	return QString("%1x%2").arg(config_get_uint(main->Config(), "Video", "OutputCX")).arg(config_get_uint(main->Config(), "Video", "OutputCY"));
}

QList<QPair<bool, QString>> PLSPlatformNaverShoppingLIVE::getChannelImagesSync(const QList<QString> &urls)
{
	if (urls.isEmpty()) {
		return QList<QPair<bool, QString>>();
	}
	QList<QNetworkReply *> replyList;
	for (auto &url : urls) {
		replyList.append(PLSNetworkReplyBuilder(url).get());
	}
	return PLSHttpHelper::downloadImagesSync(replyList, this, getTmpCacheDir(), IMAGE_FILE_NAME_PREFIX);
}

void PLSPlatformNaverShoppingLIVE::handleCommonApiType(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap)
{

	switch (apiType) {
	case PLSAPINaverShoppingType::PLSNaverShoppingInvalidAccessToken: {
		if (!PLSCHANNELS_API->isLiving()) {
			emit showLiveinfoLoading();
			PLSAlertView::Button button = PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.account.expired"));
			emit hiddenLiveinfoLoading();
			emit closeDialogByExpired();
			if (button == PLSAlertView::Button::Ok) {
				PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
			}
		} else {
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingNetworkError: {
		if (!networkErrorPopupShowing) {
			networkErrorPopupShowing = true;
			if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
				addErrorForType(ChannelData::NetWorkErrorType::NetWorkNoStable);
				showNetworkErrorAlert();
			}
			networkErrorPopupShowing = false;
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingUpdateFailed: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.update.failed"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingUpdateScheduleInfoFailed: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.update.schedule.fail.tip"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingScheduleTimeNotReached: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.reservation.not.reached"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingScheduleIsLived: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.reservation.is.lived"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingScheduleDelete: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("broadcast.invalid.schedule"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingCreateLivingFailed: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.create.schedule.fail"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingScheduleListFailed: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingAgeRestrictedProduct: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.age.restrict.product"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingUploadImageFailed: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.set_photo_error"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingCreateSchduleExternalStream: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.create.schedule.external.stream"));
		}
		break;
	}
	case PLSAPINaverShoppingType::PLSNaverShoppingCategoryListFailed: {
		if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.load.categorylist.failed"));
		}
		break;
	}
	}
}

const QString &PLSPlatformNaverShoppingLIVE::getSoftwareUUid()
{
	return m_softwareUUid;
}

void PLSPlatformNaverShoppingLIVE::getCategoryName(QString &firstCategoryName, QString &secondCategoryName, const QString &displayName, QString &parentId)
{
	if (displayName.length() > 0) {
		if (parentId.length() > 0) {
			secondCategoryName = displayName;
			firstCategoryName = getFirstCategoryTitle(parentId);
		} else {
			firstCategoryName = displayName;
		}
	}
	if (getFirstCategoryTitleList().contains(firstCategoryName)) {
		QStringList secondTitleList = getSecondCategoryTitleList(firstCategoryName);
		if (!secondTitleList.contains(secondCategoryName)) {
			secondCategoryName.clear();
		}
	} else {
		firstCategoryName.clear();
		secondCategoryName.clear();
	}
}

void PLSPlatformNaverShoppingLIVE::onCheckStatus()
{
	static QString liveEndStatus = "END";
	static QString liveBlockStatus = "BLOCK";
	static QString liveAbortStatus = "ABORT";
	static QString liveCloseStatus = "CLOSE";
	auto requestCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			QString liveStatus = livingInfo.status.toUpper();
			QString displayType = livingInfo.displayType.toUpper();
			if (liveStatus == liveEndStatus || liveStatus == liveBlockStatus || liveStatus == liveAbortStatus || displayType == liveCloseStatus) {
				if (liveStatus == liveEndStatus) {
					PLS_LIVE_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "FinishedBy navershopping because live status:%s,live displayType:%s", liveStatus.toStdString().c_str(),
						      displayType.toStdString().c_str());
				} else {
					PLS_LIVE_ABORT_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "live abort because live platform stop",
							    "live abort navershopping because live status:%s,live displayType:%s", liveStatus.toStdString().c_str(), displayType.toStdString().c_str());
				}
				deleteTimer(checkStatusTimer);
				PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.delete.permission"));
				PLSCHANNELS_API->toStopBroadcast();
			}
		} else {
			PLS_LIVE_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live call living polling failed, apiType: %d", apiType);
		}
	};
	PLSNaverShoppingLIVEAPI::getLivingInfo(this, true, requestCallback, this, [](QObject *receiver) -> bool { return receiver != nullptr; });
}

void PLSPlatformNaverShoppingLIVE::onShowScheLiveNotice()
{
	if (!isActive() || isScheLiveNoticeShown) {
		return;
	}

	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE check schedule live notice");

	PLSNaverShoppingLIVEAPI::getScheduleList(
		this, 0, 15 * 60,
		[=](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleInfos) {
			if ((apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) || scheduleInfos.isEmpty()) {
				return;
			}

			qint64 current = QDateTime::currentMSecsSinceEpoch() / 1000;
			const PLSNaverShoppingLIVEAPI::ScheduleInfo *scheduleInfo = nullptr;
			for (auto &si : scheduleInfos) {
				if (si.expectedStartDate.isEmpty() || (si.timeStamp <= 0)) {
					// no start time
				} else if (!scheduleInfo) {
					scheduleInfo = &si;
				} else if (qAbs(current - si.timeStamp) < qAbs(current - scheduleInfo->timeStamp)) {
					scheduleInfo = &si;
				}
			}

			if (scheduleInfo) {
				showScheLiveNotice(*scheduleInfo);
			}
		},
		this);
}

void PLSPlatformNaverShoppingLIVE::checkPushNotification(function<void()> onNext)
{
	if (isRehearsal()) {
		onNext();
		return;
	}

	QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Yes, tr("Yes")}, {PLSAlertView::Button::No, tr("No")}};
	PLSAlertView::Button ret = PLSAlertView::warning(nullptr, QTStr("Alert.Title"), tr("navershopping.push.notification.alert.title"), buttons);

	if (ret != PLSAlertView::Button::Yes) {
		//not call api. direct start.
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.copyright.alert.title"));
		onNext();
		return;
	}

	//call api, then start.
	requestNotifyApi([=]() {
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.copyright.alert.title"));
		onNext();
	});
}

void PLSPlatformNaverShoppingLIVE::requestNotifyApi(function<void()> onNext)
{

	PLSNaverShoppingLIVEAPI::sendPushNotification(
		this, this, [=](const QJsonDocument &) { onNext(); },
		[=](PLSAPINaverShoppingType apiType) {
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingNotFound204) {
				onNext();
				return;
			}

			if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				onNext();
				return;
			}
			auto btn = PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("navershopping.liveinfo.notify.fail.alert"),
							 {{PLSAlertView::Button::Retry, tr("navershopping.liveinfo.notify.fail.retry")},
							  {PLSAlertView::Button::Ignore, tr("navershopping.liveinfo.notify.fail.directStart")}});
			if (btn == PLSAlertView::Button::Retry) {
				requestNotifyApi(onNext);
			} else {
				onNext();
			}
		},
		nullptr);
}

void PLSPlatformNaverShoppingLIVE::setShareLink(const QString &sharelink)
{
	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	latInfo.insert(ChannelData::g_shareUrl, sharelink);
	PLSCHANNELS_API->setChannelInfos(latInfo, true);
}

bool PLSPlatformNaverShoppingLIVE::getScalePixmapPath(QString &scaleImagePath, const QString &originPath)
{
	QReadLocker readLocker(&downloadImagePixmapCacheRWLock);
	//find the memory image first
	if (scaleImagePixmapCache.contains(originPath)) {
		scaleImagePath = scaleImagePixmapCache.value(originPath);
		return true;
	}
	if (scaleImagePixmapCache.values().contains(originPath)) {
		scaleImagePath = originPath;
		return true;
	}
	//find the disk image second
	QString savePath = getScaleImagePath(originPath);
	bool exists = QFile::exists(savePath);
	if (exists) {
		scaleImagePath = savePath;
		scaleImagePixmapCache.insert(originPath, savePath);
	}
	return exists;
}

void PLSPlatformNaverShoppingLIVE::setScalePixmapPath(const QString &scaleImagePath, const QString &originPath)
{
	scaleImagePixmapCache.insert(originPath, scaleImagePath);
}

void PLSPlatformNaverShoppingLIVE::addScaleImageThread(const QString &originPath)
{
	m_imagePaths.append(originPath);
}

bool PLSPlatformNaverShoppingLIVE::isAddScaleImageThread(const QString &originPath)
{
	return m_imagePaths.contains(originPath);
}

QString PLSPlatformNaverShoppingLIVE::getScaleImagePath(const QString &originPath)
{
	QFileInfo fileInfo(originPath);
	QString scaleImagePath = QString("%1/%2-scaled.%3").arg(fileInfo.absoluteDir().absolutePath()).arg(fileInfo.baseName()).arg(fileInfo.suffix());
	return scaleImagePath;
}
