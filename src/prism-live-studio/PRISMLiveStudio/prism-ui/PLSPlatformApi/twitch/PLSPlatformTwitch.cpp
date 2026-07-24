#include "PLSPlatformTwitch.h"

#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <qurlquery.h>
#include "frontend-api.h"
#include "PLSAlertView.h"
#include "window-basic-main.hpp"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"
#include "../PLSLiveInfoDialogs.h"
#include "PLSChannelDataAPI.h"
#include "QTimer"
#include "PLSChannelDataHandlerFunctions.h"

#define TWTICH_CHAT QStringLiteral("TwitchChat")
#define TWTICH_SERVER QStringLiteral("TwitchServer")
#define TWITCH_TITLE_INVAILD QStringLiteral("Status contains banned words")
const char *globalServer = "rtmp://ingest.global-contribute.live-video.net/app";
using namespace std;
using namespace common;

PLSServiceType PLSPlatformTwitch::getServiceType() const
{
	return PLSServiceType::ST_TWITCH;
}

void PLSPlatformTwitch::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}
	PLS_INFO(MODULE_PLATFORM_TWITCH, "%s  show liveinfo value(%s)", PrepareInfoPrefix, BOOL2STR(value));
	value = pls_exec_live_Info_twitch(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_TWITCH, "%s  close liveinfo value(%s)", PrepareInfoPrefix, BOOL2STR(value));
	prepareLiveCallback(value);
}

void PLSPlatformTwitch::onAlLiveStarted(bool value) {}

void PLSPlatformTwitch::onLiveEnded()
{
	if (PLS_PLATFORM_API->isPrismLive(this)) {
		liveEndedCallback();
	} else {
		requestVideos();
	}
}

void PLSPlatformTwitch::serverHandler()
{
	auto obj = PLSInitApiFlow::instance()->getTwitchServiceList();

	auto ingests = obj["ingests"].toArray();
	auto _idConfig = config_get_uint(App()->GetUserConfig(), KeyConfigLiveInfo, KeyTwitchServer);

	auto ingestSize = ingests.count();
	if (ingestSize == 0) {
		PLS_INFO(MODULE_PLATFORM_TWITCH, "get twitch service list failed, will use globalServer = %s", globalServer);
		setStreamServer(globalServer);
	} else {
		auto server = ingests[0].toObject().value("url_template").toString();
		auto lastIndex = server.lastIndexOf('/');
		auto serverUrl = server.left(lastIndex).toStdString();
		setStreamServer(serverUrl);
		PLS_INFO(MODULE_PLATFORM_TWITCH, "get twitch service list success, selected server = %s", serverUrl.c_str());
	}
}

QString maskUrl(const QString &url, const QVariantMap &queryInfo)
{
	QUrl maskUrl(url);
	QUrlQuery maskQuery;
	for (auto info = queryInfo.constBegin(); info != queryInfo.constEnd(); ++info) {
		maskQuery.addQueryItem(info.key(), info.value().toString());
	}
	maskUrl.setQuery(maskQuery);
	return maskUrl.toString();
}

QVariantMap PLSPlatformTwitch::setHttpHead() const
{
	return {{"Client-ID", TWITCH_CLIENT_ID}, {"Authorization", "Bearer " + getChannelToken()}, {"Accept", HTTP_ACCEPT_TWITCH}};
}

void PLSPlatformTwitch::showApiRefreshError(const PLSErrorHandler::RetData &retData)
{
	auto retDataValue = retData;
	PLSErrorHandler::directShowAlert(retDataValue, nullptr);
	if (retDataValue.prismCode == PLSErrorHandler::ErrCode::COMMON_TOKEN_EXPIRED_ERROR && retDataValue.clickedBtn == PLSAlertView::Button::Ok) {
		emit closeDialogByExpired();
		PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
	}
}

