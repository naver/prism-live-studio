/*
 * @fine      NetBaseInfo
 * @brief     A brief introduction to class functionality
 * @date      2019-10-10
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef PLS_NET_URL_HPP
#define PLS_NET_URL_HPP

#include <string>
#include <QString>
#include "frontend-api-global.h"

extern const QString PLS_FACEBOOK_LOGIN_URL_DEV;
extern const QString PLS_GOOGLE_LOGIN_URL_DEV;
extern const QString PLS_GOOGLE_LOGIN_URL_TOKEN_DEV;
extern const QString PLS_LINE_LOGIN_URL_DEV;
extern const QString PLS_NAVER_LOGIN_URL_DEV;
extern const QString PLS_TWITTER_LOGIN_URL_DEV;
extern const QString PLS_TWITCH_LOGIN_URL_DEV;
extern const QString PLS_WHALESPACE_LOGIN_URL_DEV;
extern const QString PLS_SNS_LOGIN_SIGNUP_URL_DEV;
extern const QString PLS_EMAIL_LOGIN_URL_DEV;
extern const QString PLS_EMAIL_SIGNUP_URL_DEV;
extern const QString PLS_EMAIL_FOGETTON_URL_DEV;
extern const QString PLS_LOGOUT_URL_DEV;
extern const QString PLS_TERM_OF_USE_URL_DEV;
extern const QString PLS_PRIVACY_URL_DEV;
FRONTEND_API extern const QString PLS_PC_HMAC_KEY_DEV;

extern const QString PLS_TOKEN_SESSION_URL_DEV;
extern const QString PLS_NOTICE_URL_DEV;
extern const QString PLS_NOTICE_V2_URL_DEV;
extern const QString PLS_NOTICE_B2B_URL_DEV;
extern const QString PLS_SIGNOUT_URL_DEV;
extern const QString PLS_CHANGE_PASSWORD_DEV;

// Twitch
extern const QString TWITCH_CLIENT_ID_DEV;
extern const QString TWITCH_REDIRECT_URI_DEV;
extern const QString TWITCH_CLIENT_SECRET_DEV;

// Youtube
extern const QString YOUTUBE_CLIENT_ID_DEV;
extern const QString YOUTUBE_CLIENT_KEY_DEV;
extern const QString YOUTUBE_CLIENT_URL_DEV;

extern const QString PLS_CATEGORY_DEV;
extern const QString PLS_LAB_DEV;
// gpop
extern const QString PLS_GPOP_DEV;
extern const QString PLS_GPOP_CATEGORY_DEV;

// rtmp
extern const QString PLS_RTMP_ADD_DEV;
extern const QString PLS_RTMP_MODIFY_DEV;
extern const QString PLS_RTMP_DELETE_DEV;
extern const QString PLS_RTMP_LIST_DEV;
extern const QString PLS_RTMP_ADD_V2_DEV;
extern const QString PLS_RTMP_MODIFY_V2_DEV;
extern const QString PLS_RTMP_DELETE_V2_DEV;
extern const QString PLS_RTMP_LIST_V2_DEV;

extern const QString LIBRARY_POLICY_PC_ID_DEV;
extern const QString LIBRARY_SENSETIME_PC_ID_DEV;
extern const QString LABORATORY_REMOTECHAT_ID_DEV;

extern const QString MQTT_SERVER_DEV;
extern const QString MQTT_SERVER_WEB_DEV;
extern const QString MQTT_SERVER_PW_DEV;

extern const QString APP_INIT_URL_DEV;
extern const QString APP_UPDATE_URL_DEV;
extern const QString LASTEST_UPDATE_URL_DEV;
extern const QString CONTACT_SEND_EMAIL_URL_DEV;

extern const QString PRISM_AUTH_API_BASE_DEV;

extern const QString PRISM_NCP_SERVICE_ID_API_DEV;
extern const QString PRISM_NCP_AUTH_API_DEV;
extern const QString PRISM_NCP_AUTH_JOIN_API_DEV;
extern const QString PRISM_NCP_REFRESH_TOKEN_API_DEV;
extern const QString PRISM_NCP_SERVICE_CONFIG_API_DEV;

//NaverTV
extern const QString CHANNEL_NAVERTV_LOGIN_DEV;
extern const QString CHANNEL_NAVERTV_GET_AUTH_DEV;
extern const QString CHANNEL_NAVERTV_COMMENT_OPTIONS_DEV;
extern const QString CHANNEL_NAVERTV_UPLOAD_IMAGE_DEV;
extern const QString CHANNEL_NAVERTV_AUTHORIZE_DEV;
extern const QString CHANNEL_NAVERTV_AUTHORIZE_REDIRECT_DEV;
extern const QString CHANNEL_NAVERTV_TOKEN_DEV;

//Vlive
extern const QString CHANNEL_VLIVE_LOGIN_DEV;
extern const QString CHANNEL_VLIVE_LOGIN_JUMP_DEV;
extern const QString CHANNEL_VLIVE_LOGIN_JUMP_1_DEV;
extern const QString CHANNEL_VLIVE_SHARE_DEV;

//Band
extern const QString CHANNEL_BAND_LOGIN_DEV;
extern const QString CHANNEL_BAND_ID_DEV;
extern const QString CHANNEL_BAND_REDIRECTURL_DEV;
extern const QString CHANNEL_BAND_SECRET_DEV;
extern const QString CHANNEL_BAND_AUTH_DEV;
extern const QString CHANNEL_BAND_REFRESH_TOKEN_DEV;
extern const QString CHANNEL_BAND_USER_PROFILE_DEV;
extern const QString CHANNEL_BAND_CATEGORY_DEV;
extern const QString CHANNEL_BAND_LIVE_CREATE_DEV;
extern const QString CHANNEL_BAND_LIVE_OFF_DEV;
extern const QString CHANNEL_BAND_LIVE_STATUS_DEV;

//Facebook
extern const QString CHANNEL_FACEBOOK_CLIENT_ID_DEV;
extern const QString CHANNEL_FACEBOOK_SECRET_DEV;

//Chat Widget
extern const QString CHAT_SOURCE_URL_DEV;
extern const QString CHATV2_SOURCE_URL_DEV;

//Naver Shopping Live
extern const QString CHANNEL_NAVER_SHOPPING_LIVE_LOGIN_DEV;
extern const QString CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN_DEV;
extern const QString CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN_DEV;
extern const QString CHANNEL_NAVER_SHOPPING_LIVE_HEADER_DEV;

//apple id
extern const QString APPLE_ID_REDIRECT_URI_DEV;
extern const QString APPLE_ID_CLIENT_ID_DEV;

FRONTEND_API extern const int PRISM_NET_REQUEST_TIMEOUT;
FRONTEND_API extern const int PRISM_NET_DOWNLOAD_TIMEOUT;
/** Default for Policy_Publish.json "prismLiveStartRequestTimeoutMs" (ms). Used by Prism /live/start and /live/direct/v2/start. */
FRONTEND_API extern const int DEFAULT_PRISM_LIVE_START_REQUEST_TIMEOUT_MS;

