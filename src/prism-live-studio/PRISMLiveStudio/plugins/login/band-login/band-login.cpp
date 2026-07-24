#include "band-login.hpp"
#include "pls-net-url.hpp"
#include "obs-module.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"

#include "pls-channel-const.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <qnetworkcookie.h>
constexpr auto BAND_LOGIN = "Band-login";
OBS_DECLARE_MODULE()
using namespace common;
static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSBandLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSBandLoginInfo>());
}

const char *obs_module_name(void)
{
	return BAND_LOGIN;
}

const char *obs_module_description(void)
{
	return BAND_LOGIN;
}

PLSBandLoginInfo::PLSBandLoginInfo()
{
	saveUrls(getBandLoginUrl().toString());
}

PLSLoginInfo::UseFor PLSBandLoginInfo::useFor() const
{
	return UseFor::Channel;
}

PLSPlatformType PLSBandLoginInfo::platform() const
{
	return PLSPlatformType::Band;
}
QString PLSBandLoginInfo::name() const
{
	return QString::fromUtf8(BAND);
}
QString PLSBandLoginInfo::icon(UseFor useFor) const
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

bool PLSBandLoginInfo::loginWithAccount(QVariantHash &result, UseFor, QWidget *parent) const
{
	bool bResult = false;
	auto check = [](QVariantHash &result_, const QString &url, const QMap<QString, QString> &) {
		if (url.startsWith(CHANNEL_BAND_REDIRECTURL)) {

			if (!pls_get_code_from_url(url).isEmpty()) {

				result_.insert(ChannelData::g_channelCode, QJsonValue(pls_get_code_from_url(url)));
				return PLSResultCheckingResult::Ok;
			} else {
				return PLSResultCheckingResult::Close;
			}

		} else {
			qDebug() << url;
			return PLSResultCheckingResult::Continue;
		}
	};

	del_pannel_cookies(BAND);

	bResult = pls_browser_view(result, getBandLoginUrl(), {}, BAND, check, parent, false);

	return bResult;
}
PLSBandLoginInfo::ChannelSupport PLSBandLoginInfo::channelSupport() const
{
	return PLSBandLoginInfo::ChannelSupport::Both;
}
QString PLSBandLoginInfo::rtmpUrl() const
{
	return QString();
}

QUrl PLSBandLoginInfo::getBandLoginUrl() const
{
	QString urlStr = QString(CHANNEL_BAND_LOGIN).arg(CHANNEL_BAND_ID).arg(CHANNEL_BAND_REDIRECTURL);
	return QUrl(urlStr);
}

void PLSBandLoginInfo::saveUrls(const QString &url)
{
	m_urls.append(url);
}
