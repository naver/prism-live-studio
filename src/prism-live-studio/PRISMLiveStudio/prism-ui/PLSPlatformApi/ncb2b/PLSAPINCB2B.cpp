/**
 API Doc: https://wiki.navercorp.com/pages/viewpage.action?pageId=1661981717
 */

#include "PLSAPINCB2B.h"
#include <qfileinfo.h>
#include <QNetworkCookieJar>
#include <vector>
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformApi.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-gpop-data.hpp"
#include "common/PLSAPICommon.h"
#include "common/PLSDateFormate.h"
#include "login-user-info.hpp"
#include "pls-common-define.hpp"
#include "PLSCommonConst.h"

using namespace std;

namespace PLSAPINCB2B {
static QString _getNCPHost()
{
	return PRISM_API_BASE.arg(PRISM_SSL);
}

static QString _getNCPHost_auth()
{
	return pls_http_api_func::getPrismAuthGateWay();
}
void configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed,
			  const PLSAPICommon::errorCallback &onFailed, const QByteArray &logName, bool isNeedCookie)
{
	if (isNeedCookie) {
		addCommonCookieAndUserKey(_request);
	}
	_request.contentType(common::HTTP_CONTENT_TYPE_VALUE);

	_request.timeout(PRISM_NET_REQUEST_TIMEOUT)
		.receiver(receiver)
		.okResult([onSucceed, receiver](const pls::http::Reply &reply) {
			if (onSucceed && !pls_get_app_exiting()) {
				auto _data = reply.data();
				pls_async_call_mt(receiver, [onSucceed, _data] { onSucceed(_data); });
			}
		})
		.failResult([onFailed, receiver, logName, platform](const pls::http::Reply &reply) {
			showFailedLog(logName, reply, platform);
			auto _code = reply.statusCode();
			auto _data = reply.data();
			auto _err = reply.error();
			if (onFailed && !pls_get_app_exiting()) {
				pls_async_call_mt(receiver, [onFailed, _code, _data, _err] { onFailed(_code, _data, _err); });
			}
		});
}

void addCommonCookieAndUserKey(const pls::http::Request &_request)
{
	_request.cookie(PLSLoginUserInfo::getInstance()->getPrismCookie());
	_request.rawHeader("X-prism-access-token", PLSLoginUserInfo::getInstance()->getNCPPlatformToken());
}