void PLSPlatformTwitch::requestStreamKey(bool showAlert, const streamKeyCallback &callback)
{
	auto getStreamKey = [showAlert, callback, this](bool) {
		auto broadcastId = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value("broadcast_id").toString().toStdString();
		setChannelId(broadcastId);
		auto maskUrlStr = QString(CHANNEL_TWITCH_STREAMKEY).arg(pls_masking_person_info(QString::fromStdString(getChannelId())));
		auto url = QString(CHANNEL_TWITCH_STREAMKEY).arg(QString::fromStdString(getChannelId()));
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .jsonContentType() //
					   .rawHeaders(setHttpHead())
					   .withLog(maskUrlStr) //
					   .receiver(this)      //
					   .url(url)            //
					   .id(TWITCH)
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .okResult([this, showAlert, callback, maskUrlStr](const pls::http::Reply &reply) {
						   auto data = reply.data();
						   auto code = reply.statusCode();
						   auto error = reply.error();
						   auto doc = QJsonDocument::fromJson(data);
						   pls_async_call_mt(getAlertParent(), [this, doc, showAlert, data, code, callback, maskUrlStr, error]() {
							   if (doc.isObject()) {
								   auto root = doc.object().value("data").toArray().first().toObject();
								   setStreamKey(root["stream_key"].toString().toStdString());

								   m_strOriginalTitle = getTitle();
								   serverHandler();
								   getChannelInfo(callback);
							   } else {
								   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestStreamKey.error: %d", code);
								   PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_TWITCH, {{"channel start error", "twitch"}},
									     "request streamKey api error, code = %d error = %s.", code, data.constData());
								   PLSErrorHandler::ExtraData extraData(maskUrlStr);
								   extraData.errPhase = PLSErrPhaseChannel;
								   auto retData = PLSErrorHandler::showAlert({code, error, data}, TWITCH, "FailedToStartLive", extraData);
								   callback(false);
							   }
						   });
					   })
					   .failResult([this, showAlert, maskUrlStr, callback](const pls::http::Reply &reply) {
						   auto code = reply.statusCode();
						   auto error = reply.error();
						   PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_TWITCH, {{"channel start error", "twitch"}}, "request streamKey api error, code = %d error = %d.", code,
							     error);

						   PLSErrorHandler::ExtraData extraData(maskUrlStr);
						   extraData.errPhase = PLSErrPhaseChannel;
						   auto retData = PLSErrorHandler::getAlertString({code, error, reply.data()}, TWITCH, "", extraData);
						   if (showAlert) {
							   pls_async_call_mt(getAlertParent(), [this, showAlert, retData, callback]() {
								   showApiRefreshError(retData);
								   callback(false);
							   });
						   }
					   }));
	};
	pollingCheckToken(true, getStreamKey);
}
void PLSPlatformTwitch::getChannelInfo(const std::function<void(bool)> &channelInfoCallback)
{
	auto getChannelData = [this, channelInfoCallback](bool) {
		PLS_INFO("twitchPlatform", "start get channel infos");
		auto broadcastId = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value("broadcast_id").toString();

		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .jsonContentType() //
					   .rawHeaders(setHttpHead())
					   .withLog(QString(CHANNEL_TWITCH_INFO_URL).arg(pls_masking_person_info(broadcastId))) //
					   .url(QString(CHANNEL_TWITCH_INFO_URL).arg(broadcastId))                              //
					   .receiver(PLSCHANNELS_API)
					   .id(TWITCH)
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .okResult([this, channelInfoCallback](const pls::http::Reply &reply) {
						   auto jsonDoc = QJsonDocument::fromJson(reply.data()).object().value("data").toArray();
						   auto jsonMap = jsonDoc.first().toObject().toVariantMap();
						   auto category = jsonMap.value("game_name");
						   auto lastInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
						   lastInfo.insert(ChannelData::g_catogry, category);
						   lastInfo.insert(ChannelData::g_displayLine2, category);
						   PLSCHANNELS_API->setChannelInfos(lastInfo, true);
						   pls_async_call_mt(getAlertParent(), [channelInfoCallback]() { channelInfoCallback(true); });
					   })
					   .failResult([this, channelInfoCallback](const pls::http::Reply &reply) {
						   auto statusCode = reply.statusCode();
						   QString errorStr = "channel error status code " + QString::number(statusCode);
						   PLS_ERROR(MODULE_PLATFORM_TWITCH, errorStr.toUtf8().constData());
						   pls_async_call_mt(getAlertParent(), [channelInfoCallback]() { channelInfoCallback(false); });
					   }));
	};

	pollingCheckToken(false, getChannelData);
}

