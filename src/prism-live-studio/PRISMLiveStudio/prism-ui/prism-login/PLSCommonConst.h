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
constexpr auto FACEBOOK_LOGIN_URL = "/auth/sns/v2/facebook";
constexpr auto GOOGLE_LOGIN_URL_TOKEN = "/auth/sns/v2/joinLogin";
constexpr auto LINE_LOGIN_URL = "/auth/sns/v2/line";
constexpr auto NAVER_LOGIN_URL = "/auth/sns/v2/naver";
constexpr auto TWITTER_LOGIN_URL = "/auth/sns/v2/twitter";
constexpr auto TWITCH_LOGIN_URL = "/auth/sns/v2/twitch";
constexpr auto WHALESPACE_LOGIN_URL = "/auth/sns/v2/whalespace";
constexpr auto SNS_LOGIN_SIGNUP_URL = "/auth/sns/v2/joinLogin";
constexpr auto EMAIL_LOGIN_URL = "/auth/email/v2/simple/login";
constexpr auto EMAIL_SIGNUP_URL = "/auth/email/v2/simple/join";
constexpr auto EMAIL_FOGETTON_URL = "/auth/email/reset/pw";
constexpr auto CHANGE_PASSWORD_URL = "/auth/email/v2/change/pw";
constexpr auto PRISM_TOKEN_URL = "/auth/session";
constexpr auto YOUTUBE_API_TOKEN = "https://oauth2.googleapis.com/token";
constexpr auto HTTP_CONTENT_TYPE_VALUE = "application/json;charset=UTF-8";

constexpr auto GPOP_URL = "/gpop/v1/connections.json";

constexpr auto NOTICE_URL = "/notice";

constexpr auto PLS_CATEGORY_URL = "/resource/v3/latest/categories";
constexpr auto INIT_URL = "/appInit";
constexpr auto UPDATE_URL = "/update/appversion";
constexpr auto APP_LASTEST_UPDATE_URL = "/latest/appversion";

constexpr auto PLS_ACTION_LOG = "/prism-log-api/log/action";

constexpr auto YOUTUBE_CLIENT_ID_DEV = "775513731180-rc0mrf2dno2bumtf7f6chfs3uh44ur0a.apps.googleusercontent.com";
constexpr auto YOUTUBE_CLIENT_KEY_DEV = "2-EoB0ZlZbwrbkLR_VoW3hB0";

constexpr auto YOUTUBE_CLIENT_ID_ = "775513731180-bujsbge46o6lgbh07vvnljo5h54tlkd4.apps.googleusercontent.com";
constexpr auto YOUTUBE_CLIENT_KEY_ = "HnZi48VWR-Cj9sT1IZHY4NtQ";

constexpr auto PLS_PC_HMAC_KEY_DEV = "KQiAalBjZfha0FD60UdbCoE8Fa11e8GTrzwpFNZoecIeG6ygPm6iAmxfiEzK4BUG";
constexpr auto PLS_HMAC_KEY = "XNVtpB3vBL01Ui3S97a1S3GQti5a4c2NhCgrFBt7Mf4Q9xPPSVf5FehRs14Wfxqx";

constexpr auto PLS_LIBRARY_POLICY_ID_DEV = "LIBRARY_1452";
constexpr auto PLS_LIBRARY_POLICY_ID = "LIBRARY_1225";

constexpr auto PLS_LIBRARY_SENSETIME_ID_DEV = "LIBRARY_1451";
constexpr auto PLS_LIBRARY_SENSETIME_ID = "LIBRARY_0870";

constexpr auto PLS_THEMES_PATH = ":/css/%1.css";

constexpr auto PLS_APP_ICON_PATH = ":/images/PRISMLiveStudio.ico";

constexpr auto PLS_PRISM_THUMBNAIL_FILE_PATH = "user/prismThumbnail.png";
constexpr auto PLS_WHALESPACE_NAME = "whalespace";
constexpr auto EVENT_APP = "app";
constexpr auto EVENT_APP_INIT = "init";
constexpr auto EVENT_APP_INIT_RESULT_SUCCESS = "success";
constexpr auto EVENT_APP_INIT_RESULT_FAIL = "fail";
constexpr auto EVENT_APP_INIT_API_ERROR = "apiError";
constexpr auto APP_INSTALL_ACCEPT = 200;
}
namespace pls_http_api_func {

static QString getPrismHost()
{
	return pls_prism_is_dev() ? "https://dev-global.apis.naver.com" : "https://global.apis.naver.com";
}

static QString getPrismAuthGateWay()
{
	auto path = pls_prism_is_dev() ? "prism_pc/auth-api" : "prism_pc/prism-auth-api";

	return QString("%1/%2").arg(getPrismHost()).arg(path);
}

static QString getPrismSynGateWay()
{
	auto path = pls_prism_is_dev() ? "prism_pc/sync-api" : "prism_pc/prism-sync-api";
	return QString("%1/%2").arg(getPrismHost()).arg(path);
}

static QString getPrismSidekickGateWay()
{
	return QString("%1/%2").arg(getPrismHost()).arg("prism_pc/prism-sidekick-api");
}

static QString getPrismLogGateWay()
{
	return QString("%1/%2").arg(getPrismHost()).arg("prism_pc");
}
static QByteArray getPrismHamcKey()
{
	return pls_prism_is_dev() ? pls_launcher_const::PLS_PC_HMAC_KEY_DEV : pls_launcher_const::PLS_HMAC_KEY;
}
static QString getPolicyId()
{
	return pls_prism_is_dev() ? pls_launcher_const::PLS_LIBRARY_POLICY_ID_DEV : pls_launcher_const::PLS_LIBRARY_POLICY_ID;
}
static QString getSenseTimeId()
{
	return pls_prism_is_dev() ? pls_launcher_const::PLS_LIBRARY_SENSETIME_ID_DEV : pls_launcher_const::PLS_LIBRARY_SENSETIME_ID;
}
static QString getTermOfUserUrl()
{
	return pls_prism_is_dev() ? "http://dev.prismlive.com/%1/policy/terms_content.html" : "http://prismlive.com/%1/policy/terms_content.html";
}
static QString getPrivacyUrl()
{
	return pls_prism_is_dev() ? "http://dev.prismlive.com/%1/policy/privacy_content.html" : "http://prismlive.com/%1/policy/privacy_content.html";
}
}
#endif // PLSCOMMONCONST_H
