#ifndef CHANNEL_CONST_H
#define CHANNEL_CONST_H

#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include "libui-globals.h"

namespace channel_data {

#ifndef ChannelData
#define ChannelData channel_data
#endif // !ChannelData

/*key to be used in channel data map*/

LIBUI_API extern const QString g_channelUUID;
const QString g_channelName = "channel_name";          //ex. twitch navertv youtube ..
const QString g_fixPlatformName = "fix_platform_name"; //ex. NCB2B
LIBUI_API extern const QString g_displayPlatformName;

//just for youtube
const QString g_youtubePageID = "youtubePageID";
LIBUI_API extern const QString g_tokenType; // used by youtube
LIBUI_API extern const QString g_youtube_latency;

//the second row display
LIBUI_API extern const QString g_catogry;
//share url
LIBUI_API extern const QString g_shareUrl;
LIBUI_API extern const QString g_shareUrlTemp;
LIBUI_API extern const char SHARE_URL_KEY[];

LIBUI_API extern const QString g_channelStatus;     // empty valid invalid expired
LIBUI_API extern const QString g_channelUserStatus; //enable busy disbale
LIBUI_API extern const QString g_channelDualOutput;
LIBUI_API extern const QString g_channelRtmpUrl;
LIBUI_API extern const QString g_streamKey;
LIBUI_API extern const QString g_isTwitchRtmpServerAuto;

// auth from web
const QString g_channelToken = "access_token";
const QString g_channelCookie = "cookie";
const QString g_channelCode = "channel_code";
LIBUI_API extern const QString g_refreshToken;
LIBUI_API extern const QString g_expires_in;
LIBUI_API extern const int g_defaultExpiresSeconds;

LIBUI_API extern const QString g_data_type; // rtmp or login channel

//just for some platform
LIBUI_API extern const QString g_userName; //platform inner data
LIBUI_API extern const QString g_nickName; // for display on channel capsule

//temp key just for diplay
LIBUI_API extern const QString g_sortString;
LIBUI_API extern const QString g_displayState;
LIBUI_API extern const QString g_displayOrder;

LIBUI_API extern const QString g_displayLine1;
LIBUI_API extern const QString g_displayLine2;

//rtmp only
LIBUI_API extern const QString g_userID; //do not use this
LIBUI_API extern const QString g_rtmpUserID;
LIBUI_API extern const QString g_password;
LIBUI_API extern const QString g_otherInfo;
LIBUI_API extern const QString g_rtmpSeq;
LIBUI_API extern const QString g_publishService;
LIBUI_API extern const QString g_isPresetRTMP;
LIBUI_API extern const QString g_isUseNewAPI;
LIBUI_API extern const QString g_customUserDataSeq;
LIBUI_API extern const QString g_customData;

//vlive only
LIBUI_API extern const QString g_userVliveSeq;
LIBUI_API extern const QString g_userVliveCode;
LIBUI_API extern const QString g_vliveNormalChannelName;
LIBUI_API extern const QString g_vliveFanshipModel;
LIBUI_API extern const QString g_vliveProfileData;
LIBUI_API extern const QString g_userIconThumbnailUrl;
LIBUI_API extern const QString g_userProfileImg;

//flags
LIBUI_API extern const QString g_prismMatched; //rtmp only
LIBUI_API extern const QString g_isLeader;
LIBUI_API extern const QString g_isUserAsked;
LIBUI_API extern const QString g_isPlatformEnabled;
LIBUI_API extern const QString g_isFanship;
LIBUI_API extern const QString g_isUpdated;

/* channel common info used save in local */

LIBUI_API extern const QString g_createTime;
LIBUI_API extern const QString g_broadcastID;
LIBUI_API extern const QString g_userIconCachePath;   // local user icon path
LIBUI_API extern const QString g_profileThumbnailUrl; // user icon Url
LIBUI_API extern const QString g_imageCache;          // user icon
LIBUI_API extern const QString g_imageSize;
LIBUI_API extern const QString g_srcImage;
LIBUI_API extern const QString g_broadcastTitle;

LIBUI_API extern const QString g_language;
LIBUI_API extern const QString g_viewers;
LIBUI_API extern const QString g_viewersPix;
LIBUI_API extern const QString g_totalViewers;

LIBUI_API extern const QString g_likes;
LIBUI_API extern const QString g_likesPix;

LIBUI_API extern const QString g_comments;
LIBUI_API extern const QString g_commentsPix;
LIBUI_API extern const QString g_subChannelId;
LIBUI_API extern const QString g_showEndShare;

//for scheduleList
LIBUI_API extern const QString g_scheduleList;
LIBUI_API extern const QString g_timeStamp;
LIBUI_API extern const QString g_selectedSchedule;
LIBUI_API extern const QString g_prismState;

enum class PrismState { Bussy, Free };

//just for error alerts
LIBUI_API extern const QString g_errorRetdata;
LIBUI_API extern const QString g_errorString;

//default icon qrc path
LIBUI_API extern const QString g_defaultHeaderIcon;
LIBUI_API extern const QString g_defaultChzzkUserIcon;
LIBUI_API extern const QString g_defaultErrorIcon;
LIBUI_API extern const QString g_defualtPlatformIcon;
LIBUI_API extern const QString g_defualtPlatformSmallIcon;
LIBUI_API extern const QString g_defaultViewerIcon;
LIBUI_API extern const QString g_defaultLikeIcon;
LIBUI_API extern const QString g_defaultCommentsIcon;
LIBUI_API extern const QString g_defaultRTMPAddButtonIcon;

LIBUI_API extern const QString g_dashboardButtonIcon;
LIBUI_API extern const QString g_addChannelButtonIcon;
LIBUI_API extern const QString g_addChannelButtonConnectedIcon;
LIBUI_API extern const QString g_channelSettingBigIcon;
LIBUI_API extern const QString g_tagIcon;

LIBUI_API extern const QString g_twitchPlatformIcon;
LIBUI_API extern const QString g_naverTvViewersIcon;
LIBUI_API extern const QString g_naverTvLikeIcon;

//platforms which should be asked if delete previous infos before refresh
LIBUI_API extern const QStringList g_platformsToClearData;
LIBUI_API extern const QStringList g_exclusivePlatform;
LIBUI_API extern const QStringList g_rehearsalingConfigEnabledList;

LIBUI_API extern const QStringList gDefaultPlatform;
LIBUI_API extern const QStringList g_allPlatforms;

LIBUI_API extern const int g_maxActiveChannels;

LIBUI_API extern const QString g_facebookUrl;
LIBUI_API extern const QString g_twitterUrl;

LIBUI_API extern const QString g_channelCacheFile;
LIBUI_API extern const QString g_channelSettingsFile;

LIBUI_API extern const QString g_channelSreLoginFailed;

LIBUI_API extern const QString g_loadingPixPath;
LIBUI_API extern const QString g_goliveLoadingPixPath;
/*urls*/
LIBUI_API extern const QString g_youtubeV3;
LIBUI_API extern const QString g_youtubeChannels;
LIBUI_API extern const QString g_broadCastType;
LIBUI_API extern const QString g_maxResults;
LIBUI_API extern const QString g_contentDetails;
LIBUI_API extern const QString g_chzzkExtraData;
LIBUI_API extern const QString g_persistentType;
LIBUI_API extern const QString g_statusPart;
LIBUI_API extern const QString g_comma;
LIBUI_API extern const QString g_liveInfoPrefix;
LIBUI_API extern const QString defaultSourcePath;

//twitter
const QString oauth_token = "oauth_token";
const QString oauth_token_secret = "oauth_token_secret";
const QString oauth_callback_confirmed = "oauth_callback_confirmed";
const QString oauth_verifier = "oauth_verifier";
const QString twitter_source_id = "source_id";
const QString twitter_source = "source";
const QString twitter_region = "region";
const QString default_region = "ap-northeast-2";
const QString is_low_latency = "is_low_latency";
const QString twitter_broadcast_data = "broadcast_data";
const QString twitter_rtmp_key = "rtmp_stream_key";
const QString twitter_should_not_tweet = "should_not_tweet";
const QString twitter_broadcast_state = "state";
const QString twitter_broadcast_state_publish = "PUBLISH";
const QString twitter_broadcast_state_end = "END";
const QString twitter_broadcast_state_running = "RUNNING";
const QString twitter_locale = "locale";

const QString api_url = "api_url";
const QString request_type = "request_type";
const QString reply_error_code = "error_code";
const QString reply_status_code = "status_code";
const QString reply_data = "replay_data";

/*channel state */
enum ChannelStatus { Error = 0, LoginError = 1, UnInitialized = 2, WaitingActive = 3, EmptyChannel = 4, Expired = 5, InValid = 6, Valid = 7, UnAuthorized = 8 };
//Q_DECLARE_METATYPE(ChannelStatus)
/*channel user state */
enum ChannelUserStatus { NotExist = -1, Disabled = 0, Enabled = 1, BusyState = 2 };
//Q_DECLARE_METATYPE(ChannelUserStatus)

/*channel data type used by api*/
enum ChannelDataType { NoType = 0, ChannelType = 1, DownloadType = 2, CustomType = 3, RTMPType = 3, SRTType, RISTType };
//Q_DECLARE_METATYPE(ChannelDataType)

enum ChannelDualOutput { NoSet = 0, HorizontalOutput, VerticalOutput };

/*channel run state */
/* before living state :*/
/*stoptogo               on finished clicked */
/*readystate             normalstate  ,initial state,reset state */
/*broadcastgo            on obs check for streaming */
/*canbroadcast           after check start obs streaming */
enum LiveState { ReadyState, BroadcastGo, CanBroadcastState, StreamStarting, StreamStarted, StopBroadcastGo, CanBroadcastStop, StreamStopping, StreamStopped, StreamEnd };
//Q_DECLARE_METATYPE(LiveState)

LIBUI_API extern const QMap<int, QString> LiveStatesMap;

enum RecordState { RecordReady, CanRecord, RecordStarting, RecordStarted, RecordStopGo, RecordStopping, RecordStopped };
//Q_DECLARE_METATYPE(RecordState)

LIBUI_API extern const QMap<int, QString> RecordStatesMap;

//log step
const QString g_addChannelStep = "Add Channel";
const QString g_updateChannelStep = "Update Channel";
const QString g_LiveStep = "Live Step";
const QString g_recordStep = "Record Step";
const QString g_removeChannelStep = "Remove Channel";

enum ImageType {
	tagIcon = 0,
	dashboardButtonIcon = 1,
	addChannelButtonIcon = 2,
	addChannelButtonConnectedIcon = 3,
	channelSettingBigIcon = 4,
	chatIcon_offNormal = 5,
	chatIcon_offHover = 6,
	chatIcon_offClick = 7,
	chatIcon_offDisable = 8,
	chatIcon_onNormal = 9,
	chatIcon_onHover = 10,
	chatIcon_onClick = 11,
};

}

