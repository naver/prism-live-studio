/**
* @file		PLSPlatformApi.h
* @brief	A class to manage all platforms
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
**/

#pragma once

#include <vector>
#include <list>
#include <QObject>
#include <QTimer>
#include <QRecursiveMutex>

#include "obs.hpp"
#include "PLSPlatformBase.hpp"
#include "rtmp/PLSPlatformRtmp.h"
#include "twitch/PLSPlatformTwitch.h"
#include "youtube/PLSPlatformYoutube.h"
#include "facebook/PLSPlatformFacebook.h"
#include "band/PLSPlatformBand.h"
#include "PLSMosquitto.h"

#include "navertv/PLSPlatformNaverTV.h"
#include "PLSPlatformAfreecaTV.h"
#include "naver-shopping-live/PLSPlatformNaverShoppingLIVE.h"

enum class EndLiveType { MQTT_END_LIVE, CLICK_FINISH_BUTTON, OBS_END_LIVE, OTHER_TYPE };
enum class LiveStatus { Normal, PrepareLive, ToStart, LiveStarted, Living, PrepareFinish, ToStop, LiveStoped, LiveEnded };
enum class PLSEndPageType;
#define BOOL2STR(x) (x) ? "true" : "false"
#define LiveInfoPrefix ChannelData::g_liveInfoPrefix.toStdString().c_str()

constexpr auto PrepareInfoPrefix = "prepare status: ";

extern const QString ANALOG_IS_SUCCESS_KEY;
extern const QString ANALOG_FAIL_CODE_KEY;
extern const QString ANALOG_FAIL_REASON_KEY;
extern const QString ANALOG_LIVERECORD_SCENE_COUNT_KEY;
extern const QString ANALOG_LIVERECORD_SOURCE_COUNT_KEY;
const QString ANALOG_GIPHY_ID_KEY = "giphyId";
const QString ANALOG_TOUCH_STICKER_CATEGORY_ID_KEY = "categoryId";
const QString ANALOG_TOUCH_STICKER_ID_KEY = "stickerId";
const QString ANALOG_VIRTUAL_BG_ID_KEY = "virtualBgId";
const QString ANALOG_VIRTUAL_BG_CATEGORY_KEY = "categoryId";
const QString ANALOG_SOURCE_TYPE_KEY = "sourceType";
const QString ANALOG_ITEM_KEY = "item";
const QString ANALOG_DETAIL_KEY = "detail";
const QString ANALOG_FILTER_TYPE_KEY = "filterType";
const QString ANALOG_BGM_CATEGORY_KEY = "categoryId";
const QString ANALOG_BGM_TYPE_KEY = "type";
const QString ANALOG_BGM_ID_KEY = "musicId";
const QString ANALOG_BGM_DURATION_KEY = "duration";
const QString ANALOG_VIRTUAL_CAM_PROCESS_KEY = "targetProcessName";

class PLSPlatformApi : public QObject {
	Q_OBJECT

public:
	static PLSPlatformApi *instance();
	PLSPlatformApi();
	~PLSPlatformApi() override;
	bool initialize();

	//Save the push address and push key of the platform, this method is called before live broadcast
	void saveStreamSettings(std::string platform, std::string server, const std::string_view &key) const;
	void saveStreamSettings(const char *serviceId, OBSData settings) const;

	//Used for title of live information
	int getTotalSteps() const { return m_iTotalSteps; };

	//Use prism server to live, or use each platform directly
	bool isPrismLive() const { return m_bPrismLive; };
	void setPrismLive(bool prismLive) { m_bPrismLive = prismLive; };

	//Get the current live broadcast status
	LiveStatus getLiveStatus() const { return m_liveStatus; }

	//Whether in onPrepareStream status, User click "GoLive" button
	bool isPrepareLive() const { return LiveStatus::PrepareLive == m_liveStatus; };

	bool isLiveStarted() const { return LiveStatus::LiveStarted == m_liveStatus; };

	//Whether is living, the rtmp output module is started
	bool isLiving() const { return LiveStatus::Living == m_liveStatus; };

	bool isRecording() const { return m_bRecording; }

	bool isVirtualCameraActive() const { return m_bVirtualCamera; }

	//Whether in onPrepareStop status, User click "Finish" button
	bool isPrepareFinish() const { return LiveStatus::PrepareFinish == m_liveStatus; };