void requestChannelList(const QObject *receiver, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed, platform] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, "requestChannelList");

		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		QString url = QString("%1/partner/ncp/service/%2/channel/list").arg(PRISM_API_BASE.arg(PRISM_SSL)).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId());
		_request.method(pls::http::Method::Get) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.withLog();
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_NCB2B, "requestChannelList start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestScheduleList(const QObject *receiver, const QString &channelID, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			 PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, channelID, onSucceed, onFailed, platform] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, "requestScheduleList");
		QString url = QString("%1/partner/ncp/service/%2/%3/lives").arg(_getNCPHost()).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId()).arg(channelID);
		_request.method(pls::http::Method::Get) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.urlParam("status", "RESERVED");
		PLSAPICommon::maskingUrlKeys(_request, {channelID});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_NCB2B, "requestScheduleList start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestCreateLive(const QObject *receiver, const PLSNCB2BLiveinfoData &data, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
		       PLSAPICommon::RefreshType refreshType)
{

	auto _getNetworkReply = [receiver, data, onSucceed, onFailed, platform] {
		QJsonObject object;
		object["title"] = data.title;
		object["description"] = data.description;
		object["scope"] = data.scope;
		auto nowTime = PLSDateFormate::youtubeTimeStampToString(PLSDateFormate::getNowTimeStamp() + 20);
		object["reservedAt"] = nowTime;

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, "requestCreateLive");
		QString url = QString("%1/partner/ncp/service/%2/%3/live").arg(_getNCPHost()).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId()).arg(platform->subChannelID());
		_request.method(pls::http::Method::Post) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.body(object);
		PLSAPICommon::maskingUrlKeys(_request, {platform->subChannelID()});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_NCB2B, "requestCreateLive start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestUpdateLive(const QObject *receiver, const PLSNCB2BLiveinfoData &data, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
		       PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, data, onSucceed, onFailed, platform] {
		QJsonObject object;
		object["title"] = data.title;
		object["description"] = data.description;
		object["scope"] = data.scope;
		object["reservedAt"] = data.startTimeOrigin;

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, "requestUpdateLive");
		QString url = QString("%1/partner/ncp/service/%2/live/%3").arg(_getNCPHost()).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId()).arg(data._id);
		_request.method(pls::http::Method::Put) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.body(object);
		PLSAPICommon::maskingUrlKeys(_request, {data._id});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_NCB2B, "requestUpdateLive start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestGetLiveInfo(const QObject *receiver, const QString &liveId, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, liveId, onSucceed, onFailed, platform] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, "requestGetLiveInfo");
		QString url = QString("%1/partner/ncp/service/%2/live/%3").arg(_getNCPHost()).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId()).arg(liveId);
		_request.method(pls::http::Method::Get) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8());
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_NCB2B, "requestGetLiveInfo start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void refreshTokenBeforeRequest(PLSPlatformNCB2B *platform, PLSAPICommon::RefreshType refreshType, const std::function<void()> &originNetworkReplay, const QObject *originReceiver,
			       const PLSAPICommon::dataCallback &originOnSucceed, const PLSAPICommon::errorCallback &originOnFailed)
{
	pls_check_app_exiting();

	if (refreshType == PLSAPICommon::RefreshType::NotRefresh ||
	    (refreshType == PLSAPICommon::RefreshType::CheckRefresh && PLSAPICommon::isTokenValid(PLSLoginUserInfo::getInstance()->getNCPPlatformExpiresTime()))) {
		if (nullptr != originNetworkReplay) {
			originNetworkReplay();
		} else if (nullptr != originOnSucceed) {
			originOnSucceed(nullptr);
		}
		return;
	}

	auto _onSucceed = [originNetworkReplay, originOnSucceed, originOnFailed](QByteArray data) {
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			PLS_INFO(MODULE_PLATFORM_NCB2B, "refreshTokenBeforeRequest succeed");
			auto root = doc.object();
			qint64 expiredTime = root["expires_in"].toInt() + PLSDateFormate::getNowTimeStamp();
			PLSLoginUserInfo::getInstance()->updateNCB2BTokenInfo(root["access_token"].toString(), root["refresh_token"].toString(), expiredTime);
			if (originNetworkReplay != nullptr) {
				originNetworkReplay();
			} else if (nullptr != originOnSucceed) {
				originOnSucceed(data);
			}
		} else {
			PLS_ERROR(MODULE_PLATFORM_NCB2B, "refreshTokenBeforeRequest failed, doc is not object");
			if (nullptr != originOnFailed) {
				originOnFailed(1999, data, QNetworkReply::InternalServerError);
			}
		}
	};

	auto _onFail = [originOnFailed](int code, QByteArray data, QNetworkReply::NetworkError error) { originOnFailed(code, data, error); };

	PLS_INFO(MODULE_PLATFORM_NCB2B, "refreshTokenBeforeRequest start");

	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	configDefaultRequest(_request, originReceiver, platform, _onSucceed, _onFail, "refreshTokenBeforeRequest", false);
	QString url = QString("%1/oauth/ncp/refresh").arg(_getNCPHost_auth());

	QJsonObject obj;
	obj["refreshToken"] = PLSLoginUserInfo::getInstance()->getNCPPlatformRefreshToken();

	_request.method(pls::http::Method::Post) //
		.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
		.urlParam("serviceId", PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId())
		.withLog()
		.body(obj);
	pls::http::request(_request);
}

void showFailedLog(const QString &logName, const pls::http::Reply &reply, PLSPlatformNCB2B *platform)
{
	auto _code = reply.statusCode();
	auto _data = reply.data();

	int subCode = 0;
	QString errMsg("-");
	QString errException("-");

	if (!logName.isEmpty()) {
		if (getErrCode(_data, subCode, errMsg, errException)) {
			PLS_ERROR(MODULE_PLATFORM_NCB2B, "%s failed. code:%i reason:%s \tmsg:%s", logName.toUtf8().constData(), _code, errException.toUtf8().constData(), errMsg.toUtf8().constData());
		} else {
			PLS_ERROR(MODULE_PLATFORM_NCB2B, "%s failed. with not object data.", logName.toUtf8().constData());
		}
	}
}

bool getErrCode(const QByteArray &data, int &subCode, QString &message, QString &exception)
{
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		subCode = 0;
		message = data.left(200);
		exception = "-";
		return false;
	}

	auto root = doc.object();
	subCode = root["code"].toInt();
	message = root["message"].toString();
	exception = root["exception"].toString();

	if (subCode == 0 && message.isEmpty()) {
		auto errCode = root["errorCode"].toString();
		if (errCode == PRISM_API_SYSTEM_ERROR) {
			subCode = 25;
			message = root["errorMessage"].toString();
		}
	}
	return true;
}

}
