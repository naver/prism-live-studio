#include "ncb2b-login.hpp"
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
#include <liblog.h>

using namespace common;
constexpr auto NCB2B_LOGIN = "ncb2b-login";

OBS_DECLARE_MODULE()

static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSNCB2BLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSNCB2BLoginInfo>());
}

const char *obs_module_name(void)
{
	return NCB2B_LOGIN;
}

const char *obs_module_description(void)
{
	return NCB2B_LOGIN;
}

PLSLoginInfo::UseFor PLSNCB2BLoginInfo::useFor() const
{
	return UseFor::Prism;
}

PLSPlatformType PLSNCB2BLoginInfo::platform() const
{
	return PLSPlatformType::NAVER_Cloud_B2B;
}
QString PLSNCB2BLoginInfo::name() const
{
	return QString::fromUtf8(LOGIN_NAVER_CLOUDB2B__VIEW);
}
QString PLSNCB2BLoginInfo::icon(UseFor useFor) const
{
	if (UseFor::Prism == useFor) {
		return QString(":/images/login-ncb2b.svg");

	} else if (UseFor::Channel == useFor) {
		if (char *path = obs_module_file("images/destination-ncb2b.png")) {
			QString qpath = QString::fromUtf8(path);
			bfree(path);
			return qpath;
		}
	}
	return QString();
}

bool PLSNCB2BLoginInfo::loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent) const
{
	return channelLoginWithAccount(result, parent);
}
PLSNCB2BLoginInfo::ChannelSupport PLSNCB2BLoginInfo::channelSupport() const
{
	return PLSNCB2BLoginInfo::ChannelSupport::Both;
}

QString PLSNCB2BLoginInfo::rtmpUrl() const
{
	return QString("");
}

bool PLSNCB2BLoginInfo::prismLoginWithAccount(const QJsonObject &, const QWidget *) const
{
	return false;
}

bool PLSNCB2BLoginInfo::channelLoginWithAccount(QVariantHash &result, QWidget *parent) const
{
	bool bResult = false;
	QUrlQuery query(pls_get_b2b_auth_url());
	auto callbackUrl = query.queryItemValue("redirect_uri", QUrl::FullyDecoded);
	auto check = [&callbackUrl](QVariantHash &result_, const QString &url, const QMap<QString, QString> &) {
		if (callbackUrl.isEmpty()) {

			PLS_INFO(obs_module_name(), "login name %s,callback url is empty!", obs_module_name());
			return PLSResultCheckingResult::Close;
		}
		if (url.startsWith(callbackUrl)) {
			callbackUrl = url;
			return PLSResultCheckingResult::Ok;
		} else {
			return PLSResultCheckingResult::Continue;
		}
	};

	del_pannel_cookies(NCB2B);

	std_map<std::string, std::string> header;
	bResult = pls_browser_view(result, getTwitchLoginUrl(), header, NCB2B, check, parent, false);
	bool isGetTokenSucces = pls_get_b2b_acctoken(callbackUrl);

	return isGetTokenSucces;
}
QUrl PLSNCB2BLoginInfo::getTwitchLoginUrl() const
{
	return QUrl(pls_get_b2b_auth_url());
}

QString PLSNCB2BLoginInfo::getOauthTokenFromUrl(const QString &urlStr) const
{
	QUrl qurl(urlStr);
	QString token = qurl.fragment().split("&")[0];
	return token.mid(token.indexOf('=') + 1);
}
