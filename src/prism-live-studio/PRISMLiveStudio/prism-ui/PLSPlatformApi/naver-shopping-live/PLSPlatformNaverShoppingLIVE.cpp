#include <qglobal.h>
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#include "PLSPlatformNaverShoppingLIVE.h"

#include <ctime>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaEnum>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"

#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"

#include "../common/PLSDateFormate.h"
#include "PLSLiveInfoDialogs.h"
#include "PLSPlatformApi.h"

#include "../PLSScheLiveNotice.h"
#include "prism/PLSPlatformPrism.h"
#include "log/log.h"

#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSSyncServerManager.hpp"
#include "PLSNaverShoppingNotice.h"
#include "PLSNaverShoppingUseTerm.h"

#include <platform.hpp>
#include <QFile>
#include "ResolutionGuidePage.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "ui-config.h"
#include "PLSServerStreamHandler.hpp"
#include "PLSChannelsVirualAPI.h"
#include "pls-channel-const.h"
#include "utils-api.h"
#include "frontend-api.h"
#include "prism-version.h"
#include "PLSApp.h"
#include "pls/pls-dual-output.h"

constexpr auto liveInfoMoudule = "";
static const QString IMAGE_FILE_NAME_PREFIX = "";
static const int CHECK_STATUS_TIMESPAN = 5000;

constexpr auto USE_TERM_MESSAGE_INFO = "";
constexpr auto USE_TERM_URL_INFO = "";
constexpr auto OPERATION_POLICY_URL_INFO = "";
constexpr auto USE_TERM_SHOW_INFO = "";
constexpr auto USE_TERM_CLICK_CONFIRM_INFO = "";
constexpr auto PRECAUTION_MESSAGE_INFO = "";
constexpr auto PRECAUTION_URL_INFO = "";
constexpr auto PRECAUTION_SHOW_INFO = "";
constexpr auto SUPPORT_RESOLUTION_MESSAGE_INFO = "";
constexpr auto SUPPORT_RESOLUTION_SHOW_INFO = "";
constexpr auto CSTR_NAVER_SHOPPING_GROUP = "";
constexpr auto CSTR_NAVER_SHOPPING_SERVICE_ID = "";
constexpr auto CSTR_NAVER_SHOPPING_BROADCAST_ID = "";
constexpr auto CSTR_NAVER_SHOPPING_PROFILE_IMAGE_URL = "";
constexpr auto CSTR_NAVER_SHOPPING_PROFILE_IMAGE_PATH = "";
constexpr auto CSTR_NAVER_SHOPPING_NICKNAME = "";
constexpr auto CSTR_NAVER_SHOPPING_TOKEN = "";
constexpr auto CSTR_NAVER_SHOPPING_ACCOUNT_NO = "";

constexpr auto GET_NAVER_SHOPPING_USER_INFO_STR = "get navershopping user info";

void deleteTimer(QTimer *&timer)
{
	if (timer) {
		timer->stop();
		pls_delete(timer, nullptr);
	}
}

template<typename Callback> QTimer *startTimer(PLSPlatformNaverShoppingLIVE *platform, QTimer *&timer, int timeout, Callback callback, bool singleShot = true)
{
	deleteTimer(timer);
	timer = pls_new<QTimer>(platform);
	timer->setSingleShot(singleShot);
	QObject::connect(timer, &QTimer::timeout, platform, callback);
	timer->start(timeout);
	return timer;
}

PLSPlatformNaverShoppingLIVE::PLSPlatformNaverShoppingLIVE()
{
	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_channelName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();
		QString token = info.value(ChannelData::g_channelToken).toString();
		if (dataType == ChannelData::ChannelType && platformName == NAVER_SHOPPING_LIVE && token.length() > 0) {
			clearLiveInfo();
			PLSApp::plsApp()->clearNaverShoppingConfig();
			PLSNaverShoppingLIVEDataManager::instance()->tokenExpiredClear();
			PLSNaverShoppingLIVEAPI::logoutNaverShopping(this, token, this);
		}
	});

	loadUserInfo(m_userInfo);
	m_softwareUUid = pls_get_navershopping_deviceId();
}

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
	PLSPlatformBase::setIsScheduleLive(!m_prepareLiveInfo.isNowLiving);

	setShareLink(m_prepareLiveInfo.broadcastEndUrl);
	::startTimer(this, checkStatusTimer, CHECK_STATUS_TIMESPAN, &PLSPlatformNaverShoppingLIVE::onCheckStatus, false);
}

void PLSPlatformNaverShoppingLIVE::onAllPrepareLive(bool value)
{
	if (!value && m_callCreateLiveSuccess) {
		m_callCreateLiveSuccess = false;
		auto requestCallback = [](bool) {
			//because the stop living not need know the request api result
		};
		PLSNaverShoppingLIVEAPI::stopLiving(this, isRehearsal() ? false : true, requestCallback, this, [](const QObject *receiver) { return receiver != nullptr; });
	}
}

void PLSPlatformNaverShoppingLIVE::onPrepareFinish()
{
	if (!m_prepareLiveInfo.isNowLiving && isRehearsal() && m_prepareLiveInfo.releaseLevel == RELEASE_LEVEL_REAL && isModified(m_prepareLiveInfo, m_scheduleRehearsalprepareLiveInfo)) {
		QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Yes, tr("Yes")}, {PLSAlertView::Button::No, tr("No")}};
		if (PLSAlertView::Button ret = pls_alert_error_message(nullptr, QTStr("Alert.Title"), tr("navershopping.liveinfo.choose.schedule.changed.tip"), buttons);
		    ret != PLSAlertView::Button::Yes) {
			prepareFinishCallback();
			return;
		}
		//If the schedule live image and live before not same
		auto requestCallback = [this](PLSAPINaverShoppingType apiType, const QByteArray &data) {
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				PLSErrorHandler::ExtraData extraData;
				extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING;
				PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_CHANGED_FAILED, NAVER_SHOPPING_LIVE,
											  PLSErrCustomKey_LoadLiveInfoFailed, extraData);
			}
			prepareFinishCallback();
		};
		updateScheduleRequest(requestCallback, m_scheduleRehearsalprepareLiveInfo.scheduleId, this, [](const QObject *receiver) { return receiver != nullptr; });
		return;
	}
	prepareFinishCallback();
}

