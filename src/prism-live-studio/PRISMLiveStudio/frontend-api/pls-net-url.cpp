#include "pls-net-url.hpp"
#define PRISM_PC "prism_pc"
// Dev
// check all host Window V4.1.6 2024-04-29
// check all host MAC V1.1.1 2024-04-29
// check all host Window V4.1.8 2024-06-18
// check all host MAC V1.1.3 2024-06-18

const QString PLS_TERM_OF_USE_URL_DEV = "";
const QString PLS_PRIVACY_URL_DEV = "";

const QString PLS_PC_HMAC_KEY_DEV = "";

const QString LIBRARY_POLICY_PC_ID_DEV = "";
const QString LABORATORY_REMOTECHAT_ID_DEV = "";
const QString LIBRARY_SENSETIME_PC_ID_DEV = "";

// MQTT
const QString MQTT_SERVER_DEV = "";
const QString MQTT_SERVER_PW_DEV = "";
const QString MQTT_SERVER_WEB_DEV = "";

// Prism auth api
const QString PRISM_AUTH_API_BASE_DEV = "";

const QString PLS_FACEBOOK_LOGIN_URL_DEV = "";
const QString PLS_GOOGLE_LOGIN_URL_TOKEN_DEV = "";
const QString PLS_LINE_LOGIN_URL_DEV = "";
const QString PLS_NAVER_LOGIN_URL_DEV = "";
const QString PLS_TWITTER_LOGIN_URL_DEV = "";
const QString PLS_TWITCH_LOGIN_URL_DEV = "";
const QString PLS_WHALESPACE_LOGIN_URL_DEV = "";
const QString PLS_SNS_LOGIN_SIGNUP_URL_DEV = "";

const QString PLS_EMAIL_LOGIN_URL_DEV = "";
const QString PLS_EMAIL_SIGNUP_URL_DEV = "";
const QString PLS_EMAIL_FOGETTON_URL_DEV = "";
const QString PLS_CHANGE_PASSWORD_DEV = "";

const QString PLS_LOGOUT_URL_DEV = "";
const QString PLS_SIGNOUT_URL_DEV = "";
const QString PLS_TOKEN_SESSION_URL_DEV = "";

// rtmp
const QString PLS_RTMP_ADD_DEV = "";
const QString PLS_RTMP_MODIFY_DEV = "";
const QString PLS_RTMP_DELETE_DEV = "";
const QString PLS_RTMP_LIST_DEV = "";

const QString PLS_RTMP_ADD_V2_DEV = "";
const QString PLS_RTMP_MODIFY_V2_DEV = "";
const QString PLS_RTMP_DELETE_V2_DEV = "";
const QString PLS_RTMP_LIST_V2_DEV = "";

// prism sidekick api
const QString PLS_Sidekick_API_GATEWAY_DEV = "";
const QString PLS_NOTICE_URL_DEV = "";
const QString PLS_NOTICE_V2_URL_DEV = "";
const QString PLS_NOTICE_B2B_URL_DEV = "";
const QString CONTACT_SEND_EMAIL_URL_DEV = "";

// prism sync api
const QString PLS_SYNC_API_GATEWAY_DEV = "";
const QString PLS_CATEGORY_DEV = "";
const QString PLS_LAB_DEV = "";
const QString PLS_GPOP_CATEGORY_DEV = "";
const QString APP_INIT_URL_DEV = "";
const QString APP_UPDATE_URL_DEV = "";
const QString LASTEST_UPDATE_URL_DEV = "";

// Twitch
const QString TWITCH_CLIENT_ID_DEV = "";
const QString TWITCH_REDIRECT_URI_DEV = "";
const QString TWITCH_CLIENT_SECRET_DEV = "";

// Youtube
const QString YOUTUBE_CLIENT_ID_DEV = "";
const QString YOUTUBE_CLIENT_KEY_DEV = "";
const QString YOUTUBE_CLIENT_URL_DEV = "";

// gpop
const QString PLS_GPOP_DEV = "";

//NaverTv
const QString CHANNEL_NAVERTV_LOGIN_DEV = "";
const QString CHANNEL_NAVERTV_AUTHORIZE_DEV = "";
const QString CHANNEL_NAVERTV_TOKEN_DEV = "";
const QString CHANNEL_NAVERTV_AUTHORIZE_REDIRECT_DEV = "";
const QString CHANNEL_NAVERTV_GET_AUTH_DEV = "";
const QString CHANNEL_NAVERTV_COMMENT_OPTIONS_DEV = "";
const QString CHANNEL_NAVERTV_UPLOAD_IMAGE_DEV = "";

