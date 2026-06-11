#include "PLSChannelDataHandler.h"
#include <qnetworkaccessmanager.h>
#include <QJsonDocument>
#include <QNetworkReply>
#include "ChannelCommonFunctions.h"
#include "pls-channel-const.h"

#include "PLSChannelDataAPI.h"

#include <qdatastream.h>
#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QScopeGuard>
#include <QUrl>
#include <QVariantMap>
#include "LogPredefine.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSPlatformApi.h"

#include "PLSAPIYoutube.h"
#include "PLSErrorHandler.h"
#include "pls-common-define.hpp"

using namespace ChannelData;
using namespace common;
void loadMapper(const QString &platformName, ChannelsMap &src)
{
	if (src.contains(platformName)) {
		return;
	}

	QString fileName = QString(":/configs/configs/%1.json").arg(platformName.toLower());
	QVariantMap tmpMaper;
	pls_read_json(tmpMaper, fileName);
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
	auto platformName = getInfo(info, g_channelName);
	QString step = isUpdated ? g_updateChannelStep : g_addChannelStep;
	PRE_LOG_MSG_STEP(QString("finished update: platform :" + platformName + "ID :" + mSrcUUID), step, INFO)
}

ChannelDataBaseHandler::ChannelDataBaseHandler()
{
	connect(this, &ChannelDataBaseHandler::preloginFinished, this, &ChannelDataBaseHandler::loginWithWebPage, Qt::QueuedConnection);
}

void ChannelDataBaseHandler::updateDisplayInfo(InfosList &srcList)
{
	for (auto &src : srcList) {
		src[g_sortString] = src[g_nickName];
		src[g_displayLine1] = src[g_nickName];
		src[g_displayLine2] = src[g_catogry];
	}
}

void ChannelDataBaseHandler::resetWhenLiveEnd()
{
	return;
}

void ChannelDataBaseHandler::resetWhenRefresh()
{
	auto infos = PLSCHANNELS_API->getChanelInfosByPlatformName(getPlatformName());
	updateDisplayInfo(infos);
	for (auto &info : infos) {
		info.remove(g_shareUrl);
		info.remove(g_shareUrlTemp);
		PLSCHANNELS_API->setChannelInfos(info, false);
	}
}

void ChannelDataBaseHandler::resetData(const ResetReson &reson)
{
	switch (reson) {
	case ResetReson::All:
		resetWhenLiveEnd();
		resetWhenRefresh();
		break;
	case ResetReson::EndLiveReset:
		resetWhenLiveEnd();
		break;
	case ResetReson::RefreshReset:
		resetWhenRefresh();
		break;
	default:
		resetWhenRefresh();
		break;
	}
	return;
}

void addLoginChannel(const QVariantMap &retMap)
{
	PLSCHANNELS_API->addChannelInfo(retMap);
}

void ChannelDataBaseHandler::loginWithWebPage(const QString &cmdStr)
{
	this->resetData();
	myLastInfo() = createDefaultChannelInfoMap(getPlatformName(), ChannelData::ChannelType, cmdStr);
	auto retMap = myLastInfo();
	PRE_LOG_MSG_STEP(QString(" show %1 login page ").arg(cmdStr), g_addChannelStep, INFO)
	auto handleCallback = [retMap, cmdStr, this](bool ok, const QVariantHash &result) mutable {
		if (ok) {
			for (const auto &key : result.keys())
				retMap[key] = result[key];
			addLoginChannel(retMap);
			PLSBasic::instance()->updateMainViewAlwayTop();
		} else if (result.value(ChannelData::g_expires_in).toBool()) {
			PLS_INFO("Channels", "%s login failed ,prism token expired", cmdStr.toUtf8().constData());
			auto retData = result.value(ChannelData::g_errorRetdata).value<PLSErrorHandler::RetData>();
			reloginPrismExpired(retData);
		}
		emit this->loginFinished();
	};

	QMetaObject::invokeMethod(
		pls_get_main_view(),
		[handleCallback, this]() { //
			pls_channel_login_async(handleCallback, getPlatformName(), pls_get_main_view());
		},
		Qt::QueuedConnection);
}

