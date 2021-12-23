#include "PLSChannelDataHandler.h"
#include <QJsonDocument>
#include <QNetworkAccessmanager>
#include <QNetworkReply>
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"

#include "PLSChannelDataAPI.h"

#include <QBuffer>
#include <QDataStream>
#include <QImageReader>
#include <QJsonArray>
#include <QUrl>
#include <QVariantMap>
#include "LogPredefine.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSHttpApi/PLSNetworkReplyBuilder.h"
#include "PLSPlatformApi.h"
#include "log.h"
#include "pls-common-define.hpp"

using namespace ChannelData;

void loadMapper(const QString &platformName, ChannelsMap &src)
{
	if (src.contains(platformName)) {
		return;
	}

	QString fileName = QString(":/configs/configs/%1.json").arg(platformName.toLower());
	auto tmpMaper = loadMapFromJsonFile(fileName);
	if (tmpMaper.isEmpty()) {
		return;
	}
	src.insert(platformName, tmpMaper);
}

FinishTaskReleaser::~FinishTaskReleaser()
{
	auto &info = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	if (info.isEmpty()) {
		PLSCHANNELS_API->holdOnChannelArea(false);
		return;
	}
	bool isUpdated = getInfo(info, g_isUpdated, false);
	if (!isUpdated) {
		info.insert(g_isUpdated, true);
		PLSCHANNELS_API->finishAdding(mSrcUUID);
		PLSCHANNELS_API->addingHold(false);
	} else {
		PLSCHANNELS_API->channelModified(mSrcUUID);
		PLSCHANNELS_API->holdOnChannelArea(false);
	}
	auto platformName = getInfo(info, g_platformName);
	QString step = isUpdated ? g_updateChannelStep : g_addChannelStep;
	PRE_LOG_MSG_STEP(QString("finished update: platform :" + platformName + "ID :" + mSrcUUID), step, INFO);
}

inline void ChannelDataBaseHandler::updateDisplayInfo(InfosList &srcList)
{
	for (auto &src : srcList) {
		src[g_sortString] = src[g_nickName];
		src[g_displayLine1] = src[g_nickName];
		src[g_displayLine2] = src[g_catogry];
	}
}

inline void ChannelDataBaseHandler::resetData()
{
	auto infos = PLSCHANNELS_API->getChanelInfosByPlatformName(getPlatformName());
	updateDisplayInfo(infos);
	for (auto &info : infos) {
		info.remove(g_shareUrl);
		info.remove(g_shareUrlTemp);
		PLSCHANNELS_API->setChannelInfos(info, false);
	}
}

ChannelsMap TwitchDataHandler::mDataMaper = ChannelsMap();

TwitchDataHandler::TwitchDataHandler()
{
	loadMapper(getPlatformName(), mDataMaper);
}

QString TwitchDataHandler::getPlatformName()
{
	return TWITCH;
}

bool TwitchDataHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback callback)
{
	mLastInfo = srcInfo;
	mCallBack = callback;

	PLSNetworkReplyBuilder builder;
	auto token = mLastInfo[g_channelToken].toString();

	builder.setUrl(CHANNEL_TWITCH_URL);

	builder.setRawHeaders({{"client-id", TWITCH_CLIENT_ID}, {"Authorization", "Bearer " + token}, {"Accept", HTTP_ACCEPT_TWITCH}});

	auto handleSuccess = [&](QNetworkReply *, int, QByteArray data, void *) {
		auto jsonDoc = QJsonDocument::fromJson(data).object().value("data").toArray();
		auto jsonMap = jsonDoc.first().toObject().toVariantMap();
		const auto &mapper = mDataMaper[getPlatformName()];
		addToMap(mLastInfo, jsonMap, mapper);
		mLastInfo[g_channelStatus] = Valid;
		mLastInfo[g_broadcastID] = jsonMap.value("id");
		mLastInfo[g_viewers] = "0";
		mLastInfo[g_userName] = jsonMap.value("login");
		mLastInfo[g_viewersPix] = g_defaultViewerIcon;
		mLastInfo[g_shareUrl] = QString(TWITCH_URL_BASE "/%1").arg(mLastInfo[g_userName].toString());

		mLastInfo.insert(g_channelStatus, Valid);

		downloadHeaderImage(jsonMap.value("profile_image_url").toString());
		getChannelsInfo();
	};

	auto handleFail = [&](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError, void *) {
		switch (statusCode) {
		case 401: {
			mLastInfo[g_channelStatus] = Expired;
		} break;
		case Error: {
			mLastInfo.insert(g_channelStatus, Error);
			mLastInfo.insert(g_errorType, NetWorkErrorType::NetWorkNoStable);
		} break;
		default: {
			mLastInfo.insert(g_channelStatus, Error);
			mLastInfo.insert(g_errorType, NetWorkErrorType::UnknownError);
		} break;
		}
		QString errorStr = "channel error status code " + QString::number(statusCode);
		PRE_LOG_MSG(errorStr.toStdString().c_str(), ERROR);
		mCallBack(QList<QVariantMap>{mLastInfo});
	};
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	PLS_HTTP_HELPER->connectFinished(builder.get(network), PLSCHANNELS_API, handleSuccess, handleFail);

	return true;
}

