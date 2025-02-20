/*
* @file		PLSPlatformBase.hpp
* @brief	A base class for all platforms
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include <string>
#include <functional>

#include <QWidget>
#include <QPointer>
#include <QVariant>
#include <QMap>
#include <QJsonObject>
#include <QNetworkReply>

#include "PLSBasic.h"
#include "pls-channel-const.h"
#include "PLSBasic.h"
#include "liblog.h"
#include "ChannelCommonFunctions.h"

constexpr auto MODULE_PlatformService = "PlatformService";
constexpr auto MODULE_PLATFORM_TWITCH = "Platform/twitch";
constexpr auto MODULE_PLATFORM_NCB2B = "Platform/ncb2b";
constexpr auto MODULE_PLATFORM_AFREECATV = "Platform/afreecaTV";

enum class PLSServiceType { ST_CUSTOM, ST_TWITCH, ST_YOUTUBE, ST_FACEBOOK, ST_VLIVE, ST_NAVERTV, ST_BAND, ST_AFREECATV, ST_NAVER_SHOPPING_LIVE, ST_TWITTER, ST_CHZZK, ST_NCB2B, ST_MAX_PLATFORMS };
enum class PLSTokenRequestStatus { PLS_GOOD, PLS_ING, PLS_BAD };
enum class PLSMaxLiveTimerStatus { PLS_TIMER_NONE, PLS_TIMER_ING };

const int PLATFORM_SIZE = static_cast<int>(PLSServiceType::ST_MAX_PLATFORMS);
extern const std::array<const char *, PLATFORM_SIZE> NamesForChannelType;
extern const std::array<const char *, PLATFORM_SIZE> NamesForSettingId;
extern const std::array<const char *, PLATFORM_SIZE> NamesForLiveStart;

extern const char *const KeyConfigLiveInfo;
extern const char *const KeyTwitchServer;

enum class PLSPlatformApiResult {
	PAR_SUCCEED,
	PAR_NETWORK_ERROR,
	PAR_API_FAILED,
	PAR_TOKEN_EXPIRED,
	PAR_SERVER_ERROR,
	YOUTUBE_API_ERROR_NO_CHANNEL,
	YOUTUBE_API_ERROR_REDUNDANT_TRANSITION,
	YOUTUBE_API_ERROR_INVALID_TRANSITION,
	YOUTUBE_API_ERROR_LATENCY_TRANSITION,
	PAR_API_ERROR_LIVE_BROADCAST_NOT_FOUND,
	PAR_API_ERROR_FORBIDDEN,
	VLIVE_API_ERROR_NO_PROFILE,
	PAR_API_ERROR_Live_Invalid,
	PAR_API_ERROR_Scheduled_Time,
	PAR_API_ERROR_NO_PERMISSION,
	PAR_API_ERROR_Upload_Image,
	PAR_API_ERROR_StartLive_Other,
	PAR_API_ERROR_UPDATE,
	PAR_API_ERROR_StartLive_User_Blocked,
	PAR_API_ERROR_CUSTOM,
	PAR_API_ERROR_CHANNEL_NO_PERMISSON,
	PAR_API_ERROR_KIDS_READONLY,
	PAR_API_ERROR_LATENCY_CHANGED_FAILED,
	PAR_API_ERROR_CONNECT_ERROR,
	PAR_API_ERROR_SCHEDULE_API_FAILED,
	PAR_API_ERROR_TYPE_NOT_SUPPORT,
	PAR_API_ERROR_REHEARSAL,
	PAR_API_ERROR_INVALID_DESCRIPTION,
	PAR_API_ERROR_NEED_ARGEE,
	PAR_API_ERROR_SYSTEM_TIME
};

enum class PLSPlatformMqttStatus {
	PMS_NONE,
	PMS_ON_BROADCAST,
	PMS_END_BROADCAST,
	PMS_CONNECTING_TO_SERVER,
	PMS_CANNOT_FIND_SERVER,
	PMS_CANNOT_CONNECT_TO_SERVER,
	PMS_CANNOT_AUTH_TO_SERVER,
	PMS_CANNOT_CONNECT_TO_PATH,
	PMS_WAITING_TO_BROADCAST,
	PMS_LIVE_FINISHED_BY_PLATFORM
};

enum class PLSPlatformMqttTopic {
	PMS_NONE,
	PMS_STAT_TOPIC,
	PMS_LIVE_FINISHED_BY_PLATFORM_TOPIC,
	PMS_REQUEST_BROADCAST_END_TOPIC,
	PMS_REQUEST_ACCESS_TOKEN_TOPIC,
	PMS_CHAT_TOPIC,
	PMS_BROADCAST_STATUS_TOPIC,
	PMS_SIMULCAST_UNSTABLE_TOPIC
};

enum class PLSPlatformLiveStartedStatus { PLS_NONE, PLS_SUCCESS, PLS_FAILED };

class PLSPlatformBase : public QObject {
	Q_OBJECT
public:
	virtual PLSServiceType getServiceType() const = 0;

	//Corresponding UI's channel name
	const char *getNameForChannelType() const { return NamesForChannelType[static_cast<int>(getServiceType())]; }

	//The key name in service.json
	const char *getNameForSettingId() const { return NamesForSettingId[static_cast<int>(getServiceType())]; }

	//The value of "platform" in "live/start" api
	const char *getNameForLiveStart() const { return NamesForLiveStart[static_cast<int>(getServiceType())]; }

	//relevant data from dashboard
	QString getChannelUUID() const;
	virtual QString getChannelToken() const;
	QString getChannelRefreshToken() const;
	ChannelData::ChannelDataType getChannelType() const;
	QString getChannelName() const;
	QString getPlatFormName() const;
	int getChannelOrder() const;
	QString getChannelCookie() const;

	//Indicates whether the current channel is valid
	bool isValid() const;

	//Indicates whether the current channel has expired
	bool isExpired() const;

	PLSPlatformBase &setInitData(const QVariantMap &value)
	{
		mySharedData().m_mapInitData = value;
		onInitDataChanged();

		return *this;
	}
	const QVariantMap &getInitData();

	//Set whether the current channel is selected or unselected
	PLSPlatformBase &setActive(bool value)
	{
		mySharedData().m_bActive = value;
		return *this;
	}

	//Get the current channel's selected or unselected state
	bool isActive() const { return mySharedData().m_bActive; }

	PLSPlatformBase &setIsScheduleLive(bool value)
	{
		m_bScheduleLive = value;
		return *this;
	}
	bool isScheduleLive() const { return m_bScheduleLive; }

	virtual bool isSendChatToMqtt() const { return false; }

	//Set the broadcast id, currently only used by the twitch platform
	PLSPlatformBase &setChannelId(const std::string &value)
	{
		mySharedData().m_strChannelId = value;
		return *this;
	}

	//Get the broadcast id, currently only used by the twitch platform
	const std::string &getChannelId() const { return mySharedData().m_strChannelId; }

	//Set the broadcast name, currently only used by the twitch platform
	PLSPlatformBase &setDisplayName(const std::string &value)
	{
		mySharedData().m_strDisplayName = value;
		return *this;
	}

	//Get the broadcast name, currently only used by the twitch platform
	const std::string &getDisplayName() const { return mySharedData().m_strDisplayName; }

	virtual PLSPlatformBase &setTitle(const std::string &value)
	{
		m_strTitle = value;
		return *this;
	}
	const std::string &getTitle() const { return m_strTitle; }

	//set the broadcast catogory, currently only used by the twitch platform
	virtual PLSPlatformBase &setCategory(const std::string &value)
	{
		mySharedData().m_strCategory = value;
		return *this;
	}

	//get the broadcast catogory, currently only used by the twitch platform
	const std::string &getCategory() const { return mySharedData().m_strCategory; }

	//set platform allow push stream
	PLSPlatformBase &setAllowPushStream(bool isAllow)
	{
		m_bIsAllowPushStream = isAllow;
		return *this;
	}
	bool getIsAllowPushStream() const { return m_bIsAllowPushStream; }

	//set platform push stream address
	PLSPlatformBase &setStreamServer(const std::string &value)
	{
		mySharedData().m_strStreamServer = value;
		return *this;
	}

	//get platform push stream address
	const std::string &getStreamServer() const { return mySharedData().m_strStreamServer; };

	QString getStreamServerFromInitData() { return getInitData().value(ChannelData::g_channelRtmpUrl).toString(); }

	//set platform push stream stream key
	PLSPlatformBase &setStreamKey(const std::string &value)
	{
		mySharedData().m_strStreamKey = value;
		return *this;
	}

	//get platform push stream stream key
	const std::string &getStreamKey() const { return mySharedData().m_strStreamKey; }

	PLSPlatformBase &setDescription(const std::string &value)
	{
		m_strDescription = value;
		return *this;
	}
	const std::string &getDescription() const { return m_strDescription; }

	QString getStreamUrl() const { return QString::fromStdString(mySharedData().m_strStreamServer + "/" + mySharedData().m_strStreamKey); }

	PLSPlatformBase &setCurrStep(int value)
	{
		mySharedData().m_iCurrStep = value;
		return *this;
	}
	int getCurrentStep() const { return mySharedData().m_iCurrStep; }

	//set the apiPrepare of the current platform,m_bApiPreparedStatus mean the onPrepareLive result
	PLSPlatformBase &setApiPrepared(PLSPlatformLiveStartedStatus status)
	{
		m_bApiPreparedStatus = status;
		return *this;
	}

	//get the apiPrepare of the current platform
	PLSPlatformLiveStartedStatus getApiPrepared() const { return m_bApiPreparedStatus; };

	//set the apiStarted of the current platform ,m_bApiStartedStatus mean the onPrepareLive result
	PLSPlatformBase &setApiStarted(PLSPlatformLiveStartedStatus status)
	{
		m_bApiStartedStatus = status;
		return *this;
	}

	//get the apiStarted of the current platform
	PLSPlatformLiveStartedStatus getApiStarted() const { return m_bApiStartedStatus; };

	//Set whether the current platform starts the maximum duration timer state
	PLSPlatformBase &setMaxLiveTimerStatus(PLSMaxLiveTimerStatus status)
	{
		m_maxLiveTimerStatus = status;
		return *this;
	}

	//Get whether the current platform starts the maximum duration timer
	PLSMaxLiveTimerStatus getMaxLiveTimerStatus() const { return m_maxLiveTimerStatus; };

	//Start and stop the maximum stream duration timer
	void startMaxLiveTimer(int minutes, const QStringList &intervalList);
	void stopMaxLiveTimer();

	//The platform has called the live broadcast start api,only Youtube and vLive
	bool getIsSubChannelStartApiCall() const { return m_bSubChannelStartApiCall; };
	void setIsSubChannelStartApiCall(bool isCalled) { m_bSubChannelStartApiCall = isCalled; };

	PLSPlatformBase &setAlertParent(QWidget *value)
	{
		mySharedData().m_pAlertParent = value;
		return *this;
	}
	QWidget *getAlertParent()
	{
		if (mySharedData().m_pAlertParent.isNull()) {
			return PLSBasic::instance()->getMainView();
		}

		return mySharedData().m_pAlertParent;
	}

	//The channel unique identifier of the platform where the Live/start API request is successfully set
	PLSPlatformBase &setChannelLiveSeq(int value)
	{
		mySharedData().m_channelLiveSeq = value;
		return *this;
	}
	int getChannelLiveSeq() const { return mySharedData().m_channelLiveSeq; }

	//MQTT receives the REQUEST_ACCESS_TOKEN message and calls the corresponding platform to process the refresh token logic
	PLSPlatformBase &setTokenRequestStatus(PLSTokenRequestStatus value)
	{
		m_tokenRequestStatus = value;
		return *this;
	}
	PLSTokenRequestStatus getTokenRequestStatus() const { return m_tokenRequestStatus; }

	//Convert status string to enum via MQTT
	static PLSPlatformMqttStatus getMqttStatus(const QString &szStatus);

	/**
	* Occured when user activate a platform, may send some api to get necessary data
	* param: callback, notify result, true for no error, false for otherwise
	**/
	virtual void onActive()
	{
		setActive(true);
		activateCallback(true);
	};
	/**
	* Occured when user deactivate a platform
	* if in multicast, should stop single stream
	* param: callback, notify result, true for no error, false for otherwise
	**/
	virtual void onInactive()
	{
		setActive(false);
		deactivateCallback(true);
	};

	/**
	* Occured when ready to stream, typically, should get rtmp server&key
	*
	* param: value, the previous result of platform
	**/
	virtual void onPrepareLive(bool value) { prepareLiveCallback(value); }

	/**
	* Occured after all the platforms is prepared
	*
	* param: value, the result of all platforms
	*/
	virtual void onAllPrepareLive(bool) {}

	/**
	* Occured when output module succeed to stream
	* typically, may send some api after stream is started
	*
	* param: value, the previous result of platform
	**/
	virtual void onLiveStarted() { m_bApiStartedStatus = PLSPlatformLiveStartedStatus::PLS_SUCCESS; }

	/**
	* Occured after all the platforms is started
	*
	* param: value, the result of all platforms
	*/
	virtual void onAlLiveStarted(bool) {}

	/**
	* Occured when ready to stop stream, may send some api before stop stream
	**/
	virtual void onPrepareFinish() { prepareFinishCallback(); }

	/**
	* Occured when stream is stopped, may send some api after stream is stopped
	**/
	virtual void onLiveStopped() { liveStoppedCallback(); }

	/**
	* Each onPrepareLive always have an onLiveEnded call,
	* whatever onPrepareLive is succeeded or failed.
	* if onPrepareLive is failed, there is no onLiveStarted & onLiveStopped.
	**/
	virtual void onLiveEnded() { liveEndedCallback(); }

	/**
	* When the MQTT message is received, it is distributed to each platform for processing
	* MQTT topic, MQTT data received by jsonObject
	* The return value is True on behalf of PLSPlatformAPI to process this MQTT message, if it returns False, PLSPlatformAPI does not process this MQTT message
	**/
	virtual bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject);

	virtual QString getShareUrl() { return QString(); }
	virtual QString getServiceLiveLink() { return QString(); }

	//the url have encryption
	virtual QString getShareUrlEnc() { return QString(); }
	virtual QString getServiceLiveLinkEnc() { return QString(); }

	virtual QJsonObject getWebChatParams();
	virtual QJsonObject getMqttChatParams();
	virtual QJsonObject getLiveStartParams();
	virtual QJsonObject getLiveEndParams() { return QJsonObject(); }
	virtual QJsonObject getDirectStartParams() { return QJsonObject(); }
	virtual QMap<QString, QString> getDirectEndParams() { return {}; }

	virtual void onInitDataChanged() {}

	virtual bool isMqttChatCanShow(const QJsonObject &);

	virtual void toStartGetScheduleList() final;

	virtual const QVariantList &getLastScheduleList() final;

	virtual const QVariantMap &getLastError() final;

	bool isHorizontalOutput() const
	{
		return ChannelData::ChannelDualOutput::HorizontalOutput == PLSCHANNELS_API->getValueOfChannel(getChannelUUID(), ChannelData::g_channelDualOutput, ChannelData::NoSet);
	}
	bool isVerticalOutput() const
	{
		return ChannelData::ChannelDualOutput::VerticalOutput == PLSCHANNELS_API->getValueOfChannel(getChannelUUID(), ChannelData::g_channelDualOutput, ChannelData::NoSet);
	}
