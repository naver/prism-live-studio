
#include "ChannelConst.h"

namespace ChannelData {
const QString g_channelSettings = "channels_settings";
const QString g_isRecordNeeded = "record_needed";
const QString g_isMulticast = "is_multicast";
const QString g_liveStatus = "live_status";
const QString g_broadcastStatus = "broadcast_staus";
const QString g_reocordStatus = "record_status";

const QString g_channelUUID = "channel_uuid";
//const QString g_channelName = "channel_name";
QString g_publishService = "publishService";
const QString g_channelUrl = "channel_url";
const QString g_callbackUrl = "callback_url";
const QString g_catogry = "catogry";
const QString g_catogryTemp = "catogry_temp";
const QString g_handlerUUID = "hanlder_uuid";

const QString g_channelIcon = "channel_icon";
const QString g_logoUrl = "logo_url";
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

const QString g_channelRtmpUrl = "rtmp_url";
const QString g_streamKey = "stream_key";
const QString g_refreshToken = "refresh_token";
const QString g_tokenType = "token_type";
const QString g_refreshUrl = "refresh_url";
const QString g_expires_in = "expires_in";

const int g_defaultExpiresSeconds = 3500;

const QString g_userName = "user_name";
const QString g_nickName = "display_name";

const QString g_userID = "user_id";
const QString g_password = "password";
const QString g_otherInfo = "other_info";
const QString g_userIconCachePath = "user_icon_cache_path";
const QString g_rtmpSeq = "rtmpSeq";
const QString g_prismMatched = "prism_matched";
//const QString g_isVisible = "is_visible";
const QString g_isUserAsked = "is_user_asked";

const QString g_profileThumbnailUrl = "profile_thumbnail_url";
const QString g_email = "email";
const QString g_language = "language";
const QString g_createTime = "created_at";
const QString g_broadcastID = "broadcast_id";

const QString g_viewers = "views";
const QString g_totalViewers = "total_viewers";
const QString g_likes = "likes";
const QString g_viewersHtml = "<p\"><img border=\"0\" src =\":/liveend/skin/ic-liveend-view.png\"/> %L1</p>";
const QString g_likesHtml = "<p>< img src =\":/liveend/skin/ic-liveend-like.png\"/> %L1</p>";
const QString g_displayOrder = "display_order";

const QString g_fileUrl = "file_url";
const QString g_src_platform = "src_platform";
const QString g_netWorkManager = "network_manager";

const QString g_channelWidget = "channel_widget";
const QString g_channelItem = "channel_item";
const QString g_data_type = "data_type";
const QString g_CMDType = "cmd_type";
const QString g_isUpdated = "is_updated";

const QString g_taskUUID = "task_uuid";
const QString g_functions = "functions";
const QString g_initFunction = "initFunction";
const QString g_sendFunction = "sendFunction";
const QString g_callback = "callback";
const QString g_initData = "initData";
const QString g_channelHandler = "handler";

const QString g_defaultHeaderIcon = ":/Images/skin/img-setting-profile-blank.png";
const QString g_defaultErrorIcon = ":/Images/skin/thumb-error.png";
const QString g_defualtPlatformIcon = ":/Images/skin/img-rtmp-profile.png";
const QString g_defualtPlatformSmallIcon = ":/Images/skin/ic-rtmp-small.png";

const QString g_twitchPlatformIcon = ":/Images/skin/img-twich-profile.png";

const QStringList gDefaultPlatform{TWITCH, YOUTUBE, CUSTOM_RTMP};

const int g_maxActiveChannels = 6;

const QString g_facebookUrl = "facebook_url";
const QString g_twitterUrl = "twitter_url";

const QString g_channelCacheFile = "channel_cache.dat";
const QString g_channelSettingsFile = "channels_settings.dat";

const QString g_loadingPixPath = ":/Images/skin/loading-%1.png";
const QString g_goliveLoadingPixPath = ":/Images/skin/black-loading-%1.png";
const QString g_broadCastType = "broadcastType";
const QString g_maxResults = "maxResults";
const QString g_contentDetails = "contentDetails";
const QString g_persistentType = "persistent";
const QString g_statusPart = "status";
const QString g_comma = ",";

const QStringList LiveStatesLst{"ReadyState",      "BroadcastGo",      "CanBroadcastState", "StreamStarting", "StreamStarted",
				"StopBroadcastGo", "CanBroadcastStop", "StreamStopping",    "StreamStopped",  "StreamEnd"};

const QStringList RecordStatesLst{"RecordReady", "CanRecord", "RecordStarting", "RecordStarted", "RecordStopping", "RecordStopGo", "RecordStopped"};

}