bool TwitchDataHandler::getChannelsInfo()
{
	PLSNetworkReplyBuilder builder;
	auto token = mLastInfo[g_channelToken].toString();
	builder.setUrl(QString(CHANNEL_TWITCH_INFO_URL).arg(mLastInfo[g_broadcastID].toString()));

	builder.setRawHeaders({{"client-id", TWITCH_CLIENT_ID}, {"Authorization", "Bearer " + token}, {"Accept", HTTP_ACCEPT_TWITCH}});

	auto handleSuccess = [&](QNetworkReply *, int, QByteArray data, void *) {
		auto jsonDoc = QJsonDocument::fromJson(data).object().value("data").toArray();
		auto jsonMap = jsonDoc.first().toObject().toVariantMap();
		mLastInfo.insert(g_catogry, jsonMap.value("game_name"));
		mLastInfo.insert(g_language, jsonMap.value("broadcaster_language"));

		mCallBack(QList<QVariantMap>{mLastInfo});
	};

	auto handleFail = [&](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError, void *) {
		QString errorStr = "channel error status code " + QString::number(statusCode);
		PRE_LOG_MSG(errorStr.toStdString().c_str(), ERROR);
		mCallBack(QList<QVariantMap>{mLastInfo});
	};
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	auto reply = builder.get(network);
	reply->setProperty("urlMasking", QString(CHANNEL_TWITCH_INFO_URL).arg(pls_masking_person_info(mLastInfo[g_broadcastID].toString())));

	PLS_HTTP_HELPER->connectFinished(reply, PLSCHANNELS_API, handleSuccess, handleFail);
	return true;
}

bool TwitchDataHandler::downloadHeaderImage(const QString &pixUrl)
{
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	auto reply = PLSNetworkReplyBuilder(pixUrl).get(network);
	reply->setProperty("urlMasking", pls_masking_person_info(pixUrl));
	auto retP = PLSHttpHelper::downloadImageSync(reply, PLSCHANNELS_API, getTmpCacheDir(), getPlatformName());
	if (retP.first) {
		mLastInfo[g_userIconCachePath] = retP.second;
		return true;
	}
	return false;
}

YoutubeHandler::YoutubeHandler()
{
	loadMapper(getPlatformName(), mDataMaper);
}

QString YoutubeHandler::getPlatformName()
{
	return YOUTUBE;
}

bool YoutubeHandler::tryToUpdate(const QVariantMap &srcInfo, UpdateCallback callback)
{
	mLastInfo = srcInfo;
	mCallBack = callback;

	auto accessToken = getInfo(mLastInfo, g_channelToken);
	if (accessToken.isEmpty()) {
		mTaskMap.insert(mTaskMap.count(), QVariant::fromValue(&YoutubeHandler::getRealToken));
	}

	if (!isTokenValid(mLastInfo)) {
		mTaskMap.insert(mTaskMap.count(), QVariant::fromValue(&YoutubeHandler::refreshToken));
	}

	resetData();
	mTaskMap.insert(mTaskMap.count(), QVariant::fromValue(&YoutubeHandler::getBasicInfo));
	mTaskMap.insert(mTaskMap.count(), QVariant::fromValue(&YoutubeHandler::getheaderImage));
	return runTasks();
}