void PLSPlatformNaverShoppingLIVE::onLiveEnded()
{
	deleteTimer(checkStatusTimer);
	auto requestCallback = [this](bool) {
		liveEndedCallback();
		if (m_prepareLiveInfo.infoType == PrepareInfoType::GoLivePrepareInfo) {
			clearLiveInfo();
		} else if (m_prepareLiveInfo.infoType == PrepareInfoType::RehearsalPrepareInfo) {
			if (m_prepareLiveInfo.isNowLiving) {
				PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());
				m_prepareLiveInfo.broadcastEndUrl.clear();
			} else if (m_prepareLiveInfo.releaseLevel == RELEASE_LEVEL_REHEARSAL) {
				clearLiveInfo();
			} else if (m_prepareLiveInfo.releaseLevel == RELEASE_LEVEL_REAL) {
				m_prepareLiveInfo.broadcastEndUrl = m_scheduleRehearsalprepareLiveInfo.broadcastEndUrl;
				PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, m_prepareLiveInfo.broadcastEndUrl);
			}
		}
	};
	m_callCreateLiveSuccess = false;
	PLSNaverShoppingLIVEAPI::stopLiving(this, isRehearsal() ? false : true, requestCallback, this, [](const QObject *receiver) { return receiver != nullptr; });
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
	platform["isRehearsal"] = isRehearsal();
	platform["scheduled"] = !m_prepareLiveInfo.isNowLiving;
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	return platform;
}

QString PLSPlatformNaverShoppingLIVE::getServiceLiveLink()
{
	return m_prepareLiveInfo.broadcastEndUrl;
}

bool PLSPlatformNaverShoppingLIVE::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	if (top == PLSPlatformMqttTopic::PMS_LIVE_FINISHED_BY_PLATFORM_TOPIC) {
		toStopLive();
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_FINISHED_BY_PLATFORM, NAVER_SHOPPING_LIVE, QString());
		return false;
	}
	return true;
}

void PLSPlatformNaverShoppingLIVE::getUserInfo(const QString channelName, const QVariantMap srcInfo, const UpdateCallback &finishedCall, bool isClearLiveInfo)
{
	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE get userInfo");
	if (isClearLiveInfo) {
		clearLiveInfo();
	}
	auto requestCallback = [channelName, srcInfo, finishedCall, this](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo, const QByteArray &data,
									  int statusCode, QNetworkReply::NetworkError error) {
		QVariantMap info = getUserInfoFinished(srcInfo, apiType, userInfo, channelName, data, statusCode, error);
		finishedCall({info});
	};
	PLSNaverShoppingLIVEAPI::refreshChannelToken(this, requestCallback, this, [](const QObject *receiver) { return receiver != nullptr; });
}

QVariantMap PLSPlatformNaverShoppingLIVE::getUserInfoFinished(const QVariantMap &srcInfo, PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo,
							      const QString &channelName, const QByteArray &data, int statusCode, QNetworkReply::NetworkError error)
{
	QVariantMap info = srcInfo;
	if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
		setUserInfo(userInfo);
		info[ChannelData::g_channelName] = channelName;
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
	} else {
		PLSErrorHandler::ExtraData extraData;
		extraData.pathValueMap["logContent"] = QMetaEnum::fromType<PLSAPINaverShoppingUrlType>().valueToKey(static_cast<int>(PLSAPINaverShoppingUrlType::PLSRefreshToken));
		extraData.defaultArg = QStringList(tr("Channels.naver_shopping_live"));
		extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN;
		PLSErrorHandler::RetData retData = PLSErrorHandler::getAlertString({statusCode, error, data}, NAVER_SHOPPING_LIVE, "", extraData);
		if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired) {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Expired;
		} else {
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		}
		info[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
		info[ChannelData::g_errorString] = retData.alertMsg;
		QString errorCode, errorMsg;
		PLSNaverShoppingLIVEAPI::getErrorCodeOrErrorMessage(data, errorCode, errorMsg);
		if (errorMsg.isEmpty()) {
			info[ChannelData::g_channelSreLoginFailed] = QString("Refresh token failed, prism code: %1").arg(retData.prismName);
		} else {
			info[ChannelData::g_channelSreLoginFailed] = QString("Refresh token failed, server error msg: %1 error code: %2").arg(errorMsg).arg(errorCode.isEmpty() ? "null" : errorCode);
		}
	}

	return info;
}

void PLSPlatformNaverShoppingLIVE::loadUserInfo(PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo) const
{
	auto config = PLSApp::plsApp()->CookieConfig();
	userInfo.serviceId = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_SERVICE_ID));
	userInfo.broadcasterId = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_BROADCAST_ID));
	userInfo.profileImageUrl = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_URL));
	userInfo.profileImagePath = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_PATH));
	userInfo.nickname = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_NICKNAME));
	userInfo.accessToken = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_TOKEN));
	userInfo.storeAccountNo = QString::fromUtf8(config_get_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_ACCOUNT_NO));
}
void PLSPlatformNaverShoppingLIVE::saveUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo) const
{
	auto config = PLSApp::plsApp()->CookieConfig();
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_SERVICE_ID, userInfo.serviceId.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_BROADCAST_ID, userInfo.broadcasterId.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_URL, userInfo.profileImageUrl.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_PROFILE_IMAGE_PATH, userInfo.profileImagePath.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_NICKNAME, userInfo.nickname.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_TOKEN, userInfo.accessToken.toUtf8().constData());
	config_set_string(config, CSTR_NAVER_SHOPPING_GROUP, CSTR_NAVER_SHOPPING_ACCOUNT_NO, userInfo.storeAccountNo.toUtf8().constData());
	config_save(config);
}

void PLSPlatformNaverShoppingLIVE::showScheLiveNotice(const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo)
{
	if (!isActive() || isScheLiveNoticeShown) {
		return;
	}

	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE show schedule live notice");

	isScheLiveNoticeShown = true;

	PLSScheLiveNotice scheLiveNotice(this, scheduleInfo.title, scheduleInfo.startTimeUTC, PLSBasic::instance()->getMainView());
	scheLiveNotice.exec();
}

QString PLSPlatformNaverShoppingLIVE::getSubChannelId() const
{
	return mySharedData().m_mapInitData[ChannelData::g_subChannelId].toString();
}
QString PLSPlatformNaverShoppingLIVE::getSubChannelName() const
{
	return mySharedData().m_mapInitData[ChannelData::g_nickName].toString();
}

bool PLSPlatformNaverShoppingLIVE::isRehearsal() const
{
	return m_prepareLiveInfo.infoType == PrepareInfoType::RehearsalPrepareInfo;
}

bool PLSPlatformNaverShoppingLIVE::isPlanningLive() const
{
	return m_livingInfo.broadcastType == PLANNING_LIVING;
}

