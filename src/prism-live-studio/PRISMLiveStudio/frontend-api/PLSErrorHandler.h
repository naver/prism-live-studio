#pragma once

#include <QObject>
#include <QDialogButtonBox>
#include <QNetworkReply>
#include <QJsonObject>
#include <vector>
#include "frontend-api-global.h"
#include "PLSAlertView.h"
#include "libhttp-client.h"
#include <mutex>
#include <shared_mutex>

FRONTEND_API extern const QString PLSErrPhaseChannel;
FRONTEND_API extern const QString PLSErrPhaseLogin;
FRONTEND_API extern const QString PLSErrPhaseDashBoard;

FRONTEND_API extern const QString PLSErrCustomKey_LoadLiveInfoFailed;
FRONTEND_API extern const QString PLSErrCustomKey_StartLiveFailed_Single;
FRONTEND_API extern const QString PLSErrCustomKey_StartLiveFailed_Multi;
FRONTEND_API extern const QString PLSErrCustomKey_UpdateLiveInfoFailed;
FRONTEND_API extern const QString PLSErrCustomKey_StartRehearsalFailed;
FRONTEND_API extern const QString PLSErrCustomKey_LoadLiveInfoExpired;
FRONTEND_API extern const QString PLSErrCustomKey_UpdateLiveInfoFailedNoService;
FRONTEND_API extern const QString PLSErrCustomKey_UploadImageFailed;
FRONTEND_API extern const QString PLSErrCustomKey_OutputRecordFailed;

FRONTEND_API extern const QString PLSErrApiKey_OutputRecord;
FRONTEND_API extern const QString PLSErrApiKey_OutputStream;

class FRONTEND_API PLSErrorHandler : public QObject {
	Q_OBJECT

public:
	enum class AlertType {
		Error = 0, //contact us
		Error_Blog,
		Error_Link,
		Error_Blog_Link,
		Normal, //normal
		Normal_Code,
		Normal_Code_Blog,
		Normal_Code_Link
	};
	Q_ENUM(AlertType)

	enum class ErrorType {
		Other = -1,
		TokenExpired = 0,
	};
	Q_ENUM(ErrorType)

	enum ErrCode {
		INVALID = -1,
		SUCCESS = 0,

		//Common
		COMMON_NETWORK_ERROR = 10000,
		COMMON_UNKNOWN_ERROR = 10001,
		COMMON_TOKEN_EXPIRED_ERROR = 10002,
		COMMON_CHANNEL_LOGIN_TOKEN_EXPIRED_ERROR = 10003,
		COMMON_CHANNEL_LOGIN_FAIL = 10004,
		COMMON_CHANNEL_EMPTYCHANNEL,
		//Common default
		COMMON_DEFAULT_LOADLIVEINFOFAILED = 10500,
		COMMON_DEFAULT_STARTLIVEFAILED_SINGLE,
		COMMON_DEFAULT_STARTLIVEFAILED_MULTI,
		COMMON_DEFAULT_UPDATELIVEINFOFAILED,
		COMMON_DEFAULT_STARTREHEARSALFAILED,
		COMMON_DEFAULT_FAILEDTOSTARTLIVE,
		COMMON_DEFAULT_TIMEOUTTRYAGAIN,
		COMMON_DEFAULT_UPLOADIMAGEFAILED,
		COMMON_DEFAULT_SERVERERRORTRYAGAIN,
		COMMON_DEFAULT_TEMPERRORTRYAGAIN,
		COMMON_DEFAULT_NAVERTVUNKNOWN,
		COMMON_DEFAULT_PRISMLOGINFAILEDAGAIN,
		COMMON_DEFAULT_B2BLOGINFAILEDAGAIN,
		COMMON_DEFAULT_FAILEDBECAUSECONNECTION,
		COMMON_DEFAULT_OUTPUT_RECORD_ERROR,
		COMMON_DEFAULT_UPDATELIVEINFOFAILED_NOSERVICE,
		COMMON_DEFAULT_MQTT_BROADCAST_END,

		PRISM_API_INIT = 11000,
		PRISM_API_TOKEN_EXPIRED = 11001,
		PRISM_API_SYSTEM_TIME_ERROR = 11002,