const QString NEO_SESKEY = "NEO_SESKEY";

// channel rtmp default url
const QString RTMP_CHANNEL_NAVERTV_DEFAULT_URL = "";
const QString RTMP_CHANNEL_YOUTUBE_DEFAULT_URL = "rtmp://a.rtmp.youtube.com/live2";
const QString RTMP_CHANNEL_FACEBOOK_DEFAULT_URL = "rtmps://live-api-s.facebook.com:443/rtmp/";
const QString RTMP_CHANNEL_TWITCH_DEFAULT_URL = "rtmp://live-sel.twitch.tv/app/";
const QString RTMP_CHANNEL_AFREECATV_DEFAULT_URL = "rtmp://rtmpmanager-freecat.afreeca.tv/app/";

const QString CHANNEL_TWITCH_LOGIN_URL = "https://id.twitch.tv/oauth2/authorize";
const QString CHANNEL_TWITCH_URL = "https://api.twitch.tv/helix/users";
const QString CHANNEL_TWITCH_INFO_URL = "https://api.twitch.tv/helix/channels?broadcaster_id=%1";
const QString CHANNEL_TWITCH_STREAMKEY = "https://api.twitch.tv/helix/streams/key?broadcaster_id=%1";
const QString CHANNEL_TWITCH_CATEGORY = "https://api.twitch.tv/helix/search/categories?query=%1";
const QString CHANNEL_TWITCH_STATISTICS = "https://api.twitch.tv/helix/streams";
const QString CHANNEL_TWITCH_VIDEOS = "https://api.twitch.tv/helix/videos";
// Youtube
const QString CHANNEL_YOUTUBE_LOGIN_URL = "https://accounts.google.com/o/oauth2/v2/auth";
const QString YOUTUBE_REDIRECT_URI = "http://localhost";

//facebook
const QString CHANNEL_FACEBOOK_LOGIN_URL = "https://www.facebook.com/v25.0/dialog/oauth";
const auto FACEBOOK_GRAPHA_DOMAIN = QStringLiteral("https://graph.facebook.com/v25.0/");
const auto FACEBOOK_STREAMING_GRAPH_DOMAIN = QStringLiteral("https://streaming-graph.facebook.com/");
const auto FACEBOOK_AUTH_TOKEN_URL = FACEBOOK_GRAPHA_DOMAIN + QStringLiteral("oauth/access_token");
extern const QString FACEBOOK_LOGIN_REDIRECT_URI_DEV;