QList<QPair<QString, QPixmap>> ChannelDataBaseHandler::getEndLiveViewList(const QVariantMap &sourceData)
{
	QList<QPair<QString, QPixmap>> ret;
	QString count1 = sourceData.value(ChannelData::g_viewers, "").toString();
	QString count2 = sourceData.value(ChannelData::g_likes, "").toString();
	QString count3 = sourceData.value(ChannelData::g_comments, "").toString();
	QString count4 = sourceData.value(ChannelData::g_totalViewers, "").toString();
	auto dpi = 1;
	QPixmap pix1(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_viewersPix, QString(":/images/ic-liveend-view.svg")), QSize(15, 15)));
	QPixmap pix2(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_likesPix, QString(":/images/ic-liveend-like.svg")), QSize(15, 15)));
	QPixmap pix3(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_commentsPix, QString(":/images/reply-icon.svg")), QSize(15, 15)));
	QPixmap pix4(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_commentsPix, QString(":/images/ic-liveend-chat-fb.svg")), QSize(15, 15)));

	EndLivePair p1(count1, pix1);
	ret.append(p1);

	EndLivePair p2(count2, pix2);
	ret.append(p2);

	EndLivePair p3(count3, pix3);
	ret.append(p3);

	EndLivePair p4(count4, pix4);
	ret.append(p4);

	return ret;
}

void ChannelDataBaseHandler::downloadImage()
{
	auto pixUrl = myLastInfo().value(ChannelData::g_profileThumbnailUrl).toString();
	if (pixUrl.isEmpty()) {
		return;
	}
	auto uuid = getInfo(myLastInfo(), ChannelData::g_channelUUID);
	auto finisheDownload = [uuid](const pls::rsm::DownloadResult &result) {
		if (result.hasFilePath()) {
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_userIconCachePath, result.filePath());
			PLSCHANNELS_API->channelModified(uuid);
		}
	};
	PLS_INFO(getPlatformName().toUtf8().constData(), "start download image %s", pls_masking_person_info(pixUrl).toUtf8().constData());
	downloadUserImage(pixUrl, getPlatformName(), finisheDownload);
	return;
}

void ChannelDataBaseHandler::loadConfig()
{
	loadMapper(getPlatformName(), mDataMaper);
}

QVariantMap ChannelDataBaseHandler::getMyDataMaper()
{
	return mDataMaper.value(getPlatformName());
}

ChannelsMap ChannelDataBaseHandler::mDataMaper = ChannelsMap();

void TwitchDataHandler::initialization()
{
	loadConfig();
}

QString TwitchDataHandler::getPlatformName()
{
	return TWITCH;
}

bool TwitchDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback)
{
	myLastInfo() = srcInfo;
	myCallback() = callback;
	auto token = myLastInfo()[g_channelToken].toString();

	QVariantMap map = {{"client-id", TWITCH_CLIENT_ID}, {"Authorization", "Bearer " + token}, {"Accept", HTTP_ACCEPT_TWITCH}};

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(map)
				   .withLog()               //
				   .url(CHANNEL_TWITCH_URL) //
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .receiver(PLSCHANNELS_API)
				   .okResult([this](const pls::http::Reply &reply) {
					   auto jsonDoc = QJsonDocument::fromJson(reply.data()).object().value("data").toArray();
					   auto jsonMap = jsonDoc.first().toObject().toVariantMap();
					   const auto &mapper = getMyDataMaper();
					   addToMap(myLastInfo(), jsonMap, mapper);
					   myLastInfo()[g_channelStatus] = Valid;
					   myLastInfo()[g_broadcastID] = jsonMap.value("id");
					   myLastInfo()[g_viewers] = "0";
					   myLastInfo()[g_userName] = jsonMap.value("login");
					   myLastInfo()[g_viewersPix] = g_defaultViewerIcon;
					   myLastInfo()[g_shareUrl] = QString(TWITCH_URL_BASE + "/%1").arg(myLastInfo()[g_userName].toString());

					   myLastInfo().insert(g_channelStatus, Valid);
					   myLastInfo().insert(g_profileThumbnailUrl, jsonMap.value("profile_image_url").toString());
					   getChannelsInfo();
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   PLSErrorHandler::ExtraData extraData;
					   extraData.errPhase = PLSErrPhaseLogin;
					   extraData.urlEn = CHANNEL_TWITCH_URL;
					   auto retData = PLSErrorHandler::getAlertString({reply.statusCode(), reply.error(), reply.data()}, TWITCH, "", extraData);
					   if (retData.prismCode == PLSErrorHandler::ErrCode::COMMON_CHANNEL_LOGIN_TOKEN_EXPIRED_ERROR) {
						   myLastInfo()[g_channelStatus] = Expired;

					   } else {
						   myLastInfo().insert(g_channelStatus, Error);
					   }
					   myLastInfo()[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
					   myLastInfo()[g_errorString] = retData.alertMsg;
					   myLastInfo().insert(ChannelData::g_channelSreLoginFailed, "twitch get user info failed");

					   myCallback()(QList<QVariantMap>{myLastInfo()});
				   }));

	return true;
}