NaverShoppingAccountType PLSPlatformNaverShoppingLIVE::getAccountType() const
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

void PLSPlatformNaverShoppingLIVE::setAccessToken(const QString &accessToken) const
{
	QString shoppingChannelUUID = getChannelUUID();
	PLSCHANNELS_API->setValueOfChannel(shoppingChannelUUID, ChannelData::g_channelToken, accessToken);

	PLSCHANNELS_API->channelModified(shoppingChannelUUID);
	PLS_PLATFORM_API->onUpdateChannel(shoppingChannelUUID);

	if (PLS_PLATFORM_API->isLiving()) {
		PLS_PLATFORM_API->sendWebPrismInit();
	}
}

const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &PLSPlatformNaverShoppingLIVE::getUserInfo() const
{
	return m_userInfo;
}

const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &PLSPlatformNaverShoppingLIVE::getLivingInfo() const
{
	return m_livingInfo;
}

const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &PLSPlatformNaverShoppingLIVE::getPrepareLiveInfo() const
{
	return m_prepareLiveInfo;
}

const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &PLSPlatformNaverShoppingLIVE::getScheduleRehearsaPrepareLiveInfo() const
{
	return m_scheduleRehearsalprepareLiveInfo;
}

PLSNaverShoppingLIVEAPI::ScheduleInfo PLSPlatformNaverShoppingLIVE::getSelectedScheduleInfo(const QString &item_id) const
{
	for (auto info : m_scheduleList) {
		if (info.id == item_id) {
			return info;
		}
	}
	return PLSNaverShoppingLIVEAPI::ScheduleInfo();
}

void PLSPlatformNaverShoppingLIVE::setScheduleInfoExpectedStartDate(const QString &scheduleId, const QString &expectedStartDate)
{
	for (auto &info : m_scheduleList) {
		if (info.id != scheduleId) {
			continue;
		}
		info.expectedStartDate = expectedStartDate;
		info.setExpectedStartDate(expectedStartDate);
		return;
	}
}

bool PLSPlatformNaverShoppingLIVE::isHighResolutionSLV(const QString &scheduleId) const
{
	PLSNaverShoppingLIVEAPI::ScheduleInfo scheduleInfo = getSelectedScheduleInfo(scheduleId);
	if (scheduleInfo.id.isEmpty()) {
		return false;
	}
	if (scheduleInfo.broadcastType != PLANNING_LIVING) {
		return false;
	}
	return scheduleInfo.highQualityAvailable;
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
}

void PLSPlatformNaverShoppingLIVE::saveCurrentScheduleRehearsalPrepareInfo()
{
	m_scheduleRehearsalprepareLiveInfo = m_prepareLiveInfo;
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

void PLSPlatformNaverShoppingLIVE::createLiving(
	const std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo, const QByteArray &data)> &callback, const QObject *receiver,
	const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid)
{
	m_callCreateLiveSuccess = false;
	PLSPlatformBase::setTitle(m_prepareLiveInfo.title.toStdString());
	bool requestNowLiving = m_prepareLiveInfo.isNowLiving ||
				(m_prepareLiveInfo.infoType == PrepareInfoType::RehearsalPrepareInfo && !m_prepareLiveInfo.isNowLiving && m_prepareLiveInfo.releaseLevel == RELEASE_LEVEL_REAL);
	auto requestCallback = [callback, receiver, receiverIsValid, requestNowLiving, this](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo,
											     const QByteArray &data) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			m_callCreateLiveSuccess = true;
			setLivingInfo(livingInfo);
			setStreamUrlAndStreamKey();
			if (requestNowLiving) {
				auto sharelinkCallback = [callback, this, data](PLSAPINaverShoppingType l_apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &getLivingInfo) {
					if (l_apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
						m_prepareLiveInfo.broadcastEndUrl = getLivingInfo.broadcastEndUrl;
					}
					callback(l_apiType, m_livingInfo, data);
				};
				PLSNaverShoppingLIVEAPI::getLivingInfo(this, false, sharelinkCallback, receiver, receiverIsValid);
			} else {
				callback(apiType, m_livingInfo, data);
			}
			return;
		}
		callback(apiType, m_livingInfo, data);
	};
	if (requestNowLiving) {
		QJsonObject body;
		body.insert("title", m_prepareLiveInfo.title);
		PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo userInfo = getUserInfo();
		body.insert("broadcasterId", userInfo.broadcasterId);
		if (m_prepareLiveInfo.infoType == PrepareInfoType::RehearsalPrepareInfo) {
			body.insert("releaseLevel", RELEASE_LEVEL_REHEARSAL);
		} else {
			body.insert("releaseLevel", m_prepareLiveInfo.releaseLevel);
		}
		body.insert("serviceId", userInfo.serviceId);
		body.insert("standByImage", m_prepareLiveInfo.standByImageURL);
		body.insert("description", m_prepareLiveInfo.description);
		body.insert("externalExposeAgreementStatus", m_prepareLiveInfo.externalExposeAgreementStatus);
		body.insert("sendNotification", m_prepareLiveInfo.sendNotification);

		QJsonArray jsonArray;
		for (const auto &product : m_prepareLiveInfo.shoppingProducts) {
			QJsonObject jsonObject;
			jsonObject.insert("key", product.key);
			jsonObject.insert("represent", product.represent);
			jsonObject.insert("attachmentType", product.attachmentType);
			jsonObject.insert("attachable", product.attachable);
			jsonArray.append(jsonObject);
		}
		body.insert("shoppingProducts", jsonArray);
		body.insert("searchable", m_prepareLiveInfo.allowSearch);
		PLSNaverShoppingLIVEAPI::createNowLiving(this, body, requestCallback, receiver, receiverIsValid);
	} else {
		PLSNaverShoppingLIVEAPI::createScheduleLiving(this, m_prepareLiveInfo.scheduleId, requestCallback, receiver, receiverIsValid);
	}
}

