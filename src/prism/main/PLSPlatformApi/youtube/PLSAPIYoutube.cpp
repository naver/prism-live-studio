#include "PLSAPIYoutube.h"
#include <assert.h>
#include <qfileinfo.h>
#include <QNetworkCookieJar>
#include <vector>
#include "../PLSPlatformBase.hpp"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSPlatformApi.h"
#include "PLSPlatformYoutube.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-gpop-data.hpp"
#include <assert.h>
#include "PLSDateFormate.h"
#include <quuid.h>
#include "../vlive/PLSAPIVLive.h"

using namespace std;

PLSAPIYoutube::PLSAPIYoutube(QObject *parent) : QObject(parent) {}

void PLSAPIYoutube::requestLiveBroadcastStatus(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		builder.setQuerys({{"id", _id}, {"part", "id,status"}});
		addCommenCookieAndUserKey(builder);

		auto reply = builder.get();
		PLSAPIVLive::maskingUrlKeys(builder, reply, {_id});
		return reply;
	};
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestVideoStatus(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/videos").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		builder.setQuerys({{"part", "statistics, liveStreamingDetails"}, {"id", _id}});
		PLSAPIYoutube::addCommenCookieAndUserKey(builder);

		auto reply = builder.get();
		PLSAPIVLive::maskingUrlKeys(builder, reply, {_id});
		return reply;
	};
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestStopLive(const QObject *receiver, function<void()> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		if (nullptr != onNext) {
			onNext();
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		Q_UNUSED(error)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (nullptr != onNext) {
			onNext();
		}
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts/transition").arg(g_plsYoutubeAPIHost));
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
	builder.setQuerys({{"broadcastStatus", "complete"}, {"id", _id}, {"part", "status"}});
	addCommenCookieAndUserKey(builder);

	auto reply = builder.post();
	PLSAPIVLive::maskingUrlKeys(builder, reply, {_id});
	PLS_HTTP_HELPER->connectFinished(reply, receiver, _onSucceed, _onFail);
}

void PLSAPIYoutube::requestLiveBroadcastsInsert(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{

	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		builder.addQuery("part", "snippet,status,contentDetails");
		addCommenCookieAndUserKey(builder);
		builder.addJsonObject("status", QVariantMap({{"privacyStatus", PLS_PLATFORM_YOUTUBE->getTrySaveDataData().privacyStatus.toLower()},
							     {"selfDeclaredMadeForKids", PLS_PLATFORM_YOUTUBE->getTrySaveDataData().isForKids}}));

		auto nowTime = PLSDateFormate::youtubeTimeStampToString(PLSDateFormate::getNowTimeStamp());
		builder.addJsonObject("snippet", QVariantMap({{"title", PLS_PLATFORM_YOUTUBE->getTrySaveDataData().title}, {"scheduledStartTime", nowTime}}));
		QJsonObject cdnObject = QJsonObject();
		cdnObject["enableAutoStart"] = true;
		cdnObject["enableAutoStop"] = true;
		setLatency(cdnObject, PLS_PLATFORM_YOUTUBE->getTrySaveDataData().latency);
		builder.addJsonObject("contentDetails", cdnObject.toVariantMap());
		return builder.post();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestLiveStreamsInsert(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveStreams").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		builder.addQuery("part", "snippet,cdn,contentDetails,status");
		addCommenCookieAndUserKey(builder);
		QJsonObject object = QJsonObject();

		const auto &itemData = PLS_PLATFORM_YOUTUBE->getTrySaveDataData();
		if (itemData.isNormalLive) {
			QJsonObject snippetObject = QJsonObject();
			snippetObject["title"] = QUuid::createUuid().toString();
			//delete description because this length count is less than broadcast insert api. so maybe this will be failed.
			//snippetObject["description"] = PLS_PLATFORM_YOUTUBE->getTrySaveDataData().description;
			object["snippet"] = snippetObject;
			QJsonObject cdnObject = QJsonObject();
			cdnObject["frameRate"] = "variable";
			cdnObject["ingestionType"] = "rtmp";
			cdnObject["resolution"] = "variable";
			object["cdn"] = cdnObject;
		} else {
			object["snippet"] = itemData.streamAPIData["snippet"].toObject();

			QJsonObject cdnObject = QJsonObject();

			cdnObject["frameRate"] = itemData.streamAPIData["cdn"].toObject()["frameRate"].toString();
			cdnObject["ingestionType"] = itemData.streamAPIData["cdn"].toObject()["ingestionType"].toString();
			cdnObject["resolution"] = itemData.streamAPIData["cdn"].toObject()["resolution"].toString();
			object["cdn"] = cdnObject;

			QJsonObject contentDetails = QJsonObject();
			contentDetails["isReusable"] = itemData.streamAPIData["contentDetails"].toObject()["isReusable"].toBool();
			object["contentDetails"] = contentDetails;
		}

		builder.setJsonObject(object);
		return builder.post();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestLiveBroadcastsBind(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts/bind").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		auto &_id = PLS_PLATFORM_YOUTUBE->getTrySaveDataData()._id;
		auto &_bsID = PLS_PLATFORM_YOUTUBE->getTrySaveDataData().boundStreamId;
		builder.setQuerys({{"id", _id}, {"streamId", _bsID}, {"part", "id,contentDetails,status"}});
		addCommenCookieAndUserKey(builder);
		auto reply = builder.post();
		PLSAPIVLive::maskingUrlKeys(builder, reply, {_bsID, _id});
		return reply;
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestLiveBroadcastsUpdate(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	//when kids is true, then this api will call failed.
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		builder.addQuery("part", "contentDetails,status");
		addCommenCookieAndUserKey(builder);

		const auto &data = PLS_PLATFORM_YOUTUBE->getTrySaveDataData();
		builder.addJsonObject("id", data._id);
		QJsonObject details = data.contentDetails;
		setLatency(details, PLS_PLATFORM_YOUTUBE->getTrySaveDataData().latency);
		builder.addJsonObject("contentDetails", details);

		builder.addJsonObject("status", QVariantMap({{"privacyStatus", data.privacyStatus.toLower()}}));
		return builder.put();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestCategoryID(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType, bool isAllList, qint64 context)
{
	auto _getNetworkReply = [=] {
		QString liveID = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		if (isAllList) {
			for (int i = 0; i < PLS_PLATFORM_YOUTUBE->getScheduleDatas().size(); i++) {
				const auto &sche = PLS_PLATFORM_YOUTUBE->getScheduleDatas()[i];
				liveID = liveID.append("%1").arg(i == 0 ? "" : ",").append(sche._id);
			}
		}
		PLSNetworkReplyBuilder builder(QString("%1/videos").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"id", liveID}, {"part", "snippet,status"}});
		addCommenCookieAndUserKey(builder);

		auto reply = builder.get();
		PLSAPIVLive::maskingUrlKeys(builder, reply, liveID.split(","));
		return reply;
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed, nullptr, reinterpret_cast<void *>(context));
}

void PLSAPIYoutube::requestCategoryList(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/videoCategories").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"hl", pls_get_current_language()}, {"regionCode", pls_get_current_country_short_str()}, {"part", "snippet"}});
		addCommenCookieAndUserKey(builder);
		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestScheduleList(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType, qint64 context)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"part", "contentDetails,snippet,status"}, {"broadcastStatus", "upcoming"}, {"maxResults", "50"}});
		addCommenCookieAndUserKey(builder);
		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed, nullptr, reinterpret_cast<void *>(context));
}

