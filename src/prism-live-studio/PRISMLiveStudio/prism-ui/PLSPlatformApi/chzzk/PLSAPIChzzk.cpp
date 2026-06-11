#include "PLSAPIChzzk.h"
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
#include "liblog.h"

using namespace std;

// the log name of api failed
const QString CZ_Name_Pre = QString(CHZZK) + " ";
const QString CZ_API_GetChannelList = CZ_Name_Pre + "Get /partner/naver/service/chzzk/channel/list";
const QString CZ_API_GetLiveInfo = CZ_Name_Pre + "Get /partner/naver/service/chzzk/live/{id}";
const QString CZ_API_GetChannelInfo = CZ_Name_Pre + "Get /partner/naver/service/chzzk/channel/{id}";
const QString CZ_API_PutLiveInfo = CZ_Name_Pre + "Put /partner/naver/service/chzzk/live/{id}";
const QString CZ_API_GetCategories = CZ_Name_Pre + "Get /partner/naver/service/chzzk/search/categories";
const QString CZ_API_PostLiveInfo = CZ_Name_Pre + "Post /partner/naver/service/chzzk/{id}/live";
const QString CZ_API_PostThumbnail = CZ_Name_Pre + "Post /partner/naver/service/chzzk/live/{id}/thumbnail";
const QString CZ_API_DeleteThumbnail = CZ_Name_Pre + "Delete /partner/naver/service/chzzk/live/{id}/thumbnail";

namespace PLSAPIChzzk {
static QString _getNCPHost()
{
	return PRISM_API_BASE.arg(PRISM_SSL);
}
void configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed,
			  const PLSAPICommon::errorCallback &onFailed, const QByteArray &logName, bool isSetContentType)
{

	addCommonCookieAndUserKey(_request);
	if (isSetContentType) {
		_request.contentType(common::HTTP_CONTENT_TYPE_VALUE);
	}
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
	auto infos = PLSCHANNELS_API->getChanelInfosByPlatformName(CHZZK, ChannelData::ChannelType);
	if (infos.empty()) {
		PLS_ERROR(MODULE_PLATFORM_CHZZK, "chzzk get cookie failed, the channels info is empty");
		assert(false);
		return;
	}

	QString accToken = infos[0][ChannelData::g_channelCookie].toString();
	_request.cookie(PLSLoginUserInfo::getInstance()->getPrismCookie());
	_request.rawHeader("X-prism-access-token", accToken);
}