//twitter
const QString CHANNEL_TWITTER_COOKIE_URL = "https://www.twitter.com";
// NaverTv
const QString NAVERTV_REFERER = "http://tv.naver.com";

const auto APP_ORGANIZATION = "https://www.naver.com/";

const QString TWITCH_URL_BASE = "https://www.twitch.tv";
const QString TWITCH_API_BASE = "https://api.twitch.tv";
const QString TWITCH_API_INGESTS = "https://ingest.twitch.tv/ingests";

const QString g_GotoChzzkStudio = "https://studio.chzzk.naver.com/notification";

const QString g_dualoutputHelp = "https://guide.prismlive.com/desktop/guides/error-solution/error-information/dual-output-error-solution-guide";

const QString g_encoderErrorHelp = "https://guide.prismlive.com/desktop/guides/error-solution/others/handling-encoder-errors";

// youtube url
FRONTEND_API extern QString const g_plsGoogleApiHost;
FRONTEND_API extern QString const g_plsYoutubeShareHost;
FRONTEND_API extern QString const g_plsYoutubeAPIHost;
FRONTEND_API extern QString const g_plsYoutubeChatUrl;
FRONTEND_API extern QString const g_plsYoutubeShareUrl;
FRONTEND_API extern QString const g_plsYoutubeAPIToken;
FRONTEND_API extern QString const g_plsYoutubeStudioManagerUrl;
FRONTEND_API extern QString const g_plsYoutubeChatRemoteUrl;
FRONTEND_API extern QString const g_plsYoutubeRehearsalUrl;

//twitch url
FRONTEND_API extern QString const g_plsTwitchApiHostUrl;
FRONTEND_API extern QString const g_plsTwitchChatUrl;
FRONTEND_API extern QString const g_plsTwitchAuthTokenUrl;

//vlive url
FRONTEND_API extern QString const g_plsVliveSchedulePathUrl;
FRONTEND_API extern QString const g_plsVliveObjectUrl;

//streamkey url
const QString g_facebookStreamKeyUrl = "https://www.facebook.com";
const QString g_twitterStreamKeyUrl = "https://twitter.com";
const QString g_twitchStreamkeyUrlWithAccount = "https://dashboard.twitch.tv/u/%1/settings/channel";
const QString g_twitchStreamkeyUrl = "https://dashboard.twitch.tv/";
const QString g_twitchHomePage = "https://dashboard.twitch.tv/u/%1/home";
const QString g_youtubeStreamkeyUrl = "https://www.youtube.com/live_dashboard?nv=1";

//share url
const QString g_facebookShareUrl = "http://www.facebook.com/sharer/sharer.php?u=%1";
const QString g_twitterShareUrl = "https://twitter.com/intent/tweet?text=%1&url=%2";

const QString g_youtubeUrl = "https://www.youtube.com";
const QString g_youtubeV3 = "https://www.googleapis.com/youtube/v3";
const QString g_youtubeBroadcast = "https://www.googleapis.com/youtube/v3/liveBroadcasts";
const QString g_youtubeChannels = "https://www.googleapis.com/youtube/v3/channels";
const QString g_yoububeLivePage = "https://www.youtube.com/live_dashboard";
const QString g_yoububeStudioManagePage = "https://studio.youtube.com/channel/%1/livestreaming/manage";

const QString g_userGuide = "https://guide.prismlive.com/";
const QString g_streamKeyPrismHelper = "https://guide.prismlive.com/desktop/guides/streaming/rtmp-streaming/how-to-check-the-stream-key-of-an-rtmp-server";

FRONTEND_API extern const QString g_exclusiveRtmpUrl;

//AfreecaTV
FRONTEND_API extern const QString CHANNEL_AFREECA_REDIRECTURL;
FRONTEND_API extern const QString CHANNEL_AFREECA_LOGIN;
FRONTEND_API extern const QString g_plsAfreecaTVShareUrl_beforeLive;
FRONTEND_API extern const QString g_plsAfreecaTVShareUrl_living;
FRONTEND_API extern const QString g_plsAfreecaTVChannelInfo;
FRONTEND_API extern const QString g_plsAfreecaTVUserNick;
FRONTEND_API extern const QString g_plsAfreecaTVDashboard;
FRONTEND_API extern const QString g_plsAfreecaTVCategories;
FRONTEND_API extern const QString g_plsAfreecaTVUpdate;
FRONTEND_API extern const QString g_plsAfreecaTVLiveID;
FRONTEND_API extern const QString g_plsAfreecaTVMainHtml;

// chzzk
FRONTEND_API extern const QString g_plsChzzkApiHost;
FRONTEND_API extern const QString g_plsChzzkStudioHost;

//appleID
FRONTEND_API extern const QString g_plsAppleIDAuthUrl;
FRONTEND_API extern const QString g_plsAppleIDCallbackUrl;
FRONTEND_API extern const QString g_plsAppleIDCustomScheme;
FRONTEND_API extern const QString g_plsFacebookCallbackUrl;
FRONTEND_API extern const QString g_plsFacebookCustomScheme;
FRONTEND_API extern const QString g_plsOnBoardingUrl;