bool YoutubeHandler::refreshToken()
{
	auto refresh_token = getInfo(mLastInfo, g_refreshToken);
	mLastInfo[g_channelStatus] = int(Error);

	PLSNetworkReplyBuilder builder;

	QVariantMap parameters;
	parameters[g_refreshToken] = refresh_token;
	parameters[HTTP_CLIENT_ID] = YOUTUBE_CLIENT_ID;
	parameters[HTTP_CLIENT_SECRET] = YOUTUBE_CLIENT_KEY;
	parameters[HTTP_GRANT_TYPE] = g_refreshToken;
	builder.setQuerys(parameters);

	builder.setUrl(YOUTUBE_API_TOKEN);

	auto handleSuccess = [&](QNetworkReply *, int, QByteArray data, void *) {
		if (data.contains("error_description") && data.contains("Token has been expired or revoked")) {
			handleError(401);
			runTasks();
			return;
		}

		auto jsonDoc = QJsonDocument::fromJson(data);
		auto jsonMap = jsonDoc.toVariant().toMap();

		if (isInvalidGrant(jsonMap)) {
			handleError(401);
			runTasks();
			return;
		}

		addToMap(mLastInfo, jsonMap);
		mLastInfo[g_channelStatus] = Valid;
		mLastInfo[g_createTime] = QDateTime::currentDateTime();
		runTasks();
	};

	auto handleFail = [&](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError, void *) {
		auto jsonDoc = QJsonDocument::fromJson(data);
		auto jsonMap = jsonDoc.toVariant().toMap();
		if (isInvalidGrant(jsonMap)) {
			handleError(401);
			runTasks();
			return;
		}

		handleError(statusCode);
		runTasks();
	};
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	PLS_HTTP_HELPER->connectFinished(builder.post(network), PLSCHANNELS_API, handleSuccess, handleFail);

	return true;
}

using StepFunction = bool (YoutubeHandler::*)(void);
Q_DECLARE_METATYPE(StepFunction)

bool YoutubeHandler::runTasks()
{
	if (mTaskMap.isEmpty()) {
		mTaskMap.clear();
		mCallBack(QList<QVariantMap>{mLastInfo});
		return true;
	}
	auto first = mTaskMap.first();
	auto memFun = first.value<StepFunction>();
	mTaskMap.erase(mTaskMap.begin());
	return (this->*memFun)();
}

bool YoutubeHandler::getRealToken()
{
	PLSNetworkReplyBuilder builder;

	QVariantMap parameters;
	auto code = getInfo(mLastInfo, g_channelCode);
	parameters[HTTP_CODE] = code;
	parameters[HTTP_CLIENT_ID] = YOUTUBE_CLIENT_ID;
	parameters[HTTP_CLIENT_SECRET] = YOUTUBE_CLIENT_KEY;
	parameters[HTTP_REDIRECT_URI] = mLastInfo["__goole_redirect_uri"];
	parameters[HTTP_GRANT_TYPE] = HTTP_AUTHORIZATION_CODE;
	builder.setQuerys(parameters);

	builder.setUrl(YOUTUBE_API_TOKEN);

	auto handleSuccess = [&](QNetworkReply *, int, QByteArray data, void *) {
		auto jsonDoc = QJsonDocument::fromJson(data);
		auto jsonMap = jsonDoc.toVariant().toMap();

		addToMap(mLastInfo, jsonMap);
		runTasks();
	};

	auto handleFail = [&](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError, void *) {
		handleError(statusCode);
		runTasks();
	};
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	PLS_HTTP_HELPER->connectFinished(builder.post(network), PLSCHANNELS_API, handleSuccess, handleFail);

	return true;
}