//Vlive
const QString CHANNEL_VLIVE_LOGIN_DEV = "";
const QString CHANNEL_VLIVE_LOGIN_JUMP_DEV = "";
const QString CHANNEL_VLIVE_LOGIN_JUMP_1_DEV = "";
const QString CHANNEL_VLIVE_SHARE_DEV = "";

//Band
const QString CHANNEL_BAND_LOGIN_DEV = "";
const QString CHANNEL_BAND_ID_DEV = "";
const QString CHANNEL_BAND_REDIRECTURL_DEV = "";
const QString CHANNEL_BAND_SECRET_DEV = "";
const QString CHANNEL_BAND_AUTH_DEV = "";
const QString CHANNEL_BAND_REFRESH_TOKEN_DEV = "";
const QString CHANNEL_BAND_USER_PROFILE_DEV = "";
const QString CHANNEL_BAND_CATEGORY_DEV = "";
const QString CHANNEL_BAND_LIVE_CREATE_DEV = "";
const QString CHANNEL_BAND_LIVE_OFF_DEV = "";
const QString CHANNEL_BAND_LIVE_STATUS_DEV = "";

//naver cloud b2b
const QString PRISM_NCP_SERVICE_ID_API_DEV = "";
const QString PRISM_NCP_AUTH_API_DEV = "";
const QString PRISM_NCP_AUTH_JOIN_API_DEV = "";
const QString PRISM_NCP_REFRESH_TOKEN_API_DEV = "";
const QString PRISM_NCP_SERVICE_CONFIG_API_DEV = "";

//Facebook
const QString CHANNEL_FACEBOOK_CLIENT_ID_DEV = "";
const QString CHANNEL_FACEBOOK_SECRET_DEV = "";
const QString FACEBOOK_LOGIN_REDIRECT_URI_DEV = "";

//Chat Widget
const QString CHAT_SOURCE_URL_DEV = "";
const QString CHATV2_SOURCE_URL_DEV = "";

//Naver Shopping Live
const QString CHANNEL_NAVER_SHOPPING_LIVE_LOGIN_DEV = "";
const QString CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN_DEV = "";
const QString CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN_DEV = "";
const QString CHANNEL_NAVER_SHOPPING_LIVE_HEADER_DEV = "";

const QString APPLE_ID_REDIRECT_URI_DEV = "";
const QString APPLE_ID_CLIENT_ID_DEV = "";

//=============================================================================================================
//=============================================================================================================
// Real

MAKE_IMPL(PLS_TERM_OF_USE_URL, "")

MAKE_IMPL(PLS_PRIVACY_URL, "")
MAKE_IMPL(PLS_PC_HMAC_KEY, "")
MAKE_IMPL(PLS_VLIVE_HMAC_KEY, "")

MAKE_IMPL(LIBRARY_POLICY_PC_ID, "")
MAKE_IMPL(LIBRARY_SENSETIME_PC_ID, "")
MAKE_IMPL(LABORATORY_REMOTECHAT_ID, "")
MAKE_IMPL(LABORATORY_NEW_BEAUTY_EFFECT_ID, "")

// MQTT
MAKE_IMPL(MQTT_SERVER, "")
MAKE_IMPL(MQTT_SERVER_PW, "")
MAKE_IMPL(MQTT_SERVER_WEB, "")

// Prism auth api
MAKE_IMPL(PRISM_AUTH_API_BASE, "")

MAKE_IMPL(PLS_FACEBOOK_LOGIN_URL, "")
MAKE_IMPL(PLS_GOOGLE_LOGIN_URL_TOKEN, "")
MAKE_IMPL(PLS_LINE_LOGIN_URL, "")
MAKE_IMPL(PLS_NAVER_LOGIN_URL, "")
MAKE_IMPL(PLS_TWITTER_LOGIN_URL, "")
MAKE_IMPL(PLS_TWITCH_LOGIN_URL, "")
MAKE_IMPL(PLS_WHALESPACE_LOGIN_URL, "")
MAKE_IMPL(PLS_SNS_LOGIN_SIGNUP_URL, "")

MAKE_IMPL(PLS_EMAIL_LOGIN_URL, "")
MAKE_IMPL(PLS_EMAIL_SIGNUP_URL, "")
MAKE_IMPL(PLS_EMAIL_FOGETTON_URL, "")
MAKE_IMPL(PLS_CHANGE_PASSWORD, "")