namespace channel_transactions_keys {

LIBUI_API extern const QString g_CMDType;
LIBUI_API extern const QString g_taskQueue;
LIBUI_API extern const QString g_taskParameters;
LIBUI_API extern const QString g_functions;
LIBUI_API extern const QString g_context;
LIBUI_API extern const QString g_taskName;

enum class CMDTypeValue { NotDefineCMD, AddChannelCMD, RefreshChannelsCMD };
//Q_DECLARE_METATYPE(CMDTypeValue)

}

#define ChannelTransactionsKeys channel_transactions_keys

/*************** channel  name****************/
constexpr auto VLIVE = "V LIVE";
constexpr auto NAVER_TV = "NAVER TV";
constexpr auto WAV = "WAV";
constexpr auto BAND = "BAND";
constexpr auto TWITCH = "Twitch";
constexpr auto YOUTUBE = "YouTube";
constexpr auto FACEBOOK = "Facebook";
constexpr auto WHALE_SPACE = "whale space";
constexpr auto AFREECATV = "afreecaTV";
constexpr auto NOW = "NOW";
constexpr auto NAVER_SHOPPING_LIVE = "Naver Shopping LIVE";
constexpr auto SELECT_TYPE = "Select";
constexpr auto CUSTOM_RTMP = "Custom RTMP";
constexpr auto RTMPT_DEFAULT_TYPE = "CUSTOM";
constexpr auto TWITTER = "Twitter";
constexpr auto CUSTOM_SRT = "Custom SRT";
constexpr auto CUSTOM_RIST = "Custom RIST";
constexpr auto CHZZK = "CHZZK";
constexpr auto NCB2B = "NAVER Cloud B2B";
constexpr auto ALL_CHAT = "ALL CHAT PAGE";
constexpr auto MQTT_SHEET = "mqtt";
constexpr auto SOOP = "SOOP";
//Class template specialization of 'QMetaTypeId' must occur at global scope
Q_DECLARE_METATYPE(channel_data::ChannelStatus)
Q_DECLARE_METATYPE(channel_data::ChannelUserStatus)
Q_DECLARE_METATYPE(channel_data::ChannelDataType)
Q_DECLARE_METATYPE(channel_data::LiveState)
Q_DECLARE_METATYPE(channel_data::RecordState)
Q_DECLARE_METATYPE(channel_transactions_keys::CMDTypeValue)
Q_DECLARE_METATYPE(channel_data::ChannelDualOutput)

//service
constexpr auto YOUTUBE_HLS = "YouTube - HLS";
constexpr auto YOUTUBE_RTMP = "YouTube - RTMPS";
constexpr auto TWITCH_SERVICE = "Twitch";
constexpr auto WHIP_SERVICE = "WHIP";
constexpr auto AFREECATV_SERVICE = "AfreecaTV";
constexpr auto FACEBOOK_SERVICE = "Facebook Live";

//live start name
constexpr auto NCP_LIVE_START_NAME = "NCP";

#endif // !CHANNEL_CONST_H