		//Email
		PRISM_LOGIN_EMAIL_NOT_EXIST_USER = 25000,
		PRISM_LOGIN_EMAIL_RESTRICT_USER,
		PRISM_LOGIN_EMAIL_PW_FAIL,
		PRISM_LOGIN_EMAIL_RETRY_EXCEED,
		PRISM_LOGIN_EMAIL_UNSUPPORTED_METHOD,
		PRISM_LOGIN_EMAIL_SIGN_USER_EXIST,
		PRISM_LOGIN_EMAIL_SIGN_NVALID_PASSWORD,
		PRISM_LOGIN_EMAIL_RESET_PWD_NOT_FOUND_EMAIL,
		PRISM_LOGIN_EMAIL_CHANGE_PWD_SAME,
		PRISM_LOGIN_EMAIL_CHANGE_PWD_NOT_MATCH,

		//YouTube
		CHANNEL_YOUTUBE_NOTFOUND_404_CHANNELNOTFOUND = 28000,
		CHANNEL_YOUTUBE_INVALIDVALUE_400_INVALIDDESCRIPTION = 28001,
		CHANNEL_YOUTUBE_INVALIDVALUE_400_INVALIDLATENCYPREFERENCEOPTIONS = 28002,
		CHANNEL_YOUTUBE_FORBIDDEN_403_INVALIDTRANSITION = 28003,
		CHANNEL_YOUTUBE_LIVEBROADCASTNOTFOUND,
		CHANNEL_YOUTUBE_INSUFFICIENTPERMISSIONS_LIVEPERMISSIONBLOCKED,
		CHANNEL_YOUTUBE_INSUFFICIENTPERMISSIONS_LIVESTREAMINGNOTENABLED,
		CHANNEL_YOUTUBE_403_MADEFORKIDSMODIFICATIONNOTALLOWED,
		CHANNEL_YOUTUBE_FORBIDDEN_403_REDUNDANTTRANSITION = 28008,
		CHANNEL_YOUTUBE_VIDEONOTFOUND,
		CHANNEL_YOUTUBE_CUSTOM_REMOTEINVALID = 28010,
		CHANNEL_YOUTUBE_CUSTOM_BROADCASTTYPENOTSUPPORT = 28011,
		CHANNEL_YOUTUBE_CUSTOM_LATENCYCHANGEFAILED,
		CHANNEL_YOUTUBE_LIVESTREAMNOTFOUND,

		// NaverShoppingLive
		CHANNEL_NAVER_SHOPPING_LIVE_INVALID_SCHEDULE = 30000,
		CHANNEL_NAVER_SHOPPING_LIVE_OTHER_STORE_BAD_REQUEST,
		CHANNEL_NAVER_SHOPPING_LIVE_UNAUTHORIZED,
		CHANNEL_NAVER_SHOPPING_LIVE_TOKEN_EXPIRED,
		CHANNEL_NAVER_SHOPPING_LIVE_LIVE_COMMERCE_UNAUTHORIZED,
		CHANNEL_NAVER_SHOPPING_LIVE_CREATE_UNAUTHORIZED,
		CHANNEL_NAVER_SHOPPING_LIVE_UNAUTHORIZED_REASON_BLOCK,
		CHANNEL_NAVER_SHOPPING_LIVE_AUTH_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_AUTH_TEMPORARY_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_NOT_SUPPORT_STORE,
		CHANNEL_NAVER_SHOPPING_LIVE_BROADCAST_STARTS_EARLIER,
		CHANNEL_NAVER_SHOPPING_LIVE_INVALID_STREAM_ORIGIN_TYPE,
		CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVE_INFO_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_SCHEDULE_INFO_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_RESERVATION_IS_LIVED,
		CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_LIVEINFO_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_ADD_AGE_RESTRICT_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_UPLOAD_IMAGE_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_GET_CATEGORY_LIST_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_ATTACH_PRODUCTS_TO_OWN_MALL,
		CHANNEL_NAVER_SHOPPING_LIVE_ADD_UNATTACHABLE_PRODUCT_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTIFICATION_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_CHANGED_FAILED,
		CHANNEL_NAVER_SHOPPING_LIVE_ACCOUNT_EXPIRED,
		CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_UNKNOWN_ERROR,
		CHANNEL_NAVER_SHOPPING_LIVE_FINISHED_BY_PLATFORM,
		CHANNEL_NAVER_SHOPPING_LIVE_GET_SMART_STORE_URL_FAILED,

