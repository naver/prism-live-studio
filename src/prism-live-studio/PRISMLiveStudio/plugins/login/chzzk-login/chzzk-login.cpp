#include "chzzk-login.hpp"
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
#include "libhttp-client.h"
#include "liblog.h"
#include "PLSAlertView.h"
#include <qobject.h>
#include <qdesktopservices.h>

constexpr auto CHZZK_LOGIN = "Chzzk-login";
OBS_DECLARE_MODULE()
using namespace common;
constexpr auto NID_AUT = "NID_AUT";
constexpr auto NID_SES = "NID_SES";
constexpr auto NID_INF = "nid_inf";
constexpr auto CHZZKCOOKIE = "ChzzkCookie";

static void frontend_event(enum obs_frontend_event, void *v)
{
	pls_unused(v);
}

bool obs_module_load(void)
{
	if (!pls_register_login_info(PLSLoginInfo::instance<PLSChzzkLoginInfo>())) {
		return false;
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	pls_unregister_login_info(PLSLoginInfo::instance<PLSChzzkLoginInfo>());
}

const char *obs_module_name(void)
{
	return CHZZK_LOGIN;
}

const char *obs_module_description(void)
{
	return CHZZK_LOGIN;
}

PLSLoginInfo::UseFor PLSChzzkLoginInfo::useFor() const
{
	return UseFor::Channel;
}

PLSPlatformType PLSChzzkLoginInfo::platform() const
{
	return PLSPlatformType::NaverTv;
}
QString PLSChzzkLoginInfo::name() const
{
	return QString::fromUtf8(CHZZK);
}
QString PLSChzzkLoginInfo::icon(UseFor) const
{
	return QString();
}

bool PLSChzzkLoginInfo::loginWithAccount(QVariantHash &result, UseFor, QWidget *parent) const
{
	bool bResult = false;

	auto check = [this](QVariantHash &result_, const QString &url, const QMap<QString, QString> &cookies) {
		if (cookies.contains(NID_AUT) && cookies.contains(NID_SES) && cookies.contains(NID_INF)) {

			result_.insert(ChannelData::g_channelName, QJsonValue(CHZZK));
			result_.insert(ChannelData::g_channelCookie, QJsonValue(getCookies(cookies)));
			m_cookie = getCookies(cookies);
			qDebug() << url << "-----jump url-----" << result_;
			return PLSResultCheckingResult::Ok;

		} else {
			return PLSResultCheckingResult::Continue;
		}
	};

	//remove config save cookie values
	removeChzzkCookies();
	del_pannel_cookies(CHZZK);
	bResult = pls_browser_view(result, QUrl("https://nid.naver.com/nidlogin.login?nvlong=on&url=https%3A%2F%2chzzk.naver.com%2F"), {}, CHZZK, check, parent);
	if (bResult) {
		QEventLoop loop;

		getChzzkChannelList(result.value(ChannelData::g_channelCookie).toString(), loop);
		loop.exec();
		result.insert(ChannelData::g_expires_in, loop.property("expired").toBool());

		result.insert(ChannelData::g_errorRetdata, QVariant::fromValue<PLSErrorHandler::RetData>(m_retData));
		bResult = loop.property("success").toBool();
	}
	return bResult;
}
PLSChzzkLoginInfo::ChannelSupport PLSChzzkLoginInfo::channelSupport() const
{
	return PLSChzzkLoginInfo::ChannelSupport::Both;
}
QString PLSChzzkLoginInfo::rtmpUrl() const
{
	return QString();
}

PLSErrorHandler::RetData PLSChzzkLoginInfo::getErrorRetData(const pls::http::Reply &reply) const
{
	PLSErrorHandler::NetworkData data;
	data.errData = reply.data();
	data.netError = reply.error();
	data.statusCode = reply.statusCode();
	PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
	extraData.errPhase = PLSErrPhaseLogin;

	return PLSErrorHandler::getAlertString(data, CHZZK, "ChannelLoginFailedAgain", extraData);
}

bool PLSChzzkLoginInfo::errorHandler(const PLSErrorHandler::RetData &tmpRetData) const
{
	auto retData = tmpRetData;
	PLSErrorHandler::directShowAlert(retData, nullptr);
	return (retData.clickedBtn == PLSAlertView::Button::Ok && retData.prismCode == PLSErrorHandler::ErrCode::CHANNEL_CHZZK_1102_LOGIN_AGREEMENT_REQUIRED);
}
void PLSChzzkLoginInfo::getChzzkUserId(const QString &accessToken, QEventLoop &loop) const
{
	QString chzzkUserUrl = QString("%1/partner/naver/service/chzzk/user").arg(PRISM_API_BASE.arg(PRISM_SSL));
	pls::http::request(pls::http::Request()
				   .hmacUrl(chzzkUserUrl, PLS_PC_HMAC_KEY.toUtf8())
				   .withLog()
				   .receiver(&loop)
				   .contentType(common::HTTP_CONTENT_TYPE_VALUE)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .rawHeader("X-prism-access-token", accessToken)
				   .jsonOkResult([&loop, this](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   pls_unused(reply);
					   auto id = doc.object().value("userId").toString();
					   QDesktopServices::openUrl("https://studio.chzzk.naver.com/" + id);
					   loop.quit();
				   })
				   .jsonFailResult([&loop, this](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   pls_unused(doc);
					   auto retData = getErrorRetData(reply);
					   pls_async_call_mt([retData, &loop, this]() {
						   auto data = retData;
						   PLSErrorHandler::directShowAlert(data, nullptr);
						   loop.quit();
					   });
				   }));
}

void PLSChzzkLoginInfo::getChzzkChannelList(const QString &accessToken, QEventLoop &loop) const
{
	QString chzzkChannelList = QString("%1/partner/naver/service/chzzk/channel/list").arg(PRISM_API_BASE.arg(PRISM_SSL));
	pls::http::request(pls::http::Request()
				   .hmacUrl(chzzkChannelList, PLS_PC_HMAC_KEY.toUtf8())
				   .withLog()
				   .receiver(&loop)
				   .cookie(pls_get_prism_cookie())
				   .contentType(common::HTTP_CONTENT_TYPE_VALUE)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .rawHeader("X-prism-access-token", accessToken)
				   .jsonOkResult([&loop](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   pls_unused(reply, doc);
					   PLS_INFO("CHZZKLOGIN", "getChannel list success");
					   pls_async_call_mt([&loop]() {
						   loop.setProperty("success", true);
						   loop.quit();
					   });
				   })
				   .jsonFailResult([&loop, accessToken, this](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   pls_unused(doc);
					   auto retData = getErrorRetData(reply);
					   pls_async_call_mt([retData, &loop, accessToken, this]() {
						   if (retData.prismCode == PLSErrorHandler::ErrCode::COMMON_CHANNEL_LOGIN_TOKEN_EXPIRED_ERROR) {
							   PLS_INFO("CHZZKLOGIN", "prism token is expired");
							   loop.setProperty("expired", true);
							   m_retData = retData;
						   } else {
							   if (errorHandler(retData)) {
								   getChzzkUserId(accessToken, loop);
								   return;
							   }
							   PLS_ERROR("CHZZKLOGIN", "user cancel show term");
						   }
						   loop.quit();
					   });
				   }));
}

QString PLSChzzkLoginInfo::getCookies(const QMap<QString, QString> &cookies)
{
	for (auto cookie = cookies.constBegin(); cookie != cookies.constEnd(); ++cookie) {

		config_set_string(pls_get_global_cookie_config(), CHZZKCOOKIE, cookie.key().toUtf8().constData(), cookie.value().toUtf8().constData());
	}

	config_save(pls_get_global_cookie_config());
	pls_set_manual_cookies(CHZZK);
	return QString("NID_SES=%1;nid_inf=%2;NID_AUT=%3").arg(cookies.value(NID_SES)).arg(cookies.value(NID_INF)).arg(cookies.value(NID_AUT));
}

void PLSChzzkLoginInfo::removeChzzkCookies()
{
	config_remove_value(pls_get_global_cookie_config(), CHZZKCOOKIE, NID_AUT);
	config_remove_value(pls_get_global_cookie_config(), CHZZKCOOKIE, NID_SES);
	config_remove_value(pls_get_global_cookie_config(), CHZZKCOOKIE, NID_INF);
	config_save(pls_get_global_cookie_config());
}
