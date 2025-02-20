/*
 * @file      CommonDefine.h
 * @brief     Definition of all strings used internally by the system.
 * @date      2019/09/25
 * @author    liuying
 * @attention null
 * @version   2.0.1
 * @modify    liuying create 2019/09/25
 */

#ifndef COMMON_COMMONDEFINE_H
#define COMMON_COMMONDEFINE_H

namespace common {

/*************** prism ****************/
constexpr auto PLS_PRISM = "Prism";
constexpr auto PLS_LIVE_STUDIO = "PRISM Live Studio";
constexpr auto PLS_ARCHITECTURE = " Architecture ";
constexpr auto PLS_BUILD = " Build ";
constexpr auto PLS_LANGUAGE = " Language ";

/*************** enter ****************/
constexpr auto NEW_LINE = "\n";
constexpr auto ENTER = "\r";
constexpr auto ENTER_WITH_BACKSLASH = "\\r";
constexpr auto NEW_LINE_WITH_BACKSLASH = "\\n";
constexpr auto SLASH = "/";

/*************** dpi ****************/
constexpr auto HIGH_DPI_CAN_UPDATE_SIZE = "_hdpi_canUpdateSize";
constexpr auto HIGH_DPI_ORIGINAL_SIZE = "_hdpi_originalSize";
constexpr auto HIGH_DPI_TAG_COMMENT_BEGIN = "/*";
constexpr auto HIGH_DPI_TAG_COMMENT_END = "*/";
constexpr auto HIGH_DPI_COMMENT_INT = "/*hdpi:int*/";

/*************** status ****************/
constexpr auto STATUS = "status";
constexpr auto STATUS_ENABLE = "enable";
constexpr auto STATUS_DISABLE = "disable";
constexpr auto STATUS_SELECTED = "selected";
constexpr auto STATUS_UNSELECTED = "unselected";
constexpr auto STATUS_DISABLE_SELECTED = "disableSelected";
constexpr auto STATUS_NORMAL = "normal";
constexpr auto STATUS_HOVER = "hover";
constexpr auto STATUS_PRESSED = "pressed";
constexpr auto STATUS_ON = "on";
constexpr auto STATUS_OFF = "off";
constexpr auto STATUS_CLICKED = "clicked";
constexpr auto STATUS_UNCLICKED = "unclicked";
constexpr auto STATUS_TRUE = "true";
constexpr auto STATUS_FALSE = "false";
constexpr auto STATUS_ERROR = "error";
constexpr auto STATUS_ENTER = "enter";
constexpr auto STATUS_ACTION = "action";
constexpr auto STATUS_HANDLE = "handle";
constexpr auto STATUS_VISIBLE = "visible";
constexpr auto STATUS_INVISIBLE = "invisible";
constexpr auto STATUS_OPEN = "open";
constexpr auto STATUS_PLAY = "play";
constexpr auto STATUS_PAUSE = "pause";
constexpr auto STATUS_STATE = "state";

/*************** position ****************/
constexpr auto POSITION = "position";

/*************** mode ****************/
constexpr auto MODE = "mode";
constexpr auto PASSWORD_MODE = "password";
constexpr auto NORMAL_MODE = "normal";
constexpr auto LIVE_MODE = "liveMode";
constexpr auto RECORD_MODE = "recordMode";

/*************** version ****************/
constexpr auto DEFAULT_PLS_VERSION = "2.0.1.001";

/*************** state ****************/
constexpr auto READY = "ready";
constexpr auto GOLIVE = "golive";
constexpr auto RECORD = "record";
constexpr auto FINISH = "finish";
constexpr auto LIVE = "live";
constexpr auto PRESSED = "pressed";

constexpr auto CONTROLLER_TYPE = "controllerType";
constexpr auto PASSWORD = "password";

/*****************language******************/
constexpr auto EN_US = "en-US";
constexpr auto KO_KR = "ko-KR";
constexpr auto ZH_CN = "zh-CN";
constexpr auto LANGUAGE_ENGLISH = "English";

// Because of the Korean encoding format is not utf-8, it is expressed in octal code
constexpr auto LANGUAGE_KOREAN = "\355\225\234\352\265\255\354\226\264";
constexpr auto LANGUAGE_ENGLISH_TRANSLATE_PATH = "en-us/strings.xml";
constexpr auto LANGUAGE_KOREAN_TRANSLATE_PATH = "ko-kr/strings.xml";

constexpr auto LANGUAGE_SETTING_ENGLISH = "en-US,en;q=0.9";
constexpr auto LANGUAGE_SETTING_KOREAN = "ko-KR,ko;q=0.9";

constexpr auto ENCODING_SETTINGS_GUIDE_EN_US_URL = "http://prismlive.com/en_us/faq/faq.html?app=pcapp";
constexpr auto ENCODING_SETTINGS_GUIDE_KO_KR_URL = "http://prismlive.com/ko_kr/faq/faq.html?app=pcapp";

/*****************resources******************/
constexpr auto RESOURCES_PATH = "resources/";
constexpr auto RESOURCES_STYLES_PATH = "resources/styles/";
constexpr auto RESOURCES_LANGUAGES_PATH = "resources/languages/";

/***************popup objname****************/
constexpr auto TITLEBAR_VIEW = "TitleBarView";
constexpr auto CONTENT_VIEW = "ContentView";
constexpr auto DIALOG_VIEW = "DialogView";
constexpr auto MAINWINDOW_VIEW = "MainWindowView";
constexpr auto SETTINGS_VIEW = "SettingsView";

constexpr auto CONNECT_LIVEPLATFORM_VIEW = "ConnectLivePlatformView";
constexpr auto ADD_RTMP_CHANNEL_VIEW = "AddRtmpChannelView";
constexpr auto LOGIN_BACKGROUND_FRAME = "LoginBackgroundFrame";
constexpr auto TERMS_OF_AGREE_VIEW = "TermOfUserFrame";
constexpr auto LOGIN_WITH_EMAIL_VIEW = "LoginWithEmailView";
constexpr auto SIGNUP_WITH_EMAIL_VIEW = "SignupWithEmailView";
constexpr auto LOGIN_BACKGROUND_VIEW = "LoginBackgroundView";
constexpr auto CHANNEL_SETTINGS_VIEW = "ChannelSettingsView";
constexpr auto CHANNEL_SETTINGS_BLANK_VIEW = "ChannelSettingsBlankView";
constexpr auto CHANNEL_SETTINGS_ITEM_VIEW = "ChannelSettingsItemView";
constexpr auto SETTING_COMMON_LOADING_VIEW = "SettingCommonLoadingView";
constexpr auto LOGIN_SNS_VIEW = "SelectLoginPlatformView";
constexpr auto LOGIN_NAVER_CLOUDB2B__VIEW = "NAVER Cloud B2B";

/***************struct*********************/
constexpr auto SNS_CHANNEL_DATA = "SnsChannelData";
constexpr auto RTMP_CHANNEL_DATA = "RtmpChannelData";
constexpr auto RTMP_CHANNEL_Type = "RtmpChannelType";
constexpr auto CHANNEL_DATA = "ChannelData";
constexpr auto CHANNEL_TYPE = "ChannelType";
constexpr auto SNS_ALL_CHANNEL_DATA = "SnsAllChannelData";
constexpr auto RTMP_ALL_CHANNEL_DATA = "RtmpAllChannelData";
constexpr auto VIDEO_ENCODER_MAP = "VideoEncoderMap";
constexpr auto SNS_AUTH_MAP = "SnsAuthMap";
constexpr auto RtmpChannelDataVec = "RtmpChannelDataVec";

/***************configs*********************/
constexpr auto CONFIG_SHOW_MODE = "showMode";
constexpr auto CONFIGS_GROUP_LOGIN = "login";
constexpr auto CONFIGS_GROUP_COOKIE = "prism_cookie";
constexpr auto CONFIGS_LOGIN_TOKEN = "login/token";
constexpr auto CONFIGS_FILE_PATH = " /configs/prismconfig.ini";
constexpr auto CONFIGS_CHANNEL_LIST_PATH = "channellist.txt";
constexpr auto CONFIGS_CUSTOM_ENCODER_PATH = "customencoder.json";
constexpr auto CONFIGS_LIBRARY_ENCODER_POLICY_ZIP_PATH = "library_Encoder_Policy_PC.zip";
constexpr auto CONFIGS_GCC_PATH = "PRISMLiveStudio/user/gcc.json";
constexpr auto CONFIGS_USER_CONFIG_PATH = "PRISMLiveStudio/user/cache";
constexpr auto CONFIGS_CAM_USER_CONFIG_PATH = "PRISMLens/user/cache";
constexpr auto CONFIGS_USER_THUMBNAIL_PATH = "PRISMLiveStudio/user/prismThumbnail.png";
constexpr auto CONFIGS_USER_TEXTMOTION_PATH = "PRISMLiveStudio/textmotion/%1";
constexpr auto CONFIGS_CATEGORYS_PATH = "PRISMLiveStudio/user/categorys.json";
constexpr auto CONFIGS_CATEGORYS_LIBRARY_PATH = "PRISMLiveStudio/user/library.json";
constexpr auto CONFIGS_RESOLUTIONGUIDE_PATH = "PRISMLiveStudio/resources/library/Library_Policy_PC/ResolutionGuide.json";
constexpr auto CONFIGS_GPOP_PATH = "PRISMLiveStudio/user/gpop.json";
constexpr auto CONFIGS_BEATURY_USER_PATH = "PRISMLiveStudio/beauty/";
constexpr auto CONFIGS_BEATURY_JSON_FILE = "beauty.json";
constexpr auto CONFIGS_BEATURY_PRESET_IMAGE_PATH = "/thumb x3/";
constexpr auto CONFIGS_BEATURY_CUSTOM_IMAGE_PATH = "beauty_";
constexpr auto CONFIGS_BEATURY_DEFAULT_IMAGE_PATH = "PRISMLiveStudio/beauty/image/";
constexpr auto BEAUTY_CONFIG = "BeautyConfig";
constexpr auto GIPHY_STICKERS_CONFIG = "GiphyStickersConfig";
constexpr auto GIPHY_STICKERS_USER_PATH = "PRISMLiveStudio/sticker/";
constexpr auto GIPHY_STICKERS_CACHE_PATH = "PRISMLiveStudio/sticker/cache";
constexpr auto GIPHY_STICKERS_JSON_FILE = "PRISMLiveStudio/sticker/sticker.json";
constexpr auto CONFIGS_VIRTUAL_CAMERA_PATH = "PRISMLiveStudio/user/prism-virtualcam.txt";
constexpr auto CONFIGS_LIBRARY_POLICY_PATH = "PRISMLiveStudio/resources/library/Library_Policy_PC/";
constexpr auto CONFIGS_LIBRARY_POLICY_LISCENSE_PATH = "license";

constexpr auto PRISM_STICKER_USER_PATH = "PRISMLiveStudio/prism_sticker/";
constexpr auto PRISM_STICKER_CACHE_PATH = "PRISMLiveStudio/prism_sticker/cache/";
constexpr auto PRISM_STICKER_JSON_FILE = "PRISMLiveStudio/reaction/reaction.json";
constexpr auto PRISM_STICKER_DOWNLOAD_CACHE_FILE = "PRISMLiveStudio/prism_sticker/download_cache.json";
constexpr auto PRISM_STICKER_RECENT_JSON_FILE = "PRISMLiveStudio/prism_sticker/recent_used.json";
constexpr auto PRISM_DEFAULT_STICKER_ICON = ":/images/giphy/thumb-loading.svg";

constexpr auto VIRTUAL_BACKGROUND_CONFIG = "VirtualbackgroundConfig";

constexpr auto SENSETIME_UNZIP_PATH = "PRISMLiveStudio/beauty";
constexpr auto SENSETIME_ZIP = "PRISMLiveStudio/beauty/library_SenseTime_PC.zip";
constexpr auto SENSETIME_FILE_PATH = "PRISMLiveStudio/beauty/library_SenseTime_PC/";
constexpr auto SENSETIME_NEW_VERSION_FILE_PATH = "PRISMLiveStudio/beauty/library_SenseTime_PC/2.5.0/";
constexpr auto SENSETIME_NEW_VERSION_FILE_NAME = "sense_license_encode.lic";
constexpr auto SENSETIME_OLD_FILE_NAME = "sensetime_license.lic";
constexpr auto SENSETIME_NEW_FILE_NAME = "license_online.lic";
constexpr auto SENSETIME_VERSION = "2.5.0";

constexpr auto BGM_CONFIG = "BgmConfig";
constexpr auto CONFIGS_MUSIC_USER_PATH = "PRISMLiveStudio/music/";
constexpr auto MUSIC_JSON_FILE = "music.json";
constexpr auto BGM_MUSIC_PLAYING_GIF = ":/resource/images/bgm/BGM_equalizer.gif";
constexpr auto CONFIG_MUSIC_PATH = "data/prism-studio/music/";

constexpr auto SCENE_TEMPLATE_DIR = "PRISMLiveStudio/scene-templates/";
constexpr auto SCENE_TEMPLATE_JSON = "PRISMLiveStudio/scene-templates/SceneTemplates.json";
constexpr auto LAUNCHER_CONFIG = "LauncherConfig";
constexpr auto CONFIG_DONTSHOW = "DontShow";
constexpr auto AUDIO_MIXER_CONFIG = "AudioMixerConfig";

constexpr auto NEWFUNCTIONTIP_CONFIG = "NewFunctionTipConfig";
constexpr auto CONFIG_DISPLAYVERISON = "DisplayVerison";

/* **the keys of source settings need to be same with bgm source plugins ** */
constexpr auto RANDOM_PLAY = "random play";
constexpr auto PLAY_IN_ORDER = "play in order";
constexpr auto BGM_GROUP = "group";
constexpr auto PLAY_LIST = "play list";
constexpr auto BGM_TITLE = "title";
constexpr auto BGM_PRODUCER = "producer";
constexpr auto BGM_URL = "music";
constexpr auto BGM_DURATION = "duration";
constexpr auto BGM_DURATION_TYPE = "duration_type";
constexpr auto IS_LOOP = "is_loop";
constexpr auto IS_SHOW = "is_show";
constexpr auto BGM_IS_CURRENT = "is_current";
constexpr auto BGM_IS_LOCAL_FILE = "is_local_file";
constexpr auto BGM_IS_DISABLE = "is_disable";
constexpr auto BGM_HAVE_COVER = "has_cover";
constexpr auto BGM_COVER_PATH = "cover_path";
constexpr auto BGM_URLS = "urls";

/* **the keys of source settings need to be same with bgm source plugins ** */

/**************prism login******/
constexpr auto NET_REQUEST_TIME_OUT = 10000;
constexpr auto COOKIES = "cookies";
constexpr auto JUMPURL = "redrectUrl";
constexpr auto LOGIN_LOADING_TIME = 800;
constexpr auto SNS_LOGIN_URL = "snsLoginUrl";
constexpr auto LOGIN_USERINFO_EMAIL = "email";
constexpr auto LOGIN_USERINFO_PASSWORD = "password";
constexpr auto LOGIN_USERINFO_NICKNAME = "nickname";
constexpr auto LOGIN_USERINFO_AGREEMENT = "agreement";
constexpr auto LOGIN_USERINFO_USERCODE = "userCode";
constexpr auto LOGIN_USERINFO_PROFILEURL = "profileThumbnailUrl";
constexpr auto LOGIN_USERINFO_AUTHTYPE = "authType";
constexpr auto LOGIN_USERINFO_TOKEN = "token";
constexpr auto LOGIN_USERINFO_HASHUSERCODE = "hashedUserCode";
constexpr auto LOGIN_USERINFO_AUThSTATUS_CODE = "authStatusCode";
constexpr auto LOGIN_USERINFO_USER_CODE = "userCode";
constexpr auto LOGIN_CODE = "code";
constexpr auto LOGIN_AUTHTYPE_AFREECATV = "afreecaTv";
constexpr auto LOGIN_AUTHTYPE_YOUTUBE = "YouTube";
constexpr auto LOGIN_AUTHTYPE_WAV = "wav";
constexpr auto LOGIN_AUTHTYPE_VLIVE = "vlive";
constexpr auto LOGIN_AUTHTYPE_GOOGLE = "google";
constexpr auto LOGIN_AUTHTYPE_FACEBOOK = "facebook";
constexpr auto LOGIN_AUTHTYPE_TWITCH = "twitch";
constexpr auto LOGIN_AUTHTYPE_TWITTER = "twitter";
constexpr auto LOGIN_AUTHTYPE_LINE = "line";
constexpr auto LOGIN_AUTHTYPE_NAVER = "naver";
constexpr auto LOGIN_AUTHTYPE_EMAIL = "email";
constexpr auto LOGIN_AUTHTYPE_CUSTOMRTMP = "CustomRTMP";
constexpr auto HTTP_CONTENT_TYPE_VALUE = "application/json;charset=UTF-8";

/**************channel login************************/
constexpr auto HTTP_CONTENT_TYPE_URL_ENCODED_VALUE = "application/x-www-form-urlencoded;charset=UTF-8";
constexpr auto HTTP_GCC_KR = "KR";

/**************channel cookie************************/
constexpr auto COOKIE_NEO_SES = "NEO_SES";
constexpr auto COOKIE_NEO_CHK = "NEO_CHK";
constexpr auto COOKIE_SNS_TOKEN = "snsToken";
constexpr auto COOKIE_SNS_CD = "snsCd";
constexpr auto COOKIE_OAUTH_TOKEN = "oauth_token";
constexpr auto COOKIE_NID_SES = "NID_SES";
constexpr auto COOKIE_NID_INF = "nid_inf";
constexpr auto COOKIE_NID_AUT = "NID_AUT";
constexpr auto COOKIE_ACCESS_TOKEN = "access_token";
constexpr auto COOKIE_EXPIRES_IN = "expires_in";
constexpr auto COOKIE_TOKEN_TYPE = "token_type";
constexpr auto COOKIE_REFRESH_TOKEN = "refresh_token";

constexpr auto CHANNEL_NICKNAME = "nickname";
constexpr auto CHANNEL_NAME = "channelName";
constexpr auto CHANNEL_COOKIE_VLIVE = "VLive_root";
constexpr auto CHANNEL_COOKIE_TWITCH = "Twitch_root";
constexpr auto CHANNEL_COOKIE_WAV = "Wav_root";
constexpr auto CHANNEL_COOKIE_YOUTUBE = "Youtube_root";
constexpr auto CHANNEL_COOKIE_NAVERTV = "NaverTv_root";
constexpr auto CHANNEL_RTMP_LIST = "Rtmp_List";
constexpr auto HTTP_CODE = "code";
constexpr auto HTTP_CLIENT_SECRET = "client_secret";
constexpr auto HTTP_CLIENT_ID = "client_id";
constexpr auto HTTP_GRANT_TYPE = "grant_type";
constexpr auto HTTP_REDIRECT_URI = "redirect_uri";
constexpr auto HTTP_ACCEPT = "Accept";
constexpr auto HTTP_ACCEPT_TWITCH = "application/vnd.twitchtv.v5+json";
constexpr auto HTTP_DISPLAY_NAME = "display_name";
constexpr auto HTTP_NAME = "name";
constexpr auto HTTP_IN_AUTH_CHANNEL = "inAuthChannel";
constexpr auto HTTP_AUTHORIZATION_CODE = "authorization_code";
constexpr auto HTTP_AUTHORIZATION = "Authorization";
constexpr auto HTTP_PART = "part";
constexpr auto HTTP_PART_SNPIIPT = "snippet";
constexpr auto HTTP_MINE = "mine";
constexpr auto HTTP_MINE_TRUE = "true";

constexpr auto HTTP_REFERER = "Referer";
constexpr auto HTTP_FAILURE = "Failure";
constexpr auto HTTP_SUCCESS = "success";
constexpr auto HTTP_RTN_MSG = "rtn_msg";
constexpr auto HTTP_NICK_NAME = "nick_name";
constexpr auto HTTP_ITEM_ID = "itemId";
constexpr auto HTTP_ITEMS = "items";
constexpr auto HTTP_LIBRARY_1743 = "LIBRARY_1743";
constexpr auto HTTP_RESOURCE_URL = "resourceUrl";
constexpr auto CATEGORY_ID = "categoryId";
constexpr auto COLOR_THUMBNAILURL = "thumbnailUrl";
constexpr auto COLOR = "color";
constexpr auto BEAUTY = "beauty";
constexpr auto MUSIC = "music";
constexpr auto LIBRARY = "library";
constexpr auto HTTP_MESSAGE = "message";
constexpr auto RTMP_SEQ = "rtmpSeq";
constexpr auto STREAM_NAME = "streamName";
constexpr auto RTMP_URL = "rtmpUrl";
constexpr auto DESCRIPTION = "description";
constexpr auto STREAM_KEY = "streamKey";
constexpr auto ABP_FLAG = "abpFlag";
constexpr auto CONFIG_RESOLUTION = "resolution";
constexpr auto BITRATE = "bitrate";
constexpr auto FRAMERATE = "framerate";
constexpr auto INTERVAL = "interval";
constexpr auto USERNAME = "username";
constexpr auto META = "meta";
constexpr auto ENCODER_POLICIES = "encoderPolicies";
constexpr auto VIDEO_POLICIES = "videoPolicies";

constexpr auto COLOR_FILTER_COLOR = "color";
constexpr auto COLOR_FILTER_IMAGE_FORMAT_PNG = ".png";
constexpr auto COLOR_FILTER_THUMBNAIL = "_thumbnail";
constexpr auto COLOR_FILTER_JSON_FILE = "color_filter.json";
constexpr auto COLOR_FILTER_TMP_PATH = "tmp/";
constexpr auto CONFIG_COLOR_FILTER_PATH = "data/prism-studio/color_filter/";
constexpr auto CONFIG_BEAUTY_PATH = "data/prism-studio/beauty/";
constexpr auto CONFIG_BEAUTY_IMAGE_PATH = "data/prism-studio/beauty/image/";
constexpr auto CONFIG_BEAUTY_TMP_PATH = "tmp/";
constexpr auto CONFIGS_COLOR_FILTER_USER_PATH = "PRISMLiveStudio/color_filter/";

constexpr auto COLOR_FILTER_ORDER_NUMBER = 10;

/************Settings************************/

constexpr auto SETTINGS_NONE = "None";
constexpr auto BFRAME_ZERO = "0";
constexpr auto BASE_LINE = "Baseline";
constexpr auto MAIN = "Main";
constexpr auto HIGH = "High";
constexpr auto QUALITY = "Quality";
constexpr auto SPEED = "Speed";
constexpr auto BALANCED = "Balanced";
constexpr auto VERY_FAST = "Very Fast";
constexpr auto FASTER = "Faster";
constexpr auto MEDIUM = "Medium";
constexpr auto SLOW = "Slow";
constexpr auto VERY_SLOW = "Very Slow";
constexpr auto PLACEBO = "Placebo";
constexpr auto INTERVAL_ONE = "1s";
constexpr auto INTERVAL_TWO = "2s";
constexpr auto INTERVAL_THEREE = "3s";
constexpr auto INTERVAL_FOUR = "4s";
constexpr auto INTERVAL_FIVE = "5s";
constexpr auto RATE_CONTROL_CBR = "CBR";
constexpr auto RATE_CONTROL_VBR = "VBR";
constexpr auto RATE_CONTROL_VCM = "VCM";
constexpr auto RATE_CONTROL_CQP = "CQP";
constexpr auto RATE_CONTROL_AVBR = "AVBR";
constexpr auto RATE_CONTROL_ICQ = "ICQ";
constexpr auto RATE_CONTROL_LA = "LA";
constexpr auto RATE_CONTROL_LA_ICQ = "LA_ICQ";
constexpr auto FILM = "Film";
constexpr auto ANIMATION = "Animation";
constexpr auto GRAIN = "Grain";
constexpr auto STILL_IMAGE = "Still Image";
constexpr auto PSNR = "Psnr";
constexpr auto SSIM = "Ssim";
constexpr auto FAST_DECODE = "Fast Decode";
constexpr auto ZERO_LATENCY = "Zero Latency";
constexpr auto FPS_SIXTY = "60fps";
constexpr auto FPS_THIRTY = "30fps";
constexpr auto ENCODER_X264 = "x.264";
constexpr auto ENCODER_H264 = "QuickSync H.264";
constexpr auto BITRATE_4000 = "4000";
constexpr auto BITRATE_6000 = "6000";
constexpr auto BITRATE_96K = "96k";
constexpr auto BITRATE_128K = "128k";
constexpr auto BITRATE_160K = "160k";
constexpr auto BITRATE_192K = "192k";
constexpr auto RECORD_FORMAT_MP4 = "mp4";
constexpr auto RECORD_FORMAT_F4V = "f4v";
constexpr auto RECORD_FORMAT_FLV = "flv";
constexpr auto RECORD_FORMAT_MOV = "mov";
constexpr auto VIDEO_SIZE_2048 = "2048";
constexpr auto AUDIO_ENCODER_FDK_AAC = "FDK-AAC";
constexpr auto AUDIO_ENCODER_FOUNDATION_AAC = "Media Foundation AAC";
constexpr auto AUDIO_CODEC_AACLC = "AAC-LC";
constexpr auto AUDIO_SAMPLE_44_1KHZ = "44.1kHz";
constexpr auto AUDIO_SAMPLE_48KHZ = "48kHz";
constexpr auto AUDIO_DEFAULT_SPEAKERS = "Default Speakers";
constexpr auto ENCODER_1080P_WITH_60FPS = "1080p60";
constexpr auto ENCODER_1080P_WITH_30FPS = "1080p30";
constexpr auto ENCODER_720P_WITH_60FPS = "720p60";
constexpr auto ENCODER_720P_WITH_30FPS = "720p30";
constexpr auto VIDEO_POLICY_TYPE_COMMON = "common";
constexpr auto VIDEO_POLICY_TYPE_RECORD = "record";
constexpr auto VIDEO_POLICY_TYPE_SIMULCAST = "simulcast";
constexpr auto VIDEO_POLICY_TYPE_VLIVE = "vlive";
constexpr auto VIDEO_POLICY_TYPE_WAV = "wav";
constexpr auto VIDEO_POLICY_TYPE_NAVERTV = "navertv";
constexpr auto VIDEO_POLICY_TYPE_YOUTUBE = "youtube";
constexpr auto VIDEO_POLICY_TYPE_TWITCH = "twitch";
constexpr auto VIDEO_POLICY_TYPE_FACEBOOK = "facebook";
constexpr auto VIDEO_POLICY_TYPE_AFREECATV = "afreecatv";
constexpr auto VIDEO_POLICY_TYPE_PERISCOPE = "periscope";

constexpr auto QUICK_SYNC = "quickSync";
constexpr auto X264 = "x264";
constexpr auto RATE_CONTROL_METHOD = "rateControlMethod";

constexpr auto RECOMMENDED = "Recommended";
constexpr auto RATE_CONTROL = "Rate Control";
constexpr auto KEYFRAME_INTERVAL = "keyframe Interval";
constexpr auto KEY_FRAME_INTERVAL_SEC = "keyframeIntervalSec";
constexpr auto PRESET = "preset";
constexpr auto PROFILE = "profile";
constexpr auto TUNE = "tune";
constexpr auto ENCODER = "Encoder";
constexpr auto RESOLUTION = "Resolution";
constexpr auto FRAME_RATE = "Frame Rate";
constexpr auto VIDEO_QUALITY = "Video Quality";
constexpr auto VIDEO_SAVE_IN = "Video Save In";
constexpr auto VIDEO_SIZE = "Video Size";
constexpr auto FILE_NAME = "File Name";
constexpr auto RECORD_FROMAT = "Record Format";
constexpr auto AUDIO_ENCODER = "Audio Encoder";
constexpr auto AUDIO_CODEC = "Audio Codec";
constexpr auto AUDIO_SAMPLE = "Audio Sample";
constexpr auto AUDIO_BITRATE = "Audio Bitrate";
constexpr auto MONITORING_DEVICE = "Monitoring Device";
constexpr auto STREAM_ROOT = "stream_root";
constexpr auto RECORD_ROOT = "record_root";
constexpr auto AUDIO_ROOT = "audio_root";

/************RegExp************************/
constexpr auto EMAIL_REGEXP = "^[\\w!#$\\\\%&'*+/=?`{|}~^-]+(?:\\.[\\w!#$\\\\%&'*+/=?`{|}~^-]+)*@(?:[a-zA-Z0-9-]+\\.)+[a-zA-Z]{2,6}$";

constexpr auto PASSWORD_REGEXP = "((?=.*\\d)(?=.*[a-z,A-Z])(?=.*[_\\-?/!@#$%^&*()+=,.`\\[\\]{}<>:;'\"~\\\\]).{8,20})";
constexpr auto NICK_REGEXP = "[^.]{1,20}";
constexpr auto SNS_LOG_URL_REGEXP = "https://([\\w-]+\\.)+apis.naver.com+(/[\\w-]*)+\\/callback\\?\\w.*";
constexpr auto TWITTER_URL_FILTER = "#_=";
constexpr auto GOOGLE_URL_FILTER = "&scope=";
constexpr auto REGEXP_NUMBER = "[0-9]{1,}";

/***************httpMgr******************/
constexpr auto HTTP_HEAD_OS = "X-prism-os";
constexpr auto HTTP_HEAD_APP_VERSION = "X-prism-appversion";
constexpr auto HTTP_HMAC_STR = "apis.naver.com/prism";
constexpr auto HTTP_COOKIE = "Cookie";
constexpr auto HTTP_HEAD_CONTENT_TYPE = "Content-Type";
constexpr auto HTTP_HEAD_CC_TYPE = "X-prism-cc";
constexpr auto HTTP_USER_AGENT = "User-Agent";
constexpr auto HTTP_DEVICE_ID = "deviceId";
constexpr auto HTTP_VERSION = "version";
constexpr auto HTTP_GCC = "gcc";
constexpr auto HTTP_WITH_ACTIVITY_INFO = "withActivityInfo";
constexpr auto NETWORK_CHECK_URL = "apis.naver.com";

/************login response code *********/
constexpr auto HTTP_NOT_EXIST_USER = 1110;
constexpr auto HTTP_RESTRICT_USER = 1120;
constexpr auto HTTP_PW_FAIL = 1210;
constexpr auto HTTP_PW_RETYR_FAIL = 1220;
constexpr auto HTTP_PW_EXPIRED = 1230;
constexpr auto HTTP_IP_BLOCKED = 12310;
constexpr auto INTERNAL_ERROR = 0;
constexpr auto SIGNUP_NO_AGREE = 10000;
constexpr auto JOIN_EXIST_ID = 2120;
constexpr auto JOIN_SESSION_EXPIRED = 2320;
constexpr auto HTTP_STATUS_CODE_202 = 202;
constexpr auto HTTP_STATUS_CODE_400 = 400;
constexpr auto HTTP_STATUS_CODE_500 = 500;
constexpr auto HTTP_STATUS_CODE_501 = 501;
constexpr auto HTTP_STATUS_CODE_406 = 406;
constexpr auto HTTP_STATUS_CODE_401 = 401;
constexpr auto HTTP_STATUS_CODE_403 = 403;
constexpr auto HTTP_STATUS_CODE_404 = 404;
constexpr auto HTTP_TOKEN_INVAILD_CODE_3000 = 3000;

/************http response code *********/
constexpr auto HTTP_STATUS_CODE_200 = 200;

/***********reset password email********/
constexpr auto NO_EXIST_EMAIL = 9010;

/****************  combobox *************/
constexpr auto COMBOBOX_MAX_VISIBLE_NUMBER = 5;

/**************scene module****************/
constexpr auto SCENE = "scene";
constexpr auto SCENE_DRAG_MIME_TYPE = "sceneItem";
constexpr auto SCENE_DRAG_GRID_MODE = "gridMode";
constexpr auto FILTER_DRAG_MIME_TYPE = "filterItem";
constexpr auto BUTTON_CHECK = "check";
constexpr auto SCENE_MOVE_STEP = 6;
constexpr auto SCENE_SCROLLCONTENT_OFFSET = 5;
constexpr auto SCENE_SCROLLCONTENT_ITEM_HEIGHT = 68;
constexpr auto SCENE_SCROLLCONTENT_ITEM_WIDTH = 81;
constexpr auto SCENE_SCROLLCONTENT_LINE_COLOR = "#effc35";
constexpr auto SCENE_SCROLLCONTENT_DEFAULT_COLOR = "#272727";
constexpr auto SCENE_SCROLLCONTENT_COLUMN = 2;
constexpr auto SCENE_TRANSITION_DEFAULT_DURATION_VALUE = 300;
constexpr auto SCENE_ITEM_AUTO_SCROLL_MARGIN = 15;
/**************source module****************/
constexpr auto SOURCE_ITME_WIDTH = 45;
constexpr auto SOURCE_MAXINTERVAL = 5;
constexpr auto SOURCE_ROW_INTERVAL = (-2);
constexpr auto DRAG_MIME_TYPE = "sceneItem";
constexpr auto SOURCE_DRAG_MIME_TYPE = "CustomListView/text-icon-icon_hover";
constexpr auto SOURCE_VISABLE = "visable";
constexpr auto SOURCE_MOVE = "move";
constexpr auto SOURCE_WEB = "url";
constexpr auto SOURCE_GAME = "game";
constexpr auto SOURCE_TEXT = "text";
constexpr auto SOURCE_AUDIO = "audio";
constexpr auto SOURCE_IMAGE = "image";
constexpr auto SOURCE_MEDIA = "media";
constexpr auto SOURCE_CAMERA = "camera";
constexpr auto SOURCE_DISPLAY = "display";
constexpr auto SOURCE_SCREENSHOT = "screenshot";
constexpr auto ONE_ZERO_EIGHT_ZERO_P_THREE_ZERO_FPS = "1080P.30fps";
constexpr auto SOURCE_ICON_PATH = "skin/";
constexpr auto SOURCE_ITEM_NORMAL_ICON = "icon-source-%1.png";
constexpr auto SOURCE_ITEM_DISABLE_ICON = "icon-source-%1-disable.png";
constexpr auto SOURCE_ITEM_SELECT_ICON = "icon-source-%1-select.png";
constexpr auto SOURCE_ITEM_SELECT_DISABLE_ICON = "icon-source-%1-select-disable.png";
constexpr auto SOURCE_ITEM_NORMAL_ERRORICON = "erroricon-source-%1.png";
constexpr auto SOURCE_ITEM_DISABLE_ERRORICON = "erroricon-source-%1-disabled.png";
constexpr auto SOURCE_ITEM_SELECT_ERRORINCON = "erroricon-source-%1-select.png";
constexpr auto SOURCE_ITEM_SELECT_DISABLE_ERRORICON = "erroricon-source-%1-select-disable.png";
constexpr auto SOURCE_ITEM_HOVER_NORMAL_ICON = "icon-source-move.png";
constexpr auto SOURCE_ITEM_HOVER_SELECT_ICON = "icon-source-move-select.png";
constexpr auto SOURCE_ITEM_HOVER_SELECT_DISABLE_ICON = "icon-source-move-select-disable.png";
constexpr auto SOURCE_MENUSHOW_OFFSET = 10;
constexpr auto SOURCE_POLYGON = 4;
constexpr auto SOURCE_WIDTH = 1;

/************* menu manager module *****/
constexpr auto MENU_ICON_SIZE = 30;
constexpr auto MENU_SOURCEADD_DISPLAY = "skin/icon-source-display.png";
constexpr auto MENU_SOURCEADD_WORDS = "skin/icon-source-text.png";
constexpr auto MENU_SOURCEADD_WINDOW = "skin/icon-source-window.png";
constexpr auto MENU_SOURCEADD_SCREENSOT = "skin/icon-source-screenshot.png";
constexpr auto ACTION_SOURCEADD_GAME = "skin/icon-source-game.png";
constexpr auto ACTION_SOURCEADD_VIDEOCAPTURE = "skin/icon-source-camera.png";
constexpr auto ACTION_SOURCEADD_AUDIOACPTURE = "skin/icon-source-audio.png";
constexpr auto ACTION_SOURCEADD_VIDEOFILES = "skin/icon-source-media.png";
constexpr auto ACTION_SOURCEADD_IMAGE = "skin/icon-source-image.png";
constexpr auto ACTION_SOURCEADD_BROSWER = "skin/icon-source-url.png";
constexpr auto ACTION_SHORTCUT_OPEN = "Ctrl+O";
constexpr auto ACTION_SHORTCUT_SAVE = "Ctrl+S";
constexpr auto ACTON_SHORTCUT_SAVEAS = "Shift+Ctrl+S";
constexpr auto ACTION_SHORTCUT_EXIT = "Ctrl +Q";
constexpr auto ACTION_SHORTCUT_READY = "Ctrl+T";
constexpr auto ACTION_SHORTCUT_STARTBROADCAST = "Ctrl+N";
constexpr auto ACTION_SHORTCUT_ENDBROADCAST = "Ctrl+E";
constexpr auto ACTION_SHORTCUT_BROADCASETSET = "Ctrl+B";
constexpr auto ACTION_SHORTCUT_STARTECORD = "Ctrl+I";
constexpr auto ACTION_SHORTCUT_ENDRECORD = "Ctrl+F";
constexpr auto ACTION_SHORTCUT_UNDO = "Ctrl+Z";
constexpr auto ACTION_SHORTCUT_REDO = "Ctrl+Y";
constexpr auto ACTION_SHORTCUT_CUT = "Ctrl+X";
constexpr auto ACTION_SHORTCUT_COPY = "Ctrl+C";
constexpr auto ACTION_SHORTCUT_PASTE = "Ctrl+V";
constexpr auto ACTION_SHORTCUT_CROPMODE = "Alt";
constexpr auto ACTION_SHORTCUT_PROPERTIES = "Ctrl+R";
constexpr auto ACTION_SHORTCUT_REMOVE = "Del";
constexpr auto ACTION_SHORTCUT_FLIPHORZ = "Alt+H";
constexpr auto ACTION_SHORTCUT_FLIPVERT = "Alt+V";
constexpr auto ACTION_SHORTCUT_ADDSCENE = "Shift+Ctrl+N";
constexpr auto ACTION_SHORTCUT_DUPSCENE = "Shift+Ctrl+C";
constexpr auto ACTION_SHORTCUT_RENAMESCENE = "Shift+Ctrl+D";
constexpr auto ACTION_SHORTCUT_FULLSCREEN = "F11";
constexpr auto ACTION_SHORTCUT_ALWAYSONTOP = "Shift+Ctrl+T";
constexpr auto ACTION_SHORTCUT_NOEFFECT = "Alt+E";

/****************msg box**************/
constexpr auto MESSAGEBOX_BUTTON_OBJECTNAME = "msgButton";
constexpr auto MESSAGEBOX_YES = "Yes";
constexpr auto MESSAGEBOX_NO = "No";
constexpr auto MESSAGEBOX_LAYOUT_SPACE = 10;
constexpr auto MESSAGEBOX_LAYOUT_MARGIN = 0;

/*************** font ****************/
constexpr auto FILE_VERSION_PATH = "version_win_dev_qt.txt";
constexpr auto FONT_NANUM_GOTHIC_PATH = "resources/font/NanumGothic.ttf";
constexpr auto FONT_NANUM_GOTHIC_BOLD_PATH = "resources/font/NanumGothicBold.ttf";
constexpr auto DEFAULT_FONT_FAMILY = "Arial";

/*************** time format ****************/
constexpr auto TIME_FORMAT = "00:00:00";
constexpr auto TIME_ZERO = "0";

/*************** param ****************/
constexpr auto PERCENT = "%";
constexpr auto KBPS = "kbps";
constexpr auto FPS = "fps";
constexpr auto ONE_POINT = ".";
constexpr auto PRECISION_G = 'g';
constexpr auto PRECISION_FOUR = 4;
constexpr auto EMPTY_STRING = "";
constexpr auto ONE_SPACE = " ";
constexpr auto COLON = ":";
constexpr auto PIXMAP = "pixMap";
constexpr auto TYPE = "type";
constexpr auto TYPE_PICTURE = "picture";
constexpr auto TYPE_TEXT = "text";
constexpr auto IMAGE_URL = "imageUrl";
constexpr auto PNG = "PNG";
constexpr auto OBJECT = "object";
constexpr auto SELECT = "Select";
constexpr auto INDEX = "index";
constexpr auto CQSTR_STRING_BUNDLE = "stringbundle";
constexpr auto CQSTR_STRING = "string";
constexpr auto CQSTR_ID = "id";
constexpr auto VIEW = "view";
constexpr auto RESULT = "result";
constexpr auto TITLE = "title";
constexpr auto SHORT_TITLE = "shortTitle";

constexpr auto VIDEO_SAVE_DEFAULT_PATH = "C:\\Users\\admin";
/*********prism notice*************/
constexpr auto NOTICE_NOTICE_SEQ = "noticeSeq";
constexpr auto NOTICE_COUNTRY_CODE = "countryCode";
constexpr auto NOTICE_OS = "os";
constexpr auto NOTICE_OS_VERSION = "osVersion";
constexpr auto NOTICE_APP_VERSION = "appVersion";
constexpr auto NOTICE_TITLE = "title";
constexpr auto NOTICE_CONTENE = "content";
constexpr auto NOTICE_DETAIL_LINK = "detailLink";
constexpr auto NOTICE_CONTENT_TYPE = "contentType";
constexpr auto NOTICE_CREATED_AT = "createdAt";
constexpr auto NOTICE_VAILD_UNTIL_AT = "validUntilAt";
/*************** const int variable ****************/
constexpr auto LOADING_PICTURE_MAX_NUMBER = 8;
constexpr auto LOADING_TIMER_TIMEROUT = 100; // ms
constexpr auto TIMING_TIMEOUT = 1000;        // ms
constexpr auto CPU_TIMER_TIMEOUT = 1000;     // ms
constexpr auto FEED_UI_MAX_TIME = 100;       // ms
constexpr auto MAINWINDOW_MIN_WIDTH = 625;
constexpr auto ONE_HOUR_MINUTES = 60;
constexpr auto ONE_HOUR_SECONDS = 3600;
constexpr auto NUMBER_TEN = 10;
constexpr auto RIGHT_MARGIN = 25;
constexpr auto LEFT_MARGIN = 15;
constexpr auto MAX_CHANNEL_ITEM_NUMBER = 100;
constexpr auto ChannelSettingsOrder = 1;
constexpr auto DPI_VALUE_NINTY_SIX = 96;
constexpr auto DPI_VALUE_ONE = 1;

constexpr auto SCENE_LEFT_SPACING = 19;
constexpr auto SCENE_LIST_MODE_LEFT_SPACING = 0;
constexpr auto SCENE_ITEM_VSPACING = 0;
constexpr auto SCENE_ITEM_LIST_MODE_VSPACING = 10;
constexpr auto SCENE_ITEM_HSPACING = 15;
constexpr auto SCENE_ITEM_LIST_MODE_HSPACING = 0;
constexpr auto SCENE_ITEM_FIX_HEIGHT = 155;
constexpr auto SCENE_ITEM_LIST_MODE_FIX_HEIGHT = 40;
constexpr auto SCENE_SCROLL_AREA_SPACING_HEIGHT = 10;
constexpr auto SCENE_ITEM_FIX_WIDTH = 112;
constexpr auto SCENE_DISPLAY_DEFAULT_WIDTH = 112;
constexpr auto SCENE_DISPLAY_DEFAULT_HEIGHT = 112;
constexpr auto SCENE_RENDER_NUMBER = 10;
constexpr auto SCENE_ITEM_DO_NOT_NEED_AUTO_SCROLL = 1;

/*************** const for encodingsettingview ****************/
constexpr auto ENCODING_SETTING_VIEW_WIDTH = 310;
constexpr auto ENCODING_SETTING_VIEW_HEIGHT_LIVE = 183;
constexpr auto ENCODING_SETTING_VIEW_HEIGHT_RECORD = 152;

/***************    id of source plugin from obs    ****************/
constexpr auto SCENE_SOURCE_ID = "scene";
constexpr auto GROUP_SOURCE_ID = "group";

#if defined(_WIN32)
constexpr auto OBS_DSHOW_SOURCE_ID = "dshow_input";
constexpr auto AUDIO_INPUT_SOURCE_ID = "wasapi_input_capture";
constexpr auto AUDIO_OUTPUT_SOURCE_ID = "wasapi_output_capture";
constexpr auto AUDIO_OUTPUT_SOURCE_ID_V2 = "wasapi_output_capture";
constexpr auto PRISM_MONITOR_SOURCE_ID = "monitor_capture";
constexpr auto GDIP_TEXT_SOURCE_ID = "text_gdiplus";
constexpr auto GDIP_TEXT_SOURCE_ID_V2 = "text_gdiplus_v2";
constexpr auto GAME_SOURCE_ID = "game_capture";
#elif defined(__APPLE__)
constexpr auto OBS_DSHOW_SOURCE_ID = "av_capture_input";
constexpr auto OBS_DSHOW_SOURCE_ID_V2 = "av_capture_input_v2";
constexpr auto AUDIO_INPUT_SOURCE_ID = "coreaudio_input_capture";
constexpr auto AUDIO_OUTPUT_SOURCE_ID = "coreaudio_output_capture";
constexpr auto AUDIO_OUTPUT_SOURCE_ID_V2 = "sck_audio_capture";
constexpr auto PRISM_MONITOR_SOURCE_ID = "display_capture";
constexpr auto GDIP_TEXT_SOURCE_ID = "text_ft2_source";
constexpr auto GDIP_TEXT_SOURCE_ID_V2 = "text_ft2_source_v2";
constexpr auto GAME_SOURCE_ID = "syphon-input";
constexpr auto OBS_MACOS_AUDIO_CAPTURE_SOURCE_ID = "sck_audio_capture";
constexpr auto OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID = "macos-avcapture";
constexpr auto OBS_MACOS_CAPTURE_CARD_SOURCE_ID = "macos-avcapture-fast";
#endif

constexpr auto WAVEFORM_SOURCE_ID = "phandasm_waveform_source";
constexpr auto VLC_SOURCE_ID = "vlc_source";
constexpr auto WINDOW_SOURCE_ID = "window_capture";
constexpr auto PRISM_MONITOR_REGION_MENU = "monitor_region_menu";
constexpr auto PRISM_REGION_SOURCE_ID = "prism_region_source";
constexpr auto BROWSER_SOURCE_ID = "browser_source";
constexpr auto MEDIA_SOURCE_ID = "ffmpeg_source";
constexpr auto IMAGE_SOURCE_ID = "image_source";
constexpr auto SLIDESHOW_SOURCE_ID = "slideshow";
constexpr auto COLOR_SOURCE_ID = "color_source";
constexpr auto COLOR_SOURCE_ID_V3 = "color_source_v3";
constexpr auto BGM_SOURCE_ID = "prism_bgm_source";
constexpr auto PRISM_GIPHY_STICKER_SOURCE_ID = "prism_sticker_source";
constexpr auto PRISM_STICKER_SOURCE_ID = "prism_sticker_reaction";
constexpr auto PRISM_CHAT_SOURCE_ID = "prism_chat_source";
constexpr auto PRISM_TEXT_TEMPLATE_ID = "prism_text_motion_source";
constexpr auto PRISM_NDI_SOURCE_ID = "ndi_source";
constexpr auto PRISM_SPECTRALIZER_SOURCE_ID = "prism_audio_visualizer_source";
constexpr auto PRISM_BACKGROUND_TEMPLATE_SOURCE_ID = "prism_background_template_source";
constexpr auto PRISM_MOBILE_SOURCE_ID = "prism_mobile";
constexpr auto PRISM_TIMER_SOURCE_ID = "prism_timer_source";
constexpr auto PRISM_APP_AUDIO_SOURCE_ID = "audio_capture";
constexpr auto PRISM_VIEWER_COUNT_SOURCE_ID = "prism_viewer_count_source";
constexpr auto PRISM_INPUT_OVERLAY_SOURCE_ID = "input-overlay";
constexpr auto PRISM_INPUT_HISTORY_SOURCE_ID = "input-history";
constexpr auto DECKLINK_INPUT_SOURCE_ID = "decklink-input";
constexpr auto OBS_APP_AUDIO_CAPTURE_ID = "wasapi_process_output_capture";
constexpr auto OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID = "screen_capture";

constexpr auto PRISM_LENS_SOURCE_ID = "prism_lens";
constexpr auto PRISM_LENS_MOBILE_SOURCE_ID = "prism_lens_mobile";
constexpr auto OBS_INPUT_SPOUT_CAPTURE_ID = "spout_capture";
constexpr auto PRISM_CHATV2_SOURCE_ID = "prism_chatv2_source";
constexpr auto PRISM_CHZZK_SPONSOR_SOURCE_ID = "prism_chzzk_sponsor";

/***************    filter id     ****************/
constexpr auto FILTER_TYPE_ID_APPLYLUT = "clut_filter";
constexpr auto FILTER_TYPE_ID_CHROMAKEY = "chroma_key_filter";
constexpr auto FILTER_TYPE_ID_COLOR_FILTER = "color_filter";
constexpr auto FILTER_TYPE_ID_COLOR_KEY_FILTER = "color_key_filter";
constexpr auto FILTER_TYPE_ID_COMPRESSOR = "compressor_filter";
constexpr auto FILTER_TYPE_ID_CROP_PAD = "crop_filter";
constexpr auto FILTER_TYPE_ID_EXPANDER = "expander_filter";
constexpr auto FILTER_TYPE_ID_GAIN = "gain_filter";
constexpr auto FILTER_TYPE_ID_IMAGEMASK_BLEND = "mask_filter";
constexpr auto FILTER_TYPE_ID_INVERT_POLARITY = "invert_polarity_filter";
constexpr auto FILTER_TYPE_ID_LIMITER = "limiter_filter";
constexpr auto FILTER_TYPE_ID_LUMAKEY = "luma_key_filter";
constexpr auto FILTER_TYPE_ID_NOISEGATE = "noise_gate_filter";
constexpr auto FILTER_TYPE_ID_NOISE_SUPPRESSION = "noise_suppress_filter";
constexpr auto FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE = "noise_suppress_filter_rnnoise";
constexpr auto FILTER_TYPE_ID_RENDER_DELAY = "gpu_delay";
constexpr auto FILTER_TYPE_ID_SCALING_ASPECTRATIO = "scale_filter";
constexpr auto FILTER_TYPE_ID_SCROLL = "scroll_filter";
constexpr auto FILTER_TYPE_ID_SHARPEN = "sharpness_filter";
constexpr auto FILTER_TYPE_ID_VSTPLUGIN = "vst_filter";
constexpr auto FILTER_TYPE_ID_VIDEODELAY_ASYNC = "async_delay_filter";
constexpr auto FILTER_TYPE_ID_PREMULTIPLIED_ALPHA_FILTER = "premultiplied_alpha_filter";
constexpr auto FILTER_TYPE_ID_SOUND_TOUCH_FILTER = "sound_touch_filter";

/*************** properties view ****************/
constexpr auto PROPERTIES_VIEW_VERTICAL_SPACING_MIN = 10;
constexpr auto PROPERTIES_VIEW_VERTICAL_SPACING_MAX = 15;

constexpr auto DOCK_DEATTACH_MIN_SIZE = 20;
constexpr auto DISPLAY_VIEW_DEFAULT_WIDTH = 690;
constexpr auto DISPLAY_VIEW_DEFAULT_HEIGHT = 211;
constexpr auto DISPLAY_LABEL_DEFAULT_HEIGHT = 339;
constexpr auto DISPLAY_VIEW_MIN_HEIGHT = 150;
constexpr auto DISPLAY_VIEW_MAX_HEIGHT = 368;
constexpr auto FILTERS_VIEW_DEFAULT_WIDTH = 910;
constexpr auto FILTERS_VIEW_DEFAULT_HEIGHT = 700;
constexpr auto FILTERS_ITEM_VIEW_FIXED_HEIGHT = 40;
constexpr auto FILTERS_DISPLAY_VIEW_MIN_HEIGHT = 150;
constexpr auto FILTERS_DISPLAY_VIEW_MAX_HEIGHT = 319;
constexpr auto FILTERS_DISPLAY_VIEW_MIN_WIDTH = 448;
constexpr auto FILTERS_DISPLAY_VIEW_MAX_WIDTH = 690;
constexpr auto FILTERS_TRANSITION_VIEW_FIXED_HEIGHT = 40;
constexpr auto FILTERS_TRANSITION_VIEW_FIXED_WIDTH = 128;
constexpr auto FILTERS_PROPERTIES_VIEW_MAX_HEIGHT = 244;
constexpr auto COLOR_FILTERS_IMAGE_FIXED_WIDTH = 60;
constexpr auto COLOR_FILTERS_IMAGE_FIXED_HEIGHT = 60;
constexpr auto COLOR_FILTERS_VIEW_LEFT_PADDING = 15;
constexpr auto COLOR_FILTERS_VIEW_RIGHT_PADDING = 15;

/*************** object name ****************/
constexpr auto OBJECT_NMAE_PREVIEW_BUTTON = "previewBtn";
constexpr auto OBJECT_NMAE_DELETE_BUTTON = "deleteBtn";
constexpr auto OBJECT_NMAE_ADD_BUTTON = "addBtn";
constexpr auto OBJECT_NMAE_EXPORT_BUTTON = "exportBtn";
constexpr auto OBJECT_NMAE_ADD_SOURCE_BUTTON = "addSourceBtn";
constexpr auto OBJECT_NMAE_SWITCH_EFFECT_BUTTON = "switchEffectBtn";
constexpr auto OBJECT_NMAE_SWITCH_GRID_BUTTON = "switchGridBtn";
constexpr auto OBJECT_NMAE_SEPERATE_BUTTON = "seperateBtn";
constexpr auto OBJECT_NMAE_SEPERATE_SOURCE_BUTTON = "seperateSourceBtn";
constexpr auto OBJECT_NAME_BUTTON = "button";
constexpr auto OBJECT_NAME_BUTTON_BOX = "buttonBox";
constexpr auto OBJECT_NAME_WIDGET = "widget";
constexpr auto OBJECT_NAME_CHECKBOX = "checkbox";
constexpr auto OBJECT_NAME_RADIOBUTTON = "radiobutton";
constexpr auto OBJECT_NAME_PLAINTEXTEDIT = "plainTextEdit";
constexpr auto OBJECT_NAME_LINEEDIT = "lineedit";
constexpr auto OBJECT_NAME_BROWSE = "browse";
constexpr auto OBJECT_NAME_SPINBOX = "spinBox";
constexpr auto OBJECT_NAME_SLIDER = "slider";
constexpr auto OBJECT_NAME_EDITABLELIST = "editableList";
constexpr auto OBJECT_NAME_COMBOBOX = "combobox";
constexpr auto OBJECT_NAME_FONTLABEL = "fontLabel";
constexpr auto OBJECT_NAME_FONTBUTTON = "fontButton";
constexpr auto OBJECT_NAME_FORMLABEL = "formLabel";
constexpr auto OBJECT_NAME_FORMCHECKBOX = "formCheckBox";
constexpr auto OBJECT_NAME_SPACELABEL = "spaceLabel";
constexpr auto OBJECT_NAME_SEPERATOR_LABEL = "seperatorLabel";
constexpr auto OBJECT_NAME_DISPLAYTEXT = "displayText";
constexpr auto OBJECT_NAME_PROPERTYVIEW = "propertyPreview";
constexpr auto OBJECT_NAME_PROPERTY_SPLITTER = "propertyWindowSplitter";
constexpr auto OBJECT_NAME_PROPERTY_VIEW_CONTAINER = "propertyViewContainer";
constexpr auto OBJECT_NAME_RIGHT_SCROLL_BTN = "rightScrollBtn";
constexpr auto OBJECT_NAME_LEFT_SCROLL_BTN = "leftScrollBtn";
constexpr auto OBJECT_NAME_COLOR_FILTER_LABEL = "colorFilterLabel";
constexpr auto OBJECT_NAME_SOURCE_RENAME_EDIT = "sourceRenameEditor";

constexpr auto OBJECT_NAME_FILTER_ITEM_MENU = "filterItemMenu";
constexpr auto OBJECT_NAME_BASIC_FILTER_MENU = "basicFilterMenu";
constexpr auto OBJECT_NAME_ADD_STICKER_BUTTON = "addStickerBtn";
constexpr auto OBJECT_NAME_IMAGE_GROUP = "imageGroupBtn";

constexpr auto OBJECT_NAME_PROPERTIES_CONTENT_WIDGET = "properties_content";

/*************** property name ****************/
constexpr auto PROPERTY_NAME_SHOW_IMAGE = "showImage";
constexpr auto PROPERTY_NAME_ID = "id";
constexpr auto PROPERTY_NAME_TRANSITION = "transition";
constexpr auto PROPERTY_NAME_ICON_TYPE = "type";

constexpr auto DISPLAY_RESIZE_SCREEN = "displayResizeScreen";
constexpr auto DISPLAY_RESIZE_CENTER = "displayResizeCenter";

constexpr auto NO_SOURCE_TEXT_LABEL = "noSourceTipsLabel";

constexpr auto TRANSITION_APPLY_BUTTON = "transitionApplyButton";

constexpr auto PROPERTY_NAME_SOURCE_SELECT = "selected";

constexpr auto PROPERTY_NAME_STATUS = "status";

constexpr auto PROPERTY_NAME_MOUSE_STATUS = "status";
constexpr auto PROPERTY_VALUE_MOUSE_STATUS_NORMAL = "normal";
constexpr auto PROPERTY_VALUE_MOUSE_STATUS_HOVER = "hover";
constexpr auto PROPERTY_VALUE_MOUSE_STATUS_PRESSED = "pressed";

/*************** side bar button icons file ****************/
constexpr auto BEAUTYEFFECT_OFF_NORMAL = ":/images/ic-beautyeffect-off-normal.svg";
constexpr auto BEAUTYEFFECT_OFF_OVER = ":/images/ic-beautyeffect-off-over.svg";
constexpr auto BEAUTYEFFECT_OFF_CLICKED = ":/images/ic-beautyeffect-off-click.svg";
constexpr auto BEAUTYEFFECT_OFF_DISABLE = ":/images/ic-beautyeffect-off-disable.svg";
constexpr auto BEAUTYEFFECT_ON_NORMAL = ":/images/ic-beautyeffect-on-normal.svg";
constexpr auto BEAUTYEFFECT_ON_OVER = ":/images/ic-beautyeffect-on-over.svg";
constexpr auto BEAUTYEFFECT_ON_CLICKED = ":/images/ic-beautyeffect-on-click.svg";
constexpr auto BEAUTYEFFECT_ON_DISABLE = ":/images/ic-beautyeffect-on-disable.svg";

constexpr auto GIPHY_OFF_NORMAL = ":/images/giphy/ic-giphy-off-normal.svg";
constexpr auto GIPHY_OFF_OVER = ":/images/giphy/ic-giphy-off-over.svg";
constexpr auto GIPHY_OFF_CLICKED = ":/images/giphy/ic-giphy-off-click.svg";
constexpr auto GIPHY_OFF_DISABLE = ":/images/giphy/ic-giphy-off-disable.svg";
constexpr auto GIPHY_ON_NORMAL = ":/images/giphy/ic-giphy-on-normal.svg";
constexpr auto GIPHY_ON_OVER = ":/images/giphy/ic-giphy-on-over.svg";
constexpr auto GIPHY_ON_CLICKED = ":/images/giphy/ic-giphy-on-click.svg";
constexpr auto GIPHY_ON_DISABLE = ":/images/giphy/ic-giphy-on-disable.svg";

constexpr auto BGM_OFF_NORMAL = ":/images/bgm/ic-bgm-off-normal.svg";
constexpr auto BGM_OFF_OVER = ":/images/bgm/ic-bgm-off-over.svg";
constexpr auto BGM_OFF_CLICKED = ":/images/bgm/ic-bgm-off-click.svg";
constexpr auto BGM_OFF_DISABLE = ":/images/bgm/ic-bgm-off-disable.svg";
constexpr auto BGM_ON_NORMAL = ":/images/bgm/ic-bgm-on-normal.svg";
constexpr auto BGM_ON_OVER = ":/images/bgm/ic-bgm-on-over.svg";
constexpr auto BGM_ON_CLICKED = ":/images/bgm/ic-bgm-on-click.svg";
constexpr auto BGM_ON_DISABLE = ":/images/bgm/ic-bgm-on-disable.svg";

constexpr auto TOAST_OFF_NORMAL = ":/images/ic-error-off-normal.svg";
constexpr auto TOAST_OFF_OVER = ":/images/ic-error-off-over.svg";
constexpr auto TOAST_OFF_CLICKED = ":/images/ic-error-off-click.svg";
constexpr auto TOAST_OFF_DISABLE = ":/images/ic-error-off-disable.svg";
constexpr auto TOAST_ON_NORMAL = ":/images/ic-error-on-normal.svg";
constexpr auto TOAST_ON_OVER = ":/images/ic-error-on-over.svg";
constexpr auto TOAST_ON_CLICKED = ":/images/ic-error-on-click.svg";
constexpr auto TOAST_ON_DISABLE = ":/images/ic-error-on-disable.svg";

constexpr auto CHAT_OFF_NORMAL = ":/images/ic-chat-off-normal.svg";
constexpr auto CHAT_OFF_OVER = ":/images/ic-chat-off-over.svg";
constexpr auto CHAT_OFF_CLICKED = ":/images/ic-chat-off-click.svg";
constexpr auto CHAT_OFF_DISABLE = ":/images/ic-chat-off-disable.svg";
constexpr auto CHAT_ON_NORMAL = ":/images/ic-chat-on-normal.svg";
constexpr auto CHAT_ON_OVER = ":/images/ic-chat-on-over.svg";
constexpr auto CHAT_ON_CLICKED = ":/images/ic-chat-on-click.svg";
constexpr auto CHAT_ON_DISABLE = ":/images/ic-chat-on-disable.svg";

constexpr auto WIFI_OFF_NORMAL = ":/images/wifi-help/ic-mobile-off-normal.svg";
constexpr auto WIFI_OFF_OVER = ":/images/wifi-help/ic-mobile-off-over.svg";
constexpr auto WIFI_OFF_CLICKED = ":/images/wifi-help/ic-mobile-off-click.svg";
constexpr auto WIFI_OFF_DISABLE = ":/images/wifi-help/ic-mobile-off-disable.svg";
constexpr auto WIFI_ON_NORMAL = ":/images/wifi-help/ic-mobile-on-normal.svg";
constexpr auto WIFI_ON_OVER = ":/images/wifi-help/ic-mobile-on-over.svg";
constexpr auto WIFI_ON_CLICKED = ":/images/wifi-help/ic-mobile-on-click.svg";
constexpr auto WIFI_ON_DISABLE = ":/images/wifi-help/ic-mobile-on-disable.svg";

constexpr auto VIRTUAL_OFF_NORMAL = ":/resource/images/virtual/ic-vbg-off-normal.svg";
constexpr auto VIRTUAL_OFF_OVER = ":/resource/images/virtual/ic-vbg-off-over.svg";
constexpr auto VIRTUAL_OFF_CLICKED = ":/resource/images/virtual/ic-vbg-off-click.svg";
constexpr auto VIRTUAL_OFF_DISABLE = ":/resource/images/virtual/ic-vbg-off-disable.svg";
constexpr auto VIRTUAL_ON_NORMAL = ":/resource/images/virtual/ic-vbg-on-normal.svg";
constexpr auto VIRTUAL_ON_OVER = ":/resource/images/virtual/ic-vbg-on-over.svg";
constexpr auto VIRTUAL_ON_CLICKED = ":/resource/images/virtual/ic-vbg-on-click.svg";
constexpr auto VIRTUAL_ON_DISABLE = ":/resource/images/virtual/ic-vbg-on-disable.svg";

constexpr auto PRISM_STICKER_OFF_NORMAL = ":/images/prism-sticker/ic-priemsticker-off-normal.svg";
constexpr auto PRISM_STICKER_OFF_OVER = ":/images/prism-sticker/ic-priemsticker-off-over.svg";
constexpr auto PRISM_STICKER_OFF_CLICKED = ":/images/prism-sticker/ic-priemsticker-off-click.svg";
constexpr auto PRISM_STICKER_OFF_DISABLE = ":/images/prism-sticker/ic-priemsticker-off-disable.svg";
constexpr auto PRISM_STICKER_ON_NORMAL = ":/images/prism-sticker/ic-priemsticker-on-normal.svg";
constexpr auto PRISM_STICKER_ON_OVER = ":/images/prism-sticker/ic-priemsticker-on-over.svg";
constexpr auto PRISM_STICKER_ON_CLICKED = ":/images/prism-sticker/ic-priemsticker-on-click.svg";
constexpr auto PRISM_STICKER_ON_DISABLE = ":/images/prism-sticker/ic-priemsticker-on-disable.svg";

constexpr auto VIRTUAL_CAMERA_OFF_NORMAL = ":/images/ic-virtualcamera-off-normal.svg";
constexpr auto VIRTUAL_CAMERA_OFF_CLICKED = ":/images/ic-virtualcamera-off-click.svg";
constexpr auto VIRTUAL_CAMERA_OFF_OVER = ":/images/ic-virtualcamera-off-over.svg";
constexpr auto VIRTUAL_CAMERA_OFF_DISABLE = ":/images/ic-virtualcamera-off-disable.svg";

constexpr auto VIRTUAL_CAMERA_ON_NORMAL = ":/images/ic-virtualcamera-on-normal.svg";
constexpr auto VIRTUAL_CAMERA_ON_CLICKED = ":/images/ic-virtualcamera-on-click.svg";
constexpr auto VIRTUAL_CAMERA_ON_OVER = ":/images/ic-virtualcamera-on-over.svg";
constexpr auto VIRTUAL_CAMERA_ON_DISABLE = ":/images/ic-virtualcamera-on-disable.svg";

constexpr auto DEAW_PEN_OFF_NORMAL = ":/images/draw-pen/ic-drawing-off-normal.svg";
constexpr auto DEAW_PEN_OFF_OVER = ":/images/draw-pen/ic-drawing-off-over.svg";
constexpr auto DEAW_PEN_OFF_CLICKED = ":/images/draw-pen/ic-drawing-off-click.svg";
constexpr auto DEAW_PEN_OFF_DISABLE = ":/images/draw-pen/ic-drawing-off-disable.svg";
constexpr auto DEAW_PEN_ON_NORMAL = ":/images/draw-pen/ic-drawing-on-normal.svg";
constexpr auto DEAW_PEN_ON_OVER = ":/images/draw-pen/ic-drawing-on-over.svg";
constexpr auto DEAW_PEN_ON_CLICKED = ":/images/draw-pen/ic-drawing-on-click.svg";
constexpr auto DEAW_PEN_ON_DISABLE = ":/images/draw-pen/ic-drawing-on-disable.svg";


/************ app crash notice *********/
constexpr auto IS_THIRD_PARTY_PLUGINS = "isThirdPartyPlugins";
constexpr auto PLUGIN_DLL_NAME = "dllName";

constexpr auto PRISM_CRASH_CONFIG_PATH = "PRISMLiveStudio/crashDump/crash.json";
constexpr auto CURRENT_PRISM = "currentPrism";
constexpr auto PRISM_SESSION = "prismSession";
constexpr auto IS_CRASHED = "crashed";
constexpr auto VIDEO_ADAPTER = "videoAdapter";
constexpr auto MODULES = "modules";

constexpr auto LIVEINFO_STAR_HTML_TEMPLATE = "<html><head/><body><p style='white-space: pre-wrap;'>%1<span style='color:#c34151;font-weight:normal;'>*</span></p></body></html>";

//wizard
constexpr auto PLS_BANNANR_PATH = "Cache/Banner";
constexpr auto PLS_BANNANR_JSON = "resources/library/library_policy_pc/banner_resources.json";
constexpr auto PLS_BANNANR_JSON_TEST = "data/prism-launcher/testResources/banner_resources.json";

//update
constexpr auto UPDATE_IS_FORCE_UPDATE = "UpdateIsForceUpdate";
constexpr auto UPDATE_VERSION = "UpdateVersion";
constexpr auto UPDATE_RESULT = "UpdateResult";
constexpr auto UPDATE_FILE_URL = "UpdateFileUrl";
constexpr auto UPDATE_INFO_URL = "UpdateInfoUrl";
constexpr auto UPDATE_MESSAGE_INFO = "UpdateMessageInfo";
constexpr auto UPDATE_NEXT_VERSION_INFO = "UpdateNextVersionInfo";
constexpr auto UPDATE_PRISM_BUNDLE_PATH = "UpdatePrismBundlePath";

//laboratory
constexpr auto LABORATORY_JS_ACTION = "action";
constexpr auto LABORATORY_JS_INFO = "info";
constexpr auto LABORATORY_JS_PAGE = "page";
constexpr auto LABORATORY_JS_CLICK_ACTION = "clickButton";
constexpr auto LABORATORY_JS_REMOTE_CHAT_PAGE = "remoteChat";
constexpr auto LABORATORY_JS_REMOTE_CHAT_INFO = "openRemoteChat";
constexpr auto LABORATORY_JS_FACE_MAKER_PAGE = "faceMaker";

constexpr auto LABORATORY_OLD_PLUGIN_FOLDER_NAME = "prism-plugins";
constexpr auto LABORATORY_PLUGIN_FOLDER_NAME = "lab-plugins";

/*************** live record anolog error ****************/
constexpr auto ANALOG_LIVE_START_DATA_NOT_OBJECT = 9000;
constexpr auto ANALOG_LIVE_START_TIME_OUT_30S = 9001;
constexpr auto ANALOG_LIVE_DIRECT_START_DATA_NOT_OBJECT = 9002;
constexpr auto ANALOG_LIVE_DIRECT_START_REQUEST_FAILED = 9003;
constexpr auto ANALOG_LIVE_OUTPUT_HANDLER_NULL = 9004;
constexpr auto ANALOG_LIVE_OUTPUT_DUPLICATED_ACTIVE = 9005;
constexpr auto ANALOG_LIVE_DISABLED_OUTPUT_REF = 9006;
constexpr auto ANALOG_LIVE_START_STREAM_FAILED = 9007;
constexpr auto ANALOG_LIVE_START_REQUEST_FAILED = 9008;
constexpr auto ANALOG_RECORD_HANDLER_NULL = 9009;
constexpr auto ANALOG_RECORD_OUTPUT_DUPLICATED_ACTIVE = 9010;
constexpr auto ANALOG_RECORD_DISABLED_OUTPUT_REF = 9011;
constexpr auto ANALOG_RECORD_ENCODER_NOT_AVAILABLE = 9012;
constexpr auto ANALOG_RECORD_DIRECTORY_NOT_EXIST = 9013;
constexpr auto ANALOG_RECORD_LOW_DISK_SPACE = 9014;
constexpr auto ANALOG_RECORD_START_STREAM_FAILED = 9015;
constexpr auto ANALOG_LIVE_ABORT_FACEBOOK_STOP = 9016;
constexpr auto ANALOG_LIVE_ABORT_NAVERSHOPPING_STOP = 9017;
constexpr auto ANALOG_LIVE_ABORT_MQTT_REQUEST_BROADCAST_END = 9018;
constexpr auto ANALOG_LIVE_ABORT_MQTT_LIVE_FINISHED_BY_PLATFORM = 9019;
constexpr auto ANALOG_LIVE_ABORT_YOUTUBE_STOP = 9020;
constexpr auto ANALOG_LIVE_ABORT_NAVERTV_STOP = 9021;
constexpr auto ANALOG_LIVE_ABORT_BAND_STOP = 9022;
constexpr auto ANALOG_LIVE_ABORT_OBS_ERROR = 9023;
constexpr auto ANALOG_LIVE_SETUP_STREAM_FAILED = 9024;
constexpr auto ANALOG_LIVE_DIRECT_START_TIME_OUT_30S = 9025;
constexpr auto ANALOG_LIVE_ABORT_MQTT_SIMULCAST_UNSTABLE = 9026;

//common term css string
constexpr auto TERM_WEBVIEW_CSS =
	"body * {font-family: system-ui, Malgun Gothic, Dotum, Gulim, sans-serif, -apple-system, BlinkMacSystemFont !important; font-size: 14px !important; background-color: #1e1e1e !important; font-weight: 400 !important; color: #bababa !important; line-height: 20px !important; -webkit-font-smoothing: antialiased !important;} ::-webkit-scrollbar-track-piece {border-radius: 0} ::-webkit-scrollbar {width: 10px;height: 6px;background-color: #272727} ::-webkit-scrollbar-thumb:vertical {width: 6px;height: 50px;border: 2px solid #272727;background-color: #444;border-radius: 5px}";

const int PUSHBUTTON_DELAY_RESPONSE_MS = 200;

} // namespace common

#endif // COMMON_COMMONDEFINE_H