signals:
	void scheduleListUpdateFinished();

private slots:
	void reachMaxLivingTime();

private:
	void startMaxLiveTimer(int interval);

protected:
	// to convert data to map ,do not call it
	virtual void convertScheduleListToMapList();

	//to get schedule list
	virtual void updateScheduleList();

	void activateCallback(bool) const;
	void deactivateCallback(bool);

	void prepareLiveCallback(bool value);
	void liveStartedCallback(bool value);
	void prepareFinishCallback();
	void liveStoppedCallback();
	void liveEndedCallback();

	std::string &getTitleStrRef() { return m_strTitle; }

	//protected:
	struct SharedData {

		//Whether the current channel is selected or unselected
		bool m_bActive = false;

		//Get initialized data from channel data
		QVariantMap m_mapInitData;

		//the broadcast id, currently only used by the twitch platform
		std::string m_strChannelId;

		//the broadcast name, currently only used by the twitch platform
		std::string m_strDisplayName;

		//the broadcast catogory, currently only used by the twitch platform
		std::string m_strCategory;

		//platform push stream address
		std::string m_strStreamServer;

		//platform push stream streamKey
		std::string m_strStreamKey;

		//The step index when click "Go Live" button
		int m_iCurrStep = 0;

		//The parent to show alter dialog
		QPointer<QWidget> m_pAlertParent;

		//the prism live, each platform will have one sepecial seq
		int m_channelLiveSeq = 0;

		QVariantList m_scheduleList;
		QVariantMap m_lastError;
	};

	const SharedData &mySharedData() const { return mSharedData; };
	SharedData &mySharedData() { return mSharedData; };

private:
	SharedData mSharedData;

	std::string m_strTitle;

	bool m_bScheduleLive = false;

	std::string m_strDescription;

	//When live stoped will clear the status
	PLSTokenRequestStatus m_tokenRequestStatus;

	//Whether onPrepare has succeeded
	PLSPlatformLiveStartedStatus m_bApiPreparedStatus = PLSPlatformLiveStartedStatus::PLS_NONE;

	//Has the live streaming onStarted been successful
	PLSPlatformLiveStartedStatus m_bApiStartedStatus = PLSPlatformLiveStartedStatus::PLS_NONE;

	//Live broadcast maximum duration timer status
	PLSMaxLiveTimerStatus m_maxLiveTimerStatus = PLSMaxLiveTimerStatus::PLS_TIMER_NONE;
	QStringList m_intervalList;
	int m_maxLiveleftMinute = 0;
	int m_maxLiveMinutes = 0;

	QTimer *m_maxTimeLenthTimer{nullptr};
	bool m_bIsAllowPushStream = true;
	bool m_bSubChannelStartApiCall = false;
};
