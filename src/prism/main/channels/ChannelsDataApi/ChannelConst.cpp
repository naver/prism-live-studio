
#include "ChannelConst.h"

namespace ChannelData {

const QString g_broadcastStatus = "broadcast_staus";
const QString g_reocordStatus = "record_status";

const QString g_channelUUID = "channel_uuid";
//const QString g_channelName = "channel_name";
const QString g_displayPlatformName = "display_platform_name";

const QString g_publishService = "publishService";
const QString g_channelUrl = "channel_url";
const QString g_callbackUrl = "callback_url";
const QString g_catogry = "catogry";
const QString g_handlerUUID = "hanlder_uuid";

const QString g_isPresetRTMP = "isPresetRTMP";

const QString g_channelIcon = "channel_icon";
const QString g_shareUrl = "share_url";
const QString g_shareUrlTemp = "share_url_temp";
const char SHARE_URL_KEY[] = "share_url";

const QString g_channelStatus = "channel_status";
const QString g_channelUserStatus = "user_status";
const QString g_channelComplexState = "complex_state";
const QString g_authType = "authu_type";
const QString g_authStatusCode = "auth_status_code";
const QString g_appClientID = "app_client_id";
const QString g_errorTitle = "error_title";
const QString g_errorString = "error_string";
const QString g_errorType = "error_type";

const QString g_channelRtmpUrl = "rtmp_url";
const QString g_streamKey = "stream_key";
const QString g_refreshToken = "refresh_token";
const QString g_tokenType = "token_type";
const QString g_expires_in = "expires_in";
const QString g_youtube_latency = "youtube_latency";

const int g_defaultExpiresSeconds = 3500;

const QString g_userName = "user_name";
const QString g_userProfileImg = "userProfileImg";
const QString g_nickName = "display_name";

const QString g_userID = "user_id";
const QString g_rtmpUserID = "user_id";
const QString g_userVliveSeq = "user_seq";
const QString g_userVliveCode = "user_code";
const QString g_vliveNormalChannelName = "vliveNormalChannelName";
const QString g_vliveProfileData = "vliveProfileData";
const QString g_password = "password";
const QString g_otherInfo = "other_info";
const QString g_userIconCachePath = "user_icon_cache_path";
const QString g_rtmpSeq = "rtmpSeq";
const QString g_prismMatched = "prism_matched";
const QString g_isLeader = "is_leader";
const QString g_isUserAsked = "is_user_asked";
const QString g_isPlatformEnabled = "is_platform_enabled";

const QString g_userIconThumbnailUrl = "usr_icon_thumbnail_url";
const QString g_profileThumbnailUrl = "profile_thumbnail_url";
const QString g_imageCache = "image_cache"; // user icon Url
const QString g_imageSize = "image_size";
const QString g_srcImage = "src_image";

const QString g_email = "email";
const QString g_language = "language";
const QString g_createTime = "created_at";
const QString g_broadcastID = "broadcast_id";

const QString g_sortString = "sort_string";
const QString g_displayLine1 = "display_line1";
const QString g_displayLine2 = "display_line2";

const QString g_viewers = "views";
const QString g_viewersPix = "viewers_pix";
const QString g_totalViewers = "total_viewers";
const QString g_likes = "likes";
const QString g_likesPix = "likes_pix";
const QString g_comments = "comments";
const QString g_commentsPix = "comments_pix";
const QString g_viewersHtml = "<p\"><img border=\"0\" src =\":/images/ic-liveend-view.svg\"/> %L1</p>";
const QString g_likesHtml = "<p>< img src =\":/images/ic-liveend-like.svg\"/> %L1</p>";
const QString g_displayOrder = "display_order";
const QString g_subChannelId = "sub_channel_id";
const QString g_displayState = "display_state";

const QString g_channelWidget = "channel_widget";
const QString g_channelItem = "channel_item";
const QString g_data_type = "data_type";

const QString g_isUpdated = "is_updated";

const QString g_channelHandler = "handler";

const QString g_defaultHeaderIcon = ":/images/img-setting-profile-blank.svg";
const QString g_defaultErrorIcon = ":/images/thumb-error.svg";
const QString g_defualtPlatformIcon = ":/images/img-rtmp-profile.svg";
const QString g_defualtPlatformSmallIcon = ":/images/ic-rtmp-small.svg";

const QString g_defaultViewerIcon = ":/images/ic-liveend-view.svg";
const QString g_defaultLikeIcon = ":/images/ic-liveend-like.svg";
const QString g_defaultCommentsIcon = ":/images/reply-icon.svg";

const QString g_twitchPlatformIcon = ":/images/img-twich-profile.svg";
const QString g_naverTvViewersIcon = ":/images/ic-view-navertv.svg";
const QString g_naverTvLikeIcon = ":/images/ic-like-vlive.svg";
const QString g_defaultRTMPAddButtonIcon = ":/images/DefaultChannels/btn-mych-custom-rtmp-normal.svg";

const QStringList g_platformsToClearData{YOUTUBE, BAND, VLIVE, NAVER_TV, FACEBOOK, NAVER_SHOPPING_LIVE};
const QStringList g_exclusivePlatform{NOW, VLIVE, NAVER_SHOPPING_LIVE};
const QStringList g_rehearsalingConfigEnabledList{NAVER_SHOPPING_LIVE};
const QStringList gDefaultPlatform{TWITCH, YOUTUBE, FACEBOOK, NAVER_SHOPPING_LIVE, NAVER_TV, VLIVE, BAND, AFREECATV, CUSTOM_RTMP};
const int g_maxActiveChannels = 6;

const QString g_facebookUrl = "facebook_url";
const QString g_twitterUrl = "twitter_url";

const QString g_channelCacheFile = "channel_cache.dat";
const QString g_channelSettingsFile = "channels_settings.dat";

const QString g_loadingPixPath = ":/images/loading-%1.svg";
const QString g_goliveLoadingPixPath = ":/images/black-loading-%1.svg";
const QString g_broadCastType = "broadcastType";
const QString g_maxResults = "maxResults";
const QString g_contentDetails = "contentDetails";
const QString g_persistentType = "persistent";
const QString g_statusPart = "status";
const QString g_comma = ",";
const QString g_liveInfoPrefix = "live status:";
const QString defaultSourcePath = ":/Configs/DefaultResources";

const QMap<int, QString> LiveStatesMap{{ReadyState, QString("ReadyState( user can set channel )")},
				       {BroadcastGo, QString("BroadcastGo (stream is in prepare step ,checking channels status )")},
				       {CanBroadcastState, QString("CanBroadcastState ( user set selected channels live information )")},
				       {StreamStarting, QString("StreamStarting ( stream is starting , try send message to server )")},
				       {StreamStarted, QString("StreamStarted ( stream started successfull )")},
				       {StopBroadcastGo, QString("StopBroadcastGo( check if stopping )")},
				       {CanBroadcastStop, QString("CanBroadcastStop ( check if server can stop )")},
				       {StreamStopping, QString("StreamStopping ( send message to server and waiting for output stopping signal)")},
				       {StreamStopped, QString("StreamStopped ( stream has been stopped, output stopped )")},
				       {StreamEnd, QString("StreamEnd (stream stopped successfull )")}};

const QMap<int, QString> RecordStatesMap{{RecordReady, QString("RecordReady( user can set record information )")},
					 {CanRecord, QString("CanRecord (check if record setting is ok )")},
					 {RecordStarting, QString("RecordStarting (record is starting , try start output )")},
					 {RecordStarted, QString("RecordStarted (record started successfull)")},
					 {RecordStopGo, QString("RecordStopGo (check if record can be stopped )")},
					 {RecordStopping, QString("RecordStopping (record is stopping , and waiting for output stopping signal )")},
					 {RecordStopped, QString("RecordStopped( record stopped successfull )")}};

}
namespace ChannelTransactionsKeys {

const QString g_CMDType = "cmd_type";
const QString g_taskQueue = "task_queue";
const QString g_taskParameters = "parameters";
const QString g_functions = "functions";
const QString g_context = "context";
}