bool TwitchDataHandler::getChannelsInfo()
{
	PLS_INFO("twitchPlatform", "getChannelsInfo start get channel infos");
	auto token = myLastInfo()[g_channelToken].toString();

	QVariantMap map = {{"client-id", TWITCH_CLIENT_ID}, {"Authorization", "Bearer " + token}, {"Accept", HTTP_ACCEPT_TWITCH}};

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)
				   .jsonContentType() //
				   .rawHeaders(map)
				   .withLog(QString(CHANNEL_TWITCH_INFO_URL).arg(pls_masking_person_info(myLastInfo()[g_broadcastID].toString()))) //
				   .url(QString(CHANNEL_TWITCH_INFO_URL).arg(myLastInfo()[g_broadcastID].toString()))                              //
				   .receiver(PLSCHANNELS_API)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .okResult([this](const pls::http::Reply &reply) {
					   auto jsonDoc = QJsonDocument::fromJson(reply.data()).object().value("data").toArray();
					   auto jsonMap = jsonDoc.first().toObject().toVariantMap();
					   myLastInfo().insert(g_catogry, jsonMap.value("game_name"));

					   myCallback()(QList<QVariantMap>{myLastInfo()});
					   downloadHeaderImage(myLastInfo().value(g_profileThumbnailUrl).toString());
				   })
				   .failResult([this](const pls::http::Reply &reply) {
					   myLastInfo().insert(ChannelData::g_channelSreLoginFailed, "twitch get channels info failed");
					   auto statusCode = reply.statusCode();
					   QString errorStr = "channel error status code " + QString::number(statusCode);
					   PRE_LOG_MSG(errorStr.toStdString().c_str(), ERROR)
					   myCallback()(QList<QVariantMap>{myLastInfo()});
				   }));
	return true;
}

bool TwitchDataHandler::downloadHeaderImage(const QString &pixUrl)
{

	auto uuid = getInfo(myLastInfo(), g_channelUUID);
	auto finisheDownload = [uuid](const pls::rsm::DownloadResult &result) {
		if (result.hasFilePath()) {
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_userIconCachePath, result.filePath());
			PLSCHANNELS_API->channelModified(uuid);
		}
	};
	downloadUserImage(pixUrl, getPlatformName(), finisheDownload);
	return true;
}

QString YoutubeHandler::getPlatformName()
{
	return YOUTUBE;
}

bool YoutubeHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback)
{
	myLastInfo() = srcInfo;
	myCallback() = callback;

	if (auto accessToken = getInfo(myLastInfo(), g_channelToken); accessToken.isEmpty()) {
		mTaskMap.insert(static_cast<int>(mTaskMap.count()), QVariant::fromValue(&YoutubeHandler::getRealToken));
	}

	if (!isTokenValid(myLastInfo())) {
		mTaskMap.insert(static_cast<int>(mTaskMap.count()), QVariant::fromValue(&YoutubeHandler::refreshToken));
	}

	resetData();
	mTaskMap.insert(static_cast<int>(mTaskMap.count()), QVariant::fromValue(&YoutubeHandler::getBasicInfo));
	mTaskMap.insert(static_cast<int>(mTaskMap.count()), QVariant::fromValue(&YoutubeHandler::getheaderImage));
	return runTasks();
}