	//Long lifecycle, from user click "GoLive" button to live is ended
	bool isGoLive() const { return m_liveStatus > LiveStatus::Normal; };

	bool isGoLiveOrRecording() const { return isGoLive() || isRecording(); }

	//Whether user need to exit app
	bool isStopForExit() const { return m_bStopForExit; };

	//Whether MQTT is currently connected
	bool isConnectedMQTT() const { return m_isConnectedMQTT; };

	//Whether platfrom prepared api is successful after GoLive
	PLSPlatformLiveStartedStatus isApiPrepared() const { return m_bApiPrepared; };

	//Whether platfrom relevant api is successful after stream is started
	PLSPlatformLiveStartedStatus isApiStarted() const { return m_bApiStarted; };

	//Get all platform UUID array before live broadcast
	const std::list<QString> &getUuidOnStarted() const { return uuidOnStarted; }

	template<typename MatchFunType> PLSPlatformBase *findMatchedPlatform(const MatchFunType &isMatched)
	{
		QMutexLocker locker(&platformListMutex);
		auto ret = std::find_if(platformList.cbegin(), platformList.cend(), isMatched);
		if (ret == platformList.cend()) {
			return nullptr;
		}
		return *ret;
	}

	template<typename MatchValueType, typename MatchFunType> PLSPlatformBase *findMatchedPlatform(const MatchValueType &value, MatchFunType fun)
	{
		auto isMatched = [&value, &fun](const PLSPlatformBase *item) { return value == (item->*fun)(); };
		return findMatchedPlatform(isMatched);
	}

	//Processing logic when receiving the RequestBroadcastEnd message of MQTT
	void doMqttRequestBroadcastEnd(PLSPlatformBase *base);

	//Use which to get whether there is a corresponding platform initialization, if not, use which to initialize the platform object
	PLSPlatformBase *getPlatformById(const QString &channelUUID, const QVariantMap &info);

	//Get the existing platform pointer through the channel UUID, can not find return nullptr
	PLSPlatformBase *getExistedPlatformById(const QString &channelUUID);

	//Get the existing platform pointer by channel type, can not find return nullptr
	PLSPlatformBase *getExistedPlatformByType(PLSServiceType type);

	//Get platform pointer object by platform type
	PLSPlatformBase *getExistedActivePlatformByType(PLSServiceType type);

	//Get platform pointer list by platform type
	std::list<PLSPlatformBase *> getExistedPlatformsByType(PLSServiceType type);

	//Get the existing platform pointer by the name of the live start api
	//name The value of "platform" in "live/start" api
	PLSPlatformBase *getExistedPlatformByLiveStartName(const QString &name);

	//Get the active platform pointer by the name of the live start api
	//name The value of "platform" in "live/start" api
	PLSPlatformBase *getExistedActivePlatformByLiveStartName(const QString &name);

	//Obtain the corresponding platform pointer through the channel's liveId
	PLSPlatformBase *getPlatformBySimulcastSeq(int simulcastSeq);

	//Get all platform pointer objects logged in
	const std::list<PLSPlatformBase *> &getAllPlatforms() const { return platformList; };
	std::list<PLSPlatformBase *> getActivePlatforms() const;
	std::list<PLSPlatformBase *> getActiveValidPlatforms() const;

	//Whether the current platform type is active
	bool isPlatformActived(PLSServiceType) const;

	//Determine whether the current platform exists
	bool isPlatformExisted(PLSServiceType) const;

	//Get the rtmp platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformRtmp *getPlatformRtmp();
	PLSPlatformRtmp *getPlatformRtmpActive();

	//Get the Twitch platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformTwitch *getPlatformTwitch();
	PLSPlatformTwitch *getPlatformTwitchActive();

	//Get the Band platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformBand *getPlatformBand();
	PLSPlatformBand *getPlatformBandActive();

	//Get the Youtube platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformYoutube *getPlatformYoutube();
	PLSPlatformYoutube *getPlatformYoutubeActive();

	//Get the Facebook platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformFacebook *getPlatformFacebook();
	PLSPlatformFacebook *getPlatformFacebookActive();

	//Get the NaverTv platform pointer, the pointer is empty when the channel is not logged in
	std::list<PLSPlatformNaverTV *> getPlatformNaverTV();
	PLSPlatformNaverTV *getPlatformNaverTVActive();

	//Get the AffrecaTv platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformAfreecaTV *getPlatformAfreecaTV();
	PLSPlatformAfreecaTV *getPlatformAfreecaTVEActive();