//
//step 1 make value in class PLSNetUrlValues
//step 2 define macro under class
//step 3 make impl in cpp
//step 4 delete define before in this hpp
//use this in hpp
#define MAKE_VALUE(name) static QString name##_ACT;

//use this in cpp
#define MAKE_IMPL(name, value) QString PLSNetUrlValues::name##_ACT = value;

//step 1
class FRONTEND_API PLSNetUrlValues {
public:
	MAKE_VALUE(PLS_PRIVACY_URL)
	MAKE_VALUE(PLS_PC_HMAC_KEY)
	MAKE_VALUE(PLS_WHALESPACE_LOGIN_URL)

		//rtmp urls
		MAKE_VALUE(PLS_RTMP_ADD) MAKE_VALUE(PLS_RTMP_MODIFY) MAKE_VALUE(PLS_RTMP_DELETE) MAKE_VALUE(PLS_RTMP_LIST) MAKE_VALUE(PLS_RTMP_ADD_V2) MAKE_VALUE(PLS_RTMP_MODIFY_V2)
			MAKE_VALUE(PLS_RTMP_DELETE_V2) MAKE_VALUE(PLS_RTMP_LIST_V2)

		//Naver Shopping Live
		MAKE_VALUE(CHANNEL_NAVER_SHOPPING_HOST)
#define CHANNEL_NAVER_SHOPPING_HOST PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_HOST_ACT
			MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN)
#define CHANNEL_NAVER_SHOPPING_LIVE_LOGIN PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_LOGIN_ACT
				MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN)
#define CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN_ACT
					MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN)
#define CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN_ACT
						MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
#define CHANNEL_NAVER_SHOPPING_LIVE_HEADER PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_HEADER_ACT
							MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_GET_STORE_LOGIN)
#define CHANNEL_NAVER_SHOPPING_LIVE_GET_STORE_LOGIN PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_GET_STORE_LOGIN_ACT
								MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH)
#define CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH_ACT
									MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN)
#define CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN_ACT
										MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_URL)
#define CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_URL PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_URL_ACT
											MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_TAG)
#define CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_TAG PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_TAG_ACT
												MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS)
#define CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS_ACT
													MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING)
#define CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING_ACT
														MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING)
#define CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING_ACT
															MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_STOP_LIVING)
#define CHANNEL_NAVER_SHOPPING_LIVE_STOP_LIVING PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_STOP_LIVING_ACT
																MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING)
#define CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING_ACT
																	MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST)
#define CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST_ACT
																		MAKE_VALUE(CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST)
#define CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST_ACT
																			MAKE_VALUE(
																				CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION)
#define CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION_ACT
																				MAKE_VALUE(
																					CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTICE)
#define CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTICE PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTICE_ACT
																					MAKE_VALUE(
																						CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO)
#define CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO_ACT
																						MAKE_VALUE(
																							CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY)
#define CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY_ACT
																							MAKE_VALUE(
																								CHANNEL_NAVER_SHOPPING_LIVE_DELETE_TOKEN)
#define CHANNEL_NAVER_SHOPPING_LIVE_DELETE_TOKEN PLSNetUrlValues::CHANNEL_NAVER_SHOPPING_LIVE_DELETE_TOKEN_ACT

																								MAKE_VALUE(
																									PLS_FACEBOOK_LOGIN_URL)
#define PLS_FACEBOOK_LOGIN_URL PLSNetUrlValues::PLS_FACEBOOK_LOGIN_URL_ACT
																									MAKE_VALUE(
																										PLS_GOOGLE_LOGIN_URL)
#define PLS_GOOGLE_LOGIN_URL PLSNetUrlValues::PLS_GOOGLE_LOGIN_URL_ACT
																										MAKE_VALUE(
																											PLS_GOOGLE_LOGIN_URL_TOKEN)
#define PLS_GOOGLE_LOGIN_URL_TOKEN PLSNetUrlValues::PLS_GOOGLE_LOGIN_URL_TOKEN_ACT
																											MAKE_VALUE(
																												PLS_LINE_LOGIN_URL)
#define PLS_LINE_LOGIN_URL PLSNetUrlValues::PLS_LINE_LOGIN_URL_ACT
																												MAKE_VALUE(
																													PLS_NAVER_LOGIN_URL)
#define PLS_NAVER_LOGIN_URL PLSNetUrlValues::PLS_NAVER_LOGIN_URL_ACT
																													MAKE_VALUE(
																														PLS_TWITTER_LOGIN_URL)
#define PLS_TWITTER_LOGIN_URL PLSNetUrlValues::PLS_TWITTER_LOGIN_URL_ACT
																														MAKE_VALUE(
																															PLS_TWITCH_LOGIN_URL)
