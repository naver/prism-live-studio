#include "PLSPlatformBand.h"

#include <QUrl>

#include "PLSChannelDataHandlerFunctions.h"
#include "json-data-handler.hpp"
#include "pls-common-define.hpp"
#include "../PLSLiveInfoDialogs.h"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../channels/ChannelsDataApi/ChannelCommonFunctions.h"
#include "pls-channel-const.h"
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include "PLSLiveInfoBand.h"
#include "PLSBandDataHandler.h"
#include "prism/PLSPlatformPrism.h"
#include "libhttp-client.h"
#include <QUrlQuery>
#include "PLSResCommonFuns.h"

using namespace common;
using namespace std;
#define EXPIRE_OFFSET(X) (((X) - (30 * 60))) //half hour(s)

extern QString maskUrl(const QString &url, const QVariantMap &queryInfo);

void PLSPlatformBand::initLiveInfo(bool isUpdate) const
{
	if (isUpdate) {
		for (auto bandPlatform : PLS_PLATFORM_API->getExistedPlatformsByType(PLSServiceType::ST_BAND)) {
			bandPlatform->setTitle("");
		}
	}
}

void PLSPlatformBand::clearBandInfos()
{
	m_bandInfos.clear();
}

void PLSPlatformBand::getBandRefreshTokenInfo(const refreshTokenCallback &callbackfunc, bool isForceUpdate)
{

	auto channelInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	if (!isTokenValid(channelInfo) || isForceUpdate) {
		QString refreshToken = channelInfo.value(ChannelData::g_refreshToken).toString();
		QString authUrlStr = QString(CHANNEL_BAND_REFRESH_TOKEN).arg(refreshToken);

		QString codeBase64 = QString("%1:%2").arg(CHANNEL_BAND_ID).arg(CHANNEL_BAND_SECRET).toUtf8().toBase64();
		QString authHeadValue = QString("Basic %1").arg(codeBase64);

		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .jsonContentType() //
					   .rawHeader("Authorization", authHeadValue)
					   .withLog()             //
					   .receiver(this)        //
					   .workInMainThread()    //
					   .url(QUrl(authUrlStr)) //
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .okResult([this, callbackfunc](const pls::http::Reply &reply) {
						   auto data = reply.data();
						   auto statusCode = reply.statusCode();
						   responseRefreshTokenHandler(callbackfunc, data, statusCode);
					   })
					   .failResult([callbackfunc](const pls::http::Reply &reply) {
						   pls_unused(reply);
						   PLS_ERROR(MODULE_PLATFORM_BAND, "getBandRefreshTokenInfo .refresh band token failed error: %d-%d", reply.statusCode(), reply.error());

						   if (callbackfunc) {
							   callbackfunc(false);
						   }
					   }));

	} else {
		if (callbackfunc) {
			callbackfunc(false);
		}
	}
}
void PLSPlatformBand::getBandTokenInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	m_bandLoginInfo = srcInfo;

	QString code = m_bandLoginInfo.value(ChannelData::g_channelCode).toString();
	QString authUrlStr = QString(CHANNEL_BAND_AUTH).arg(code);

	QString codeBase64 = QString("%1:%2").arg(CHANNEL_BAND_ID).arg(CHANNEL_BAND_SECRET).toUtf8().toBase64();
	QString authHeadValue = QString("Basic %1").arg(codeBase64);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeader("Authorization", authHeadValue)
				   .withLog()             //
				   .receiver(this)        //
				   .workInMainThread()    //
				   .url(QUrl(authUrlStr)) //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this, finishedCall](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   auto statusCode = reply.statusCode();
					   responseTokenHandler(finishedCall, data, statusCode);
				   })
				   .failResult([this, finishedCall](const pls::http::Reply &reply) {

					   auto statusCode = reply.statusCode();
					   auto error = reply.error();
					   PLS_ERROR(MODULE_PLATFORM_BAND, "getBandTokenInfo .error: %d -%d", statusCode, error);
					   showChannelInfoError(getApiResult(statusCode, error));
					   m_bandLoginInfo[ChannelData::g_channelSreLoginFailed] = "band get token failed";

					   m_bandInfos.append(m_bandLoginInfo);
					   finishedCall(m_bandInfos);
				   }));
}

