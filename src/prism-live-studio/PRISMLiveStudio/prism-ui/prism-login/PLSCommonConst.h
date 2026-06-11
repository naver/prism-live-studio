#ifndef PLSCOMMONCONST_H
#define PLSCOMMONCONST_H

#include "log/module_names.h"
#include <qstring.h>
#include <optional>
#include <qsettings.h>
#include "libutils-api.h"

enum class pls_window_type {
	PLS_CHECK_UPDATE_VIEW = 1, //
	PLS_CHECK_ENV_RES_VIEW,
	PLS_UPDATE_CHOICE_VIEW,
	PLS_NOTICE_VIEW,
	PLS_UPDATING_VIEW,
	PLS_RES_DOWNLOADING_VIEW,
	PLS_LOGIN_VIEW,
	PLS_APP_RUNNING
};

namespace pls_launcher_const {
constexpr auto HTTP_REQUEST_TIME_OUT = 15000;
constexpr auto HEADER_USER_AGENT_KEY = "User-Agent";
constexpr auto HEADER_PRISM_LANGUAGE = "Accept-Language";
constexpr auto HEADER_PRISM_GCC = "X-prism-cc";
constexpr auto HEADER_PRISM_USERCODE = "X-prism-usercode";
constexpr auto HEADER_PRISM_OS = "X-prism-os";
constexpr auto HEADER_PRISM_IP = "X-prism-ip";
constexpr auto HEADER_PRISM_DEVICE = "X-prism-device";
constexpr auto HEADER_PRISM_APPVERSION = "X-prism-appversion";
constexpr auto FACEBOOK_LOGIN_URL = "";
constexpr auto GOOGLE_LOGIN_URL_TOKEN = "";
constexpr auto LINE_LOGIN_URL = "";
constexpr auto NAVER_LOGIN_URL = "";
constexpr auto TWITTER_LOGIN_URL = "";
constexpr auto TWITCH_LOGIN_URL = "";
constexpr auto WHALESPACE_LOGIN_URL = "";
constexpr auto SNS_LOGIN_SIGNUP_URL = "";
constexpr auto EMAIL_LOGIN_URL = "";
constexpr auto EMAIL_SIGNUP_URL = "";
constexpr auto EMAIL_FOGETTON_URL = "";
constexpr auto CHANGE_PASSWORD_URL = "";
constexpr auto PRISM_TOKEN_URL = "";
constexpr auto YOUTUBE_API_TOKEN = "";
constexpr auto HTTP_CONTENT_TYPE_VALUE = "";

constexpr auto GPOP_URL = "";

constexpr auto NOTICE_URL = "";

constexpr auto PLS_CATEGORY_URL = "/";
constexpr auto INIT_URL = "";
constexpr auto UPDATE_URL = "";
constexpr auto APP_LASTEST_UPDATE_URL = "";

constexpr auto PLS_ACTION_LOG = "";

constexpr auto YOUTUBE_CLIENT_ID_DEV = "";
constexpr auto YOUTUBE_CLIENT_KEY_DEV = "";

constexpr auto YOUTUBE_CLIENT_ID_ = "";
constexpr auto YOUTUBE_CLIENT_KEY_ = "";

constexpr auto PLS_PC_HMAC_KEY_DEV = "";
constexpr auto PLS_HMAC_KEY = "";

constexpr auto PLS_LIBRARY_POLICY_ID_DEV = "";
constexpr auto PLS_LIBRARY_POLICY_ID = "";

constexpr auto PLS_LIBRARY_SENSETIME_ID_DEV = "";
constexpr auto PLS_LIBRARY_SENSETIME_ID = "";

constexpr auto PLS_THEMES_PATH = "";

constexpr auto PLS_APP_ICON_PATH = "";

constexpr auto PLS_PRISM_THUMBNAIL_FILE_PATH = "";
constexpr auto PLS_WHALESPACE_NAME = "whalespace";
constexpr auto EVENT_APP = "app";
constexpr auto EVENT_APP_INIT = "init";
constexpr auto EVENT_APP_INIT_RESULT_SUCCESS = "success";
constexpr auto EVENT_APP_INIT_RESULT_FAIL = "fail";
constexpr auto EVENT_APP_INIT_API_ERROR = "apiError";
constexpr auto APP_INSTALL_ACCEPT = 200;
constexpr auto CLOSE_APP_FROM_LOGIN = 201;
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