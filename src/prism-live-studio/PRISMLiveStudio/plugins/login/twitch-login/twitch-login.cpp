#include "twitch-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-channel-const.h"
#include <QUrl>
#include <QDebug>
#include <qnetworkcookie.h>
#include <liblog.h>
#include <QUrlQuery>

using namespace common;
constexpr auto TWITCH_LOGIN = "twitch-login";

OBS_DECLARE_MODULE()

static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSTwitchLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSTwitchLoginInfo>());
}

const char *obs_module_name(void)
{
	return TWITCH_LOGIN;
}

const char *obs_module_description(void)
{
	return TWITCH_LOGIN;
}

PLSLoginInfo::UseFor PLSTwitchLoginInfo::useFor() const
{
	return UseFor::Prism;
}

PLSPlatformType PLSTwitchLoginInfo::platform() const
{
	return PLSPlatformType::Twitch;
}
QString PLSTwitchLoginInfo::name() const
{
	return QString::fromUtf8("Twitch");
}
QString PLSTwitchLoginInfo::icon(UseFor useFor) const
{
	if (UseFor::Prism == useFor) {
		return QString(":/images/login-twitch.svg");

	} else if (UseFor::Channel == useFor) {
		if (char *path = obs_module_file("images/destination-twitch.png")) {
			QString qpath = QString::fromUtf8(path);
			bfree(path);
			return qpath;
		}
	}
	return QString();
}

bool PLSTwitchLoginInfo::loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent) const
{
	return channelLoginWithAccount(result, parent);
}
PLSTwitchLoginInfo::ChannelSupport PLSTwitchLoginInfo::channelSupport() const
{
	return PLSTwitchLoginInfo::ChannelSupport::Both;
}

QString PLSTwitchLoginInfo::rtmpUrl() const
{
	return QString("httpffefe");
}

bool PLSTwitchLoginInfo::prismLoginWithAccount(const QJsonObject &, const QWidget *) const
{
	return false;
}

bool PLSTwitchLoginInfo::channelLoginWithAccount(QVariantHash &result, QWidget *parent) const
{
	bool bResult = false;
	auto check = [](QVariantHash &result_, const QString &url, const QMap<QString, QString> &) {
		if (url.startsWith(TWITCH_REDIRECT_URI)) {
			QUrlQuery queryCode(QUrl(url).query());
			auto code = queryCode.queryItemValue("code", QUrl::FullyDecoded);
			result_.insert(ChannelData::g_channelName, QJsonValue(TWITCH));

			if (!code.isEmpty()) {
				result_.insert(ChannelData::g_channelCode, QJsonValue(code));
				return PLSResultCheckingResult::Ok;
			} else {
				return PLSResultCheckingResult::Close;
			}
		} else {
			return PLSResultCheckingResult::Continue;
		}
	};
	del_pannel_cookies(TWITCH);

	std_map<std::string, std::string> header;
	bResult = pls_browser_view(result, getTwitchLoginUrl(), header, TWITCH, check, parent, false);

	return bResult;
}
QUrl PLSTwitchLoginInfo::getTwitchLoginUrl() const
{
	QString twitchLoginUrlStr = QString(CHANNEL_TWITCH_LOGIN_URL)
					    .append("?")
					    .append(HTTP_CLIENT_ID)
					    .append("=")
					    .append(TWITCH_CLIENT_ID)
					    .append("&")
					    .append(HTTP_REDIRECT_URI)
					    .append("=")
					    .append(TWITCH_REDIRECT_URI)
					    .append("&response_type=code&scope=channel_read+chat_login+channel_editor&force_verify=true");
	return QUrl(twitchLoginUrlStr);
}