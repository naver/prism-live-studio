#include "facebook-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-channel-const.h"
#include <QUrl>
#include <QDebug>
#include <QDesktopServices>
#include <QLocalServer>
#include <QLocalSocket>
#include <qnetworkcookie.h>
#include <QUrlQuery>
#include <QUuid>
#if defined(Q_OS_MACOS)
#include "PLSEvents.h"
#endif
#include "liblog.h"
using namespace common;
constexpr auto FACEBOOK_LOGIN = "facebook-login";
constexpr auto FRONTEND_PLUGINS_FACEBOOK_LOGIN = "frontend-plugins/facebook-login";
OBS_DECLARE_MODULE()

namespace {

bool runCustomSchemeServer(const QString &callbackUrl, const std::function<void(const QString &code)> &callback)
{
	auto getCode = [](const QString &url) {
		PLS_INFO(FACEBOOK_LOGIN, "facebook auth code from url = %s", qPrintable(url));
		QUrlQuery queryCode(QUrl(url).query());
		return queryCode.queryItemValue("code", QUrl::FullyDecoded);
	};

#if defined(Q_OS_WIN)
	auto server = new QLocalServer();
	if (!server->removeServer("PRISMLiveStudio")) {
		PLS_WARN(FACEBOOK_LOGIN, "remove local server failed");
		server->deleteLater();
		return false;
	}
	if (!server->listen("PRISMLiveStudio")) {
		PLS_WARN(FACEBOOK_LOGIN, "listen local server failed");
		server->deleteLater();
		return false;
	}
	QObject::connect(server, &QLocalServer::newConnection, [server, callbackUrl, callback, getCode] {
		QLocalSocket *client = server->nextPendingConnection();
		if (!client)
			return;
		QObject::connect(client, &QLocalSocket::readyRead, [client, server, callbackUrl, callback, getCode] {
			QString msg = QString::fromUtf8(client->readAll());
			if (msg.startsWith(callbackUrl)) {
				auto code = getCode(msg);
				server->disconnect();
				client->disconnect();
				client->flush();
				client->close();
				client->deleteLater();
				server->close();
				server->deleteLater();
				pls_invoke_safe(callback, code);
			}
		});
	});
#elif defined(Q_OS_MACOS)
	QObject::connect(
		PLS_EVENTS, &PLSEvents::facebookAuthCallbackUrl, PLS_EVENTS,
		[getCode, callbackUrl, callback](const QString &url) {
			if (!url.startsWith(callbackUrl)) {
				return;
			}
			pls_invoke_safe(callback, getCode(url));
		},
		Qt::SingleShotConnection);
#endif
	return true;
}

}

static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSFacebookLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSFacebookLoginInfo>());
}

const char *obs_module_name(void)
{
	return FACEBOOK_LOGIN;
}

const char *obs_module_description(void)
{
	return FACEBOOK_LOGIN;
}

PLSLoginInfo::UseFor PLSFacebookLoginInfo::useFor() const
{
	return UseFor::Prism_Channel;
}

PLSPlatformType PLSFacebookLoginInfo::platform() const
{
	return PLSPlatformType::Facebook;
}

QString PLSFacebookLoginInfo::name() const
{
	return QString::fromUtf8("Facebook");
}

QString PLSFacebookLoginInfo::icon(UseFor useFor) const
{
	if (UseFor::Prism == useFor) {
		return QString(":/images/login-facebook_login.svg");
	}
	return QString();
}

PLSLoginInfo::ImplementType PLSFacebookLoginInfo::loginWithAccountImplementType() const
{
	return PLSLoginInfo::ImplementType::Asynchronous;
}

void PLSFacebookLoginInfo::loginWithAccountAsync(const std::function<void(bool ok, const QVariantHash &)> &callback, UseFor, QWidget *) const
{
	del_pannel_cookies(FACEBOOK);
	const QString expectedState = QStringLiteral("login");

	if (!runCustomSchemeServer(g_plsFacebookCallbackUrl, [callback](const QString &code) {
		    if (!code.isEmpty()) {
			    QVariantHash result;
			    result.insert(ChannelData::g_channelName, QJsonValue(FACEBOOK));
			    result.insert(ChannelData::g_channelCode, QJsonValue(code));
			    callback(true, result);
		    } else {
			    PLS_WARN(FRONTEND_PLUGINS_FACEBOOK_LOGIN, "facebook code is empty");
			    callback(false, {});
		    }
	    })) {
		PLS_ERROR(FACEBOOK_LOGIN, "run Facebook auth custom scheme server failed");
		callback(false, {});
		return;
	}

	QUrl url = getFacebookChannelUrl(FACEBOOK_LOGIN_REDIRECT_URI, expectedState);
	PLS_INFO(FACEBOOK_LOGIN, "facebook login start url is %s", url.toDisplayString(QUrl::FullyDecoded).toUtf8().constData());
	if (!QDesktopServices::openUrl(url)) {
		PLS_ERROR(FACEBOOK_LOGIN, "open Facebook auth url failed");
		callback(false, {});
	}
	PLS_UI_ACTION("open Facebook login url finished");
}

PLSLoginInfo::ChannelSupport PLSFacebookLoginInfo::channelSupport() const
{
	return PLSLoginInfo::ChannelSupport::Rtmp;
}

QString PLSFacebookLoginInfo::rtmpUrl() const
{
	return QString();
}

QUrl PLSFacebookLoginInfo::getFacebookChannelUrl(const QString &redirectUri, const QString &state) const
{
	QUrl url(CHANNEL_FACEBOOK_LOGIN_URL);
	QUrlQuery query;
	query.addQueryItem(HTTP_CLIENT_ID, CHANNEL_FACEBOOK_CLIENT_ID);
	query.addQueryItem(HTTP_REDIRECT_URI, redirectUri);
	query.addQueryItem("response_type", "code");
	query.addQueryItem("auth_type", "rerequest");
	query.addQueryItem("state", state);
	url.setQuery(query);
	return url;
}