	//Get the NaverShopping platform pointer, the pointer is empty when the channel is not logged in
	PLSPlatformNaverShoppingLIVE *getPlatformNaverShoppingLIVE();
	PLSPlatformNaverShoppingLIVE *getPlatformNaverShoppingLIVEActive();

	//Platform onActive callback function
	void activateCallback(const PLSPlatformBase *, bool) const;

	//The callback function of platform onDeactive
	void deactivateCallback(const PLSPlatformBase *, bool) const;

	//Status callback of onPrepare method
	void prepareLiveCallback(bool value);

	//Check if LiveStarted call is successful for all platforms
	void checkAllPlatformLiveStarted();

	//Reach platform max live time
	void notifyLiveLeftMinutes(PLSPlatformBase *platform, int maxLiveTime, uint leftMinutes);

	//Status callback of liveStarted method
	void liveStartedCallback(bool value);

	//Callback of onPrepareFinish method
	void prepareFinishCallback();

	//Callback of onLiveStopped method
	void liveStoppedCallback();

	//must call, when finish live
	void onLiveEnded();

	//Callback of onLiveEnded method
	void liveEndedCallback();

	//Receive data for web pages from CEF
	static const char *invokedByWeb(const char *data);

	//Show the end page of the live stream(Display the end page of the live broadcast (currently, the live started of all platforms will only be displayed when the results are returned))
	void showEndView(bool isRecord, bool isShowDialog = true);

	//Called when the chat permission of the platform is changed, it notifies the chat page permission has changed, and the chat page refreshes the UI display.
	void forwardWebMessagePrivateChanged(const PLSPlatformBase *platform, bool isPrivate) const;

	//when chat tab from hide to show, then send message to web
	void sendWebChatTabShown(const QString &channelName, bool isAllTab) const;

	//Get the output bit rate set by the setting
	static int getOutputBitrate();

	//The maximum duration to start the live broadcast of the platform set on Gpop
	void doStartGpopMaxTimeLiveTimer();
	void startGeneralMaxTimeLiveTimer();
	void stopGeneralMaxTimeLiveTimer();

	//MQTT stopped
	void stopMqtt();

	//Receive client-sent chat messages from CEF
	void doWebSendChatRequest(const QJsonObject &data) const;

	//Get json data of init message sent to chat
	QJsonObject getWebPrismInit() const;

	//Send platform initialization information to the chat window
	void sendWebPrismInit() const;

	//When calling to end the live broadcast, the displayed reason for stopping the live broadcast
	const QString &getLiveEndReason() const;
	void setLiveEndReason(const QString &liveEndReason, EndLiveType endLiveType = EndLiveType::OTHER_TYPE);
	void stopStreaming(const QString &reason, EndLiveType endLiveType = EndLiveType::OTHER_TYPE);

	//send analog request
	void createAnalogInfo(QVariantMap &uploadVariantMap) const;
	void sendLiveAnalog(bool success, const QString &reason = QString(), int code = 0) const;
	void sendLiveAnalog(const QVariantMap &info) const;
	void sendRecordAnalog(bool success, const QString &reason = QString(), int code = 0) const;
	void sendRecordAnalog(const QVariantMap &info) const;
	void sendAnalog(AnalogType type, const QVariantMap &info) const;
	void sendBeautyAnalog(const QVariantMap &info) const;
	void sendGiphyAnalog(const QVariantMap &info) const;
	void sendTouchStickerAnalog(const QVariantMap &info) const;
	void sendVirtualBgAnalog(const QVariantMap &info) const;
	void sendDrawPenAnalog(const QVariantMap &info) const;
	void sendSourceAnalog(const QVariantMap &info) const;
	void sendFilterAnalog(const QVariantMap &info) const;
	void sendBgmAnalog(const QVariantMap &info) const;
	void sendVirtualCamAnalog(const QVariantMap &info) const;
	void sendClockWidgetAnalog(const QVariantMap &info) const;
	void sendBgTemplateAnalog(OBSData privious, OBSData current) const;
	void sendAudioVisualizerAnalog(OBSData privious, OBSData current) const;
	void sendCameraDeviceAnalog(OBSData privious, OBSData current) const;
	void sendAnalogOnUserConfirm(OBSSource source, OBSData privious, OBSData current) const;
	void sendCodecAnalog(const QVariantMap &info) const;