		//AfreecaTV
		CHANNEL_AFREECATV_LOGIN_EXPIRED = COMMON_CHANNEL_LOGIN_TOKEN_EXPIRED_ERROR,
		CHANNEL_AFREECATV_LOGIN_ERROR = COMMON_CHANNEL_LOGIN_FAIL,
		CHANNEL_AFREECATV_EMPTYCHANNEL = COMMON_CHANNEL_EMPTYCHANNEL,
		CHANNEL_AFREECATV_NETWORK_ERROR = COMMON_NETWORK_ERROR,
		CHANNEL_AFREECATV_API_EXPIRED = COMMON_TOKEN_EXPIRED_ERROR,

		//PRISM
		PRISM_API_COUNTRY_FAILED = 11501,
		PRISM_API_NO_LONGER_VALID,
		PRISM_API_INVALID_PLATFORM,
		PRISM_API_TERM_OF_AGREE,
		PRISM_API_NO_APP_UPDATE,

		//RTMP
		CHANNEL_CUSTOM_RTMP_TOKEN_EXPIRED = PRISM_API_TOKEN_EXPIRED,
		CHANNEL_CUSTOM_RTMP_RUNTIMEEXCEPTION = 56001,
		CHANNEL_CUSTOM_RTMP_NOTEXIST = 56002,
		CHANNEL_CUSTOM_RTMP_SYSTEMTIMEERROR = PRISM_API_SYSTEM_TIME_ERROR,

		//Output/Recording (57000~57999)
		PRISM_OUTPUT_BAD_PATH = 57999,       //OBS_OUTPUT_BAD_PATH (-1)
		PRISM_OUTPUT_CONNECT_FAILED = 57998, //OBS_OUTPUT_CONNECT_FAILED (-2)
		PRISM_OUTPUT_ERROR = 57996,          //OBS_OUTPUT_ERROR (-4)
		PRISM_OUTPUT_DISCONNECTED = 57995,   //OBS_OUTPUT_DISCONNECTED (-5)
		PRISM_OUTPUT_UNSUPPORTED = 57994,    //OBS_OUTPUT_UNSUPPORTED (-6)
		PRISM_OUTPUT_NO_SPACE = 57993,       //OBS_OUTPUT_NO_SPACE (-7)
		PRISM_OUTPUT_ENCODE_ERROR = 57992,   //OBS_OUTPUT_ENCODE_ERROR (-8)
		PRISM_OUTPUT_INVALID_STREAM = 57997, //OBS_OUTPUT_INVALID_STREAM (-3)
		PRISM_OUTPUT_HDR_DISABLED = 57991,   //OBS_OUTPUT_HDR_DISABLED (-9)

		//BAND
		CHANNEL_BAND_FORBIDDEN_ERROR = 33002,
		CHANNEL_BAND_NO_PERMISSION_ERROR,
		CHANNEL_BAND_ALEARD_BOARDCAST,
		CHANNEL_BAND_OTHER_ERROR,

		//NaverTV
		CHANNEL_NAVERTV_LIVENOTFOUND = 32000,
		CHANNEL_NAVERTV_PERMITEXCEPTION,
		CHANNEL_NAVERTV_LIVESTATUSEXCEPTION,
		CHANNEL_NAVERTV_START30MINEXCEPTION,
		CHANNEL_NAVERTV_ALREADYONAIR,
		CHANNEL_NAVERTV_PAIDSPONSORSHIPINFO,
		CHANNEL_NAVERTV_AUTH_FAILED,

		//B2B
		PRISM_LOGIN_NCP_B2B_SERVICE_NOT_FOUND = 19000,
		PRISM_LOGIN_NCP_B2B_SERVICE_DELETED,
		PRISM_LOGIN_NCP_B2B_USERINFO_NO_EXIT,
		CHANNEL_NCP_B2B_1022_SERVICE_DELETED = 26000,
		CHANNEL_NCP_B2B_1101_SERVICE_DISABLED = 26001,
		CHANNEL_NCP_B2B_1102_CHANNEL_DISABLED = 26002,
		CHANNEL_NCP_B2B_1103_PARTNERCHANNELDISABLEDBYTRIAL = 26003,
		CHANNEL_NCP_B2B_1104_RESOURCE_NOT_FOUND = 26004,
		CHANNEL_NCP_B2B_401_PREPARELIVE = 26005,