void requestChannelList(const QObject *receiver, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed, platform] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, CZ_API_GetChannelList.toUtf8());
		QString url = QString("%1/partner/naver/service/chzzk/channel/list").arg(_getNCPHost());
		_request.method(pls::http::Method::Get) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.withLog();
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_CHZZK, "requestChannelList start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestChannelOrLiveInfo(const QObject *receiver, bool isChannel, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			      PLSAPICommon::RefreshType refreshType)
{

	auto _getNetworkReply = [receiver, onSucceed, onFailed, platform, isChannel] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, isChannel ? CZ_API_GetChannelInfo.toUtf8() : CZ_API_GetLiveInfo.toUtf8());
		QString url;
		if (isChannel) {
			url = QString("%1/partner/naver/service/chzzk/channel/%2").arg(_getNCPHost()).arg(platform->subChannelID());
		} else {
			url = QString("%1/partner/naver/service/chzzk/live/%2").arg(_getNCPHost()).arg(platform->getSelectData()._id);
		}

		_request.method(pls::http::Method::Get) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.withLog();
		pls::http::request(_request);
	};
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestUpdateLiveInfo(const QObject *receiver, const PLSChzzkLiveinfoData &data, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed,
			   const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{

	auto _getNetworkReply = [receiver, data, onSucceed, onFailed, platform] {
		QJsonObject object;
		object["title"] = data.title;
		object["liveCategory"] = data.categoryData.id;
		QJsonObject extra;
		extra["adult"] = data.isAgeLimit;
		extra["paidPromotion"] = data.isNeedMoney;
		extra["chatAvailableGroup"] = data.chatPermission;
		extra["clipActive"] = data.clipActive;
		object["extraFields"] = extra;

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, CZ_API_PutLiveInfo.toUtf8());
		QString url = QString("%1/partner/naver/service/chzzk/live/%2").arg(_getNCPHost()).arg(data._id);

		_request.method(pls::http::Method::Put) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.body(object);
		PLSAPICommon::maskingUrlKeys(_request, {});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_CHZZK, "requestCreateLive start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestSearchCategory(const QObject *receiver, const QString &keyword, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			   PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, keyword, onSucceed, onFailed, platform] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, CZ_API_GetCategories.toUtf8());
		QString url = QString("%1/partner/naver/service/chzzk/search/categories").arg(_getNCPHost());
		_request.method(pls::http::Method::Get) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.urlParam("keyword", keyword)
			.withLog();
		;
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_CHZZK, "requestScheduleList start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void requestCreateLive(const QObject *receiver, const PLSChzzkLiveinfoData &data, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
		       PLSAPICommon::RefreshType refreshType)
{

	auto _getNetworkReply = [receiver, data, onSucceed, onFailed, platform] {
		QJsonObject object;
		object["title"] = data.title;
		object["liveCategory"] = data.categoryData.id;

		QJsonObject extra;
		extra["adult"] = data.isAgeLimit;
		extra["paidPromotion"] = data.isNeedMoney;
		extra["chatAvailableGroup"] = data.chatPermission;
		extra["clipActive"] = data.clipActive;
		object["extraFields"] = extra;

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, CZ_API_PostLiveInfo.toUtf8());
		QString url = QString("%1/partner/naver/service/chzzk/%2/live").arg(_getNCPHost()).arg(platform->subChannelID());
		_request.method(pls::http::Method::Post) //
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.body(object);
		PLSAPICommon::maskingUrlKeys(_request, {});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_CHZZK, "requestCreateLive start");
	refreshTokenBeforeRequest(platform, refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void uploadImage(const QObject *receiver, const PLSChzzkLiveinfoData &data, const QString &imageFilePath, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed,
		 const PLSAPICommon::errorCallback &onFailed)
{
	auto _getNetworkReply = [receiver, data, onSucceed, onFailed, platform, imageFilePath] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, CZ_API_PostThumbnail.toUtf8(), false);
		QString url = QString("%1/partner/naver/service/chzzk/live/%2/thumbnail").arg(_getNCPHost()).arg(data._id);

		_request.method(pls::http::Method::Post) //
			.url(url)
			.timeout(PRISM_NET_DOWNLOAD_TIMEOUT)
			.form("thumbnailFile", imageFilePath, true)
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8());
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_CHZZK, "chzzk uploadImage start");
	refreshTokenBeforeRequest(platform, PLSAPICommon::RefreshType::NotRefresh, _getNetworkReply, receiver, onSucceed, onFailed);
}

void deleteImage(const QObject *receiver, const PLSChzzkLiveinfoData &data, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed)
{
	auto _getNetworkReply = [receiver, data, onSucceed, onFailed, platform] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		configDefaultRequest(_request, receiver, platform, onSucceed, onFailed, CZ_API_DeleteThumbnail.toUtf8(), false);

		QString url = QString("%1/partner/naver/service/chzzk/live/%2/thumbnail").arg(_getNCPHost()).arg(data._id);

		_request.method(pls::http::Method::Delete) //
			.url(url)
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8());
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PLATFORM_CHZZK, "chzzk delete start");
	refreshTokenBeforeRequest(platform, PLSAPICommon::RefreshType::NotRefresh, _getNetworkReply, receiver, onSucceed, onFailed);
}

void refreshTokenBeforeRequest(PLSPlatformChzzk *platform, PLSAPICommon::RefreshType refreshType, const std::function<void()> &originNetworkReplay, const QObject *originReceiver,
			       const PLSAPICommon::dataCallback &originOnSucceed, const PLSAPICommon::errorCallback &originOnFailed)
{
	pls_check_app_exiting();

	if (originNetworkReplay != nullptr) {
		originNetworkReplay();
	}
}

void showFailedLog(const QString &logName, const pls::http::Reply &reply, PLSPlatformChzzk *platform)
{
	auto _code = reply.statusCode();
	auto _data = reply.data();

	int subCode = 0;
	QString errMsg("-");
	QString errException("-");

	if (!logName.isEmpty()) {
		if (getErrCode(_data, subCode, errMsg, errException)) {
			PLS_ERROR(MODULE_PLATFORM_CHZZK, "%s failed. statusCode:%i exception:%s \tmsg:%s \t subCode:%i", logName.toUtf8().constData(), _code, errException.toUtf8().constData(),
				  errMsg.toUtf8().constData(), subCode);
		} else {
			PLS_ERROR(MODULE_PLATFORM_CHZZK, "%s failed. with not object data.", logName.toUtf8().constData());
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
