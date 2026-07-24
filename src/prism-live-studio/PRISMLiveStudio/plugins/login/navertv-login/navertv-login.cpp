#include "navertv-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-channel-const.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <qnetworkcookie.h>
#include "libutils-api.h"
#include <util/config-file.h>

constexpr auto NAVERTV_LOGIN = "NaverTV-login";
OBS_DECLARE_MODULE()
using namespace common;
constexpr auto NID_AUT = "NID_AUT";
constexpr auto NID_SES = "NID_SES";
constexpr auto NID_INF = "nid_inf";
constexpr auto NAVERCOOKIE = "naverCookie";

static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSNaverTVLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSNaverTVLoginInfo>());
}

const char *obs_module_name(void)
{
	return NAVERTV_LOGIN;
}

const char *obs_module_description(void)
{
	return NAVERTV_LOGIN;
}

PLSLoginInfo::UseFor PLSNaverTVLoginInfo::useFor() const
{
	return UseFor::Channel;
}

PLSPlatformType PLSNaverTVLoginInfo::platform() const
{
	return PLSPlatformType::NaverTv;
}
QString PLSNaverTVLoginInfo::name() const
{
	return QString::fromUtf8(NAVER_TV);
}
QString PLSNaverTVLoginInfo::icon(UseFor) const
{
	return QString();
}

bool PLSNaverTVLoginInfo::loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent) const
{
	bool bResult = false;

	auto check = [](QVariantHash &result_, const QString &url, const QMap<QString, QString> &cookies) {
		if (cookies.contains(NID_AUT) && cookies.contains(NID_SES) && cookies.contains(NID_INF)) {

			result_.insert(ChannelData::g_channelName, QJsonValue(NAVER_TV));
			result_.insert(ChannelData::g_channelCookie, QJsonValue(getCookies(cookies)));
			qDebug() << url << "-----jump url-----" << result_;

			return PLSResultCheckingResult::Ok;

		} else {
			return PLSResultCheckingResult::Continue;
		}
	};

	//remove config save cookie values
	removeNaverTvCookies();
	del_pannel_cookies(NAVER_TV);

	bResult = pls_browser_view(result, CHANNEL_NAVERTV_LOGIN, {}, NAVER_TV, check, parent);

	return bResult;
}

PLSNaverTVLoginInfo::ChannelSupport PLSNaverTVLoginInfo::channelSupport() const
{
	return PLSNaverTVLoginInfo::ChannelSupport::Both;
}
QString PLSNaverTVLoginInfo::rtmpUrl() const
{
	return QString();
}

QString PLSNaverTVLoginInfo::getCookies(const QMap<QString, QString> &cookies)
{
	for (auto cookie = cookies.constBegin(); cookie != cookies.constEnd(); ++cookie) {

		config_set_string(pls_get_global_cookie_config(), NAVERCOOKIE, cookie.key().toUtf8().constData(), cookie.value().toUtf8().constData());
	}

	config_save(pls_get_global_cookie_config());
	pls_set_manual_cookies(NAVER_TV);
	return QString("NID_SES=%1\nnid_inf=%2\nNID_AUT=%3").arg(cookies.value(NID_SES)).arg(cookies.value(NID_INF)).arg(cookies.value(NID_AUT));
}

void PLSNaverTVLoginInfo::removeNaverTvCookies()
{
	config_remove_value(pls_get_global_cookie_config(), NAVERCOOKIE, NID_AUT);
	config_remove_value(pls_get_global_cookie_config(), NAVERCOOKIE, NID_SES);
	config_remove_value(pls_get_global_cookie_config(), NAVERCOOKIE, NID_INF);
	config_save(pls_get_global_cookie_config());
}