	void updateAllScheduleList();
	void loadingWidzardCheck(bool isCheck = true);
	QVariantList getAllScheduleList() const;
	QVariantList getAllLastErrors() const;

	void setTaskCount(const QString &taskKey, int taskCount);
	int currentTaskCount(const QString &taskKey) const;
	void decreaseCount(const QString &taskKey, int vol = 1);
	void resetTaskCount(const QString &taskKey);

	//Receive a message that the program has been closed
	void ensureStopOutput();

signals:

	/**
	* Occurs when the channel is removed
	* To remove the corresponding platform pointer object
	*/
	void channelRemoved(const QVariantMap &info);

	/**
	* Occured when user click "GoLive" button to prepare stream server&key
	* To notify dashboard whether prepare stream server&key is successful
	*/
	void livePrepared(bool value);

	/**
	* Occured all platform api is sended after stream is started
	* To notify dashboard whether api is successful after stream is started
	*/
	void liveStarted(bool value);

	/**
	* Occured all platform api is sended before stream is going to stop
	* To notify dashboard to stop stream
	*/
	void liveToStop();

	/**
	* Occured all platform api is sended after stream is stopped
	* May popup LiveEnd dialog
	* param apiPrepared, whether all the platform prepared api is successful
	* param apiStarted, whether all the platform special start api is successful
	* ***This signal is before liveEndedForUi***
	*/
	void liveEnded(bool apiPrepared, bool apiStarted);

	/**
	* Occured all platform api is sended after stream is stopped
	* To notify dashboard switch to "GoLive" status
	* ***This signal is after liveEnded***
	*/
	void liveEndedForUi();

	void outputStopped();
	void outputStateChanged();

	/**
	* The cef browser event is not in main thread
	* different thread, should use value param, not ref
	**/
	void onWebRequest(const QString data);

	/**
	* when click golive button, emit isEnterd=true
	* when start live failed or succeed, emit isEnterd=false
	**/
	void enterLivePrepareState(bool isEnterd);

	/**
	* The EndPage page shows that it is done sending this signal
	*  isRecordEnd represents the EndPage showing whether Record or Live is finished
	**/
	void liveEndPageShowComplected(bool isRecordEnd);

	void allScheduleListUpdated();
	/**
	* 
	*  Get the Data data of the chat data sent to the Web, this interface is used for communication between WebChat and RemoteChat
	**/
	void sendWebChatDataJsonObject(const QJsonObject &data);

	/**
    *
    * deal channelActiveChanged signal, then emit this signals
    **/
	void platformActiveDone();

public slots:
	void onActive(const QString &which);
	void onInactive(const QString &which);
	void onClearChannel();
	void onAddChannel(const QString &channelUUID);
	void onRemoveChannel(const QString &channelUUID);
	void onUpdateChannel(const QString &which);
	void onAllChannelRefreshDone();
	void onPrepareLive();
	void onMqttMessage(const QString, const QString); //From different thread
	void doWebRequest(const QString &data);
	void doChannelInitialized(); //All channels are refreshed and the GoLive button becomes Ready
	void onPrepareFinish();

	void onUpdateScheduleListFinished();

private:
	//The function called during the preparation phase of the onPrepare method
	//Display Multi-Platform Disable Live Alert with Resolutions greater than 1080p
	void showMultiplePlatformGreater1080pAlert();
	void sortPlatforms();
	bool checkNetworkConnected();
	void resetPlatformsLivingInfo();
	bool checkWaterMarkAndOutroResource();
	bool checkOutputBitrateValid();

	//MQTT  related
	void doStatRequest(const QJsonObject &data);
	void doMqttStatForPlatform(const PLSPlatformBase *base, const QJsonObject &data) const;
	void doStatusRequest(const QJsonObject &data);
	void doNoticeLong(const QJsonObject &data) const;
	void doNaverShoppingMaxLiveTime(int leftMinutes) const;
	void doGeneralMaxLiveTime(int leftMinutes) const;
	void doLiveFnishedByPlatform(const QJsonObject &data);
	void doOtherMqttStatusType(const QJsonObject &data, const QString &statusType);

	void doMqttRequestAccessToken(PLSPlatformBase *base) const;
	void doMqttBroadcastStatus(const PLSPlatformBase *base, PLSPlatformMqttStatus status) const;
	void doMqttSimulcastUnstable(PLSPlatformBase *base, PLSPlatformMqttStatus status);
	void doMqttSimulcastUnstableError(PLSPlatformBase *platform, PLSPlatformMqttStatus status);
	void doMqttChatRequest(QString value);

