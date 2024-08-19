#ifndef PLSCOMMONCONST_H
#define PLSCOMMONCONST_H

#include <qstring.h>
#include <optional>
#include <qsettings.h>
#include "libutils-api.h"

namespace pls_resource_const {
constexpr auto RESOURCE_DOWNLOAD = "resource/download";
constexpr auto MAIN_FRONTEND_API = "main/frontend-api";
constexpr auto DATA_ZIP_UZIP = "Zip/UZip";
constexpr auto HTTP_REQUEST_TIME_OUT = 15000;
constexpr auto HEADER_USER_AGENT_KEY = "";
constexpr auto HEADER_PRISM_LANGUAGE = "";
constexpr auto HEADER_PRISM_GCC = "";
constexpr auto HEADER_PRISM_USERCODE = "";
constexpr auto HEADER_PRISM_OS = "";
constexpr auto HEADER_PRISM_IP = "";
constexpr auto HEADER_PRISM_DEVICE = "";
constexpr auto HEADER_PRISM_APPVERSION = "";
constexpr auto PLS_FACEBOOK_LOGIN_URL = "";
constexpr auto PLS_GOOGLE_LOGIN_URL_TOKEN = "";
constexpr auto PLS_LINE_LOGIN_URL = "";
constexpr auto PLS_NAVER_LOGIN_URL = "";
constexpr auto PLS_TWITTER_LOGIN_URL = "";
constexpr auto PLS_TWITCH_LOGIN_URL = "";
constexpr auto PLS_WHALESPACE_LOGIN_URL = "";
constexpr auto PLS_SNS_LOGIN_SIGNUP_URL = "";
constexpr auto PLS_EMAIL_LOGIN_URL = "";
constexpr auto PLS_EMAIL_SIGNUP_URL = "";
constexpr auto PLS_EMAIL_FOGETTON_URL = "";
constexpr auto PLS_CHANGE_PASSWORD_URL = "";
constexpr auto PLS_PRISM_TOKEN_URL = "";
constexpr auto YOUTUBE_API_TOKEN = "";
constexpr auto HTTP_CONTENT_TYPE_VALUE = "";

constexpr auto PLS_GPOP_URL = "";

constexpr auto PLS_NOTICE_URL = "";

constexpr auto PLS_CATEGORY_URL = "";
constexpr auto APP_INIT_URL = "";
constexpr auto APP_UPDATE_URL = "";
constexpr auto LASTEST_UPDATE_URL = "";

constexpr auto PLS_ACTION_LOG = "";

constexpr auto YOUTUBE_CLIENT_ID_DEV = "";
constexpr auto YOUTUBE_CLIENT_KEY_DEV = "";

constexpr auto YOUTUBE_CLIENT_ID = "";
constexpr auto YOUTUBE_CLIENT_KEY = "";

constexpr auto PLS_PC_HMAC_KEY_DEV = "";
constexpr auto PLS_PC_HMAC_KEY = "";

constexpr auto PLS_LIBRARY_POLICY_ID_DEV = "";
constexpr auto PLS_LIBRARY_POLICY_ID = "";

constexpr auto PLS_LIBRARY_SENSETIME_ID_DEV = "";
constexpr auto PLS_LIBRARY_SENSETIME_ID = "";

constexpr auto PLS_THEMES_PATH = "";

constexpr auto PLS_APP_ICON_PATH = ":/images/PRISMLiveStudio.ico";

constexpr auto PLS_PRISM_THUMBNAIL_FILE_PATH = "";
constexpr auto PLS_WHALESPACE_NAME = "whalespace";
constexpr auto EVENT_APP = "app";
constexpr auto EVENT_APP_INIT = "init";
constexpr auto EVENT_APP_INIT_RESULT_SUCCESS = "success";
constexpr auto EVENT_APP_INIT_RESULT_FAIL = "fail";
constexpr auto EVENT_APP_INIT_API_ERROR = "apiError";
}
namespace pls_http_api_func {

static QString getPrismHost()
{
	return "";
}

static QString getPrismAuthGateWay()
{
	return "";
}

static QString getPrismSynGateWay()
{
	return "";
}

static QString getPrismSidekickGateWay()
{
	return "";
}

static QString getPrismLogGateWay()
{
	return "";
}

static QByteArray getPrismHamcKey()
{
	return "";
}
static QString getPolicyId()
{
	return "";
}
static QString getSenseTimeId()
{
	return "";
}
static QString getTermOfUserUrl()
{
	return "";
}
static QString getPrivacyUrl()
{
	return "";
}
}
#endif // PLSCOMMONCONST_H
