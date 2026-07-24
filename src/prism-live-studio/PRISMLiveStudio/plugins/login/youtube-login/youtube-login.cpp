#include "youtube-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "PLSErrorHandler.h"
#include "pls-common-define.hpp"

#include "pls-channel-const.h"
#include "liblog.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <qnetworkcookie.h>
#include <qdesktopservices.h>
#include <QJsonDocument>
#include <QPointer>

constexpr auto YOUTUBE_LOGIN = "Youtube-login";
constexpr auto FRONTEND_PLUGINS_YOUTUBE_LOGIN = "frontend-plugins/youtube-login";

OBS_DECLARE_MODULE()
using namespace common;

struct LocalGlobalVars {
	static PLSYoutubeLoginInfo YoutubeLoginInfo;
};
PLSYoutubeLoginInfo LocalGlobalVars::YoutubeLoginInfo;

static void frontend_event(enum obs_frontend_event, void *)
{ /*not used*/
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(&LocalGlobalVars::YoutubeLoginInfo)) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(&LocalGlobalVars::YoutubeLoginInfo);
}

const char *obs_module_name(void)
{
	return YOUTUBE_LOGIN;
}

const char *obs_module_description(void)
{
	return YOUTUBE_LOGIN;
}

PLSLoginInfo::UseFor PLSYoutubeLoginInfo::useFor() const
{
	return UseFor::Channel;
}

PLSPlatformType PLSYoutubeLoginInfo::platform() const
{
	return PLSPlatformType::YouTube;
}
QString PLSYoutubeLoginInfo::name() const
{
	return QString::fromUtf8("YouTube");
}
QString PLSYoutubeLoginInfo::icon(UseFor useFor) const
{
	if (UseFor::Channel == useFor) {
		if (char *path = obs_module_file("images/destination-youtube.png")) {
			QString qpath = QString::fromUtf8(path);
			bfree(path);
			return qpath;
		}
	}
	return QString();
}

PLSLoginInfo::ImplementType PLSYoutubeLoginInfo::loginWithAccountImplementType() const
{
	return PLSLoginInfo::ImplementType::Asynchronous;
}

void PLSYoutubeLoginInfo::loginWithAccountAsync(const std::function<void(bool ok, const QVariantHash &)> &callback, UseFor, QWidget *) const
{
	del_pannel_cookies(YOUTUBE);
	if (m_httpServerAddress.isEmpty()) {
		if (pls_run_http_server("/oauth2/callback/google", this->m_httpServerAddress, [this, callback](const auto &url, const auto &json) {
			    this->m_httpServerAddress.clear();

			    if (QString code = json.value("code").toString(); !code.isEmpty()) {
				    QVariantHash result;
				    result.insert(ChannelData::g_channelName, QJsonValue(YOUTUBE));
				    result.insert(ChannelData::g_channelCode, QJsonValue(code));
				    result.insert("__goole_redirect_uri", QString("http://%1/oauth2/callback/google").arg(url));
				    callback(true, result);
			    } else {
				    PLS_WARN(FRONTEND_PLUGINS_YOUTUBE_LOGIN, json.value("message").toString().toUtf8().data());
				    callback(false, {});
			    }

			    pls_singleton_wakeup();
		    })) {
			//Do some process if succeeded to start httpserver
		} else {
			//Do some process if failed to start httpserver
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_YOUTUBE_HOST_BROWSER_ERROR, PLSErrKeyAllAlert, {},
							      PLSErrorHandler::ExtraData("PLSYoutubeLoginInfo::loginWithAccountAsync"));
		}
	}

	if (!m_httpServerAddress.isEmpty()) {
		QString url(
			"https://accounts.google.com/o/oauth2/v2/auth?client_id=%1&redirect_uri=http://%2/oauth2/callback/google&scope=https://www.googleapis.com/auth/youtube&email&response_type=code");
		url = url.arg(YOUTUBE_CLIENT_ID, m_httpServerAddress);

		if (!QDesktopServices::openUrl(url)) {
			PLS_ERROR(YOUTUBE_LOGIN, "open YouTube auth url failed");
		}
	} else {
		PLS_ERROR(YOUTUBE_LOGIN, "http server address is empty");
	}
	PLS_UI_ACTION("open Youtube login url finished");
}
PLSYoutubeLoginInfo::ChannelSupport PLSYoutubeLoginInfo::channelSupport() const
{
	return PLSYoutubeLoginInfo::ChannelSupport::Both;
}
QString PLSYoutubeLoginInfo::rtmpUrl() const
{
	return QString();
}
QUrl PLSYoutubeLoginInfo::getYoutubeLoginUrl() const
{
	return QUrl(QString(CHANNEL_YOUTUBE_LOGIN_URL)
			    .append("?")
			    .append(HTTP_CLIENT_ID)
			    .append("=")
			    .append(YOUTUBE_CLIENT_ID)
			    .append("&")
			    .append(HTTP_REDIRECT_URI)
			    .append("=")
			    .append(YOUTUBE_REDIRECT_URI)
			    .append("&scope=https://www.googleapis.com/auth/youtube&email&response_type=code"));
}