MAKE_IMPL(PLS_LOGOUT_URL, "")
MAKE_IMPL(PLS_SIGNOUT_URL, "")
MAKE_IMPL(PLS_TOKEN_SESSION_URL, "")

// rtmp
MAKE_IMPL(PLS_RTMP_ADD, "")
MAKE_IMPL(PLS_RTMP_MODIFY, "")
MAKE_IMPL(PLS_RTMP_DELETE, "");
MAKE_IMPL(PLS_RTMP_LIST, "");
MAKE_IMPL(PLS_RTMP_ADD_V2, "");
MAKE_IMPL(PLS_RTMP_MODIFY_V2, "");
MAKE_IMPL(PLS_RTMP_DELETE_V2, "");
MAKE_IMPL(PLS_RTMP_LIST_V2, "");

// prism sidekick api
const QString PLS_Sidekick_API_GATEWAY = "";
MAKE_IMPL(PLS_NOTICE_URL, "")
MAKE_IMPL(PLS_NOTICE_V2_URL, "")
MAKE_IMPL(PLS_NOTICE_B2B_URL, "")
MAKE_IMPL(CONTACT_SEND_EMAIL_URL, "")

// prism sync api
const QString PLS_SYNC_API_GATEWAY = "";
MAKE_IMPL(PLS_CATEGORY, "")
MAKE_IMPL(PLS_LAB, "")
MAKE_IMPL(PLS_GPOP_CATEGORY, "")
MAKE_IMPL(APP_INIT_URL, "")
MAKE_IMPL(APP_UPDATE_URL, "");
MAKE_IMPL(LASTEST_UPDATE_URL, "")

// prism action log
MAKE_IMPL(PRISM_API_ACTION, "")

// prism status log
MAKE_IMPL(PRISM_API_STATUS, "")

// prism api
MAKE_IMPL(PRISM_API_BASE, "")

// Twitch
MAKE_IMPL(TWITCH_CLIENT_ID, "")
MAKE_IMPL(TWITCH_REDIRECT_URI, "")
MAKE_IMPL(TWITCH_CLIENT_SECRET, "")

// Youtube
MAKE_IMPL(YOUTUBE_CLIENT_ID, "")
MAKE_IMPL(YOUTUBE_CLIENT_KEY, "")
MAKE_IMPL(
	YOUTUBE_CLIENT_URL,
	"")

// gpop
MAKE_IMPL(PLS_GPOP, "")

//NaverTV
MAKE_IMPL(CHANNEL_NAVERTV_LOGIN, "")
MAKE_IMPL(CHANNEL_NAVERTV_GET_USERINFO, "")
MAKE_IMPL(CHANNEL_NAVERTV_AUTHORIZE, "")
MAKE_IMPL(CHANNEL_NAVERTV_TOKEN, "")
MAKE_IMPL(CHANNEL_NAVERTV_AUTHORIZE_REDIRECT, "")
MAKE_IMPL(CHANNEL_NAVERTV_GET_AUTH, "")
MAKE_IMPL(CHANNEL_NAVERTV_COMMENT_OPTIONS, "")
MAKE_IMPL(CHANNEL_NAVERTV_UPLOAD_IMAGE, "")
MAKE_IMPL(CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL, "")
MAKE_IMPL(CHANNEL_NAVERTV_GET_LIVES, "")
MAKE_IMPL(CHANNEL_NAVERTV_GET_STREAM_INFO, "")
MAKE_IMPL(CHANNEL_NAVERTV_QUICK_START, "")
MAKE_IMPL(CHANNEL_NAVERTV_OPEN, "")
MAKE_IMPL(CHANNEL_NAVERTV_CLOSE, "")
MAKE_IMPL(CHANNEL_NAVERTV_MODIFY, "")
MAKE_IMPL(CHANNEL_NAVERTV_STATUS, "")

//VLive
MAKE_IMPL(CHANNEL_VLIVE_LOGIN, "")
MAKE_IMPL(CHANNEL_VLIVE_LOGIN_JUMP, "")
MAKE_IMPL(CHANNEL_VLIVE_LOGIN_JUMP_1, "")
MAKE_IMPL(CHANNEL_VLIVE_SHARE, "")

//Band
MAKE_IMPL(CHANNEL_BAND_LOGIN, "")
MAKE_IMPL(CHANNEL_BAND_ID, "")
MAKE_IMPL(CHANNEL_BAND_REDIRECTURL, "")
MAKE_IMPL(CHANNEL_BAND_SECRET, "")
MAKE_IMPL(CHANNEL_BAND_AUTH, "")
MAKE_IMPL(CHANNEL_BAND_REFRESH_TOKEN, "")