		//CHZZK
		CHANNEL_CHZZK_1106_AGREEMENT_REQUIRED = 31000,
		CHANNEL_CHZZK_1102_LOGIN_AGREEMENT_REQUIRED = 31001,

		//Facebook
		CHANNEL_FACEBOOK_DECLINED = 29000,
		CHANNEL_FACEBOOK_DECLINED_60DAYS,
		CHANNEL_FACEBOOK_DECLINED_100FOLLOWERS,
		CHANNEL_FACEBOOK_INVALIDACCESSTOKEN,
		CHANNEL_FACEBOOK_OBJECTNOTEXIST,

		//MQTT
		MQTT_DUPLICATED = 12000,
		MQTT_NOTICE_LONG_BROADCAST,
		MQTT_PARTNER_SERVICE_DISABLED,

	};
	Q_ENUM(ErrCode)

	enum class SearchType { apiError = 0, prismCode, errorCode, customError };

	struct ExtraData {

		ExtraData() {}

		QString platformName{};

		//when not found any matched error, will show unknown error.
		bool isShowUnknownError{true};

		//append object, which will combine with json->platform->extra->path value
		/**
		 default add key value:
		 {
		 "prismCode": 10000...
		 "prismName": COMMON_NETWORK_ERROR...
		 "platformName": Youtube, Chzzk...
		 "$statusCode": 200, only in network alert
		 "logPlatformName": = platformName
		 }
		 */
		QMap<QString, QString> pathValueMap{};
		//this list will append to alert message. join with appendJoin key.
		QStringList appendList{};
		QString appendJoin{" "};

		//if alert msg have 1%, this will use when json not contain err.arg parameter.
		QStringList defaultArg{};

		//json->platform->phase will be search with this value
		QString errPhase = PLSErrPhaseChannel;

		//log from
		QString urlKr{};
		QString urlEn{};
		//{{xxx}} will be format, will be combine with extra->logAppend
		QStringList logAppend{};

		bool printLog{true};
	};

	struct RetData {
		bool isMatched{false}; //is matched this code
		QDialogButtonBox::StandardButton clickedBtn{QDialogButtonBox::NoButton};
		ErrCode prismCode{PLSErrorHandler::ErrCode::INVALID};
		QString prismName{};                   //prism code value name
		QString alertMsg{};                    //alert msg
		QString alertMsgKey{};                 //alert key
		AlertType alertType{AlertType::Error}; //contact us or normal
		ErrorType errorType{ErrorType::Other}; //error type
		QString prismCodePrefix{};             //prism code value name prefix

		QJsonObject matchedObj{};  //json obj found
		QString failedLogString{}; //extra->logAppend formatted
		bool isNotError{false};
		ExtraData extraData{};   //extra data, with temp data
		bool isExactMatch{true}; //Find the exact error instead of default or unknown.
	};
	struct NetworkData {
		int statusCode{0};
		QNetworkReply::NetworkError netError{QNetworkReply::NetworkError::NoError};
		QByteArray errData{};
	};

	static PLSErrorHandler *instance();
	QJsonObject getRootObj() const;
	bool getTranslateString(const QString &key, QString &transVal) const;
	//for unit test, can not send analog and uistep
	bool isPrintLog() const { return m_isPrintLog; };
	void setIsPrintLog(bool isPrintLog) { m_isPrintLog = isPrintLog; };
	QJsonObject loadJson();

	static bool getUItranslation(const char *text, const char **out);

	/**
	 platformName: json first key
	 customErrName: the last compare key, default->data->api->defaultKey path
	 errPhase: the second path, YouTube->channel or YouTube->login or YouTube->live
	 defaultArg: msgkey %1, get the value
	 */
	static RetData getAlertString(const NetworkData &netData, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {});
	static RetData showAlert(const NetworkData &netData, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {}, QWidget *showParent = nullptr);
	static RetData getAlertString(const pls::http::Reply &reply, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {})
	{
		return getAlertString({reply.statusCode(), reply.error(), reply.data()}, platformName, customErrName, extraData);
	};
	static RetData showAlert(const pls::http::Reply &reply, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {}, QWidget *showParent = nullptr)
	{
		return showAlert({reply.statusCode(), reply.error(), reply.data()}, platformName, customErrName, extraData, showParent);
	};