void PLSPlatformNaverShoppingLIVE::getLivingInfo(bool livePolling,
						 const std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> &callback,
						 const QObject *receiver, const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid)
{
	auto sharelinkCallback = [callback, receiver, receiverIsValid, this](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			auto imageCallback = [this, callback, livingInfo](bool ok, const QString &imagePath) {
				if (!ok) {
					callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo());
					return;
				}
				m_prepareLiveInfo.standByImagePath = imagePath;
				m_prepareLiveInfo.title = livingInfo.title;
				m_prepareLiveInfo.standByImageURL = livingInfo.standByImage;
				m_prepareLiveInfo.description = livingInfo.description;
				m_prepareLiveInfo.allowSearch = livingInfo.searchable;
				m_prepareLiveInfo.shoppingProducts = livingInfo.shoppingProducts;
				m_prepareLiveInfo.externalExposeAgreementStatus = livingInfo.externalExposeAgreementStatus;
				m_prepareLiveInfo.sendNotification = livingInfo.sendNotification;
				setScheduleInfoExpectedStartDate(m_prepareLiveInfo.scheduleId, livingInfo.expectedStartDate);
				callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, livingInfo);
			};
			PLSNaverShoppingLIVEDataManager::instance()->downloadImage(this, livingInfo.standByImage, imageCallback, receiver, receiverIsValid);
			return;
		}
		callback(apiType, livingInfo);
	};
	PLSNaverShoppingLIVEAPI::getLivingInfo(this, livePolling, sharelinkCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::updateLivingRequest(const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &data)> &callback, const QString &liveId, const QObject *receiver,
						       const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid)
{
	QJsonObject body;
	body.insert("title", m_prepareLiveInfo.title);
	PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo userInfo = getUserInfo();
	body.insert("broadcasterId", userInfo.broadcasterId);
	body.insert("serviceId", userInfo.serviceId);
	body.insert("standByImage", m_prepareLiveInfo.standByImageURL);
	QJsonArray jsonArray;
	for (const auto &product : m_prepareLiveInfo.shoppingProducts) {
		QJsonObject jsonObject;
		jsonObject.insert("key", product.key);
		jsonObject.insert("represent", product.represent);
		jsonObject.insert("attachmentType", product.attachmentType);
		jsonObject.insert("introducing", product.introducing);
		jsonObject.insert("attachable", product.attachable);
		jsonArray.append(jsonObject);
	}
	body.insert("shoppingProducts", jsonArray);
	body.insert("description", m_prepareLiveInfo.description);
	body.insert("searchable", m_prepareLiveInfo.allowSearch);
	auto requestCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, data); };
	PLSNaverShoppingLIVEAPI::updateNowLiving(this, liveId, body, requestCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::updateScheduleRequest(const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &data)> &callback, const QString &scheduleId,
							 const QObject *receiver, const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid)
{
	QJsonObject body;
	body.insert("title", m_prepareLiveInfo.title);
	PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo userInfo = getUserInfo();
	body.insert("broadcasterId", userInfo.broadcasterId);
	body.insert("serviceId", userInfo.serviceId);
	body.insert("standByImage", m_prepareLiveInfo.standByImageURL);
	body.insert("externalExposeAgreementStatus", m_prepareLiveInfo.externalExposeAgreementStatus);
	QJsonArray jsonArray;
	for (const auto &product : m_prepareLiveInfo.shoppingProducts) {
		QJsonObject jsonObject;
		jsonObject.insert("key", product.key);
		jsonObject.insert("represent", product.represent);
		jsonObject.insert("attachmentType", product.attachmentType);
		jsonObject.insert("introducing", product.introducing);
		jsonObject.insert("attachable", product.attachable);
		jsonArray.append(jsonObject);
	}
	body.insert("shoppingProducts", jsonArray);
	body.insert("description", m_prepareLiveInfo.description);
	body.insert("searchable", m_prepareLiveInfo.allowSearch);
	if (!m_prepareLiveInfo.isNowLiving) {
		qint64 timestamp = PLSNaverShoppingLIVEAPI::getLocalTimeStamp(m_prepareLiveInfo.ymdDate, m_prepareLiveInfo.hour, m_prepareLiveInfo.minute, m_prepareLiveInfo.ap);
		int koreanLocal = 9;
		qint64 koreanTimestamp = timestamp - QDateTime::currentDateTime().offsetFromUtc() + koreanLocal * 3600;
		QDateTime koreanDateTime = QDateTime::fromSecsSinceEpoch(koreanTimestamp);
		koreanDateTime.setTimeSpec(Qt::UTC);
		QString expectedStartDate = koreanDateTime.toString("yyyy-MM-dd'T'HH:mm:ss.zzz");
		body.insert("expectedStartDate", expectedStartDate);
	}
	auto requestCallback = [callback, this, scheduleId](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo, const QByteArray &data) {
		if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			callback(apiType, data);
			return;
		}
		for (auto &info : m_scheduleList) {
			if (info.id == scheduleId) {
				info = scheduleInfo;
				break;
			}
		}
		callback(apiType, data);
	};
	PLSNaverShoppingLIVEAPI::updateScheduleInfo(this, scheduleId, body, requestCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::downloadScheduleListImage(const PLSNaverShoppingLIVEAPI::GetScheduleListCallback &callback, int page, int totalCount, const QObject *receiver,
							     const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid)
{
	QList<QString> urls;
	for (const PLSNaverShoppingLIVEAPI::ScheduleInfo &liveInfo : m_scheduleList) {
		if (!liveInfo.standByImage.isEmpty() && liveInfo.standByImage != "noImage") {
			urls.append(liveInfo.standByImage);
		}
	}
	auto imageCallback = [callback, page, totalCount, this](const QMap<QString, QString> &imagePaths) {
		for (int i = 0; i < m_scheduleList.count(); ++i) {
			m_scheduleList[i].standByImagePath = imagePaths.value(m_scheduleList[i].standByImage);
		}
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, m_scheduleList, page, totalCount, "");
	};
	PLSNaverShoppingLIVEDataManager::instance()->downloadImages(this, urls, imageCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::getScheduleList(const PLSNaverShoppingLIVEAPI::GetScheduleListCallback &callback, int currentPage, uint64_t flag, const QString &type, QObject *receiver,
						   const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid)
{
	if (currentPage == SCHEDULE_FIRST_PAGE_NUM) {
		m_duplicateFlagMap.insert(type, flag);
		m_duplicateListMap.insert(type, QList<PLSNaverShoppingLIVEAPI::ScheduleInfo>());
	}
	auto RequestCallback = [callback, currentPage, flag, type, receiver, receiverIsValid, this](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList,
												    int page, int totalCount, const QByteArray &data) {
		getSchduleListRequestSuccess(flag, apiType, callback, page, totalCount, scheduleList, currentPage, type, receiver, receiverIsValid, data);
	};
	bool isNotice = (type != LIVEINFO_GET_SCHEDULE_LIST);
	PLSNaverShoppingLIVEAPI::getScheduleList(this, currentPage, isNotice, RequestCallback, receiver, receiverIsValid);
}

void PLSPlatformNaverShoppingLIVE::getSchduleListRequestSuccess(const uint64_t &flag, PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::GetScheduleListCallback &callback, int page,
								int totalCount, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, int currentPage, const QString &type,
								QObject *receiver, const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid, const QByteArray &data)
{
	if (m_duplicateFlagMap.value(type) != flag) {
		return;
	}

	if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
		callback(apiType, QList<PLSNaverShoppingLIVEAPI::ScheduleInfo>(), page, totalCount, data);
		return;
	}

	if (page <= 0) {
		return;
	}

	m_duplicateListMap[type].append(scheduleList);
	if ((page < currentPage) || (page == currentPage && totalCount <= SCHEDULE_PER_PAGE_MAX_NUM * currentPage)) {
		if (type == LIVEINFO_GET_SCHEDULE_LIST) {
			m_scheduleList = m_duplicateListMap[type];
			downloadScheduleListImage(callback, page, totalCount, receiver, receiverIsValid);
		} else {

			callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, m_duplicateListMap[type], page, totalCount, "");
		}
		return;
	}
	getScheduleList(callback, currentPage + 1, flag, type, receiver, receiverIsValid);
}

