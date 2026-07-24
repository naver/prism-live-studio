#include "afreecatv-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"

#include "pls-channel-const.h"
#include <QDebug>
#include <qnetworkcookie.h>
using namespace common;
static const char *const s_afreecaTV_login = "AfreecaTV-login";

OBS_DECLARE_MODULE()

static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSAfreecaTVLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSAfreecaTVLoginInfo>());
}

const char *obs_module_name(void)
{
	return s_afreecaTV_login;
}

const char *obs_module_description(void)
{
	return s_afreecaTV_login;
}

PLSLoginInfo::UseFor PLSAfreecaTVLoginInfo::useFor() const
{
	return UseFor::Channel;
}

PLSPlatformType PLSAfreecaTVLoginInfo::platform() const
{
	return PLSPlatformType::AfreecaTV;
}
QString PLSAfreecaTVLoginInfo::name() const
{
	return QString::fromUtf8(AFREECATV);
}
QString PLSAfreecaTVLoginInfo::icon(UseFor) const
{
	return QString();
}

// Exclude known in-flow pages on the same host; complete only on redirect root (Sign up opens /policy/ etc. under www).
bool PLSAfreecaTVLoginInfo::isAfreecaChannelLoginReturnUrl(const QString &urlString) const
{
	if (!urlString.startsWith(CHANNEL_AFREECA_REDIRECTURL, Qt::CaseInsensitive))
		return false;
	const QString rest = urlString.mid(CHANNEL_AFREECA_REDIRECTURL.size());
	if (rest.startsWith(QLatin1String("/policy/"), Qt::CaseInsensitive) || rest.startsWith(QLatin1String("/terms"), Qt::CaseInsensitive) ||
	    rest.startsWith(QLatin1String("/legal/"), Qt::CaseInsensitive) || rest.startsWith(QLatin1String("/agreement"), Qt::CaseInsensitive))
		return false;

	if (rest.isEmpty() || rest == QLatin1String("/"))
		return true;
	if (rest.startsWith(QLatin1String("/?")) || rest.startsWith(QLatin1Char('?')))
		return true;
	return false;
}

bool PLSAfreecaTVLoginInfo::loginWithAccount(QVariantHash &result, UseFor, QWidget *parent) const
{
	bool bResult = false;

	auto check = [this](QVariantHash &resultCallback, const QString &url, const QMap<QString, QString> &cookies) {
		if (isAfreecaChannelLoginReturnUrl(url)) {

			resultCallback.insert(ChannelData::g_channelCookie, QJsonValue(getCookies(cookies)));
			qDebug() << url << "-----jump url-----" << resultCallback;
			return PLSResultCheckingResult::Ok;

		} else {
			qDebug() << url;
			return PLSResultCheckingResult::Continue;
		}
	};

	del_pannel_cookies(AFREECATV);
	bResult = pls_browser_view(result, CHANNEL_AFREECA_LOGIN, {}, AFREECATV, check, parent);

	return bResult;
}
PLSAfreecaTVLoginInfo::ChannelSupport PLSAfreecaTVLoginInfo::channelSupport() const
{
	return PLSAfreecaTVLoginInfo::ChannelSupport::Both;
}
QString PLSAfreecaTVLoginInfo::rtmpUrl() const
{
	return QString();
}

QString PLSAfreecaTVLoginInfo::getCookies(const QMap<QString, QString> &cookies)
{
	QStringList cookieList;
	for (QMap<QString, QString>::const_iterator i = cookies.constBegin(); i != cookies.constEnd(); i++) {
		cookieList.append(i.key() + "=" + i.value());
	}
	QString finalStr = cookieList.join(";");
	return finalStr;
}
