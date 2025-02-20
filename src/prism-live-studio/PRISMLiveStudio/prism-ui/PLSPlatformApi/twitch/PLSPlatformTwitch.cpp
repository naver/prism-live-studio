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
	if (PLS_PLATFORM_API->isPrismLive()) {
		liveEndedCallback();
	} else {
		requestVideos();
	}
}

void PLSPlatformTwitch::serverHandler() {}

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
	if (retDataValue.prismCode == PLSErrorHandler::ErrCode::COMMON_TOKEN_EXPIRED_ERROR && retData.clickedBtn == PLSAlertView::Button::Ok) {
		emit closeDialogByExpired();
		PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
	}
}

void PLSPlatformTwitch::requestStreamKey(bool showAlert, const streamKeyCallback &callback)
{
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
							   callback(true);
						   } else {
							   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestStreamKey.error: %d", code);
							   PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_TWITCH, {{"channel start error", "twitch"}}, "request streamKey api error, code = %d error = %s.",
								     code, data.constData());
							   PLSErrorHandler::ExtraData extraData;
							   extraData.errPhase = PLSErrPhaseChannel;
							   extraData.urlEn = maskUrlStr;
							   auto retData = PLSErrorHandler::showAlert({code, error, data}, TWITCH, "FailedToStartLive", extraData);
							   callback(false);
						   }
					   });
				   })
				   .failResult([this, showAlert, maskUrlStr, callback](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_TWITCH, {{"channel start error", "twitch"}}, "request streamKey api error, code = %d error = %d.", code, error);

					   PLSErrorHandler::ExtraData extraData;
					   extraData.errPhase = PLSErrPhaseChannel;
					   extraData.urlEn = maskUrlStr;
					   auto retData = PLSErrorHandler::getAlertString({code, error, reply.data()}, TWITCH, "", extraData);
					   if (showAlert) {
						   pls_async_call_mt(getAlertParent(), [this, showAlert, retData, callback]() {
							   showApiRefreshError(retData);
							   callback(false);
						   });
					   }
				   }));
}
void PLSPlatformTwitch::getChannelInfo()
{
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
				   .okResult([this](const pls::http::Reply &reply) {
					   auto jsonDoc = QJsonDocument::fromJson(reply.data()).object().value("data").toArray();
					   auto jsonMap = jsonDoc.first().toObject().toVariantMap();
					   auto category = jsonMap.value("game_name");
					   auto lastInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
					   lastInfo.insert(ChannelData::g_catogry, category);
					   lastInfo.insert(ChannelData::g_displayLine2, category);
					   PLSCHANNELS_API->setChannelInfos(lastInfo, true);
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto statusCode = reply.statusCode();
					   QString errorStr = "channel error status code " + QString::number(statusCode);
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, errorStr.toStdString().c_str());
				   }));
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
				   .workInMainThread()  //
				   .url(url)            //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this](const pls::http::Reply &reply) {
					   auto data = reply.data();
					   auto code = reply.statusCode();

					   if (auto doc = QJsonDocument::fromJson(data); doc.isObject()) {
						   auto root = doc.object()["data"].toArray();
						   if (!root.isEmpty()) {
							   m_strEndUrl = root[0].toObject()["url"].toString();
						   }
					   } else {
						   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestVideos.error: %d", code);
					   }

					   QMetaObject::invokeMethod(
						   this, [this]() { liveEndedCallback(); }, Qt::QueuedConnection);
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   auto code = reply.statusCode();
					   auto error = reply.error();
					   PLS_ERROR(MODULE_PLATFORM_TWITCH, "requestVideos.error: %d-%d", code, error);

					   QMetaObject::invokeMethod(
						   this, [this]() { liveEndedCallback(); }, Qt::QueuedConnection);
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

bool PLSPlatformTwitch::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}