MAKE_IMPL(CHANNEL_BAND_USER_PROFILE, "")
MAKE_IMPL(CHANNEL_BAND_CATEGORY, "")
MAKE_IMPL(CHANNEL_BAND_LIVE_CREATE, "")
MAKE_IMPL(CHANNEL_BAND_LIVE_OFF, "")
MAKE_IMPL(CHANNEL_BAND_LIVE_STATUS, "")

//Facebook
MAKE_IMPL(CHANNEL_FACEBOOK_CLIENT_ID, "")
MAKE_IMPL(CHANNEL_FACEBOOK_REDIRECTURL, "")
MAKE_IMPL(CHANNEL_FACEBOOK_SECRET, "")
MAKE_IMPL(FACEBOOK_LOGIN_REDIRECT_URI, "")

//Chat Widget
MAKE_IMPL(CHAT_SOURCE_URL, "")
MAKE_IMPL(CHATV2_SOURCE_URL, "")

//Naver Shopping Live
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_HOST, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN,
	  "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_HEADER, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_GET_STORE_LOGIN, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_URL, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_TAG, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_STOP_LIVING, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTICE, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY, "")
MAKE_IMPL(CHANNEL_NAVER_SHOPPING_LIVE_DELETE_TOKEN, "")

//naver cloud b2b

MAKE_IMPL(PRISM_NCP_AUTH_API, "");
MAKE_IMPL(PRISM_NCP_SERVICE_ID_API, "");
MAKE_IMPL(PRISM_NCP_AUTH_JOIN_API, "");
MAKE_IMPL(PRISM_NCP_REFRESH_TOKEN_API, "");
MAKE_IMPL(PRISM_NCP_SERVICE_CONFIG_API, "");

MAKE_IMPL(APPLE_ID_REDIRECT_URI, "");
MAKE_IMPL(APPLE_ID_CLIENT_ID, "");

const int PRISM_NET_REQUEST_TIMEOUT = 15000;
const int PRISM_NET_DOWNLOAD_TIMEOUT = 30000;
const int DEFAULT_PRISM_LIVE_START_REQUEST_TIMEOUT_MS = 30000;

// youtube
const QString g_plsGoogleApiHost = "";
const QString g_plsYoutubeShareHost = "";
const QString g_plsYoutubeAPIHost = QString("").arg(g_plsGoogleApiHost);
const QString g_plsYoutubeShareUrl = QString("").arg(g_plsYoutubeShareHost);
const QString g_plsYoutubeAPIToken = QString("");
const QString g_plsYoutubeChatUrl = QString("");
const QString g_plsYoutubeStudioManagerUrl = "";
const QString g_plsYoutubeChatRemoteUrl = "";
const QString g_plsYoutubeRehearsalUrl = "";

//twitch
const QString g_plsTwitchApiHostUrl = "";
const QString g_plsTwitchChatUrl = QString("").arg(g_plsTwitchApiHostUrl);
const QString g_plsTwitchAuthTokenUrl = QStringLiteral("");
//vlive
QString const g_plsVliveSchedulePathUrl = "";
QString const g_plsVliveObjectUrl = "";

const QString g_exclusiveRtmpUrl = "";

//AfreecaTV
const QString CHANNEL_AFREECA_REDIRECTURL = "";
const QString CHANNEL_AFREECA_LOGIN = QString("").arg(CHANNEL_AFREECA_REDIRECTURL);
const QString g_plsAfreecaTVShareUrl_beforeLive = "";
const QString g_plsAfreecaTVShareUrl_living = "";
const QString g_plsAfreecaTVChannelInfo = "";
const QString g_plsAfreecaTVUserNick = "";
const QString g_plsAfreecaTVDashboard = "";
const QString g_plsAfreecaTVCategories = "";
const QString g_plsAfreecaTVUpdate = "";
const QString g_plsAfreecaTVMainHtml = "";

// chzzk
const QString g_plsChzzkApiHost = "";
const QString g_plsChzzkStudioHost = "";

const QString g_plsAppleIDAuthUrl = "";
const QString g_plsAppleIDCustomScheme = "";
const QString g_plsAppleIDCallbackUrl = g_plsAppleIDCustomScheme + "";
const QString g_plsFacebookCustomScheme = "";
const QString g_plsFacebookCallbackUrl = g_plsFacebookCustomScheme + "";

//on-boarding
const QString g_plsOnBoardingUrl = "";
