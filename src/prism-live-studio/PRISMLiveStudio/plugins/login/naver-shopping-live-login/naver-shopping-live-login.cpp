#include "naver-shopping-live-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"

#include "pls-channel-const.h"
#include "plslogindlg.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <qnetworkcookie.h>
#include <qdialog.h>

using namespace common;
constexpr auto NAVER_SHOPPING_LOGIN = "NaverShoppingLive-login";
constexpr auto SMART_STORE = "-SmartStore";

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("naver-shopping-live-login", "en-US")

constexpr auto NID_AUT = "NID_AUT";
constexpr auto NID_SES = "NID_SES";
constexpr auto NID_INF = "nid_inf";

namespace {
struct LocalGlobalVars {
	static PLSNaverShoppingLiveLoginInfo naverShoppingLiveLoginInfo;
};

PLSNaverShoppingLiveLoginInfo LocalGlobalVars::naverShoppingLiveLoginInfo;
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(&LocalGlobalVars::naverShoppingLiveLoginInfo)) {
		return false;
	}

	return true;
}

void obs_module_unload(void)
{
	pls_unregister_login_info(&LocalGlobalVars::naverShoppingLiveLoginInfo);
}

const char *obs_module_name(void)
{
	return NAVER_SHOPPING_LOGIN;
}

const char *obs_module_description(void)
{
	return NAVER_SHOPPING_LOGIN;
}

PLSLoginInfo::UseFor PLSNaverShoppingLiveLoginInfo::useFor() const
{
	return UseFor::Channel_Store;
}

PLSPlatformType PLSNaverShoppingLiveLoginInfo::platform() const
{
	return PLSPlatformType::NaverShoppingLive;
}
QString PLSNaverShoppingLiveLoginInfo::name() const
{
	return QString::fromUtf8(NAVER_SHOPPING_LIVE);
}
QString PLSNaverShoppingLiveLoginInfo::icon(UseFor useFor) const
{
	return QString();
}

bool PLSNaverShoppingLiveLoginInfo::loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent) const
{
	if ((useFor != UseFor::Channel) && (useFor != UseFor::Store)) {
		return false;
	}

	obs_frontend_push_ui_translation(obs_module_get_string);
	PLSLoginDlg dlg;
	if (dlg.exec() != QDialog::Accepted) {
		pls_check_app_exiting(false);
		obs_frontend_pop_ui_translation();
		return false;
	}
	pls_check_app_exiting(false);
	obs_frontend_pop_ui_translation();

	bool loginAgain = false;
	auto check = [&loginAgain, this](QVariantHash &l_result, const QString &url, const QMap<QString, QString> &cookies) { return naverShoppingWebCallback(url, l_result, loginAgain, cookies); };

	QString loginUrl = CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN.arg(pls_get_navershopping_deviceId());
	if (useFor == UseFor::Store) {
		del_pannel_cookies(QString(NAVER_SHOPPING_LIVE) + SMART_STORE);
		return pls_browser_view(result, QUrl(loginUrl), {}, QString(NAVER_SHOPPING_LIVE) + SMART_STORE, {}, check, parent);
	}

	del_pannel_cookies(NAVER_SHOPPING_LIVE);
	if (pls_browser_view(result, QUrl(dlg.loginUrl()), {}, NAVER_SHOPPING_LIVE, std::string(), check, parent)) {
		return true;
	} else if (loginAgain) {
		return loginWithAccount(result, useFor, parent);
	}
	return false;
}

PLSResultCheckingResult PLSNaverShoppingLiveLoginInfo::naverShoppingWebCallback(const QString &url, QVariantHash &l_result, bool &loginAgain, const QMap<QString, QString> &cookies) const
{
	qDebug() << "jump url " << url;
	if (url.startsWith("selective://close?") || url.startsWith("naverncp://livestream?") || url.startsWith("prism://broadcastlogin?")) {
		bool findToken = false;
		QUrl _url(url);
		for (auto str : _url.query(QUrl::FullyDecoded).split("&")) {
			if (str.startsWith("liveCommerceToken=", Qt::CaseInsensitive) || str.startsWith("token=", Qt::CaseInsensitive)) {
				l_result.insert(ChannelData::g_channelName, QJsonValue(NAVER_SHOPPING_LIVE));
				l_result.insert(ChannelData::g_channelToken, str.mid(str.indexOf('=') + 1));
				findToken = true;
			}
		}
		if (findToken) {
			return PLSResultCheckingResult::Ok;
		} else {
			loginAgain = true;
			return PLSResultCheckingResult::Close;
		}
	} else if (cookies.contains(NID_AUT) && cookies.contains(NID_SES) && cookies.contains(NID_INF)) {
		l_result.insert(ChannelData::g_channelName, QJsonValue(NAVER_SHOPPING_LIVE));
		l_result.insert(ChannelData::g_channelCookie, QJsonValue(getCookies(cookies)));
		return PLSResultCheckingResult::Ok;
	} else {
		return PLSResultCheckingResult::Continue;
	}
}

PLSNaverShoppingLiveLoginInfo::ChannelSupport PLSNaverShoppingLiveLoginInfo::channelSupport() const
{
	return PLSNaverShoppingLiveLoginInfo::ChannelSupport::Account;
}

QString PLSNaverShoppingLiveLoginInfo::rtmpUrl() const
{
	return QString();
}

QString PLSNaverShoppingLiveLoginInfo::getCookies(const QMap<QString, QString> &cookies)
{
	return QString("NID_SES=%1\nnid_inf=%2\nNID_AUT=%3").arg(cookies.value(NID_SES)).arg(cookies.value(NID_INF)).arg(cookies.value(NID_AUT));
}
