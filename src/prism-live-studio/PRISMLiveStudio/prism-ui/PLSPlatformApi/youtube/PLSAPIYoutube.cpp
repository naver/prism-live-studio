#include "PLSAPIYoutube.h"
#include <assert.h>
#include <qfileinfo.h>
#include <QNetworkCookieJar>
#include <vector>
#include "PLSPlatformBase.hpp"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSPlatformApi.h"
#include "PLSPlatformYoutube.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "pls-gpop-data.hpp"
#include <assert.h>
#include "PLSDateFormate.h"
#include <quuid.h>
#include "../common/PLSAPICommon.h"
#include <libutils-api.h>
#include <QFile>
#include <QStandardPaths>

using namespace std;
using namespace common;

const static int k_refreshDiff = 60 * 3;

// the log name of api failed
const QString YT_Name_Pre = QString(YOUTUBE) + " ";
const QString YT_API_RequestTestLive = YT_Name_Pre + "Post /liveBroadcasts/transition?broadcastStatus=testing";
const QString YT_API_GetLiveBroadcastLoop = YT_Name_Pre + "Get /liveBroadcasts in loop"; //in loop, not print error
const QString YT_API_RequestScheduleList = YT_Name_Pre + "Get /liveBroadcasts?broadcastStatus=upcoming";
const QString YT_API_RequestCurrentSelectData = YT_Name_Pre + "Get /liveBroadcasts?id=x";
const QString YT_API_PostLiveBroadcasts = YT_Name_Pre + "Post /liveBroadcasts";
const QString YT_API_PutLiveBroadcasts = YT_Name_Pre + "Put /liveBroadcasts";
const QString YT_API_GetLiveStream = YT_Name_Pre + "Get /liveStreams";
const QString YT_API_PostLiveStreams = YT_Name_Pre + "Post /liveStreams";
const QString YT_API_DeleteLiveStreams = YT_Name_Pre + "Delete /liveStreams";
const QString YT_API_PostLiveBroadcastsBind = YT_Name_Pre + "Post /liveBroadcasts/bind";
const QString YT_API_GetVideo = YT_Name_Pre + "Get /videos";
const QString YT_API_PutVideo = YT_Name_Pre + "Put /videos";
const QString YT_API_GetCategoryList = YT_Name_Pre + "Get /videoCategories";
const QString YT_API_RefreshYoutubeToken = YT_Name_Pre + "Post /token?grant_type=refresh_token";
const QString YT_API_PostToken = YT_Name_Pre + "Post /token?grant_type=authorization_code";
const QString YT_API_UploadImage = YT_Name_Pre + "Post /upload/youtube/v3/thumbnails/set";
const QString YT_API_GetChannels = YT_Name_Pre + "Get /channels";
const QString YT_API_liveStreamingDetails = YT_Name_Pre + "Get /videos in loop";
const QString YT_API_RequestStopLive = YT_Name_Pre + "Post /liveBroadcasts/transition?broadcastStatus=complete";

PLSAPIYoutube::PLSAPIYoutube(QObject *parent) : QObject(parent) {}