	//Receive platform token expired message from CEF
	void doWebTokenRequest(const QJsonObject &jsonData);
	void sendWebPrismToken(const PLSPlatformBase *platform) const;
	void sendWebPrismPlatformClose(const PLSPlatformBase *platform) const;

	//Receive broadcast message from CEF
	void doWebBroadcastMessage(const QJsonObject &data) const;

	//Receive webPageLogs message from CEF
	void doWebPageLogsMessage(const QJsonObject &obj) const;

	//Get the type of the platform through the platform's data Info
	PLSServiceType getServiceType(const QVariantMap &info) const;

	//Create a pointer object corresponding to the platform
	PLSPlatformBase *buildPlatform(PLSServiceType type);

	//Set live broadcast status
	void setLiveStatus(LiveStatus);

	//Determine whether the current channel is valid
	static bool isValidChannel(const QVariantMap &info);

	//Receive messages related to OBS streaming
	static void onFrontendEvent(enum obs_frontend_event event, void *private_data);

	//Received the message that OBS has started streaming
	void onLiveStarted();

	//The function called after onLiveStarted receives the underlying push stream
	//After the live broadcast starts, clear data such as the number of likes and views on the live broadcast platform
	void clearLiveStatisticsInfo() const;

	//Receive a message that OBS has stopped streaming
	//maybe not call, when finish live
	void onLiveStopped();

	//Received the message that OBS has started recording
	void onRecordingStarted();

	//Receive a message that OBS has stopped recording
	void onRecordingStoped();

	//Receive a message that OBS has started Reply Buffer
	void onReplayBufferStarted();

	//Receive a message that OBS has stopped Reply Buffer
	void onReplayBufferStoped();

	//Received the message that OBS has started Virtual Camera
	void onVirtualCameraStarted();

	//Receive a message that OBS has stopped Virtual Camera
	void onVirtualCameraStopped();

	//Display the EndPage page
	void showEndViewByType(PLSEndPageType pageType) const;
	void showEndView_Record(bool isShowDialog, bool isLivingAndRecording, bool isRehearsal);
	void showEndView_Live(bool isShowDialog, bool isLivingAndRecording, bool isRehearsal, bool isStreamingRecordStopAuto);

	//private
	mutable QRecursiveMutex platformListMutex;
	std::list<PLSPlatformBase *> platformList;
	std::list<QString> uuidOnStarted;

	//Live State Variables
	int m_iTotalSteps = 0;
	bool m_bPrismLive = false;
	LiveStatus m_liveStatus = LiveStatus::Normal;
	PLSPlatformLiveStartedStatus m_bApiPrepared = PLSPlatformLiveStartedStatus::PLS_NONE;
	PLSPlatformLiveStartedStatus m_bApiStarted = PLSPlatformLiveStartedStatus::PLS_NONE;
	QString m_endLiveReason;

	bool m_bRecording = false;
	bool m_bReplayBuffer = false;
	bool m_bStopForExit = false;
	bool m_bVirtualCamera = false;

	//Used for mqtt
	QPointer<PLSMosquitto> m_pMQTT = nullptr;
	QTimer m_timerMQTT;
	QString m_strLastMqttChat;
	QString m_strLastMqttStatus;
	QString m_strLastMqttStat;

	//when live and record stop same time, will ignore record end page show and toast.
	bool m_isIgnoreNextRecordShow = false;
	bool m_ignoreRequestBroadcastEnd = false;
	bool m_isConnectedMQTT = false;
	QMap<QString, int> m_taskWaiting;
	PLSPlatformBase *m_generalPlatform{nullptr};
};

#define PLS_PLATFORM_API PLSPlatformApi::instance()

#define PLS_PLATFORM_ALL PLS_PLATFORM_API->getAllPlatforms()
#define PLS_PLATFORM_ACTIVIED PLS_PLATFORM_API->getActivePlatforms()
#define PLS_PLATFORM_TWITCH PLS_PLATFORM_API->getPlatformTwitch()
#define PLS_PLATFORM_YOUTUBE PLS_PLATFORM_API->getPlatformYoutube()
#define PLS_PLATFORM_FACEBOOK PLS_PLATFORM_API->getPlatformFacebook()
#define PLS_PLATFORM_AFREECATV PLS_PLATFORM_API->getPlatformAfreecaTV()
