/*
 * @file      CommonDefine.h
 * @brief     Definition of all strings used internally by the system.
 * @date      2019/09/25
 * @author    liuying
 * @attention null
 * @version   2.0.1
 * @modify    liuying create 2019/09/25
 */

#ifndef COMMONDEFINE_H
#define COMMONDEFINE_H

namespace common {
/*************** prism ****************/

#define PRISM_LIVE_STUDIO "PRISM Live Studio"
#define PRISM_ARCHITECTURE " Architecture "
#define PRISM_BUILD " Build "
#define PRISM_LANGUAGE " Language "

/*************** enter ****************/
#define NEW_LINE "\n"
#define ENTER "\r"
#define ENTER_WITH_BACKSLASH "\\r"
#define NEW_LINE_WITH_BACKSLASH "\\n"
#define SLASH "/"

/*************** status ****************/
#define STATUS "status"
#define STATUS_ENABLE "enable"
#define STATUS_DISABLE "disable"
#define STATUS_SELECTED "selected"
#define STATUS_DISABLE_SELECTED "disableSelected"
#define STATUS_NORMAL "normal"
#define STATUS_HOVER "hover"
#define STATUS_PRESSED "pressed"
#define STATUS_ON "on"
#define STATUS_OFF "off"
#define STATUS_CLICKED "clicked"
#define STATUS_UNCLICKED "unclicked"
#define STATUS_TRUE "true"
#define STATUS_FALSE "false"
#define STATUS_ERROR "error"
#define STATUS_ONE "one"
#define STATUS_TWO "two"
#define STATUS_THREE "three"

/*************** position ****************/
#define POSITION "position"

#define STATUS_SELECTED_DISVISIBLE "selected_disvisible"
#define STATUS_NORMAL_DISVISIBLE "normal_disvisible"

#define HTTP_CONTENT_TYPE_VALUE "application/json;charset=UTF-8"

/**************channel login************************/
#define HTTP_CONTENT_TYPE_URL_ENCODED_VALUE "application/x-www-form-urlencoded;charset=UTF-8"
#define HTTP_DEFAULT_GCC "KR"

/**************channel cookie************************/
#define COOKIE_NEO_SES "NEO_SES"
#define COOKIE_SNS_TOKEN "snsToken"
#define COOKIE_SNS_CD "snsCd"
#define COOKIE_OAUTH_TOKEN "oauth_token"
#define COOKIE_NID_SES "NID_SES"
#define COOKIE_NID_INF "nid_inf"
#define COOKIE_NID_AUT "NID_AUT"
#define COOKIE_ACCESS_TOKEN "access_token"
#define COOKIE_EXPIRES_IN "expires_in"
#define COOKIE_TOKEN_TYPE "token_type"
#define COOKIE_REFRESH_TOKEN "refresh_token"

#define CHANNEL "channel"
#define CHANNEL_NICKNAME "nickname"
#define CHANNEL_NAME "channelName"
#define CHANNEL_EMBLEM "channelEmblem"
#define CHANNEL_COOKIE_VLIVE "VLive_root"
#define CHANNEL_COOKIE_TWITCH "Twitch_root"
#define CHANNEL_COOKIE_WAV "Wav_root"
#define CHANNEL_COOKIE_YOUTUBE "Youtube_root"
#define CHANNEL_COOKIE_NAVERTV "NaverTv_root"
#define CHANNEL_RTMP_LIST "Rtmp_List"
#define HTTP_CODE "code"
#define HTTP_CLIENT_SECRET "client_secret"
#define HTTP_CLIENT_ID "client_id"
#define HTTP_GRANT_TYPE "grant_type"
#define HTTP_REDIRECT_URI "redirect_uri"
#define HTTP_ACCEPT "Accept"
#define HTTP_ACCEPT_TWITCH "application/vnd.twitchtv.v5+json"
#define HTTP_DISPLAY_NAME "display_name"
#define HTTP_NAME "name"
#define HTTP_IN_AUTH_CHANNEL "inAuthChannel"
#define HTTP_AUTHORIZATION_CODE "authorization_code"
#define HTTP_AUTHORIZATION "Authorization"
#define HTTP_PART "part"
#define HTTP_PART_SNPIIPT "snippet"
#define HTTP_MINE "mine"
#define HTTP_MINE_TRUE "true"
#define HTTP_MINE "mine"
#define HTTP_REFERER "Referer"
#define HTTP_FAILURE "Failure"
#define HTTP_SUCCESS "success"
#define HTTP_RTN_MSG "rtn_msg"
#define HTTP_NICK_NAME "nick_name"
#define HTTP_ITEM_ID "itemId"
#define HTTP_ITEMS "items"
#define HTTP_LIBRARY_1743 "LIBRARY_1743"
#define HTTP_RESOURCE_URL "resourceUrl"
#define CATEGORY_ID "categoryId"
#define LIBRARY "library"
#define HTTP_MESSAGE "message"
#define RTMP_SEQ "rtmpSeq"
#define STREAM_NAME "streamName"
#define RTMP_URL "rtmpUrl"
#define DESCRIPTION "description"
#define STREAM_KEY "streamKey"
#define ABP_FLAG "abpFlag"
#define CONFIG_RESOLUTION "resolution"
#define BITRATE "bitrate"
#define FRAMERATE "framerate"
#define INTERVAL "interval"
#define USERNAME "username"
#define META "meta"
#define LOGO "logo"
#define ENCODER_POLICIES "encoderPolicies"
#define VIDEO_POLICIES "videoPolicies"
#define PROFILE_IMG "profileImg"
#define BADGE_IMG "badgeImg"
#define THUMBNAIILS "thumbnails"
#define URL "url"
#define HIGH_LOWER "high"
#define SELECT_CHANNELS "Select Channels"
#define RTMP_CHANNELS "Rtmp Channels"
#define SNS_CHANNELS "Sns Channels"
#define MULTISTREAM "Multistream"

/***************httpMgr******************/
#define HTTP_HMAC_STR "apis.naver.com/prism"
#define HTTP_COOKIE "Cookie"
#define HTTP_HEAD_CONTENT_TYPE "Content-Type"
#define HTTP_USER_AGENT "User-Agent"
#define HTTP_DEVICE_ID "deviceId"
#define HTTP_VERSION "version"
#define HTTP_GCC "gcc"
#define HTTP_WITH_ACTIVITY_INFO "withActivityInfo"
#define NETWORK_CHECK_URL "www.baidu.com"

} // namespace common

#endif // COMMONDEFINE_H