#define PLS_TWITCH_LOGIN_URL PLSNetUrlValues::PLS_TWITCH_LOGIN_URL_ACT

																															MAKE_VALUE(
																																PLS_SNS_LOGIN_SIGNUP_URL)
#define PLS_SNS_LOGIN_SIGNUP_URL PLSNetUrlValues::PLS_SNS_LOGIN_SIGNUP_URL_ACT
																																MAKE_VALUE(
																																	PLS_EMAIL_LOGIN_URL)
#define PLS_EMAIL_LOGIN_URL PLSNetUrlValues::PLS_EMAIL_LOGIN_URL_ACT
																																	MAKE_VALUE(
																																		PLS_EMAIL_SIGNUP_URL)
#define PLS_EMAIL_SIGNUP_URL PLSNetUrlValues::PLS_EMAIL_SIGNUP_URL_ACT
																																		MAKE_VALUE(
																																			PLS_EMAIL_FOGETTON_URL)
#define PLS_EMAIL_FOGETTON_URL PLSNetUrlValues::PLS_EMAIL_FOGETTON_URL_ACT
																																			MAKE_VALUE(
																																				PLS_TERM_OF_USE_URL)
#define PLS_TERM_OF_USE_URL PLSNetUrlValues::PLS_TERM_OF_USE_URL_ACT

																																				MAKE_VALUE(
																																					PLS_VLIVE_HMAC_KEY)
#define PLS_VLIVE_HMAC_KEY PLSNetUrlValues::PLS_VLIVE_HMAC_KEY_ACT
																																					MAKE_VALUE(
																																						PLS_NAVERSHOPPING_HMAC_KEY)
#define PLS_NAVERSHOPPING_HMAC_KEY PLSNetUrlValues::PLS_NAVERSHOPPING_HMAC_KEY_ACT
																																						MAKE_VALUE(
																																							PLS_LOGOUT_URL)
#define PLS_LOGOUT_URL PLSNetUrlValues::PLS_LOGOUT_URL_ACT
																																							MAKE_VALUE(
																																								PLS_TOKEN_SESSION_URL)
#define PLS_TOKEN_SESSION_URL PLSNetUrlValues::PLS_TOKEN_SESSION_URL_ACT
																																								MAKE_VALUE(
																																									PLS_NOTICE_URL)
#define PLS_NOTICE_URL PLSNetUrlValues::PLS_NOTICE_URL_ACT
																																									MAKE_VALUE(
																																										PLS_NOTICE_V2_URL)
#define PLS_NOTICE_V2_URL PLSNetUrlValues::PLS_NOTICE_V2_URL_ACT
																																										MAKE_VALUE(
																																											PLS_NOTICE_B2B_URL)
#define PLS_NOTICE_B2B_URL PLSNetUrlValues::PLS_NOTICE_B2B_URL_ACT
																																										MAKE_VALUE(
																																											PLS_SIGNOUT_URL)
#define PLS_SIGNOUT_URL PLSNetUrlValues::PLS_SIGNOUT_URL_ACT
																																										MAKE_VALUE(
																																											PLS_CHANGE_PASSWORD)
#define PLS_CHANGE_PASSWORD PLSNetUrlValues::PLS_CHANGE_PASSWORD_ACT

		// Twitch
		MAKE_VALUE(TWITCH_CLIENT_ID)
#define TWITCH_CLIENT_ID PLSNetUrlValues::TWITCH_CLIENT_ID_ACT
			MAKE_VALUE(TWITCH_REDIRECT_URI)
#define TWITCH_REDIRECT_URI PLSNetUrlValues::TWITCH_REDIRECT_URI_ACT
				MAKE_VALUE(TWITCH_CLIENT_SECRET)
#define TWITCH_CLIENT_SECRET PLSNetUrlValues::TWITCH_CLIENT_SECRET_ACT

		// Youtube
		MAKE_VALUE(YOUTUBE_CLIENT_ID)
#define YOUTUBE_CLIENT_ID PLSNetUrlValues::YOUTUBE_CLIENT_ID_ACT
			MAKE_VALUE(YOUTUBE_CLIENT_KEY)
#define YOUTUBE_CLIENT_KEY PLSNetUrlValues::YOUTUBE_CLIENT_KEY_ACT
				MAKE_VALUE(YOUTUBE_CLIENT_URL)
#define YOUTUBE_CLIENT_URL PLSNetUrlValues::YOUTUBE_CLIENT_URL_ACT

					MAKE_VALUE(PLS_CATEGORY)
#define PLS_CATEGORY PLSNetUrlValues::PLS_CATEGORY_ACT

						MAKE_VALUE(PLS_LAB)
#define PLS_LAB PLSNetUrlValues::PLS_LAB_ACT
		// gpop
		MAKE_VALUE(PLS_GPOP)