	/**
	 customErrName: compare key, default->api->defaultKey path
	 */
	static RetData getAlertStringByCustomErrName(const QString &customErrName, const QString &platformName, const ExtraData &extraData = {});
	static RetData showAlertByCustomErrName(const QString &customErrName, const QString &platformName, const ExtraData &extraData = {}, QWidget *showParent = nullptr);

	/**
	 Streaming/Record Core Error
	 errorCode: YouTube->data->api->errorCode
	 customErrName: compare key, default->data->api->defaultKey path
	 */
	static RetData getAlertStringByErrCode(const QString &errorCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {});
	static RetData showAlertByErrCode(const QString &errorCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {}, QWidget *showParent = nullptr);

	static RetData getAlertStringByPrismCode(ErrCode prismCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {});
	static RetData showAlertByPrismCode(ErrCode prismCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData = {}, QWidget *showParent = nullptr);

	static QDialogButtonBox::StandardButton directShowAlert(RetData &data, QWidget *showParent = nullptr);

	static PLSErrorHandler::AlertType getButtonAndMsgDatas(QMap<PLSAlertView::Button, QString> &actionMap, QMap<PLSAlertView::Button, QString> &buttonsMap, RetData &data);
	static QString getButtonOpenUrl(const QString &str, const QMap<QString, QString> &pathValueMap);
	static void printLog(const RetData &retData, bool forcePrint = true);

private:
	PLSErrorHandler();
	PLSErrorHandler(const PLSErrorHandler &other) = delete;
	PLSErrorHandler &operator=(const PLSErrorHandler &other) = delete;

	static PLSErrorHandler::RetData getDataWithInheritList(const QJsonObject &rootObj, const QString &platformName, const NetworkData &netData, const QString &customErrName, ExtraData &extraData,
							       SearchType searchType);

	static QJsonObject getErrObjByPrismCode(ErrCode prismCode, const QJsonArray &jsonArray, const QString &platformName, const QString &customErrName, const ExtraData &extraData);
	static void fillUnknownDataIfEmpty(RetData &retData, ErrCode prismCode, const QString &platformName, const QString &customErrName, ExtraData &extraData, SearchType searchType);
	static QJsonObject getErrObj(const QJsonArray &jsonArray, const QString &platformName, const QString &customErrName, const ExtraData &extraData);

	static bool isDefaultKeyMatch(const QJsonObject &apiData, const QString &customErrName);

	static QJsonObject getPathValue(const QJsonObject &obj, const QString &platformName, const QJsonObject &apiJson);
	static std::pair<QString, QString> getOriginalAlertString(const QJsonObject &errorObj, const QString &msgKeyPath, const QString &msgTextPath);
	static QString dealOriginalAlertString(const QString &originalStr, const QJsonObject &errorObj, ExtraData &extraData);
	static QJsonArray getButtonsObj(const QJsonObject &errorObj);
	static AlertType isContactUsDialog(const QString &str);
	static bool dealAlertAction(const QString &str, const QMap<QString, QString> &pathMap);
	static void showAlertInMain(RetData &data, QWidget *showParent);

	static RetData fillRetDataWithMatchObj(const QJsonObject &matchedObj, const QString &platformName, ExtraData &extraData, SearchType searchType);
	static QStringList getInheritList(const QJsonObject &obj, const QString &platformName);
	static bool isValidErrPhase(const QString &jsonPhase, const QString &userPhase);
	static ExtraData generateNewOtherData(const ExtraData &extraData, const QString &platformName);
	static QString &transAndDealValStr(QString &dealStr, const QJsonObject &errorObj, const ExtraData &extraData);

	QJsonObject m_rootJson{};
	QMap<QString, QString> m_iniMap;
	bool m_isPrintLog{true};
	mutable std::shared_mutex m_mutex;
};

Q_DECLARE_METATYPE(PLSErrorHandler::RetData)