bool YoutubeHandler::refreshToken()
{
	auto refresh_token = getInfo(myLastInfo(), g_refreshToken);
	myLastInfo()[g_channelStatus] = int(Error);

	QVariantMap parameters;
	parameters[g_refreshToken] = refresh_token;
	parameters[HTTP_CLIENT_ID] = YOUTUBE_CLIENT_ID;
	parameters[HTTP_CLIENT_SECRET] = YOUTUBE_CLIENT_KEY;
	parameters[HTTP_GRANT_TYPE] = g_refreshToken;

	pls::http::Request request;
	request.urlParams(parameters);

	request.url(g_plsYoutubeAPIToken);

	auto handleSuccess = [this](const pls::http::Reply &reply) {
		auto data = reply.data();
		auto jsonDoc = QJsonDocument::fromJson(data);
		auto jsonMap = jsonDoc.toVariant().toMap();

		addToMap(myLastInfo(), jsonMap);
		myLastInfo()[g_channelStatus] = Valid;
		myLastInfo()[g_createTime] = QDateTime::currentDateTime();
		runTasks();
	};

	auto handleFail = [this](const pls::http::Reply &reply) {
		auto jsonDoc = QJsonDocument::fromJson(reply.data());
		PLSAPIYoutube::showFailedLog(YT_API_RefreshYoutubeToken, reply);
		QString errReason = YT_API_RefreshYoutubeToken + " Failed";
		if (PLS_PLATFORM_YOUTUBE) {
			errReason = PLS_PLATFORM_YOUTUBE->getFailedErr();
		}
		myLastInfo()[g_channelSreLoginFailed] = errReason;
		handleError(reply.statusCode(), reply.data(), reply.error(), YT_API_RefreshYoutubeToken);

		runTasks();
	};
	request.okResult(handleSuccess);
	request.failResult(handleFail);
	request.receiver(PLSCHANNELS_API);
	request.timeout(PRISM_NET_REQUEST_TIMEOUT);
	request.method(pls::http::Method::Post);
	request.withLog();
	pls::http::request(request);

	return true;
}

using StepFunction = bool (YoutubeHandler::*)(void);
Q_DECLARE_METATYPE(StepFunction)

bool YoutubeHandler::runTasks()
{
	if (mTaskMap.isEmpty()) {
		mTaskMap.clear();
		myCallback()(QList<QVariantMap>{myLastInfo()});
		return true;
	}
	auto first = mTaskMap.first();
	auto memFun = first.value<StepFunction>();
	mTaskMap.erase(mTaskMap.begin());
	return (this->*memFun)();
}

bool YoutubeHandler::getRealToken()
{
	QVariantMap parameters;
	auto code = getInfo(myLastInfo(), g_channelCode);
	parameters[HTTP_CODE] = code;
	parameters[HTTP_CLIENT_ID] = YOUTUBE_CLIENT_ID;
	parameters[HTTP_CLIENT_SECRET] = YOUTUBE_CLIENT_KEY;
	parameters[HTTP_REDIRECT_URI] = myLastInfo()["__goole_redirect_uri"];
	parameters[HTTP_GRANT_TYPE] = HTTP_AUTHORIZATION_CODE;

	pls::http::Request request;
	request.urlParams(parameters);

	request.url(g_plsYoutubeAPIToken);

	auto handleSuccess = [this](const pls::http::Reply &reply) {
		auto jsonDoc = QJsonDocument::fromJson(reply.data());
		auto jsonMap = jsonDoc.toVariant().toMap();

		addToMap(myLastInfo(), jsonMap);
		runTasks();
	};

	auto handleFail = [this](const pls::http::Reply &reply) {
		PLSAPIYoutube::showFailedLog(YT_API_PostToken, reply);
		QString errReason = YT_API_PostToken + " Failed";
		if (PLS_PLATFORM_YOUTUBE) {
			errReason = PLS_PLATFORM_YOUTUBE->getFailedErr();
		}
		myLastInfo()[g_channelSreLoginFailed] = errReason;
		handleError(reply.statusCode(), reply.data(), reply.error(), YT_API_PostToken);
		runTasks();
	};

	request.okResult(handleSuccess);
	request.failResult(handleFail);
	request.receiver(PLSCHANNELS_API);
	request.timeout(PRISM_NET_REQUEST_TIMEOUT);
	request.method(pls::http::Method::Post);
	pls::http::request(request);

	return true;
}