#define PLS_GPOP PLSNetUrlValues::PLS_GPOP_ACT
			MAKE_VALUE(PLS_GPOP_CATEGORY)
#define PLS_GPOP_CATEGORY PLSNetUrlValues::PLS_GPOP_CATEGORY_ACT

				MAKE_VALUE(LIBRARY_POLICY_PC_ID)
#define LIBRARY_POLICY_PC_ID PLSNetUrlValues::LIBRARY_POLICY_PC_ID_ACT

					MAKE_VALUE(LABORATORY_REMOTECHAT_ID)
#define LABORATORY_REMOTECHAT_ID PLSNetUrlValues::LABORATORY_REMOTECHAT_ID_ACT

						MAKE_VALUE(LABORATORY_NEW_BEAUTY_EFFECT_ID)
#define LABORATORY_NEW_BEAUTY_EFFECT_ID PLSNetUrlValues::LABORATORY_NEW_BEAUTY_EFFECT_ID_ACT

							MAKE_VALUE(LIBRARY_SENSETIME_PC_ID)
#define LIBRARY_SENSETIME_PC_ID PLSNetUrlValues::LIBRARY_SENSETIME_PC_ID_ACT

								MAKE_VALUE(MQTT_SERVER)
#define MQTT_SERVER PLSNetUrlValues::MQTT_SERVER_ACT
									MAKE_VALUE(MQTT_SERVER_PW)
#define MQTT_SERVER_PW PLSNetUrlValues::MQTT_SERVER_PW_ACT
										MAKE_VALUE(MQTT_SERVER_WEB)
#define MQTT_SERVER_WEB PLSNetUrlValues::MQTT_SERVER_WEB_ACT

											MAKE_VALUE(APP_INIT_URL)
#define APP_INIT_URL PLSNetUrlValues::APP_INIT_URL_ACT
												MAKE_VALUE(APP_UPDATE_URL)
#define APP_UPDATE_URL PLSNetUrlValues::APP_UPDATE_URL_ACT
													MAKE_VALUE(LASTEST_UPDATE_URL)
#define LASTEST_UPDATE_URL PLSNetUrlValues::LASTEST_UPDATE_URL_ACT
														MAKE_VALUE(CONTACT_SEND_EMAIL_URL)
#define CONTACT_SEND_EMAIL_URL PLSNetUrlValues::CONTACT_SEND_EMAIL_URL_ACT

															MAKE_VALUE(PRISM_API_BASE)
#define PRISM_API_BASE PLSNetUrlValues::PRISM_API_BASE_ACT

																MAKE_VALUE(PRISM_NCP_AUTH_API)
#define PRISM_NCP_AUTH_API PLSNetUrlValues::PRISM_NCP_AUTH_API_ACT
																	MAKE_VALUE(PRISM_NCP_SERVICE_ID_API)
#define PRISM_NCP_SERVICE_ID_API PLSNetUrlValues::PRISM_NCP_SERVICE_ID_API_ACT
																		MAKE_VALUE(PRISM_NCP_AUTH_JOIN_API)
#define PRISM_NCP_AUTH_JOIN_API PLSNetUrlValues::PRISM_NCP_AUTH_JOIN_API_ACT
																			MAKE_VALUE(PRISM_NCP_REFRESH_TOKEN_API)
#define PRISM_NCP_REFRESH_TOKEN_API PLSNetUrlValues::PRISM_NCP_REFRESH_TOKEN_API_ACT
																				MAKE_VALUE(PRISM_NCP_SERVICE_CONFIG_API)
#define PRISM_NCP_SERVICE_CONFIG_API PLSNetUrlValues::PRISM_NCP_SERVICE_CONFIG_API_ACT

																					MAKE_VALUE(PRISM_AUTH_API_BASE)
#define PRISM_AUTH_API_BASE PLSNetUrlValues::PRISM_AUTH_API_BASE_ACT
																						MAKE_VALUE(
																							PRISM_API_ACTION)
#define PRISM_API_ACTION PLSNetUrlValues::PRISM_API_ACTION_ACT
																							MAKE_VALUE(
																								PRISM_API_STATUS)
#define PRISM_API_STATUS PLSNetUrlValues::PRISM_API_STATUS_ACT

		//NaverTV
		MAKE_VALUE(CHANNEL_NAVERTV_LOGIN)
#define CHANNEL_NAVERTV_LOGIN PLSNetUrlValues::CHANNEL_NAVERTV_LOGIN_ACT
			MAKE_VALUE(CHANNEL_NAVERTV_GET_AUTH)
#define CHANNEL_NAVERTV_GET_AUTH PLSNetUrlValues::CHANNEL_NAVERTV_GET_AUTH_ACT
				MAKE_VALUE(CHANNEL_NAVERTV_GET_USERINFO)
