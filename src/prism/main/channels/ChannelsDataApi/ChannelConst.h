#ifndef CHANNEL_CONST_H
#define CHANNEL_CONST_H

#include <QString>
#include <QStringList>
#include <QMetaType>

namespace ChannelData {

/*key to be used in channel data map*/

extern const QString g_channelSettings;
extern const QString g_isRecordNeeded;
extern const QString g_isMulticast;
extern const QString g_liveStatus;
extern const QString g_broadcastStatus;
extern const QString g_reocordStatus;

extern const QString g_channelUUID;
const QString g_channelName = "channel_name";
const QString g_youtubePageID = "youtubePageID";
extern QString g_publishService;
extern const QString g_channelUrl;
extern const QString g_callbackUrl;
extern const QString g_catogry;
extern const QString g_catogryTemp;
extern const QString g_handlerUUID;

extern const QString g_channelIcon;
extern const QString g_logoUrl;
extern const QString g_shareUrl;
extern const QString g_shareUrlTemp;
extern const char SHARE_URL_KEY[];

extern const QString g_channelStatus;
extern const QString g_channelUserStatus;
extern const QString g_channelComplexState;
extern const QString g_authType;
extern const QString g_authStatusCode;
extern const QString g_appClientID;
extern const QString g_errorTitle;
extern const QString g_errorString;

extern const QString g_channelRtmpUrl;
extern const QString g_streamKey;
const QString g_channelToken = "access_token";
const QString g_channelCode = "channel_code";
extern const QString g_refreshToken;

extern const QString g_tokenType;
extern const QString g_refreshUrl;
extern const QString g_expires_in;
extern const int g_defaultExpiresSeconds;

extern const QString g_userName;
extern const QString g_nickName;

extern const QString g_userID;
extern const QString g_password;
extern const QString g_otherInfo;
extern const QString g_userIconCachePath;
extern const QString g_rtmpSeq;
extern const QString g_prismMatched;
//extern const QString g_isVisible;
extern const QString g_isUserAsked;

extern const QString g_profileThumbnailUrl;
extern const QString g_email;
extern const QString g_language;
extern const QString g_createTime;
extern const QString g_broadcastID;
/* channel info used */
extern const QString g_viewers;
extern const QString g_totalViewers;
extern const QString g_likes;
extern const QString g_viewersHtml;
extern const QString g_likesHtml;
extern const QString g_displayOrder;

extern const QString g_fileUrl;
extern const QString g_src_platform;
extern const QString g_netWorkManager;

extern const QString g_channelWidget;
extern const QString g_channelItem;
extern const QString g_data_type;
extern const QString g_CMDType;
extern const QString g_isUpdated;

/*task handler used*/
extern const QString g_taskUUID;
extern const QString g_functions;
extern const QString g_initFunction;
extern const QString g_sendFunction;
extern const QString g_callback;
extern const QString g_initData;
extern const QString g_channelHandler;

extern const QString g_defaultHeaderIcon;
extern const QString g_defaultErrorIcon;
extern const QString g_defualtPlatformIcon;
extern const QString g_defualtPlatformSmallIcon;

extern const QString g_twitchPlatformIcon;

/*************** channel  name****************/
#define VLIVE "V LIVE"
#define NAVER_TV "NAVER TV"
#define WAV "WAV"
#define TWITCH "Twitch"
#define YOUTUBE "YouTube"
#define FACEBOOK "Facebook"
#define AFREECATV "afreecaTv"
#define NOW "NOW"
#define SELECT "Select"
#define CUSTOM_RTMP "Custom RTMP"
#define RTMPT_DEFAULT_TYPE "CUSTOM"

extern const QStringList gDefaultPlatform;
extern const int g_maxActiveChannels;

extern const QString g_facebookUrl;
extern const QString g_twitterUrl;

extern const QString g_channelCacheFile;
extern const QString g_channelSettingsFile;

extern const QString g_loadingPixPath;
extern const QString g_goliveLoadingPixPath;
/*urls*/
extern const QString g_youtubeV3;
extern const QString g_youtubeChannels;
extern const QString g_broadCastType;
extern const QString g_maxResults;
extern const QString g_contentDetails;
extern const QString g_persistentType;
extern const QString g_statusPart;
extern const QString g_comma;
/*channel state */
enum ChannelStatus { Error = 0, UnInitialized, Waiting, Expired, InValid, Valid };
Q_DECLARE_METATYPE(ChannelStatus)
/*channel user state */
enum ChannelUserStatus { NotExist = -1, Disabled = 0, Enabled, BusyState };
Q_DECLARE_METATYPE(ChannelUserStatus)
/*channel data type used by api*/
enum ChannelDataType { NoType = 0, ChannelType, DownloadType, RTMPType };
Q_DECLARE_METATYPE(ChannelDataType)

enum RefreshResult {
	RequestError_Valid,
	RequestError_Expired,
	NoError_Expired,
	NoError_Valid

};

Q_DECLARE_METATYPE(RefreshResult)

/*channel run state */
/* before living state :*/
/*stoptogo               on finished clicked */
/*readystate             normalstate  ,initial state,reset state */
/*broadcastgo            on obs check for streaming */
/*canbroadcast           after check start obs streaming */
enum LiveState { ReadyState = 0, BroadcastGo, CanBroadcastState, StreamStarting, StreamStarted, StopBroadcastGo, CanBroadcastStop, StreamStopping, StreamStopped, StreamEnd };
Q_DECLARE_METATYPE(LiveState)

extern const QStringList LiveStatesLst;

enum RecordState { RecordReady, CanRecord, RecordStarting, RecordStarted, RecordStopping, RecordStopGo, RecordStopped };
Q_DECLARE_METATYPE(RecordState)

extern const QStringList RecordStatesLst;
}

#endif // !CHANNEL_CONST_H