bool PLSPlatformNaverShoppingLIVE::isClickConfirmUseTerm() const
{
	bool confirmed = config_get_bool(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_CLICK_CONFIRM_INFO);
	PLS_INFO(liveInfoMoudule, "Live Term Status: PLSPlatformNaverShoppingLIVE isClickConfirmUseTerm() value is %s", BOOL2STR(confirmed));
	return confirmed;
}

void PLSPlatformNaverShoppingLIVE::loginFinishedPopupAlert()
{
	PLS_INFO(liveInfoMoudule, "Live Term Status: loginFinishedPopupAlert() login navershopping channel success");
	if (checkNaverShoppingTermOfAgree(false)) {
		pls_modal_check_app_exiting();
		checkNaverShoopingNotes(false);
		checkSupportResolution();
		PLS_INFO(liveInfoMoudule, "Live Term Status: loginFinishedPopupAlert() checkNaverShoppingTermOfAgree() value is true");
	} else {
		pls_modal_check_app_exiting();
		checkSupportResolution();
		PLS_INFO(liveInfoMoudule, "Live Term Status: loginFinishedPopupAlert() checkNaverShoppingTermOfAgree() value is false");
	}

	//After the NaverShopping is logged in, as long as the resolution is detected, it will not be displayed next time
	config_set_bool(PLSApp::plsApp()->NaverShoppingConfig(), SUPPORT_RESOLUTION_MESSAGE_INFO, SUPPORT_RESOLUTION_SHOW_INFO, true);
	config_save(PLSApp::plsApp()->NaverShoppingConfig());
	PLS_INFO(liveInfoMoudule, "Live Term Status: loginFinishedPopupAlert() SUPPORT_RESOLUTION_SHOW_INFO value is true");

	if (!m_naverShoppingTermAndNotesChecked) {
		m_naverShoppingTermAndNotesChecked = true;
		QMetaObject::invokeMethod(this, &PLSPlatformNaverShoppingLIVE::onShowScheLiveNotice, Qt::QueuedConnection);
	}
}

bool PLSPlatformNaverShoppingLIVE::checkNaverShoppingTermOfAgree(bool isGolive) const
{
	//get term of agree url
	QString newUseTermUrl = PLSSyncServerManager::instance()->getNaverShoppingTermOfUse();
	if (newUseTermUrl.isEmpty()) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: Sync Server can not find NaverShoppingLiveServiceTermsOfUse");
		return false;
	}

	//get operation
	QString newOperationPolicyUrl = PLSSyncServerManager::instance()->getNaverShoppingOperationPolicy();
	if (newOperationPolicyUrl.isEmpty()) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: Sync Server can not find NaverShoppingOperationPolicy");
		return false;
	}

	//read cache version info
	bool needPopup = true;
	bool show = config_get_bool(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_SHOW_INFO);
	PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm of isGolive value is (%s) , show value is (%s)", BOOL2STR(isGolive), BOOL2STR(show));
	if (isGolive) {
		const char *oldUseTermUrl = config_get_string(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_URL_INFO);
		const char *oldOperationPlolicyUrl = config_get_string(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, OPERATION_POLICY_URL_INFO);
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm oldUseTermUrl is %s, oldOperationPlolicyUrl is %s", oldUseTermUrl, oldOperationPlolicyUrl);
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm newUseTermUrl is %s, newOperationPolicyUrl is %s", newUseTermUrl.toUtf8().constData(),
			 newOperationPolicyUrl.toUtf8().constData());
		//when click GoLive button,ok button not click, then need present this widget
		if (!isClickConfirmUseTerm()) {
			PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm never clicks the confirm button");
			needPopup = true;
		} else if ((oldUseTermUrl && strlen(oldUseTermUrl) > 0 && (QString::compare(oldUseTermUrl, newUseTermUrl.toUtf8().constData()) == 0)) &&
			   (oldOperationPlolicyUrl && strlen(oldOperationPlolicyUrl) > 0 && (QString::compare(oldOperationPlolicyUrl, newOperationPolicyUrl.toUtf8().constData()) == 0))) {
			//when use term url is changed
			needPopup = false;
			PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm of url are all the latest");
		}
	} else if (show) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm has shown before");
		needPopup = false;
	}

	//decide is show popup view
	if (needPopup) {
		config_remove_value(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_CLICK_CONFIRM_INFO);
		config_set_bool(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_SHOW_INFO, true);
		PLS_INFO(liveInfoMoudule, "Live Term Status: show PLSNaverShoppingUseTerm view");
		PLSNaverShoppingUseTerm term;
		term.setLoadingURL(newUseTermUrl, newOperationPolicyUrl);
		bool isOk = (term.exec() == QDialog::Accepted);
		config_set_string(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_URL_INFO, newUseTermUrl.toUtf8().constData());
		config_set_string(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, OPERATION_POLICY_URL_INFO, newOperationPolicyUrl.toUtf8().constData());
		if (isOk) {
			config_set_bool(PLSApp::plsApp()->NaverShoppingConfig(), USE_TERM_MESSAGE_INFO, USE_TERM_CLICK_CONFIRM_INFO, true);
		}
		config_save(PLSApp::plsApp()->NaverShoppingConfig());
		PLS_INFO(liveInfoMoudule, "Live Term Status: exec PLSNaverShoppingUseTerm view, needPopup value is (%s), isOk value is (%s)", BOOL2STR(needPopup), BOOL2STR(isOk));
	}

	//return to the click state of the confirm button of PLSNaverShoppingUseTerm
	bool isClickConfirmButton = isClickConfirmUseTerm();
	PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingUseTerm needPopup value is (%s), isClickConfirmButton value is (%s)", BOOL2STR(needPopup), BOOL2STR(isClickConfirmButton));
	return isClickConfirmButton;
}