void PLSPlatformTwitch::pollingCheckToken(bool isFoceUpdate, const refreshTokenCallback &callback)
{
	auto expireTime = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value(ChannelData::g_expires_in).toLongLong();
	if (PLSAPICommon::isTokenValid(expireTime) && !isFoceUpdate) {
		if (callback) {
			callback(false);
		}
		return;
	}
	auto refresh_token = getChannelRefreshToken();
	auto uuid = getChannelUUID();
	QVariantMap parameters;
	parameters[ChannelData::g_refreshToken] = refresh_token;
	parameters[HTTP_CLIENT_ID] = TWITCH_CLIENT_ID;
	parameters[HTTP_CLIENT_SECRET] = TWITCH_CLIENT_SECRET;
	parameters[HTTP_GRANT_TYPE] = ChannelData::g_refreshToken;
	QVariantMap headerMap;
	headerMap[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_URL_ENCODED_VALUE;

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post) //
				   .url(g_plsTwitchAuthTokenUrl)
				   .id(TWITCH)
				   .form(parameters)
				   .rawHeaders(headerMap)
				   .withLog()      //
				   .receiver(this) //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .objectOkResult([this, uuid, callback](const pls::http::Reply &reply, const QJsonObject &obj) {
					   auto data = reply.data();
					   auto jsonDoc = QJsonDocument::fromJson(data);
					   auto jsonMap = obj.toVariantMap();
					   PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_channelToken, obj.value(ChannelData::g_channelToken));
					   PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_refreshToken, obj.value(ChannelData::g_refreshToken));
					   PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_createTime, QDateTime::currentDateTime());
					   PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_expires_in, obj.value(ChannelData::g_expires_in).toInt() + QDateTime::currentSecsSinceEpoch());

					   PLSCHANNELS_API->channelModified(uuid);
					   PLS_PLATFORM_API->onUpdateChannel(uuid);
					   if (callback) {
						   callback(true);
					   }
				   })
				   .failResult([this, callback](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
					   extraData.errPhase = PLSErrPhaseChannel;
					   auto retData = PLSErrorHandler::getAlertString({code, error, reply.data()}, TWITCH, "", extraData);
					   if (callback) {
						   callback(false);
					   }
				   }));
}

void PLSPlatformTwitch::onResumeStreaming(const QMap<QString, QVariant> &params)
{
	QString shareUrl = params.value("shareUrl").toString();
	if (!shareUrl.isEmpty()) {
		PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, shareUrl);
		PLSCHANNELS_API->channelModified(getChannelUUID());
		PLS_INFO(MODULE_PLATFORM_TWITCH, "Resume streaming: restored shareUrl for channel %s", pls_masking_person_info(getChannelUUID()).toUtf8().constData());
	}
}

QMap<QString, QVariant> PLSPlatformTwitch::getResumeStreamingParams() const
{
	QMap<QString, QVariant> saveData;
	// Directly read from channel info to avoid calling non-const getShareUrl() in const function
	QString shareUrl = PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value(ChannelData::g_shareUrl).toString();
	if (!shareUrl.isEmpty()) {
		saveData.insert("shareUrl", shareUrl);
		PLS_INFO(MODULE_PLATFORM_TWITCH, "Save resume streaming params: shareUrl for channel %s", pls_masking_person_info(getChannelUUID()).toUtf8().constData());
	}
	return saveData;
}

void PLSPlatformTwitch::requestVideos()
{
	auto maskUrlStr = maskUrl(CHANNEL_TWITCH_VIDEOS, {{"user_id", pls_masking_person_info(QString::fromStdString(getChannelId()))}});
	QUrl url(CHANNEL_TWITCH_VIDEOS);
	QUrlQuery query;
	query.addQueryItem("user_id", getChannelId().c_str());
	url.setQuery(query);
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(setHttpHead())
				   .withLog(maskUrlStr) //
				   .receiver(this)      //
				   .url(url)            //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   auto code = reply.statusCode();
					   auto root = QJsonDocument::fromJson(data).object().value("data").toArray();
					   if (root.isEmpty()) {
						   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestVideos.error: %d", code);
					   }
					   pls_async_call_mt(this, [root, this]() {
						   if (!root.isEmpty()) {
							   m_strEndUrl = root[0].toObject()["url"].toString();
						   }
						   liveEndedCallback();
					   });
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestVideos.error: %d-%d", code, error);
					   pls_async_call_mt(this, [this]() { liveEndedCallback(); });
				   }));
}

QJsonObject PLSPlatformTwitch::getLiveStartParams()
{
	QJsonObject platform(PLSPlatformBase::getLiveStartParams());

	platform["simulcastChannel"] = QString::fromStdString(getDisplayName());

	return platform;
}

QJsonObject PLSPlatformTwitch::getWebChatParams()
{
	QJsonObject platform(PLSPlatformBase::getWebChatParams());

	platform["clientId"] = TWITCH_CLIENT_ID;

	return platform;
}

QString PLSPlatformTwitch::getServiceLiveLink()
{
	return m_strEndUrl;
}

QString PLSPlatformTwitch::getShareUrl()
{
	return PLSCHANNELS_API->getChannelInfo(getChannelUUID()).value(ChannelData::g_shareUrl).toString();
}

QString PLSPlatformTwitch::getShareUrlEnc()
{
	return QString(pls_masking_person_info(getShareUrl()));
}

QString PLSPlatformTwitch::getServiceLiveLinkEnc()
{
	return pls_masking_person_info(m_strEndUrl);
}