void PLSAPIYoutube::requestCurrentSelectData(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"part", "contentDetails,snippet,status"}, {"id", PLS_PLATFORM_YOUTUBE->getSelectData()._id}});
		addCommenCookieAndUserKey(builder);

		auto reply = builder.get();
		PLSAPIVLive::maskingUrlKeys(builder, reply, {PLS_PLATFORM_YOUTUBE->getSelectData()._id});
		return reply;
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed, nullptr);
}

void PLSAPIYoutube::requestLiveStreamKey(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType)
{
	auto _getNetworkReply = [=] {
		QStringList ids;
		for (const auto &data : PLS_PLATFORM_YOUTUBE->getScheduleDatas()) {
			ids.append(data.boundStreamId);
		}
		QString boundStreamID = ids.join(",");
		PLSNetworkReplyBuilder builder(QString("%1/liveStreams").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"part", "snippet,cdn,contentDetails,status"}, {"id", boundStreamID}, {"maxResults", "50"}});
		addCommenCookieAndUserKey(builder);

		auto reply = builder.get();
		PLSAPIVLive::maskingUrlKeys(builder, reply, ids);
		return reply;
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");

	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestUpdateVideoData(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, RefreshType refreshType, PLSYoutubeLiveinfoData data)
{
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/videos").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).addQuery("part", "snippet,status");
		addCommenCookieAndUserKey(builder);
		QJsonObject tempStatus = data.statusData;
		tempStatus["privacyStatus"] = data.privacyStatus.toLower();
		tempStatus["selfDeclaredMadeForKids"] = data.isForKids;
		builder.addJsonObject("status", tempStatus);
		builder.addJsonObject("id", data._id);
		QJsonObject tempSnippet = data.snippetData;
		tempSnippet["description"] = data.description;
		tempSnippet["title"] = data.title;
		tempSnippet["categoryId"] = data.categoryID;
		builder.addJsonObject("snippet", tempSnippet);
		return builder.put();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::refreshYoutubeTokenBeforeRequest(RefreshType refreshType, function<QNetworkReply *()> originNetworkReplay, const QObject *originReceiver, dataFunction originOnSucceed,
						     dataErrorFunction originOnFailed, dataErrorFunction originOnFinished, void *const originContext)
{
	const QString &youtubeUUID = PLS_PLATFORM_YOUTUBE->getChannelUUID();
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto access_token = root["access_token"].toString();
			PLSCHANNELS_API->setValueOfChannel(youtubeUUID, ChannelData::g_channelToken, access_token);
			PLSCHANNELS_API->setValueOfChannel(youtubeUUID, ChannelData::g_createTime, QDateTime::currentDateTime());
			PLSCHANNELS_API->channelModified(youtubeUUID);
			if (originNetworkReplay != nullptr) {
				PLS_HTTP_HELPER->connectFinished(originNetworkReplay(), originReceiver, originOnSucceed, originOnFailed, originOnFinished, originContext);
			} else if (nullptr != originOnSucceed) {
				originOnSucceed(networkReplay, code, data, context);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			if (nullptr != originOnFailed) {
				originOnFailed(networkReplay, 1999, data, QNetworkReply::InternalServerError, context);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		originOnFailed(networkReplay, code, data, error, context);
	};

	assert(!youtubeUUID.isEmpty());

	if (refreshType == RefreshType::NotRefresh || (refreshType == RefreshType::CheckRefresh && isTokenValid(youtubeUUID))) {
		if (originNetworkReplay != nullptr) {
			PLS_HTTP_HELPER->connectFinished(originNetworkReplay(), originReceiver, originOnSucceed, originOnFailed, originOnFinished, originContext);
		} else if (nullptr != originOnSucceed) {
			originOnSucceed(nullptr, 200, nullptr, originContext);
		}
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");

	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(youtubeUUID);
	auto token = channelData.value(ChannelData::g_refreshToken).toString();

	QString bodyStr(QString("refresh_token=%0&client_id=%1&client_secret=%2&grant_type=refresh_token").arg(token).arg(YOUTUBE_CLIENT_ID).arg(YOUTUBE_CLIENT_KEY));

	QByteArray bodyByte = bodyStr.toUtf8();
	PLSNetworkReplyBuilder builder(QString("%1/token").arg(g_plsYoutubeAPIHostV4));
	builder.setContentType(HTTP_CONTENT_TYPE_URL_ENCODED_VALUE);
	builder.setBody(bodyByte);
	PLS_HTTP_HELPER->connectFinished(builder.post(), originReceiver, _onSucceed, _onFail);
}

void PLSAPIYoutube::getLatency(const QJsonObject &object, PLSYoutubeLatency &latency)
{
	auto latencyPreference = object["latencyPreference"].toString();
	if (latencyPreference == s_latencyNormal) {
		latency = PLSYoutubeLatency::Normal;
	} else if (latencyPreference == s_latencyLow) {
		latency = PLSYoutubeLatency::Low;
	} else if (latencyPreference == s_latencyUltraLow) {
		latency = PLSYoutubeLatency::UltraLow;
	} else {
		latency = PLSYoutubeLatency::Low;
	}
}

void PLSAPIYoutube::setLatency(QJsonObject &object, PLSYoutubeLatency latency)
{
	switch (latency) {
	case PLSYoutubeLatency::Normal:
		object["latencyPreference"] = s_latencyNormal;
		break;
	case PLSYoutubeLatency::Low:
		object["latencyPreference"] = s_latencyLow;
		break;
	case PLSYoutubeLatency::UltraLow:
		object["latencyPreference"] = s_latencyUltraLow;
		break;
	default:
		break;
	}

	if (object.contains("enableLowLatency")) {
		object.remove("enableLowLatency");
	}
}

void PLSAPIYoutube::addCommenCookieAndUserKey(PLSNetworkReplyBuilder &builder)
{
	builder.addQuery("key", YOUTUBE_CLIENT_KEY);
	builder.setRawHeaders({{"Authorization", QString("Bearer %1").arg(PLS_PLATFORM_YOUTUBE->getChannelToken())}});
}