void PLSPlatformNaverShoppingLIVE::checkNaverShoopingNotes(bool isGolive) const
{
	//get notes url
	QString newUrl = PLSSyncServerManager::instance()->getNaverShoppingNotes();
	if (newUrl.isEmpty()) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingNotice of newUrl is Empty");
		return;
	}

	//read cache version info
	bool needPopup = true;
	bool show = config_get_bool(PLSApp::plsApp()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_SHOW_INFO);
	PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingNotice of isGolive value is (%s) , show value is (%s)", BOOL2STR(isGolive), BOOL2STR(show));
	if (isGolive) {
		const char *oldUrl = config_get_string(PLSApp::plsApp()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_URL_INFO);
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingNotice oldUrl is %s , newUrl is %s", oldUrl, newUrl.toUtf8().constData());
		if (oldUrl && strlen(oldUrl) > 0 && (QString::compare(oldUrl, newUrl.toUtf8().constData()) == 0)) {
			needPopup = false;
			PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingNotice of url are all the latest");
		}
	} else if (show) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSNaverShoppingNotice has shown before");
		needPopup = false;
	}

	//decide is show popup view
	if (needPopup) {
		config_set_bool(PLSApp::plsApp()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_SHOW_INFO, true);
		PLSNaverShoppingNotice term;
		term.setURL(newUrl);
		term.exec();
		config_set_string(PLSApp::plsApp()->NaverShoppingConfig(), PRECAUTION_MESSAGE_INFO, PRECAUTION_URL_INFO, newUrl.toUtf8().constData());
		config_save(PLSApp::plsApp()->NaverShoppingConfig());
	}
}

void PLSPlatformNaverShoppingLIVE::checkSupportResolution() const
{
	PLS_INFO(liveInfoMoudule, "Live Term Status: PLSPlatformNaverShoppingLIVE call checkSupportResolution() method");
	if (config_get_bool(PLSApp::plsApp()->NaverShoppingConfig(), SUPPORT_RESOLUTION_MESSAGE_INFO, SUPPORT_RESOLUTION_SHOW_INFO)) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkSupportResolution() SUPPORT_RESOLUTION_SHOW_INFO value is true");
		return;
	}
	PLS_INFO(liveInfoMoudule, "Live Term Status: checkSupportResolution() SUPPORT_RESOLUTION_SHOW_INFO value is false");
	if (!isPortraitSupportResolution()) {
		QString resulution = ResolutionGuidePage::getPreferResolutionStringOfPlatform(NAVER_SHOPPING_LIVE);
		QString message = QTStr("navershopping.no.support.horizontal.mode.tip").arg(resulution);
		showResolutionAlertView(message, true);
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkSupportResolution() isPortraitSupportResolution() value is false");
		return;
	}
	PLS_INFO(liveInfoMoudule, "Live Term Status: checkSupportResolution() isPortraitSupportResolution() value is true");
	QString tipString;
	if (!PLSServerStreamHandler::instance()->isSupportedResolutionFPS(tipString)) {
		QString resulution = ResolutionGuidePage::getPreferResolutionStringOfPlatform(NAVER_SHOPPING_LIVE);
		QString message = QTStr("navershopping.no.support.resolution.mode.tip").arg(resulution);
		showResolutionAlertView(message, true);
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkSupportResolution() isSupportedResolutionFPS() value is false");
		return;
	}
	PLS_INFO(liveInfoMoudule, "Live Term Status: checkSupportResolution() isSupportedResolutionFPS() value is true");
}

bool PLSPlatformNaverShoppingLIVE::checkGoLiveShoppingResolution(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo) const
{
	if (!isPortraitSupportResolution()) {
		QString resulution = ResolutionGuidePage::getPreferResolutionStringOfPlatform(NAVER_SHOPPING_LIVE);
		QString message = QTStr("navershopping.no.support.horizontal.mode.tip").arg(resulution);
		pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), message);
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() isPortraitSupportResolution() is false");
		return false;
	}

	QVariantMap platformFPSMap = PLSSyncServerManager::instance()->getSupportedResolutionFPSMap();
	if (platformFPSMap.count() == 0) {

#if defined(Q_OS_WIN)
		QString fileName = "";
#elif defined(Q_OS_MACOS)
		QString fileName = "";
#endif

		PLS_ERROR(liveInfoMoudule, "Live Term Status: %s file is not existed", fileName.toUtf8().constData());
		return false;
	}

	auto bVertical = pls_is_dual_output_on() && PLS_PLATFORM_API->getPlatformNaverShoppingLIVE()->isVerticalOutput();
	QString outResolution = PLSServerStreamHandler::instance()->getOutputResolution(bVertical);
	QString outFps = PLSServerStreamHandler::instance()->getOutputFps();
	QVariantMap resolutionFps;
	if (isHighResolutionSLV(prepareInfo.scheduleId)) {
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() isHighResolutionSLV() is true");
		resolutionFps = platformFPSMap.value(NAVER_SHOPPING_HIGH_SLV_RESOLUTION_KEY).toMap();
	} else {
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() isHighResolutionSLV() is false");
		resolutionFps = platformFPSMap.value(NAVER_SHOPPING_RESOLUTION_KEY).toMap();
	}

	//The sync server did not find the corresponding resolution
	if (!resolutionFps.keys().contains(outResolution)) {
		showGoLiveResolutionInvalidAlert();
		PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() not find outResolution(%s)", outResolution.toUtf8().constData());
		return false;
	}

	//After the resolution detection is completed, the frame rate is checked again.
	QVariant fpsVariant = resolutionFps.value(outResolution);
	if (fpsVariant.canConvert(QVariant::String)) {
		if (QString fps = fpsVariant.toString(); fps != outFps) {
			showGoLiveResolutionInvalidAlert();
			PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() not find fps string");
			return false;
		}
	} else if (fpsVariant.canConvert(QVariant::StringList)) {
		if (QStringList fps = fpsVariant.toStringList(); !fps.contains(outFps)) {
			showGoLiveResolutionInvalidAlert();
			PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() not find fps stringlist");
			return false;
		}
	}
	PLS_INFO(liveInfoMoudule, "Live Term Status: checkGoLiveShoppingResolution() method return value is true");
	return true;
}

