#include "../pls-net-url.hpp"

// Dev


const QString PLS_TERM_OF_USE_URL_DEV = "";
const QString PLS_PRIVACY_URL_DEV = "";

const QString PLS_HMAC_KEY_DEV = "";

const QString g_streamKeyPrismHelperEn_DEV = "";
const QString g_streamKeyPrismHelperKr_DEV = "";

const QString LIBRARY_POLICY_PC_ID_DEV = "LIBRARY_1452";

// MQTT
const QString MQTT_SERVER_DEV = "";
const QString MQTT_SERVER_PW_DEV = "client";
const QString MQTT_SERVER_WEB_DEV = "dev";

// Prism auth api
const QString PRISM_AUTH_API_BASE_DEV = "";

const QString PLS_FACEBOOK_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_GOOGLE_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_LINE_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_NAVER_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_TWITTER_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_TWITCH_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_SNS_LOGIN_SIGNUP_URL_DEV = PRISM_AUTH_API_BASE_DEV ;

const QString PLS_EMAIL_LOGIN_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_EMAIL_SIGNUP_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_EMAIL_FOGETTON_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_CHANGE_PASSWORD_DEV = PRISM_AUTH_API_BASE_DEV ;

const QString PLS_LOGOUT_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_SIGNOUT_URL_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_TOKEN_SESSION_URL_DEV = PRISM_AUTH_API_BASE_DEV ;

// rtmp
const QString PLS_RTMP_ADD_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_RTMP_MODIFY_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_RTMP_DELETE_DEV = PRISM_AUTH_API_BASE_DEV ;
const QString PLS_RTMP_LIST_DEV = PRISM_AUTH_API_BASE_DEV ;

// prism sidekick api
const QString PLS_Sidekick_API_GATEWAY_DEV = "";
const QString PLS_NOTICE_URL_DEV = PLS_Sidekick_API_GATEWAY_DEV ;
const QString CONTACT_SEND_EMAIL_URL_DEV = PLS_Sidekick_API_GATEWAY_DEV ;

// prism sync api
const QString PLS_SYNC_API_GATEWAY_DEV = "";
const QString PLS_CATEGORY_DEV = PLS_SYNC_API_GATEWAY_DEV ;


// prism action log
const QString PRISM_API_ACTION_DEV = "";

// prism api
const QString PRISM_API_BASE_DEV = "";

// Twitch
const QString TWITCH_CLIENT_ID_DEV = "";
const QString TWITCH_REDIRECT_URI_DEV = "";

// Youtube
const QString YOUTUBE_CLIENT_ID_DEV = "";
const QString YOUTUBE_CLIENT_KEY_DEV = "";
const QString YOUTUBE_CLIENT_URL_DEV =
	"https://accounts.google.com/o/oauth2/v2/auth?client_id=";

// gpop
const QString PLS_GPOP_DEV = "";

//=============================================================================================================
//=============================================================================================================
// Real

QString PLS_TERM_OF_USE_URL = "";
QString PLS_PRIVACY_URL = "";

QString PLS_HMAC_KEY = "";

QString g_streamKeyPrismHelperEn = "";
QString g_streamKeyPrismHelperKr = "";

QString LIBRARY_POLICY_PC_ID = "LIBRARY_1225";

// MQTT
QString MQTT_SERVER = "";
QString MQTT_SERVER_PW = "";
QString MQTT_SERVER_WEB = "real";

// Prism auth api
QString PRISM_AUTH_API_BASE = "";

QString PLS_FACEBOOK_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_GOOGLE_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_LINE_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_NAVER_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_TWITTER_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_TWITCH_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_SNS_LOGIN_SIGNUP_URL = PRISM_AUTH_API_BASE ;

QString PLS_EMAIL_LOGIN_URL = PRISM_AUTH_API_BASE ;
QString PLS_EMAIL_SIGNUP_URL = PRISM_AUTH_API_BASE ;
QString PLS_EMAIL_FOGETTON_URL = PRISM_AUTH_API_BASE ;
QString PLS_CHANGE_PASSWORD = PRISM_AUTH_API_BASE ;

QString PLS_LOGOUT_URL = PRISM_AUTH_API_BASE ;
QString PLS_SIGNOUT_URL = PRISM_AUTH_API_BASE ;
QString PLS_TOKEN_SESSION_URL = PRISM_AUTH_API_BASE ;

// rtmp
QString PLS_RTMP_ADD = PRISM_AUTH_API_BASE ;
QString PLS_RTMP_MODIFY = PRISM_AUTH_API_BASE ;
QString PLS_RTMP_DELETE = PRISM_AUTH_API_BASE ;
QString PLS_RTMP_LIST = PRISM_AUTH_API_BASE ;

// prism sidekick api
QString PLS_Sidekick_API_GATEWAY = "";
QString PLS_NOTICE_URL = PLS_Sidekick_API_GATEWAY ;
QString CONTACT_SEND_EMAIL_URL = PLS_Sidekick_API_GATEWAY ;

// prism sync api
QString PLS_SYNC_API_GATEWAY = "";
QString PLS_CATEGORY = PLS_SYNC_API_GATEWAY ;


// prism action log
QString PRISM_API_ACTION = "";

// prism api
QString PRISM_API_BASE = "";

// Twitch
QString TWITCH_CLIENT_ID = "";
QString TWITCH_REDIRECT_URI = "";

// Youtube
QString YOUTUBE_CLIENT_ID = "";
QString YOUTUBE_CLIENT_KEY = "";
QString YOUTUBE_CLIENT_URL =
	"https://accounts.google.com/o/oauth2/v2/auth?client_id=";

// gpop
QString PLS_GPOP = "";

int PRISM_NET_REQUEST_TIMEOUT = 10000;

// youtube
const QString g_plsGoogleApiHost = "https://www.googleapis.com";
const QString g_plsYoutubeShareHost = "https://www.youtube.com";
const QString g_plsYoutubeAPIHost = QString("%1/youtube/v3").arg(g_plsGoogleApiHost);
const QString g_plsYoutubeShareUrl = QString("%1/watch?v=%2").arg(g_plsYoutubeShareHost);
const QString g_plsYoutubeAPIHostV4 = QString("%1/oauth2/v4").arg(g_plsGoogleApiHost);
const QString g_plsYoutubeChatPre = QString("%1/signin?action_handle_signin=true&next=https%3A%2F%2Fwww.youtube.com%2Flive_chat%3Fis_popout%3D1%26v%3D").arg(g_plsYoutubeShareHost);
const QString g_plsYoutubeChatParmater = QString("%3%1&authuser=0&skip_identity_prompt=True&feature=identity_prompt&pageId=%2");

//twitch
const QString g_plsTwitchApiHostUrl = "https://www.twitch.tv";
const QString g_plsTwitchChatUrl = QString("%1/popout/%2/chat?darkpopout").arg(g_plsTwitchApiHostUrl);