void PLSPlatformBand::getBandCategoryInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	m_bandLoginInfo = srcInfo;

	auto failedResult = [this, finishedCall](const pls::http::Reply &reply) {
		showChannelInfoError(getApiResult(reply.statusCode(), reply.error()));
		m_bandLoginInfo[ChannelData::g_channelSreLoginFailed] = "band get categoryInfo failed";

		m_bandInfos.append(m_bandLoginInfo);
		finishedCall(m_bandInfos);
	};
	auto callbackFun = [this, finishedCall, failedResult](bool) {
		QString token = PLSCHANNELS_API->getChannelInfo(getChannelUUID())[ChannelData::g_channelToken].toString();
		if (token.isEmpty()) {
			token = m_bandLoginInfo[ChannelData::g_channelToken].toString();
		}
		QMap<QString, QString> queryParams = {{"filter", "create_live"}, {COOKIE_ACCESS_TOKEN, token}};
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .jsonContentType()
					   .withLog()
					   .receiver(this)
					   .workInMainThread()
					   .url(CHANNEL_BAND_CATEGORY)
					   .urlParams(queryParams)
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .okResult([this, finishedCall](const pls::http::Reply &reply) { responseBandCategoryHandler(finishedCall, reply.data(), reply.statusCode()); })
					   .failResult(failedResult));
	};

	//refresh token
	getBandRefreshTokenInfo(callbackFun);
}

void PLSPlatformBand::getChannelEmblemAsync() const
{
	auto bandInfos = PLSCHANNELS_API->getChanelInfosByPlatformName(BAND, ChannelData::ChannelType);
	for (const auto &band : bandInfos) {
		auto url = band.value(ChannelData::g_userIconThumbnailUrl).toString();
		QString bandUUID = band.value(ChannelData::g_channelUUID).toString();
		auto path = getTmpCacheDir() + "/" + BAND + url.split('/').last();
		PLSResCommonFuns::downloadResource(
			url,
			[bandUUID, path](PLSResEvents event, const QString &) {
				if (event == PLSResEvents::RES_DOWNLOAD_SUCCESS) {
					PLSCHANNELS_API->setValueOfChannel(bandUUID, ChannelData::g_userIconCachePath, path);
					PLSCHANNELS_API->channelModified(bandUUID);
				} else {
					PLS_INFO(MODULE_PLATFORM_BAND, "download user icon failed");
				}
			},
			path, false, false, false);
	}
}

PLSServiceType PLSPlatformBand::getServiceType() const
{
	return PLSServiceType::ST_BAND;
}

void PLSPlatformBand::onPrepareLive(bool value)
{
	if (!value) {
		PLSPlatformBase::onPrepareLive(value);
		return;
	}
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s show liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	value = pls_exec_live_Info_band(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s liveinfo closed value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	PLSPlatformBase::onPrepareLive(value);
}

QString PLSPlatformBand::getShareUrl()
{
	return QString();
}

void PLSPlatformBand::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}
	pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("Live.Check.Band.Broardcast.Start.Tip"));
}

bool PLSPlatformBand::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}

void PLSPlatformBand::onAllPrepareLive(bool isOk)
{
	if (!isOk && m_isRequestStart) {

		requesetLiveEnd([this] { PLSPlatformBase::onAllPrepareLive(false); });
		return;
	}
	PLSPlatformBase::onAllPrepareLive(isOk);
}

void PLSPlatformBand::onLiveEnded()
{
	m_isRequestStart = false;
	requesetLiveEnd([this] { liveEndedCallback(); });
	setTitle(std::string());
}

PLSPlatformApiResult PLSPlatformBand::getApiResult(int code, QNetworkReply::NetworkError error) const
{
	PLS_ERROR(MODULE_PLATFORM_BAND, "getApiResult .error: %d -%d", code, error);

	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {
		return result;
	} else if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		switch (code) {
		case 400:
			result = PLSPlatformApiResult::PAR_SERVER_ERROR;
			break;
		case 401:
			result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
			break;
		case 403:
			result = PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN;
			break;
		case 60106:
			result = PLSPlatformApiResult::PAR_API_ERROR_NO_PERMISSION;
			break;
		default:
			result = PLSPlatformApiResult::PAR_API_FAILED;
			break;
		}
	}

	return result;
}