bool PLSPlatformNaverShoppingLIVE::isPortraitSupportResolution() const
{
	auto main = qobject_cast<PLSBasic *>(App()->GetMainWindow());
	uint64_t out_cx = 0;
	uint64_t out_cy = 0;
	out_cx = config_get_uint(main->Config(), "Video", "OutputCX");
	out_cy = config_get_uint(main->Config(), "Video", "OutputCY");
	if (out_cx > out_cy) {
		return false;
	}
	return true;
}

bool PLSPlatformNaverShoppingLIVE::showResolutionAlertView(const QString &message, bool errorMsg) const
{
	QMap<PLSAlertView::Button, QString> buttons;
	buttons.insert(PLSAlertView::Button::Ok, QTStr("navershopping.recommend.resolution.text"));
	buttons.insert(PLSAlertView::Button::No, QTStr("Cancel"));
	pls::Button ret;
	if (!errorMsg) {
		ret = PLSAlertView::warning(PLSBasic::Get(), QTStr("Alert.Title"), message, buttons, PLSAlertView::Button::Ok);
	} else {
		ret = pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), message, buttons, PLSAlertView::Button::Ok);
	}
	if (ret == PLSAlertView::Button::Ok) {
		if (bool isOK = ResolutionGuidePage::setUsingPlatformPreferResolution(NAVER_SHOPPING_LIVE); !isOK) {
			ResolutionGuidePage::showAlertOnSetResolutionFailed();
			return false;
		}
		return true;
	}
	return false;
}

QString PLSPlatformNaverShoppingLIVE::getOutputResolution() const
{
	auto main = qobject_cast<PLSBasic *>(App()->GetMainWindow());
	return QString("%1x%2").arg(config_get_uint(main->Config(), "Video", "OutputCX")).arg(config_get_uint(main->Config(), "Video", "OutputCY"));
}

void PLSPlatformNaverShoppingLIVE::handleCommonApiType(PLSErrorHandler::RetData retData, PLSAPINaverShoppingType apiType, PLSAPINaverShoppingUrlType urlType, ApiPropertyMap apiPropertyMap)
{
	if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired) {
		handleInvalidToken(retData, apiType, apiPropertyMap);
	} else if (PLSNaverShoppingLIVEAPI::isShowApiAlert(apiType, apiPropertyMap)) {
		PLSErrorHandler::directShowAlert(retData, nullptr);
	}
}

void PLSPlatformNaverShoppingLIVE::handleInvalidToken(PLSErrorHandler::RetData retData, PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap)
{
	if (!PLSNaverShoppingLIVEAPI::isHandleTokenExpired(apiType, apiPropertyMap)) {
		return;
	}

	if (!PLSCHANNELS_API->isLiving()) {
		if (invalidTokenPopupShowing) {
			return;
		}
		emit showLiveinfoLoading();
		invalidTokenPopupShowing = true;
		PLSErrorHandler::directShowAlert(retData, nullptr);
		emit hiddenLiveinfoLoading();
		emit closeDialogByExpired();
		if (retData.clickedBtn == PLSAlertView::Button::Ok) {
			invalidTokenPopupShowing = false;
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
	} else {
		PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
	}
}

const QString &PLSPlatformNaverShoppingLIVE::getSoftwareUUid() const
{
	return m_softwareUUid;
}

void PLSPlatformNaverShoppingLIVE::liveNoticeScheduleListSuccess(const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList)
{
	QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> scheduleInfos;
	for (const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo : scheduleList) {
		if (scheduleInfo.checkStartTime(0, 15 * 60)) {
			scheduleInfos.append(scheduleInfo);
		}
	}
	if (scheduleInfos.isEmpty()) {
		PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "There is no Naver Shopping LIVE schedule live notice need to show");
		return;
	}
	qint64 current = QDateTime::currentMSecsSinceEpoch() / 1000;
	const PLSNaverShoppingLIVEAPI::ScheduleInfo *scheduleInfo = nullptr;
	for (const PLSNaverShoppingLIVEAPI::ScheduleInfo &si : scheduleInfos) {
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
}

bool PLSPlatformNaverShoppingLIVE::isHighResolutionSlvByPrepareInfo() const
{
	return isHighResolutionSLV(m_prepareLiveInfo.scheduleId);
}

void PLSPlatformNaverShoppingLIVE::onCheckStatus()
{
	auto requestCallback = [this](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			QString liveStatus = livingInfo.status.toUpper();
			QString displayType = livingInfo.displayType.toUpper();
			if (isInvalidRemoteLiving(liveStatus, displayType)) {
				deleteTimer(checkStatusTimer);
				return;
			}
			m_prepareLiveInfo.title = livingInfo.title;
			m_prepareLiveInfo.standByImageURL = livingInfo.standByImage;
			m_prepareLiveInfo.description = livingInfo.description;
			m_prepareLiveInfo.allowSearch = livingInfo.searchable;
			m_prepareLiveInfo.shoppingProducts = livingInfo.shoppingProducts;
			m_prepareLiveInfo.externalExposeAgreementStatus = livingInfo.externalExposeAgreementStatus;
			m_prepareLiveInfo.sendNotification = livingInfo.sendNotification;
			setScheduleInfoExpectedStartDate(m_prepareLiveInfo.scheduleId, livingInfo.expectedStartDate);
		} else {
			PLS_LIVE_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live call living polling failed, apiType: %d", apiType);
		}
	};
	PLSNaverShoppingLIVEAPI::getLivingInfo(this, true, requestCallback, this, [](const QObject *receiver) { return receiver != nullptr; });
}

void PLSPlatformNaverShoppingLIVE::onShowScheLiveNotice()
{
	if (!isActive() || isScheLiveNoticeShown) {
		return;
	}

	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE check schedule live notice");
	auto RequestCallback = [this](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, int, int, const QByteArray &) {
		if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			return;
		}
		liveNoticeScheduleListSuccess(scheduleList);
	};

	static long long flag = 0;
	flag++;
	getScheduleList(RequestCallback, SCHEDULE_FIRST_PAGE_NUM, flag, NOTICE_GET_SCHEDULE_LIST, this, [](const QObject *receiver) { return receiver != nullptr; });
}