#define CHANNEL_NAVERTV_GET_USERINFO PLSNetUrlValues::CHANNEL_NAVERTV_GET_USERINFO_ACT
					MAKE_VALUE(CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL)
#define CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL PLSNetUrlValues::CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL_ACT
						MAKE_VALUE(CHANNEL_NAVERTV_GET_LIVES)
#define CHANNEL_NAVERTV_GET_LIVES PLSNetUrlValues::CHANNEL_NAVERTV_GET_LIVES_ACT
							MAKE_VALUE(CHANNEL_NAVERTV_GET_STREAM_INFO)
#define CHANNEL_NAVERTV_GET_STREAM_INFO PLSNetUrlValues::CHANNEL_NAVERTV_GET_STREAM_INFO_ACT
								MAKE_VALUE(CHANNEL_NAVERTV_QUICK_START)
#define CHANNEL_NAVERTV_QUICK_START PLSNetUrlValues::CHANNEL_NAVERTV_QUICK_START_ACT
									MAKE_VALUE(CHANNEL_NAVERTV_OPEN)
#define CHANNEL_NAVERTV_OPEN PLSNetUrlValues::CHANNEL_NAVERTV_OPEN_ACT
										MAKE_VALUE(CHANNEL_NAVERTV_CLOSE)
#define CHANNEL_NAVERTV_CLOSE PLSNetUrlValues::CHANNEL_NAVERTV_CLOSE_ACT
											MAKE_VALUE(CHANNEL_NAVERTV_MODIFY)
#define CHANNEL_NAVERTV_MODIFY PLSNetUrlValues::CHANNEL_NAVERTV_MODIFY_ACT
												MAKE_VALUE(CHANNEL_NAVERTV_COMMENT_OPTIONS)
#define CHANNEL_NAVERTV_COMMENT_OPTIONS PLSNetUrlValues::CHANNEL_NAVERTV_COMMENT_OPTIONS_ACT
													MAKE_VALUE(CHANNEL_NAVERTV_UPLOAD_IMAGE)
#define CHANNEL_NAVERTV_UPLOAD_IMAGE PLSNetUrlValues::CHANNEL_NAVERTV_UPLOAD_IMAGE_ACT
														MAKE_VALUE(CHANNEL_NAVERTV_STATUS)
#define CHANNEL_NAVERTV_STATUS PLSNetUrlValues::CHANNEL_NAVERTV_STATUS_ACT
															MAKE_VALUE(CHANNEL_NAVERTV_AUTHORIZE)
#define CHANNEL_NAVERTV_AUTHORIZE PLSNetUrlValues::CHANNEL_NAVERTV_AUTHORIZE_ACT
																MAKE_VALUE(CHANNEL_NAVERTV_AUTHORIZE_REDIRECT)
#define CHANNEL_NAVERTV_AUTHORIZE_REDIRECT PLSNetUrlValues::CHANNEL_NAVERTV_AUTHORIZE_REDIRECT_ACT
																	MAKE_VALUE(CHANNEL_NAVERTV_TOKEN)
#define CHANNEL_NAVERTV_TOKEN PLSNetUrlValues::CHANNEL_NAVERTV_TOKEN_ACT

		//Vlive
		MAKE_VALUE(CHANNEL_VLIVE_LOGIN)
#define CHANNEL_VLIVE_LOGIN PLSNetUrlValues::CHANNEL_VLIVE_LOGIN_ACT
			MAKE_VALUE(CHANNEL_VLIVE_LOGIN_JUMP)
#define CHANNEL_VLIVE_LOGIN_JUMP PLSNetUrlValues::CHANNEL_VLIVE_LOGIN_JUMP_ACT
				MAKE_VALUE(CHANNEL_VLIVE_LOGIN_JUMP_1)
#define CHANNEL_VLIVE_LOGIN_JUMP_1 PLSNetUrlValues::CHANNEL_VLIVE_LOGIN_JUMP_1_ACT
					MAKE_VALUE(CHANNEL_VLIVE_SHARE)
#define CHANNEL_VLIVE_SHARE PLSNetUrlValues::CHANNEL_VLIVE_SHARE_ACT

		//Band
		MAKE_VALUE(CHANNEL_BAND_LOGIN)
#define CHANNEL_BAND_LOGIN PLSNetUrlValues::CHANNEL_BAND_LOGIN_ACT
			MAKE_VALUE(CHANNEL_BAND_ID)
#define CHANNEL_BAND_ID PLSNetUrlValues::CHANNEL_BAND_ID_ACT
				MAKE_VALUE(CHANNEL_BAND_REDIRECTURL)
#define CHANNEL_BAND_REDIRECTURL PLSNetUrlValues::CHANNEL_BAND_REDIRECTURL_ACT
					MAKE_VALUE(CHANNEL_BAND_SECRET)
