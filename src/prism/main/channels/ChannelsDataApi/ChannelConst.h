#ifndef CHANNEL_CONST_H
#define CHANNEL_CONST_H

#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace ChannelData {

/*key to be used in channel data map*/

extern const QString g_channelUUID;
const QString g_platformName = "channel_name"; //ex. twitch navertv youtube ..
extern const QString g_displayPlatformName;

//just for youtube
const QString g_youtubePageID = "youtubePageID";
extern const QString g_tokenType; // used by youtube
extern const QString g_youtube_latency;

//the second row display
extern const QString g_catogry;
//share url
extern const QString g_shareUrl;
extern const QString g_shareUrlTemp;
extern const char SHARE_URL_KEY[];

extern const QString g_channelStatus;     // empty valid invalid expired
extern const QString g_channelUserStatus; //enable busy disbale

extern const QString g_channelRtmpUrl;
extern const QString g_streamKey;

// auth from web
const QString g_channelToken = "access_token";
const QString g_channelCookie = "cookie";
const QString g_channelCode = "channel_code";
extern const QString g_refreshToken;
extern const QString g_expires_in;
extern const int g_defaultExpiresSeconds;

extern const QString g_data_type; // rtmp or login channel

//just for some platform
extern const QString g_userName; //platform inner data
extern const QString g_nickName; // for display on channel capsule

//temp key just for diplay
extern const QString g_sortString;
extern const QString g_displayState;
extern const QString g_displayOrder;

extern const QString g_displayLine1;
extern const QString g_displayLine2;

//rtmp only
extern const QString g_userID; //do not use this
extern const QString g_rtmpUserID;
extern const QString g_password;
extern const QString g_otherInfo;
extern const QString g_rtmpSeq;
extern const QString g_publishService;
extern const QString g_isPresetRTMP;

//vlive only
extern const QString g_userVliveSeq;
extern const QString g_userVliveCode;
extern const QString g_vliveNormalChannelName;
extern const QString g_vliveFanshipModel;
extern const QString g_vliveProfileData;
extern const QString g_userIconThumbnailUrl;
extern const QString g_userProfileImg;

//flags
extern const QString g_prismMatched; //rtmp only
extern const QString g_isLeader;
extern const QString g_isUserAsked;
extern const QString g_isPlatformEnabled;
extern const QString g_isFanship;
extern const QString g_isUpdated;

/* channel common info used save in local */

extern const QString g_createTime;
extern const QString g_broadcastID;
extern const QString g_userIconCachePath;   // local user icon path
extern const QString g_profileThumbnailUrl; // user icon Url
extern const QString g_imageCache;          // user icon
extern const QString g_imageSize;
extern const QString g_srcImage;

extern const QString g_language;
extern const QString g_viewers;
extern const QString g_viewersPix;
extern const QString g_totalViewers;

extern const QString g_likes;
extern const QString g_likesPix;

extern const QString g_comments;
extern const QString g_commentsPix;
extern const QString g_subChannelId;

//just for error alerts
extern const QString g_errorTitle;
extern const QString g_errorString;
extern const QString g_errorType;

//default icon qrc path
extern const QString g_defaultHeaderIcon;
extern const QString g_defaultErrorIcon;
extern const QString g_defualtPlatformIcon;
extern const QString g_defualtPlatformSmallIcon;
extern const QString g_defaultViewerIcon;
extern const QString g_defaultLikeIcon;
extern const QString g_defaultCommentsIcon;
extern const QString g_defaultRTMPAddButtonIcon;

extern const QString g_twitchPlatformIcon;
extern const QString g_naverTvViewersIcon;
extern const QString g_naverTvLikeIcon;

/*************** channel  name****************/
#define VLIVE "V LIVE"
#define NAVER_TV "NAVER TV"
#define WAV "WAV"
#define BAND "BAND"
#define TWITCH "Twitch"
#define YOUTUBE "YouTube"
#define FACEBOOK "Facebook"
#define WHALE_SPACE "whale space"
#define AFREECATV "afreecaTV"
#define NOW "NOW"
#define NAVER_SHOPPING_LIVE "Naver Shopping LIVE"
#define SELECT "Select"
#define CUSTOM_RTMP "Custom RTMP"
#define RTMPT_DEFAULT_TYPE "CUSTOM"

//platforms which should be asked if delete previous infos before refresh
extern const QStringList g_platformsToClearData;
extern const QStringList g_exclusivePlatform;
extern const QStringList g_rehearsalingConfigEnabledList;

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
extern const QString g_liveInfoPrefix;
extern const QString defaultSourcePath;

/*channel state */
enum ChannelStatus { Error = 0, LoginError, UnInitialized, WaitingActive, EmptyChannel, Expired, InValid, Valid, UnAuthorized };
Q_DECLARE_METATYPE(ChannelStatus)
/*channel user state */
enum ChannelUserStatus { NotExist = -1, Disabled = 0, Enabled, BusyState };
Q_DECLARE_METATYPE(ChannelUserStatus)
/*channel data type used by api*/
enum ChannelDataType { NoType = 0, ChannelType, DownloadType, RTMPType };
Q_DECLARE_METATYPE(ChannelDataType)

enum NetWorkErrorType { NoError, PlatformExpired, NetWorkNoStable, PlatformUinitialized, ChannelIsEmpty = 40400, UnknownError, SpecializedError };
Q_DECLARE_METATYPE(NetWorkErrorType)

/*channel run state */
/* before living state :*/
/*stoptogo               on finished clicked */
/*readystate             normalstate  ,initial state,reset state */
/*broadcastgo            on obs check for streaming */
/*canbroadcast           after check start obs streaming */
enum LiveState { ReadyState = 0, BroadcastGo, CanBroadcastState, StreamStarting, StreamStarted, StopBroadcastGo, CanBroadcastStop, StreamStopping, StreamStopped, StreamEnd };
Q_DECLARE_METATYPE(LiveState)

extern const QMap<int, QString> LiveStatesMap;

enum RecordState { RecordReady, CanRecord, RecordStarting, RecordStarted, RecordStopGo, RecordStopping, RecordStopped };
Q_DECLARE_METATYPE(RecordState)

extern const QMap<int, QString> RecordStatesMap;

//log step
const QString g_addChannelStep = "Add Channel";
const QString g_updateChannelStep = "Update Channel";
const QString g_LiveStep = "Live Step";
const QString g_recordStep = "Record Step";
const QString g_removeChannelStep = "Remove Channel";

}

namespace ChannelTransactionsKeys {

extern const QString g_CMDType;
extern const QString g_taskQueue;
extern const QString g_taskParameters;
extern const QString g_functions;
extern const QString g_context;

enum CMDTypeValue { NotDefineCMD, AddChannelCMD, RefreshChannelsCMD };
Q_DECLARE_METATYPE(CMDTypeValue)

}

#endif // !CHANNEL_CONST_H