void PLSPlatformBand::showApiRefreshError(PLSPlatformApiResult value) const
{
	switch (value) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
	case PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN:
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_NO_PERMISSION:
		break;
	default:
		pls_alert_error_message(nullptr, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Band.Failed"));
		break;
	}
}
void PLSPlatformBand::showChannelInfoError(PLSPlatformApiResult value)
{
	switch (value) {
	case PLSPlatformApiResult::PAR_SUCCEED:
		break;
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		//network error
		m_bandLoginInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		break;

	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
	case PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN:
		m_bandLoginInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Expired;

		break;
	case PLSPlatformApiResult::PAR_API_ERROR_Live_Invalid:
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_Scheduled_Time:
		break;
	case PLSPlatformApiResult::PAR_SERVER_ERROR:
		m_bandLoginInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		m_bandLoginInfo[ChannelData::g_isUpdated] = false;
		break;
	default:
		break;
	}
}
void PLSPlatformBand::responseRefreshTokenHandler(const refreshTokenCallback &callbackfunc, const QByteArray &data, int code)
{
	QVariant token;
	QVariant refreshToken;
	QVariant expire_in;
	PLSJsonDataHandler::getValueFromByteArray(data, COOKIE_ACCESS_TOKEN, token);
	PLSJsonDataHandler::getValueFromByteArray(data, COOKIE_REFRESH_TOKEN, refreshToken);
	PLSJsonDataHandler::getValueFromByteArray(data, COOKIE_EXPIRES_IN, expire_in);

	if (token.isValid() && code == HTTP_STATUS_CODE_200) {
		auto bandInfos = PLSCHANNELS_API->getChanelInfosByPlatformName(BAND, ChannelData::ChannelType);

		m_bandLoginInfo.insert(ChannelData::g_channelToken, token);
		m_bandLoginInfo.insert(ChannelData::g_refreshToken, refreshToken);
		m_bandLoginInfo.insert(ChannelData::g_expires_in, EXPIRE_OFFSET(expire_in.toLongLong()));
		m_bandLoginInfo.insert(ChannelData::g_createTime, QDateTime::currentDateTime());
		for (const auto &band : bandInfos) {
			QString bandUUID = band.value(ChannelData::g_channelUUID).toString();

			PLSCHANNELS_API->setValueOfChannel(bandUUID, ChannelData::g_channelToken, token);
			PLSCHANNELS_API->setValueOfChannel(bandUUID, ChannelData::g_refreshToken, refreshToken);
			PLSCHANNELS_API->setValueOfChannel(bandUUID, ChannelData::g_createTime, QDateTime::currentDateTime());
			PLSCHANNELS_API->setValueOfChannel(bandUUID, ChannelData::g_expires_in, EXPIRE_OFFSET(expire_in.toLongLong()));

			PLSCHANNELS_API->channelModified(bandUUID);
			PLS_PLATFORM_API->onUpdateChannel(bandUUID);
		}
	}
	if (callbackfunc) {
		callbackfunc(true);
	}
	PLS_INFO(MODULE_PLATFORM_BAND, "responseRefreshTokenHandler refresh band token success");
}
void PLSPlatformBand::responseTokenHandler(const UpdateCallback &finishedCall, const QByteArray &data, int code)
{
	QVariant token;
	QVariant refreshToken;
	QVariant expire_in;
	PLSJsonDataHandler::getValueFromByteArray(data, COOKIE_ACCESS_TOKEN, token);
	PLSJsonDataHandler::getValueFromByteArray(data, COOKIE_REFRESH_TOKEN, refreshToken);
	PLSJsonDataHandler::getValueFromByteArray(data, COOKIE_EXPIRES_IN, expire_in);

	if (token.isValid() && code == HTTP_STATUS_CODE_200) {
		m_bandLoginInfo.insert(ChannelData::g_channelToken, token);
		m_bandLoginInfo.insert(ChannelData::g_refreshToken, refreshToken);
		m_bandLoginInfo.insert(ChannelData::g_expires_in, EXPIRE_OFFSET(expire_in.toLongLong()));
		m_bandLoginInfo.insert(ChannelData::g_createTime, QDateTime::currentDateTime());
		m_bandLoginInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
		getBandCategoryInfo(m_bandLoginInfo, finishedCall);

	} else {
		m_bandLoginInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::UnInitialized;
		m_bandInfos.append(m_bandLoginInfo);
		finishedCall(m_bandInfos);
	}
}
template<typename finshedCallFun> void PLSPlatformBand::responseBandCategoryHandler(const finshedCallFun &finishedCall, const QByteArray &data, int code)
{
	pls_unused(code);
	QVariantList bands;
	PLSJsonDataHandler::getValuesFromByteArray(data, "bands", bands);
	if (!bands.isEmpty()) {
		for (const auto &band : bands) {
			QVariantMap tmpband;
			tmpband[ChannelData::g_channelToken] = m_bandLoginInfo[ChannelData::g_channelToken];
			tmpband[ChannelData::g_channelCode] = m_bandLoginInfo[ChannelData::g_channelCode];
			tmpband[ChannelData::g_platformName] = m_bandLoginInfo[ChannelData::g_platformName];
			tmpband[ChannelData::g_expires_in] = m_bandLoginInfo[ChannelData::g_expires_in];
			tmpband[ChannelData::g_createTime] = m_bandLoginInfo[ChannelData::g_createTime];
			tmpband[ChannelData::g_refreshToken] = m_bandLoginInfo[ChannelData::g_refreshToken];
			tmpband[ChannelData::g_userIconThumbnailUrl] = band.toMap()["cover"].toString();
			tmpband[ChannelData::g_nickName] = band.toMap()["name"].toString();
			tmpband[ChannelData::g_subChannelId] = band.toMap()["band_key"].toString();
			tmpband[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;

			m_bandInfos.append(tmpband);
		}

	} else {
		m_bandLoginInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		m_bandInfos.append(m_bandLoginInfo);
	}
	finishedCall(m_bandInfos);
	pls_async_call_mt(this, [this]() { getChannelEmblemAsync(); });
}
template<typename responseCallbackFunc> void PLSPlatformBand::responseStreamLiveKeyHandler(const responseCallbackFunc &callback, const QByteArray &data, int code)
{
	PLSPlatformApiResult result = PLSPlatformApiResult::PAR_API_FAILED;
	QVariant resultCode;
	QVariant publishUrl;
	QVariant streamKey;
	QVariant publishUrlNotKey;
	QVariant liveId;
	QVariant description;
	QVariant maxLiveTime;
	if (HTTP_STATUS_CODE_200 == code) {

		PLSJsonDataHandler::getValueFromByteArray(data, "result_code", resultCode);

		PLSJsonDataHandler::getValueFromByteArray(data, "publish_url", publishUrl);
		PLSJsonDataHandler::getValueFromByteArray(data, "stream_key", streamKey);
		PLSJsonDataHandler::getValueFromByteArray(data, "publish_url_without_stream_key", publishUrlNotKey);
		PLSJsonDataHandler::getValueFromByteArray(data, "live_id", liveId);
		PLSJsonDataHandler::getValueFromByteArray(data, "description", description);
		PLSJsonDataHandler::getValueFromByteArray(data, "max_running_time_mins", maxLiveTime);

		setStreamKey(streamKey.toString().toUtf8().data());
		setStreamServer(publishUrlNotKey.toString().toUtf8().data());
		setMaxLiveTime(maxLiveTime.toInt());
		setLiveId(liveId.toString());
		// handler multi and single stream
		if (resultCode == 1) {
			PLS_LOGEX(PLS_LOG_INFO, MODULE_PLATFORM_BAND, {{"platformName", "band"}, {"startLiveStatus", "Success"}, },
				  "band start live success");
			PLS_INFO(MODULE_PLATFORM_BAND, "responseStreamLiveKeyHandler band perpare ok");
			m_isRequestStart = true;
			result = PLSPlatformApiResult::PAR_SUCCEED;
		} else if (resultCode == 60903) {
			PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_BAND, {{"platformName", "band"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "streamKey api error, band aleardy boardcast"}},
				  "band start live failed");

			PLS_WARN(MODULE_PLATFORM_BAND, "responseStreamLiveKeyHandler band aleardy boardcast");
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("Live.Check.Band.Have.Broardcast"));
			result = PLSPlatformApiResult::PAR_API_FAILED;
		} else if (resultCode == 60106) {
			PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_BAND, {{"platformName", "band"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "streamKey api error, band have no perssion"}},
				  "band start live failed");
			pls_alert_error_message(nullptr, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Band.Have.No.Perssion"));
			result = PLSPlatformApiResult::PAR_API_FAILED;
		} else {
			PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_BAND, {{"platformName", "band"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "streamKey api error, band unkonow error"}},
				  "band start live failed");
			pls_alert_error_message(nullptr, QTStr("Alert.Title"), QTStr("Live.Check.Band.Other.Error"));
			result = PLSPlatformApiResult::PAR_API_FAILED;
		}
	}

	callback(static_cast<int>(result));
}
void PLSPlatformBand::requestLiveStreamKey(const streamLiveKeyCallback &callback)
{
	auto failResult = [this, callback](const pls::http::Reply &reply) {
		auto code = reply.statusCode();
		PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_BAND, {{"platformName", "band"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "streamKey api error"}}, "band start live failed");
		PLS_ERROR(MODULE_PLATFORM_BAND, "requestLiveStreamKey .error: %d-%d", code, reply.error());
		QVariant resultCode;
		PLSJsonDataHandler::getValueFromByteArray(reply.data(), "result_code", resultCode);
		PLSPlatformApiResult result = getApiResult(code, reply.error());
		callback(static_cast<int>(result));
		showApiRefreshError(result);
	};
	auto callbackFun = [this, callback, failResult](bool) {
		auto infos = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
		QMap<QString, QString> queryParams = {
			{"band_key", infos[ChannelData::g_subChannelId].toString()}, {"description", getDescription().c_str()}, {COOKIE_ACCESS_TOKEN, infos[ChannelData::g_channelToken].toString()}};
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Post)
					   .jsonContentType()             //
					   .withLog()                     //
					   .receiver(this)                //
					   .workInMainThread()            //
					   .url(CHANNEL_BAND_LIVE_CREATE) //
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .urlParams(queryParams)
					   .okResult([this, callback](const pls::http::Reply &reply) {
						   auto data = reply.data();
						   auto statusCode = reply.statusCode();
						   responseStreamLiveKeyHandler(callback, data, statusCode);
					   })
					   .failResult(failResult));
	};

	//refresh token
	getBandRefreshTokenInfo(callbackFun, true);
}