void PLSPlatformNaverShoppingLIVE::updateScheduleList()
{
	auto RequestCallback = [this](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, int, int, const QByteArray &data) {
		emit scheduleListUpdateFinished();
		if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			m_wizardScheduleList.clear();

			QString errorCode, errorMsg;
			PLSNaverShoppingLIVEAPI::getErrorCodeOrErrorMessage(data, errorCode, errorMsg);
			PLSErrorHandler::ExtraData extraData;
			extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST;
			extraData.pathValueMap["errorCode"] = errorCode;
			extraData.pathValueMap["errorMessage"] = errorMsg;

			if (PLSAPINaverShoppingType::PLSNaverShoppingNetworkError == apiType) {
				auto retData = PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::COMMON_NETWORK_ERROR, NAVER_SHOPPING_LIVE, "", extraData);
				mySharedData().m_lastError = createScheduleGetError(getChannelName(), retData);
			} else if (PLSAPINaverShoppingType::PLSNaverShoppingInvalidAccessToken == apiType) {
				auto retData = PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_TOKEN_EXPIRED, NAVER_SHOPPING_LIVE, "", extraData);
				mySharedData().m_lastError = createScheduleGetError(getChannelName(), retData);
			}
			return;
		}
		m_wizardScheduleList = scheduleList;
	};
	static long long flag = 0;
	flag++;
	getScheduleList(RequestCallback, SCHEDULE_FIRST_PAGE_NUM, flag, LAUNCHER_GET_SCHEDULE_LIST, this, [](const QObject *receiver) { return receiver != nullptr; });
}

void PLSPlatformNaverShoppingLIVE::convertScheduleListToMapList()
{
	mySharedData().m_scheduleList.clear();
	auto uuid = this->getChannelUUID();
	auto tmpList = m_wizardScheduleList;
	QVariantList tmpRet;
	auto convertData = [&uuid](const PLSNaverShoppingLIVEAPI::ScheduleInfo &data) {
		QVariantMap mapData;
		mapData.insert(ChannelData::g_timeStamp, data.timeStamp);
		mapData.insert(ChannelData::g_nickName, data.title);
		mapData.insert(ChannelData::g_channelUUID, uuid);
		mapData.insert(ChannelData::g_channelName, NAVER_SHOPPING_LIVE);
		return QVariant::fromValue(mapData);
	};
	std::transform(tmpList.cbegin(), tmpList.cend(), std::back_inserter(tmpRet), convertData);
	mySharedData().m_scheduleList = tmpRet;
}

void PLSPlatformNaverShoppingLIVE::checkPushNotification(const std::function<void()> &onNext)
{
	if (isRehearsal()) {
		onNext();
		return;
	}

	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo info = getPrepareLiveInfo();
	if (!info.sendNotification) {
		//not call api. direct start.
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.copyright.alert.title"));
		onNext();
		return;
	}

	//call api, then start.
	requestNotifyApi([onNext]() {
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.copyright.alert.title"));
		onNext();
	});
}

void PLSPlatformNaverShoppingLIVE::requestNotifyApi(const std::function<void()> &onNext)
{

	PLSNaverShoppingLIVEAPI::sendPushNotification(
		this, this, [onNext](const QJsonDocument &) { onNext(); },
		[this, onNext](PLSAPINaverShoppingType apiType, const QByteArray &) {
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingNotFound204) {
				onNext();
				return;
			}

			if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				onNext();
				return;
			}
			PLSErrorHandler::ExtraData extraData;
			extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION;
			PLSErrorHandler::RetData retData =
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTIFICATION_FAILED, NAVER_SHOPPING_LIVE, "", extraData);
			if (retData.clickedBtn == PLSAlertView::Button::Retry) {
				requestNotifyApi(onNext);
			} else {
				onNext();
			}
		},
		nullptr);
}

bool PLSPlatformNaverShoppingLIVE::isModified(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &srcInfo, const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &destInfo) const
{
	if (!srcInfo.isNowLiving) {
		return false;
	}

	if (srcInfo.standByImageURL != destInfo.standByImageURL) {
		return true;
	}

	if (srcInfo.title != destInfo.title) {
		return true;
	}

	if (srcInfo.description != destInfo.description) {
		return true;
	}

	if (srcInfo.allowSearch != destInfo.allowSearch) {
		return true;
	}

	if (srcInfo.sendNotification != destInfo.sendNotification) {
		return true;
	}

	if (srcInfo.shoppingProducts.count() != destInfo.shoppingProducts.count()) {
		return true;
	}

	for (int i = 0; i < srcInfo.shoppingProducts.count(); i++) {
		QString newKey = srcInfo.shoppingProducts.at(i).key;
		QString oldKey = destInfo.shoppingProducts.at(i).key;
		bool newPresent = srcInfo.shoppingProducts.at(i).represent;
		bool oldPresent = destInfo.shoppingProducts.at(i).represent;
		PLSProductType newType = srcInfo.shoppingProducts.at(i).productType;
		PLSProductType oldType = destInfo.shoppingProducts.at(i).productType;
		if (newKey != oldKey || newPresent != oldPresent || newType != oldType) {
			return true;
		}
	}

	return false;
}

bool PLSPlatformNaverShoppingLIVE::isInvalidRemoteLiving(const QString &liveStatus, const QString &displayType) const
{
	static QString liveEndStatus = "END";
	static QString liveBlockStatus = "BLOCK";
	static QString liveAbortStatus = "ABORT";
	static QString liveCloseStatus = "CLOSE";
	if (liveStatus == liveEndStatus || liveStatus == liveBlockStatus || liveStatus == liveAbortStatus || displayType == liveCloseStatus) {
		return true;
	}
	return false;
}

void PLSPlatformNaverShoppingLIVE::showGoLiveResolutionInvalidAlert() const
{
	//Invalid resolution string prompt text, Korean and English
	QString outTipString = PLSServerStreamHandler::instance()->getResolutionAndFpsInvalidTip(tr("navershopping.liveinfo.title"));
	pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), outTipString);
}

void PLSPlatformNaverShoppingLIVE::setShareLink(const QString &sharelink) const
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

bool PLSPlatformNaverShoppingLIVE::isAddScaleImageThread(const QString &originPath) const
{
	return m_imagePaths.contains(originPath);
}

QString PLSPlatformNaverShoppingLIVE::getScaleImagePath(const QString &originPath) const
{
	QFileInfo fileInfo(originPath);
	QString scaleImagePath = QString("%1/%2-scaled.%3").arg(fileInfo.absoluteDir().absolutePath()).arg(fileInfo.baseName()).arg(fileInfo.suffix());
	return scaleImagePath;
}