bool YoutubeHandler::getBasicInfo()
{
	PLSNetworkReplyBuilder builder;
	QVariantMap headerMap;
	QString accessToken = getInfo(mLastInfo, g_channelToken);
	QString tokenType = getInfo(mLastInfo, g_tokenType);
	headerMap[HTTP_AUTHORIZATION] = QString(tokenType).append(ONE_SPACE).append(accessToken);
	headerMap[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_VALUE;
	builder.setRawHeaders(headerMap);

	QVariantMap urlParamMap;
	urlParamMap[HTTP_PART] = HTTP_PART_SNPIIPT;
	urlParamMap[HTTP_MINE] = HTTP_MINE_TRUE;
	builder.setQuerys(urlParamMap);

	builder.setUrl(YOUTUBE_API_INFO);

	auto handleSuccess = [&](QNetworkReply *, int, QByteArray data, void *) {
		auto jsonDoc = QJsonDocument::fromJson(data);
		auto jsonMap = jsonDoc.toVariant().toMap();

		if (isInvalidGrant(jsonMap)) {
			handleError(401);
			runTasks();
			return;
		}

		if (!isItemsExists(jsonMap)) {
			handleError(403);
			runTasks();
			return;
		}

		if (isChannelItemEmpty(jsonMap)) {
			handleError(ChannelIsEmpty);
			runTasks();
			return;
		}

		const auto &mapper = mDataMaper[getPlatformName()];
		addToMap(mLastInfo, jsonMap, mapper);
		mLastInfo[g_channelStatus] = Valid;
		mLastInfo[g_profileThumbnailUrl] = getYoutubeImageUrl(mLastInfo);
		mLastInfo[g_nickName] = getYoutubeName(mLastInfo);
		auto subId = getYoutubeFirstID(mLastInfo);
		mLastInfo[g_subChannelId] = subId;
		mLastInfo[g_shareUrl] = g_youtubeUrl + "/channel/" + subId;

		runTasks();
	};

	auto handleFail = [&](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError, void *) {
		auto jsonDoc = QJsonDocument::fromJson(data);
		auto jsonMap = jsonDoc.toVariant().toMap();
		if (isInvalidGrant(jsonMap)) {
			handleError(401);
			runTasks();
			return;
		}
		handleError(statusCode);
		runTasks();
	};
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	PLS_HTTP_HELPER->connectFinished(builder.get(network), PLSCHANNELS_API, handleSuccess, handleFail);

	return true;
}

bool YoutubeHandler::getheaderImage()
{
	auto pixUrl = getInfo(mLastInfo, g_profileThumbnailUrl);
	downloadHeaderImage(pixUrl);
	runTasks();
	return true;
}

void YoutubeHandler::handleError(int statusCode)
{
	switch (statusCode) {
	case 400: {
		mLastInfo.insert(g_channelStatus, LoginError);
	} break;
	case 401: {
		mLastInfo[g_channelStatus] = Expired;
	} break;
	case 403: {
		mLastInfo.insert(g_channelStatus, int(UnInitialized));
	} break;

	case ChannelIsEmpty: {
		mLastInfo.insert(g_channelStatus, int(WaitingActive));
	} break;
	case Error: {
		mLastInfo.insert(g_channelStatus, Error);
		mLastInfo.insert(g_errorType, NetWorkErrorType::NetWorkNoStable);
	} break;
	default: {
		mLastInfo.insert(g_channelStatus, Error);
		mLastInfo.insert(g_errorType, NetWorkErrorType::UnknownError);

	} break;
	}
	QString errorStr = "channel error status code " + QString::number(statusCode);
	PRE_LOG_MSG(errorStr.toStdString().c_str(), ERROR);
	mTaskMap.clear();
}

void YoutubeHandler::resetData()
{
	mLastInfo[g_catogry] = "Public";
	mLastInfo[g_viewers] = "0";
	mLastInfo[g_viewersPix] = g_defaultViewerIcon;
	mLastInfo[g_likes] = "0";
	mLastInfo[g_comments] = "0";
	mLastInfo[g_displayLine2] = "Public";
	mLastInfo[g_shareUrlTemp] = mLastInfo[g_shareUrl];
	auto youtubePlatform = PLS_PLATFORM_API->getPlatformYoutube(false);
	if (youtubePlatform != nullptr) {
		youtubePlatform->reInitLiveInfo();
	}
}