void PLSPlatformBand::requesetLiveEnd(const requesetLiveEndCallback &callback)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	const auto &bandKey = channelInfo[ChannelData::g_subChannelId];
	const auto &accessToken = channelInfo[ChannelData::g_channelToken];
	QUrl url(CHANNEL_BAND_LIVE_OFF);
	QUrlQuery query;
	query.addQueryItem("band_key", bandKey.toString());
	query.addQueryItem("live_id", getLiveId());
	query.addQueryItem(COOKIE_ACCESS_TOKEN, accessToken.toString());

	url.setQuery(query);
	auto maskUrlStr =
		maskUrl(CHANNEL_BAND_LIVE_OFF, {{"band_key", pls_masking_person_info(bandKey.toString())}, {"live_id", pls_masking_person_info(getLiveId())}, {COOKIE_ACCESS_TOKEN, accessToken}});

	auto okResult = [callback](const pls::http::Reply &reply) {
		if (HTTP_STATUS_CODE_200 == reply.statusCode()) {
			PLS_INFO(MODULE_PLATFORM_BAND, "requesetLiveEnd .success: %d", reply.statusCode());
		}
		if (nullptr != callback) {
			callback();
		}
	};
	auto callbackFun = [url, maskUrlStr, this, callback, okResult](bool) {
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Post)
					   .jsonContentType()   //
					   .withLog(maskUrlStr) //
					   .receiver(this)      //
					   .workInMainThread()  //
					   .url(url)            //
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .okResult(okResult)
					   .allowAbort(false)
					   .failResult([callback](const pls::http::Reply &reply) {
						   auto code = reply.statusCode();
						   PLS_ERROR(MODULE_PLATFORM_BAND, "requesetLiveEnd .error: %d-%d", code, reply.error());
						   if (nullptr != callback) {
							   callback();
						   }
					   }));
	};

	//refresh token
	getBandRefreshTokenInfo(callbackFun);
}

void PLSPlatformBand::setMaxLiveTime(const int &min)
{
	m_maxLiveTime = min;
}

int PLSPlatformBand::getMaxLiveTime() const
{
	return m_maxLiveTime;
}

void PLSPlatformBand::setLiveId(const QString &liveId)
{
	m_liveId = liveId;
}

QString PLSPlatformBand::getLiveId() const
{
	return m_liveId;
}

QJsonObject PLSPlatformBand::getLiveStartParams()
{

	QJsonObject platform(PLSPlatformBase::getLiveStartParams());

	platform["liveId"] = getLiveId();
	platform["bandKey"] = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value(ChannelData::g_subChannelId).toString();
	platform["simulcastChannel"] = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value(ChannelData::g_nickName).toString();

	return platform;
}