void PLSAPIYoutube::requestTestLive(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{

	auto _getNetworkReply = [receiver, onSucceed, onFailed] {
		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		QMap<QString, QString> map = {{"broadcastStatus", "testing"}, {"id", _id}, {"part", "status,snippet,contentDetails"}};
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_RequestTestLive.toUtf8());
		_request.method(pls::http::Method::Post).url(QString("%1/liveBroadcasts/transition").arg(g_plsYoutubeAPIHost)).urlParams(map);
		PLSAPICommon::maskingUrlKeys(_request, {_id});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestTestLive start");

	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					 const QByteArray &logName, bool isSetContentType)
{
	PLSAPIYoutube::addCommonCookieAndUserKey(_request);
	if (isSetContentType) {
		_request.contentType(HTTP_CONTENT_TYPE_VALUE);
	}
	_request.timeout(PRISM_NET_REQUEST_TIMEOUT)
		.receiver(receiver)
		.okResult([onSucceed, receiver](const pls::http::Reply &reply) {
			if (onSucceed && !pls_get_app_exiting()) {
				auto _data = reply.data();
				pls_async_call_mt(receiver, [onSucceed, _data] { onSucceed(_data); });
			}
		})
		.failResult([onFailed, receiver, logName](const pls::http::Reply &reply) {
			showFailedLog(logName, reply);
			auto _code = reply.statusCode();
			auto _data = reply.data();
			auto _err = reply.error();
			if (onFailed && !pls_get_app_exiting()) {
				pls_async_call_mt(receiver, [onFailed, _code, _data, _err] { onFailed(_code, _data, _err); });
			}
		});
}

void PLSAPIYoutube::requestLiveBroadcastStatus(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_GetLiveBroadcastLoop.toUtf8());
		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		QMap<QString, QString> map = {{"id", _id}, {"part", "id,status,contentDetails,snippet"}};
		_request.method(pls::http::Method::Get) //
			.url(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		PLSAPICommon::maskingAfterUrlKeys(_request, {_id});
		pls::http::request(_request);
	};
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestVideoStatus(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_liveStreamingDetails.toUtf8());

		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		QMap<QString, QString> map = {{"part", "statistics, liveStreamingDetails"}, {"id", _id}};
		_request.method(pls::http::Method::Get) //
			.url(QString("%1/videos").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		PLSAPICommon::maskingAfterUrlKeys(_request, {_id});
		pls::http::request(_request);
	};
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestStopLive(const QObject *receiver, const std::function<void()> &onNext)
{
	auto _onSucceed = [onNext](QByteArray) {
		if (onNext)
			onNext();
	};

	auto _onFail = [onNext](int, QByteArray, QNetworkReply::NetworkError) {
		if (onNext)
			onNext();
	};

	PLS_INFO(MODULE_PlatformService, "requestStopLive start");
	auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	const auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;

	PLSAPIYoutube::configDefaultRequest(_request, receiver, _onSucceed, _onFail, YT_API_RequestStopLive.toUtf8(), true);
	QMap<QString, QString> map = {{"broadcastStatus", "complete"}, {"id", _id}, {"part", "status"}};
	_request.method(pls::http::Method::Post).url(QString("%1/liveBroadcasts/transition").arg(g_plsYoutubeAPIHost)).urlParams(map).allowAbort(false);
	PLSAPICommon::maskingUrlKeys(_request, {_id});
	pls::http::request(_request);
}

void PLSAPIYoutube::requestLiveBroadcastsInsert(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
						PLSAPICommon::RefreshType refreshType)
{
	const auto &tryData = PLS_PLATFORM_YOUTUBE->getTrySaveDataData();
	auto object = QJsonObject();
	object["status"] = QJsonObject({{"privacyStatus", tryData.privacyStatus.toLower()}, {"selfDeclaredMadeForKids", PLS_PLATFORM_YOUTUBE->getTrySaveDataData().isForKids}});

	auto nowTime = PLSDateFormate::youtubeTimeStampToString(PLSDateFormate::getNowTimeStamp() + 20);
	object["snippet"] = QJsonObject({{"title", tryData.title}, {"scheduledStartTime", nowTime}, {"description", tryData.description}});

	auto cdnObject = QJsonObject();
	cdnObject["enableAutoStart"] = tryData.startData.enableAutoStart;
	cdnObject["enableAutoStop"] = tryData.startData.enableAutoStop;
	setLatency(cdnObject, tryData.latency);

	auto monitorObj = QJsonObject();
	monitorObj["enableMonitorStream"] = tryData.startData.enableMonitorStream;
	cdnObject["monitorStream"] = monitorObj;
	object["contentDetails"] = cdnObject;

	auto _getNetworkReply = [receiver, onSucceed, onFailed, object] {
		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_PostLiveBroadcasts.toUtf8());

		_request.method(pls::http::Method::Post) //
			.url(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost))
			.urlParam("part", "snippet,status,contentDetails")
			.body(object);
		PLSAPICommon::maskingUrlKeys(_request, {_id});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestLiveBroadcastsInsert start with start time: %s", nowTime.toUtf8().constData());
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestLiveStreamsInsert(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{

	auto object = QJsonObject();
	bool isHls = PLSYoutubeLiveinfoData::IngestionType::Hls == PLS_PLATFORM_YOUTUBE->getSettingIngestionType();
	const char *ingestionType = isHls ? "hls" : "rtmp";

	const auto &itemData = PLS_PLATFORM_YOUTUBE->getTrySaveDataData();
	if (itemData.isNormalLive) {
		auto snippetObject = QJsonObject();
		snippetObject["title"] = QUuid::createUuid().toString();
		object["snippet"] = snippetObject;
		auto cdnObject = QJsonObject();
		cdnObject["frameRate"] = "variable";
		cdnObject["ingestionType"] = ingestionType;

		cdnObject["resolution"] = "variable";
		object["cdn"] = cdnObject;

		auto contentDetails = QJsonObject();
		contentDetails["isReusable"] = false;
		object["contentDetails"] = contentDetails;

	} else {
		object["snippet"] = itemData.streamAPIData["snippet"].toObject();

		auto cdnObject = QJsonObject();

		cdnObject["frameRate"] = itemData.streamAPIData["cdn"].toObject()["frameRate"].toString();
		cdnObject["ingestionType"] = ingestionType;

		cdnObject["resolution"] = itemData.streamAPIData["cdn"].toObject()["resolution"].toString();
		object["cdn"] = cdnObject;

		auto contentDetails = QJsonObject();
		contentDetails["isReusable"] = itemData.streamAPIData["contentDetails"].toObject()["isReusable"].toBool();
		object["contentDetails"] = contentDetails;
	}

	auto _getNetworkReply = [receiver, onSucceed, onFailed, object] {
		auto &_id = PLS_PLATFORM_YOUTUBE->getSelectData()._id;
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_PostLiveStreams.toUtf8());
		_request.method(pls::http::Method::Post) //
			.url(QString("%1/liveStreams").arg(g_plsYoutubeAPIHost))
			.urlParam("part", "snippet,cdn,contentDetails,status")
			.body(object);
		PLSAPICommon::maskingUrlKeys(_request, {_id});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestLiveStreamsInsert start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestLiveBroadcastsBindOrUnbind(const QObject *receiver, const PLSYoutubeLiveinfoData &data, bool isBind, const PLSAPICommon::dataCallback &onSucceed,
						      const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed, isBind, data] {
		auto &_id = data._id;
		auto &_bsID = data.boundStreamId;
		QMap<QString, QString> map = {{"id", _id}, {"part", "id,contentDetails,status"}};
		if (isBind) {
			map.insert("streamId", _bsID);
		}
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_PostLiveBroadcastsBind.toUtf8());

		_request.method(pls::http::Method::Post).url(QString("%1/liveBroadcasts/bind").arg(g_plsYoutubeAPIHost)).urlParams(map);

		QStringList maskKeys = {_id};
		if (isBind) {
			maskKeys.append(_bsID);
		}
		PLSAPICommon::maskingUrlKeys(_request, maskKeys);
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestLiveBroadcastsBindOrUnbind start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestDeleteStream(const QObject *receiver, const QString &deleteID, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed, deleteID] {
		QStringList ids;
		for (const auto &data : PLS_PLATFORM_YOUTUBE->getScheduleDatas()) {
			ids.append(data.boundStreamId);
		}
		QMap<QString, QString> map = {{"id", deleteID}};

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_DeleteLiveStreams.toUtf8());

		_request.method(pls::http::Method::Delete) //
			.url(QString("%1/liveStreams").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		PLSAPICommon::maskingUrlKeys(_request, {deleteID});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestDeleteStream start");

	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestLiveBroadcastsUpdate(const QObject *receiver, const PLSYoutubeStart &startData, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
						PLSAPICommon::RefreshType refreshType)
{
	const auto &data = PLS_PLATFORM_YOUTUBE->getTrySaveDataData();
	QJsonObject details = data.livebroadcastAPIData["contentDetails"].toObject();
	setLatency(details, data.latency);

	details["enableAutoStart"] = startData.enableAutoStart;
	details["enableAutoStop"] = startData.enableAutoStop;

	auto monitorObj = details["monitorStream"].toObject();
	monitorObj["enableMonitorStream"] = startData.enableMonitorStream;
	if (monitorObj["enableMonitorStream"].toBool() == true && !monitorObj.contains("broadcastStreamDelayMs")) {
		monitorObj["broadcastStreamDelayMs"] = 0;
	}
	details["monitorStream"] = monitorObj;

	auto object = QJsonObject();
	object["id"] = data._id;
	object["contentDetails"] = details;
	object["status"] = QJsonObject({{"privacyStatus", data.privacyStatus.toLower()}});

	auto snippet = data.livebroadcastAPIData["snippet"].toObject();

	snippet["title"] = data.title;
	snippet["description"] = data.description;
	object["snippet"] = snippet;

	//when kids is true, then this api will call failed.  Put api can't change kids value.
	auto _getNetworkReply = [receiver, onSucceed, onFailed, object] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_PutLiveBroadcasts.toUtf8());

		_request.method(pls::http::Method::Put) //
			.url(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost))
			.urlParam("part", "contentDetails,status,snippet")
			.body(object);
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestLiveBroadcastsUpdate start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestCategoryID(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType,
				      const QString &searchID)
{
	QStringList ids;

	if (!searchID.isEmpty()) {
		ids << searchID;
	} else {
		for (int i = 0; i < PLS_PLATFORM_YOUTUBE->getScheduleDatas().size(); i++) {
			const auto &sche = PLS_PLATFORM_YOUTUBE->getScheduleDatas()[i];
			if (!sche._id.isEmpty()) {
				ids << sche._id;
			}
		}
	}

	QString liveID = ids.join(",");

	auto _getNetworkReply = [liveID, receiver, onSucceed, onFailed, ids] {
		QMap<QString, QString> map = {{"id", liveID}, {"part", "snippet,status"}};
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_GetVideo.toUtf8());

		_request.method(pls::http::Method::Get) //
			.url(QString("%1/videos").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		PLSAPICommon::maskingUrlKeys(_request, ids);
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestCategoryID start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestCategoryList(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed] {
		QMap<QString, QString> map = {{"hl", pls_get_current_language()}, {"regionCode", "US"}, {"part", "snippet"}};

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_GetCategoryList.toUtf8());

		_request.method(pls::http::Method::Get) //
			.url(QString("%1/videoCategories").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestCategoryList start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestScheduleList(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed] {
		//The API say the max is 50, but if set 50, they only return 7 items, So change to 51 > 50, they can return 50 items.
		QMap<QString, QString> map = {{"part", "contentDetails,snippet,status"}, {"broadcastStatus", "upcoming"}, {"maxResults", "51"}};

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_RequestScheduleList.toUtf8());
		_request.method(pls::http::Method::Get) //
			.url(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestScheduleList start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestCurrentSelectData(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed] {
		QMap<QString, QString> map = {{"part", "contentDetails,snippet,status"}, {"id", PLS_PLATFORM_YOUTUBE->getSelectData()._id}};

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_RequestCurrentSelectData.toUtf8());

		_request.method(pls::http::Method::Get) //
			.url(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		PLSAPICommon::maskingUrlKeys(_request, {PLS_PLATFORM_YOUTUBE->getSelectData()._id});
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestCurrentSelectData start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}
void PLSAPIYoutube::requestLiveStream(const QStringList &ids, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
				      PLSAPICommon::RefreshType refreshType, const QString &queryKeys, const QByteArray &logName)
{
	auto _getNetworkReply = [receiver, onSucceed, onFailed, ids, queryKeys, logName] {
		QMap<QString, QString> map = {{"part", queryKeys}, {"id", ids.join(",")}, {"maxResults", "50"}};

		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, logName);

		_request.method(pls::http::Method::Get) //
			.url(QString("%1/liveStreams").arg(g_plsYoutubeAPIHost))
			.urlParams(map);
		pls::http::request(_request);
	};

	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver, onSucceed, onFailed);
}

void PLSAPIYoutube::requestUpdateVideoData(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType,
					   const PLSYoutubeLiveinfoData &data)
{

	auto tempStatus = data.statusData;
	tempStatus["privacyStatus"] = data.privacyStatus.toLower();
	tempStatus["selfDeclaredMadeForKids"] = data.isForKids;
	if (data.isNormalLive) {
		tempStatus["embeddable"] = true;
	}

	auto tempSnippet = data.snippetData;
	tempSnippet["description"] = data.description;
	tempSnippet["title"] = data.title;
	tempSnippet["categoryId"] = data.categoryID;

	QJsonObject object;
	object["snippet"] = tempSnippet;
	object["status"] = tempStatus;
	object["id"] = data._id;

	auto _getNetworkReply = [object, receiver, onSucceed, onFailed] {
		const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
		PLSAPIYoutube::configDefaultRequest(_request, receiver, onSucceed, onFailed, YT_API_PutVideo.toUtf8());

		_request.method(pls::http::Method::Put) //
			.url(QString("%1/videos").arg(g_plsYoutubeAPIHost))
			.urlParam("part", "snippet,status")
			.body(object);

		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "requestUpdateVideoData start");
	refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, receiver);
}

static void updateTheNewTokenToChannel(const QString &youtubeUUID, QJsonObject &root)
{
	auto access_token = root["access_token"].toString();
	auto expires_in = root.value("expires_in").toInt() - k_refreshDiff;
	PLSCHANNELS_API->setValueOfChannel(youtubeUUID, ChannelData::g_channelToken, access_token);
	PLSCHANNELS_API->setValueOfChannel(youtubeUUID, ChannelData::g_createTime, QDateTime::currentDateTime());
	PLSCHANNELS_API->setValueOfChannel(youtubeUUID, ChannelData::g_expires_in, expires_in);

	PLS_PLATFORM_API->onUpdateChannel(youtubeUUID);
	PLSCHANNELS_API->channelModified(youtubeUUID);

	QMetaObject::invokeMethod(PLS_PLATFORM_YOUTUBE, &PLSPlatformYoutube::refreshTokenSucceed, Qt::QueuedConnection);
}

void PLSAPIYoutube::refreshYoutubeTokenBeforeRequest(PLSAPICommon::RefreshType refreshType, const std::function<void()> &originNetworkReplay, const QObject *originReceiver,
						     const PLSAPICommon::dataCallback &originOnSucceed, const PLSAPICommon::errorCallback &originOnFailed)
{
	if (pls_get_app_exiting()) {
		return;
	}

	const QString &youtubeUUID = PLS_PLATFORM_YOUTUBE->getChannelUUID();
	auto _onSucceed = [youtubeUUID, originNetworkReplay, originOnSucceed, originOnFailed](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "refreshYoutubeTokenBeforeRequest succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			updateTheNewTokenToChannel(youtubeUUID, root);
			if (originNetworkReplay != nullptr) {
				originNetworkReplay();
			} else if (nullptr != originOnSucceed) {
				originOnSucceed(data);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, "refreshYoutubeTokenBeforeRequest failed, doc is not object");
			if (nullptr != originOnFailed) {
				originOnFailed(1999, data, QNetworkReply::InternalServerError);
			}
		}
	};

	auto _onFail = [originOnFailed](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "refreshYoutubeTokenBeforeRequest failed");
		originOnFailed(code, data, error);
	};

	assert(!youtubeUUID.isEmpty());

	if (refreshType == PLSAPICommon::RefreshType::NotRefresh || (refreshType == PLSAPICommon::RefreshType::CheckRefresh && isTokenValid(youtubeUUID))) {
		if (originNetworkReplay != nullptr) {
			originNetworkReplay();

		} else if (nullptr != originOnSucceed) {
			originOnSucceed(nullptr);
		}
		return;
	}

	PLS_INFO(MODULE_PlatformService, "refreshYoutubeTokenBeforeRequest start");

	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(youtubeUUID);
	auto token = channelData.value(ChannelData::g_refreshToken).toString();

	QString bodyStr(QString("refresh_token=%0&client_id=%1&client_secret=%2&grant_type=refresh_token").arg(token).arg(YOUTUBE_CLIENT_ID).arg(YOUTUBE_CLIENT_KEY));

	QByteArray bodyByte = bodyStr.toUtf8();

	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIYoutube::configDefaultRequest(_request, originReceiver, _onSucceed, _onFail, YT_API_RefreshYoutubeToken.toUtf8());
	_request.contentType(HTTP_CONTENT_TYPE_URL_ENCODED_VALUE);

	_request.method(pls::http::Method::Post).url(g_plsYoutubeAPIToken).urlParam("part", "snippet,status").body(bodyByte);

	pls::http::request(_request);
}

bool PLSAPIYoutube::dealUploadImageSucceed(const QByteArray &data, QString &imgUrl)
{
	QJsonParseError jsonError;
	QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
	if (jsonError.error != QJsonParseError::NoError) {
		PLS_INFO(MODULE_PlatformService, "dealUploadImageSucceed failed, reason: %s", jsonError.errorString().toUtf8().constData());
		return false;
	}

	QJsonObject object = respJson.object();
	if (object.contains("items")) {
		auto items = object["items"].toArray();
		if (!items.isEmpty()) {
			imgUrl = items[0].toObject()["medium"].toObject()["url"].toString();
		}
	}

	if (!imgUrl.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, "dealUploadImageSucceed success");
		return true;
	} else {
		PLS_INFO(MODULE_PlatformService, "dealUploadImageSucceed failed");
		return false;
	}
}

void PLSAPIYoutube::uploadImage(const QObject *receiver, const QString &imageFilePath, const PLSAPICommon::uploadImageCallback &callback, const PLSAPICommon::errorCallback &onFailed)
{
	PLS_INFO(MODULE_PlatformService, "uploadImage start");

	auto okCallback = [callback](QByteArray data) {
		enum PLSPlatformApiResult _result;
		QString imgUrl;
		callback(dealUploadImageSucceed(data, imgUrl), imgUrl);
	};
	const auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	PLSAPIYoutube::configDefaultRequest(_request, receiver, okCallback, onFailed, YT_API_UploadImage.toUtf8(), false);

	_request.urlParam("videoId", PLS_PLATFORM_YOUTUBE->getTrySaveDataData()._id)
		.timeout(PRISM_NET_DOWNLOAD_TIMEOUT)
		.method(pls::http::Method::Post)
		.url(QString("%1/upload/youtube/v3/thumbnails/set").arg(g_plsGoogleApiHost))
		.form("name", imageFilePath, true);
	pls::http::request(_request);
}

void PLSAPIYoutube::getLatency(const QJsonObject &object, PLSYoutubeLiveinfoData::Latency &latency)
{
	auto latencyPreference = object["latencyPreference"].toString();
	if (latencyPreference == s_latencyNormal) {
		latency = PLSYoutubeLiveinfoData::Latency::Normal;
	} else if (latencyPreference == s_latencyLow) {
		latency = PLSYoutubeLiveinfoData::Latency::Low;
	} else if (latencyPreference == s_latencyUltraLow) {
		latency = PLSYoutubeLiveinfoData::Latency::UltraLow;
	} else {
		latency = PLSYoutubeLiveinfoData::Latency::Low;
	}
}

void PLSAPIYoutube::setLatency(QJsonObject &object, PLSYoutubeLiveinfoData::Latency latency)
{
	switch (latency) {
	case PLSYoutubeLiveinfoData::Latency::Normal:
		object["latencyPreference"] = s_latencyNormal;
		break;
	case PLSYoutubeLiveinfoData::Latency::Low:
		object["latencyPreference"] = s_latencyLow;
		break;
	case PLSYoutubeLiveinfoData::Latency::UltraLow:
		object["latencyPreference"] = s_latencyUltraLow;
		break;
	default:
		break;
	}

	if (object.contains("enableLowLatency")) {
		object.remove("enableLowLatency");
	}
}

void PLSAPIYoutube::addCommonCookieAndUserKey(const pls::http::Request &_request)
{
	auto &info = PLSCHANNELS_API->getChanelInfoRefByPlatformName(YOUTUBE, ChannelData::ChannelType);
	if (info.isEmpty()) {
		return;
	}

	auto allToken = info[ChannelData::g_channelToken].toString();
	_request.urlParam("key", YOUTUBE_CLIENT_KEY);
	_request.rawHeader("Authorization", QString("Bearer %1").arg(allToken));
}

void PLSAPIYoutube::showFailedLog(const QString &logName, const pls::http::Reply &reply)
{
	auto _code = reply.statusCode();
	auto _data = reply.data();
	int youtubeCode = 0;
	QString errorReason;
	auto doc = QJsonDocument::fromJson(_data);
	if (!logName.isEmpty()) {
		if (doc.isObject()) {
			auto errObj = doc.object()["error"].toObject();
			if (errObj.contains("code")) {
				youtubeCode = errObj["code"].toInt();
			}

			auto errorArrs = errObj["errors"].toArray();
			if (!errorArrs.empty()) {
				errorReason = errorArrs[0].toObject()["reason"].toString();
				QString errorMsg = errorArrs[0].toObject()["message"].toString();
				QString errorHelp = errorArrs[0].toObject()["extendedHelp"].toString();

				PLS_ERROR(MODULE_PlatformService, "%s failed. code:%i reason:%s \tmsg:%s \textendedHelp:%s", logName.toUtf8().constData(), _code, errorReason.toUtf8().constData(),
					  errorMsg.toUtf8().constData(), errorHelp.toUtf8().constData());
			}
		} else {
			auto errorMsg = _data.left(200);
			PLS_ERROR(MODULE_PlatformService, "%s failed. with not object data. data:%s", logName.toUtf8().constData(), errorMsg.constData());
		}
	}
}