#define CHANNEL_BAND_SECRET PLSNetUrlValues::CHANNEL_BAND_SECRET_ACT
						MAKE_VALUE(CHANNEL_BAND_AUTH)
#define CHANNEL_BAND_AUTH PLSNetUrlValues::CHANNEL_BAND_AUTH_ACT
							MAKE_VALUE(CHANNEL_BAND_REFRESH_TOKEN)
#define CHANNEL_BAND_REFRESH_TOKEN PLSNetUrlValues::CHANNEL_BAND_REFRESH_TOKEN_ACT
								MAKE_VALUE(CHANNEL_BAND_USER_PROFILE)
#define CHANNEL_BAND_USER_PROFILE PLSNetUrlValues::CHANNEL_BAND_USER_PROFILE_ACT
									MAKE_VALUE(CHANNEL_BAND_CATEGORY)
#define CHANNEL_BAND_CATEGORY PLSNetUrlValues::CHANNEL_BAND_CATEGORY_ACT
										MAKE_VALUE(CHANNEL_BAND_LIVE_CREATE)
#define CHANNEL_BAND_LIVE_CREATE PLSNetUrlValues::CHANNEL_BAND_LIVE_CREATE_ACT
											MAKE_VALUE(CHANNEL_BAND_LIVE_OFF)
#define CHANNEL_BAND_LIVE_OFF PLSNetUrlValues::CHANNEL_BAND_LIVE_OFF_ACT
												MAKE_VALUE(CHANNEL_BAND_LIVE_STATUS)
#define CHANNEL_BAND_LIVE_STATUS PLSNetUrlValues::CHANNEL_BAND_LIVE_STATUS_ACT

		//FACEBOOK
		MAKE_VALUE(CHANNEL_FACEBOOK_CLIENT_ID)
#define CHANNEL_FACEBOOK_CLIENT_ID PLSNetUrlValues::CHANNEL_FACEBOOK_CLIENT_ID_ACT
			MAKE_VALUE(CHANNEL_FACEBOOK_REDIRECTURL)
#define CHANNEL_FACEBOOK_REDIRECTURL PLSNetUrlValues::CHANNEL_FACEBOOK_REDIRECTURL_ACT
				MAKE_VALUE(CHANNEL_FACEBOOK_SECRET)
#define CHANNEL_FACEBOOK_SECRET PLSNetUrlValues::CHANNEL_FACEBOOK_SECRET_ACT
					MAKE_VALUE(FACEBOOK_LOGIN_REDIRECT_URI)
#define FACEBOOK_LOGIN_REDIRECT_URI PLSNetUrlValues::FACEBOOK_LOGIN_REDIRECT_URI_ACT

		//Chat Widget
		MAKE_VALUE(CHAT_SOURCE_URL)
#define CHAT_SOURCE_URL PLSNetUrlValues::CHAT_SOURCE_URL_ACT
			MAKE_VALUE(CHATV2_SOURCE_URL)
#define CHATV2_SOURCE_URL PLSNetUrlValues::CHATV2_SOURCE_URL_ACT
		//apple id
		MAKE_VALUE(APPLE_ID_REDIRECT_URI)
#define APPLE_ID_REDIRECT_URI PLSNetUrlValues::APPLE_ID_REDIRECT_URI_ACT
			MAKE_VALUE(APPLE_ID_CLIENT_ID)
#define APPLE_ID_CLIENT_ID PLSNetUrlValues::APPLE_ID_CLIENT_ID_ACT
};

//step 2
#define PLS_PRIVACY_URL PLSNetUrlValues::PLS_PRIVACY_URL_ACT
#define PLS_PC_HMAC_KEY PLSNetUrlValues::PLS_PC_HMAC_KEY_ACT
#define PLS_WHALESPACE_LOGIN_URL PLSNetUrlValues::PLS_WHALESPACE_LOGIN_URL_ACT
#define PLS_RTMP_ADD PLSNetUrlValues::PLS_RTMP_ADD_ACT
#define PLS_RTMP_MODIFY PLSNetUrlValues::PLS_RTMP_MODIFY_ACT
#define PLS_RTMP_DELETE PLSNetUrlValues::PLS_RTMP_DELETE_ACT
#define PLS_RTMP_LIST PLSNetUrlValues::PLS_RTMP_LIST_ACT
#define PLS_RTMP_ADD_V2 PLSNetUrlValues::PLS_RTMP_ADD_V2_ACT
#define PLS_RTMP_MODIFY_V2 PLSNetUrlValues::PLS_RTMP_MODIFY_V2_ACT
#define PLS_RTMP_DELETE_V2 PLSNetUrlValues::PLS_RTMP_DELETE_V2_ACT
#define PLS_RTMP_LIST_V2 PLSNetUrlValues::PLS_RTMP_LIST_V2_ACT

#endif // PLS_NET_URL_HPP