bool YoutubeHandler::getBasicInfo()
{
	QVariantMap headerMap;
	QString accessToken = getInfo(myLastInfo(), g_channelToken);
	QString tokenType = getInfo(myLastInfo(), g_tokenType);
	headerMap[HTTP_AUTHORIZATION] = QString(tokenType).append(ONE_SPACE).append(accessToken);
	headerMap[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_VALUE;

	pls::http::Request request;
	request.rawHeaders(headerMap);

	QVariantMap urlParamMap;
	urlParamMap[HTTP_PART] = HTTP_PART_SNPIIPT;
	urlParamMap[HTTP_MINE] = HTTP_MINE_TRUE;
	request.urlParams(urlParamMap);

	request.url(QString("%1/channels").arg(g_plsYoutubeAPIHost));

	auto handleSuccess = [this](const pls::http::Reply &reply) {
		auto jsonDoc = QJsonDocument::fromJson(reply.data());
		auto jsonMap = jsonDoc.toVariant().toMap();
		auto guard = qScopeGuard([this] { runTasks(); });

		if (!isItemsExists(jsonMap)) {
			myLastInfo()[g_channelSreLoginFailed] = QString("refreshChannelList Failed, code:items is not exist");
			myLastInfo().insert(g_channelStatus, int(UnInitialized));
			mTaskMap.clear();
			return;
		}

		if (isChannelItemEmpty(jsonMap)) {
			myLastInfo().insert(g_channelStatus, int(UnInitialized));
			mTaskMap.clear();
			return;
		}
		finishUpdateBasicInfo(jsonMap);
	};

	auto handleFail = [this](const pls::http::Reply &reply) {
		PLSAPIYoutube::showFailedLog(YT_API_GetChannels, reply);

		QString errReason = YT_API_GetChannels + " Failed";
		if (PLS_PLATFORM_YOUTUBE) {
			errReason = PLS_PLATFORM_YOUTUBE->getFailedErr();
		}

		myLastInfo()[g_channelSreLoginFailed] = errReason;
		handleError(reply.statusCode(), reply.data(), reply.error(), YT_API_GetChannels);

		runTasks();
	};
	request.okResult(handleSuccess);
	request.failResult(handleFail);
	request.receiver(PLSCHANNELS_API);
	request.timeout(PRISM_NET_REQUEST_TIMEOUT);
	request.method(pls::http::Method::Get);
	request.withLog();
	pls::http::request(request);

	return true;
}

void YoutubeHandler::finishUpdateBasicInfo(const QVariantMap &jsonMap)
{
	const auto &mapper = getMyDataMaper();
	addToMap(myLastInfo(), jsonMap, mapper);
	myLastInfo()[g_channelStatus] = Valid;
	myLastInfo()[g_profileThumbnailUrl] = getYoutubeImageUrl(myLastInfo());
	myLastInfo()[g_nickName] = getYoutubeName(myLastInfo());
	auto subId = getYoutubeFirstID(myLastInfo());
	myLastInfo()[g_subChannelId] = subId;
	myLastInfo()[g_shareUrl] = g_youtubeUrl + "/channel/" + subId;
}

bool YoutubeHandler::getheaderImage()
{
	auto pixUrl = getInfo(myLastInfo(), g_profileThumbnailUrl);
	downloadHeaderImage(pixUrl);
	runTasks();
	return true;
}

void YoutubeHandler::handleError(int code, QByteArray data, QNetworkReply::NetworkError error, const QString &logFrom)
{
	PLSErrorHandler::ExtraData extraData;
	extraData.urlEn = logFrom;
	extraData.errPhase = PLSErrPhaseLogin;
	auto retData = PLSErrorHandler::getAlertString({code, error, data}, YOUTUBE, "", extraData);
	ChannelStatus chStatus = ChannelData::ChannelStatus::Error;
	if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired) {
		chStatus = ChannelData::ChannelStatus::Expired;
	}
	myLastInfo()[ChannelData::g_channelStatus] = chStatus;
	myLastInfo()[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
	myLastInfo()[ChannelData::g_errorString] = retData.alertMsg;
	mTaskMap.clear();
}
void YoutubeHandler::resetWhenRefresh()
{
	myLastInfo()[g_catogry] = "Public";
	myLastInfo()[g_viewers] = "0";
	myLastInfo()[g_viewersPix] = g_defaultViewerIcon;
	myLastInfo()[g_likes] = "0";
	myLastInfo()[g_comments] = "0";
	myLastInfo()[g_displayLine2] = "Public";
	myLastInfo()[g_shareUrlTemp] = myLastInfo()[g_shareUrl];
	auto youtubePlatform = PLS_PLATFORM_API->getPlatformYoutube();
	if (youtubePlatform != nullptr) {
		youtubePlatform->reInitLiveInfo();
	}
}
