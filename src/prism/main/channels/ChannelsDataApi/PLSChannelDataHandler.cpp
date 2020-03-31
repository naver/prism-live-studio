#include "PLSChannelDataHandler.h"
#include "ChannelConst.h"
#include "NetWorkAPI.h"
#include "PLSChannelDataAPI.h"
#include "NetBaseInfo.h"
#include <QNetworkAccessmanager>
#include "NetWorkCommonDefines.h"
#include <QNetworkReply>
#include "ChannelCommonFunctions.h"
#include <QJsonDocument>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include "LogPredefine.h"
#include <QDataStream>
#include <QImageReader>
#include <QBuffer>
#include "log.h"
#include "frontend-api.h"
#include "NetWorkCommonFunctions.h"
#include <QJsonArray>
#include "PLSPlatformApi.h"
#include <QRunnable>
#include <QThreadPool>
#include "pls-common-define.hpp"
#include "ui-config.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSChannelsVirualAPI.h"

#define HEADER_PRISM_DEVICE QStringLiteral("X-prism-device")
#define HEADER_PRISM_USERCODE QStringLiteral("X-prism-usercode")
#define HEADER_PRISM_IP QStringLiteral("X-prism-ip")
#define HEADER_WINDOWS_OS QStringLiteral("Windows OS")

using namespace ChannelData;

ChannelsMap ChannelDataHandler::mDataMaper = ChannelsMap();

struct FinishTaskReleaser {
	FinishTaskReleaser(const QString &srcUUID) : mSrcUUID(srcUUID){};
	~FinishTaskReleaser()
	{
		auto &info = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
		if (info.isEmpty()) {
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
		PRE_LOG(handler End, INFO);
	}

private:
	QString mSrcUUID;
};

struct TaskMapGuarder {
	TaskMapGuarder(const QVariantMap &src) { mTaskID = getInfo(src, UUID); }
	~TaskMapGuarder() { PLSCHANNELS_API->getNetWorkAPI()->removeTaskMap(mTaskID); }
	QString mTaskID;
};

inline ChannelDataHandler::ChannelDataHandler(const QString &uuid) : mHanderlID(uuid), mSrcUUID(uuid)
{
	loadMaper();
	QString platform = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_channelName, QString(" no platform "));
	PRE_LOG_MSG(QString("create handler: src -> " + mSrcUUID + " plaform " + platform + " hanlder ->" + mHanderlID).toStdString().c_str(), INFO);
	PLSCHANNELS_API->release();
}

inline ChannelDataHandler::~ChannelDataHandler()
{
	PLSCHANNELS_API->acquire();
	if (PLSCHANNELS_API->hasError()) {
		PLSCHANNELS_API->networkInvalidOcurred();
	}
	QString platform = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_channelName, QString(" no platform "));
	PRE_LOG_MSG(QString("destory handler: src -> " + mSrcUUID + " plaform " + platform + " hanlder ->" + mHanderlID).toStdString().c_str(), INFO);
	FinishTaskReleaser finishAdd(mSrcUUID);
}

QVariantMap loadMapFromJsonFile(const QString &fileName)
{
	QVariantMap tmpMaper;
	QFile file(fileName);
	if (file.open(QIODevice::ReadOnly)) {
		auto data = file.readAll();
		auto jsonDoc = QJsonDocument::fromJson(data);
		tmpMaper = jsonDoc.toVariant().toMap();
	}

	return tmpMaper;
}

void ChannelDataHandler::loadMaper()
{
	if (mHanderlID.isEmpty() || mDataMaper.contains(mHanderlID)) {
		return;
	}

	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	QString channelName = getInfo(channelInfo, g_channelName, QString());
	if (mDataMaper.contains(channelName)) {
		return;
	}

	QString fileName = QString(":/configs/configs/%1.json").arg(channelName.toLower());
	auto tmpMaper = loadMapFromJsonFile(fileName);
	if (tmpMaper.isEmpty()) {
		return;
	}
	mDataMaper.insert(channelName, tmpMaper);
}

class TwitchDataHanbler : public ChannelDataHandler {

public:
	using ChannelDataHandler::ChannelDataHandler;
	void initData() override;
	bool sendRequest() override;
	void callback(const QVariantMap &retData) override;
	virtual void feedbackToServer() override;

private:
	QVariantMap header;
	QVariantMap urlParamMap;
};

void TwitchDataHanbler::initData()
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	header = createDefaultHeaderMap();
	header[HTTP_ACCEPT] = HTTP_ACCEPT_TWITCH;

	urlParamMap = createDefaultUrlMap();
	urlParamMap[HTTP_CLIENT_ID] = TWITCH_CLIENT_ID;
	auto token = channelInfo[g_channelToken].toString();
	urlParamMap[COOKIE_OAUTH_TOKEN] = token;
}

bool TwitchDataHanbler::sendRequest()
{
	//to get user data
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	if (network) {
		requestStartLog(CHANNEL_TWITCH_URL, "Get");
		auto &taskMap = network->createHttpRequest(QNetworkAccessManager::GetOperation, CHANNEL_TWITCH_URL, true, header, urlParamMap, false);
		if (taskMap.isEmpty()) {
			PRE_LOG(GetTwitchInfo, ERROR);
			return false;
		}
		taskMap.insert(g_handlerUUID, mHanderlID);
		return true;
	}
	return false;
}

void TwitchDataHanbler::callback(const QVariantMap &taskData)
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	auto reply = taskData.value(REPLY_POINTER).value<ReplyPtrs>();
	if (channelInfo.isEmpty() || reply.isNull()) {
		return;
	}
	auto Arraydatas = taskData.value(BODY_DATA).toByteArray();
	if (reply->error() != QNetworkReply::NoError) {
		int statusCode = getReplyStatusCode(reply);
		if (statusCode == 401) {
			ChannelsNetWorkPretestWithAlerts(reply, Arraydatas, false);
			channelInfo[g_channelStatus] = Expired;
			return;
		}
		ChannelsNetWorkPretestWithAlerts(reply, Arraydatas);
		bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
		if (!isUpdated) {
			channelInfo[g_channelStatus] = Error;
		}
		return;
	}

	formatNetworkLogs(reply, Arraydatas);

	if (Arraydatas.isEmpty()) {
		return;
	}

	auto jsonDoc = QJsonDocument::fromJson(Arraydatas);
	auto jsonMap = jsonDoc.toVariant().toMap();
	//ViewMapData(jsonMap);
	QString channelName = getInfo(channelInfo, g_channelName, QString());
	const auto &mapper = mDataMaper[channelName];
	addToMap(channelInfo, jsonMap, mapper);
	channelInfo[g_channelStatus] = Valid;
	channelInfo[g_viewers] = "0";
	//ViewMapData(channelInfo);
}

void TwitchDataHanbler::feedbackToServer()
{

	int status = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_channelStatus, InValid);
	switch (status) {
	case Expired: {
		PLSCHANNELS_API->channelExpired(mSrcUUID);
		return;
	} break;
	case Error: {
		bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
		if (!isUpdated) {
			PLSCHANNELS_API->removeChannelInfo(mSrcUUID);
		}
		return;
	} break;
	default:
		break;
	}

	if (PLSCHANNELS_API->hasError()) {
		return;
	}
	QString loadUUID = createDownloadHandler(mSrcUUID);
	PLSCHANNELS_API->sigSendRequest(loadUUID);
}

void createTwitchHandler(const QString &uuid)
{
	//PRE_LOG(twitch handler create, INFO);
	ChannelDataHandlerPtr handler(new TwitchDataHanbler(uuid));
	handler->initData();
	PLSCHANNELS_API->addHandler(QVariant::fromValue(handler));
}

class DowloadHandler : public ChannelDataHandler {
public:
	using ChannelDataHandler::ChannelDataHandler;
	void initData() override;
	bool sendRequest() override;
	void callback(const QVariantMap &retData) override;
	virtual void feedbackToServer() override;
	void setSrc(const QString &src) { mSrcUUID = src; }
};

void DowloadHandler::initData() {}
bool DowloadHandler::sendRequest()
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	if (channelInfo.isEmpty()) {
		return false;
	}
	QString fileUrl = channelInfo.value(g_profileThumbnailUrl).toString();
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	if (network) {
		requestStartLog(fileUrl, "Get");
		auto &taskMap = network->createHttpRequest(QNetworkAccessManager::GetOperation, fileUrl, false);
		if (taskMap.isEmpty()) {
			return false;
		}
		taskMap.insert(g_handlerUUID, mHanderlID);
		return true;
	}
	return false;
}

void DowloadHandler::callback(const QVariantMap &taskData)
{
	auto reply = getInfo(taskData, REPLY_POINTER, ReplyPtrs());
	if (reply.isNull()) {
		return;
	}
	auto Arraydatas = taskData.value(BODY_DATA).toByteArray();
	if (reply->error() != QNetworkReply::NoError) {
		ChannelsNetWorkPretestWithAlerts(reply, Arraydatas);
		return;
	}
	formatNetworkLogs(reply, Arraydatas);

	if (Arraydatas.isEmpty()) {
		return;
	}
	QString suffix = ".png";
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	if (channelInfo.isEmpty()) {
		return;
	}
	QString fileName = getInfo(channelInfo, g_channelName, QString());
	QString path = getChannelCacheDir() + "/" + fileName + "_" + g_userIconCachePath + "." + suffix;
	QBuffer buffer(&const_cast<QByteArray &>(Arraydatas));
	if (!buffer.open(QIODevice::ReadOnly)) {
		return;
	}
	QImageReader reader(&buffer);
	auto image = QPixmap::fromImage(reader.read());
	image.save(path);
	channelInfo[g_userIconCachePath] = path;
}

void DowloadHandler::feedbackToServer()
{
	if (PLSCHANNELS_API->hasError()) {
		return;
	}
}

QString createDownloadHandler(const QString &srcChannelUUID)
{
	PRE_LOG(download handler create, INFO);
	if (!PLSCHANNELS_API->isChannelInfoExists(srcChannelUUID)) {
		return QString();
	}

	auto channelInfo = PLSCHANNELS_API->getChannelInfo(srcChannelUUID);
	QString newUUID = createUUID();
	auto download = new DowloadHandler(newUUID);
	download->setSrc(srcChannelUUID);
	ChannelDataHandlerPtr handler(download);
	handler->initData();
	PLSCHANNELS_API->addHandler(qVariantFromValue(handler));
	return newUUID;
}

QString getYoutubeImageUrl(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}

	return list[0].toMap()["snippet"].toMap()["thumbnails"].toMap()["high"].toMap()["url"].toString();
}

QString getYoutubeName(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}

	return list[0].toMap()["snippet"].toMap()["title"].toString();
}

QString getYoutubeFirstID(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}
	return list[0].toMap()["id"].toString();
}

bool isChannelInitialized(const QVariantMap &src)
{
	return src.contains("items");
}

bool isChannelItemEmpty(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return true;
	}

	return !list[0].toMap().contains("status");
}

QString getYoutubePriacyStatus(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}
	auto mapS = list[0].toMap()["status"].toMap();
	return mapS["privacyStatus"].toString();
}

class YouTubeDataHanbler : public ChannelDataHandler {

public:
	using ChannelDataHandler::ChannelDataHandler;
	void initData() override;
	bool sendRequest() override;
	void callback(const QVariantMap &retData) override;
	virtual void feedbackToServer() override;

	bool getRealToken();
	bool checkExpiresIn();
	bool getBroadCastInfo();
	void getChannelsBasicInfo();

private:
	QVariantMap mHeaders;
	QVariantMap mUrlParamMap;
};

void YouTubeDataHanbler::initData()
{
	mHeaders = createDefaultHeaderMap();
}

bool YouTubeDataHanbler::getRealToken()
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	if (channelInfo.isEmpty()) {
		return false;
	}
	auto accessToken = getInfo(channelInfo, g_channelToken);
	if (!accessToken.isEmpty()) {
		return true;
	}
	auto code = getInfo(channelInfo, g_channelCode);
	mUrlParamMap[HTTP_CODE] = code;
	mUrlParamMap[HTTP_CLIENT_ID] = YOUTUBE_CLIENT_ID;
	mUrlParamMap[HTTP_CLIENT_SECRET] = YOUTUBE_CLIENT_KEY;
	mUrlParamMap[HTTP_REDIRECT_URI] = YOUTUBE_REDIRECT_URI;
	mUrlParamMap[HTTP_GRANT_TYPE] = HTTP_AUTHORIZATION_CODE;

	auto network = PLSCHANNELS_API->getNetWorkAPI();
	if (network == nullptr) {
		return false;
	}
	requestStartLog(YOUTUBE_API_TOKEN, "Post");
	QEventLoop loop;
	QObject::connect(convertToObejct(network), SIGNAL(replySyncData(const QString &)), &loop, SLOT(quit()), Qt::DirectConnection);
	auto &taskMap = network->createHttpRequest(QNetworkAccessManager::PostOperation, YOUTUBE_API_TOKEN, false, mHeaders, mUrlParamMap, true);
	loop.exec();
	TaskMapGuarder guarder(taskMap);
	if (taskMap.isEmpty()) {
		return false;
	}

	auto reply = getInfo(taskMap, REPLY_POINTER, ReplyPtrs());
	if (reply.isNull()) {
		return false;
	}
	auto Arraydatas = taskMap.value(BODY_DATA).toByteArray();
	if (reply->error() != QNetworkReply::NoError) {
		bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
		if (!isUpdated) {
			channelInfo[g_channelStatus] = Error;
		}
		ChannelsNetWorkPretestWithAlerts(reply, Arraydatas);
		return false;
	}

	formatNetworkLogs(reply, Arraydatas);

	if (Arraydatas.isEmpty()) {
		return false;
	}

	auto jsonDoc = QJsonDocument::fromJson(Arraydatas);
	auto jsonMap = jsonDoc.toVariant().toMap();
	addToMap(channelInfo, jsonMap);
	//ViewMapData(channelInfo);

	return true;
}

bool YouTubeDataHanbler::checkExpiresIn()
{
	return checkAndUpdateToken(mSrcUUID);
}

bool YouTubeDataHanbler::getBroadCastInfo()
{
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	if (network == nullptr) {
		return false;
	}
	auto channelInfoTemp = PLSCHANNELS_API->getChannelInfo(mSrcUUID);
	if (channelInfoTemp.isEmpty()) {
		return false;
	}
	QString accessToken = getInfo(channelInfoTemp, g_channelToken);
	QString tokenType = getInfo(channelInfoTemp, g_tokenType);

	QVariantMap headerMap;
	headerMap[HTTP_AUTHORIZATION] = QString(tokenType).append(ONE_SPACE).append(accessToken);
	headerMap[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_VALUE;

	QVariantMap urlParamMap;
	urlParamMap[HTTP_PART] = HTTP_PART_SNPIIPT + g_comma + g_statusPart + g_comma + g_contentDetails;
	urlParamMap[HTTP_MINE] = HTTP_MINE_TRUE;
	urlParamMap[g_broadCastType] = g_persistentType;

	//to get user data
	requestStartLog(g_youtubeBroadcast, "Get");
	QEventLoop loop;
	QObject::connect(convertToObejct(network), SIGNAL(replySyncData(const QString &)), &loop, SLOT(quit()), Qt::DirectConnection);
	auto &taskMap = network->createHttpRequest(QNetworkAccessManager::GetOperation, g_youtubeBroadcast, true, headerMap, urlParamMap, true);
	loop.exec();

	if (taskMap.isEmpty()) {
		return false;
	}
	TaskMapGuarder guarder(taskMap);
	auto reply = getInfo(taskMap, REPLY_POINTER, ReplyPtrs());
	if (reply.isNull()) {
		return false;
	}
	auto Arraydatas = taskMap.value(BODY_DATA).toByteArray();
	auto errorFlag = reply->error();
	if (errorFlag != QNetworkReply::NoError) {
		int statusCode = getReplyStatusCode(reply);
		switch (statusCode) {
		case 401: {
			PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_channelStatus, int(Expired));
			return false;
		} break;
		case 403: {
			PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_channelStatus, int(UnInitialized));
			PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_channelUserStatus, int(Disabled));
			PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_errorString, CHANNELS_TR(emptyChannelError));
			return false;
		} break;
		case 0: {
			bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
			if (!isUpdated) {
				PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_channelStatus, int(Error));
			}
			return false;
		} break;
		case 500: {
			// prism error
		} break;
		default:
			break;
		}
		ChannelsNetWorkPretestWithAlerts(reply, Arraydatas);
		//return;
	}
	formatNetworkLogs(reply, Arraydatas);

	if (Arraydatas.isEmpty()) {
		return false;
	}
	auto jsonDoc = QJsonDocument::fromJson(Arraydatas);
	auto jsonMap = jsonDoc.toVariant().toMap();

	if (isChannelItemEmpty(jsonMap)) {
		PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_channelStatus, int(Waiting));
		PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_channelUserStatus, int(Disabled));
		PLSCHANNELS_API->setValueOfChannel(mSrcUUID, g_errorString, CHANNELS_TR(initialChannelError));
		return false;
	}

	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	if (!channelInfo.isEmpty()) {
		QWriteLocker locker(&PLSCHANNELS_API->getLock());
		channelInfo[g_catogry] = getYoutubePriacyStatus(jsonMap);
		auto broadCastID = getYoutubeFirstID(jsonMap);
		channelInfo[g_broadcastID] = broadCastID;
		channelInfo[g_shareUrl] = getYoutubeShareUrl(broadCastID);
		channelInfo[g_channelStatus] = InValid;
		channelInfo[g_viewers] = "0";
		channelInfo[g_likes] = "0";
	}

	//ViewMapData(jsonMap);
	return true;
}

void YouTubeDataHanbler::getChannelsBasicInfo()
{
	auto network = PLSCHANNELS_API->getNetWorkAPI();
	if (network == nullptr) {
		return;
	}
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	if (channelInfo.isEmpty()) {
		return;
	}
	QString accessToken = getInfo(channelInfo, g_channelToken);
	QString tokenType = getInfo(channelInfo, g_tokenType);

	QVariantMap headerMap;
	headerMap[HTTP_AUTHORIZATION] = QString(tokenType).append(ONE_SPACE).append(accessToken);
	headerMap[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_VALUE;

	QVariantMap urlParamMap;
	urlParamMap[HTTP_PART] = HTTP_PART_SNPIIPT;
	urlParamMap[HTTP_MINE] = HTTP_MINE_TRUE;

	//to get user data
	requestStartLog(YOUTUBE_API_INFO, "Get");
	auto &taskMap = network->createHttpRequest(QNetworkAccessManager::GetOperation, YOUTUBE_API_INFO, true, headerMap, urlParamMap, false);
	if (taskMap.isEmpty()) {
		return;
	}
	taskMap.insert(g_handlerUUID, mHanderlID);
}

bool YouTubeDataHanbler::sendRequest()
{
	if (!getRealToken()) {
		return false;
	}
	if (!checkExpiresIn()) {
		return false;
	}
	getBroadCastInfo();
	getChannelsBasicInfo();
	return true;
}

void YouTubeDataHanbler::callback(const QVariantMap &taskData)
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	auto reply = taskData.value(REPLY_POINTER).value<ReplyPtrs>();
	if (channelInfo.isEmpty() || reply.isNull()) {
		return;
	}
	auto Arraydatas = taskData.value(BODY_DATA).toByteArray();
	QWriteLocker lokcer(&PLSCHANNELS_API->getLock());
	auto errorFlag = reply->error();
	if (errorFlag != QNetworkReply::NoError) {
		int statusCode = getReplyStatusCode(reply);
		if (statusCode == 401) {
			channelInfo[g_channelStatus] = Expired;
			return;
		}

		bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
		if (!isUpdated) {
			channelInfo[g_channelStatus] = Error;
		}

		ChannelsNetWorkPretestWithAlerts(reply, Arraydatas);
		//return;
	}

	if (Arraydatas.isEmpty()) {
		bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
		if (!isUpdated) {
			channelInfo[g_channelStatus] = Error;
		}
		return;
	}
	formatNetworkLogs(reply, Arraydatas);
	auto jsonDoc = QJsonDocument::fromJson(Arraydatas);
	auto jsonMap = jsonDoc.toVariant().toMap();
	//ViewMapData(jsonMap);
	QString channelName = getInfo(channelInfo, g_channelName, QString());
	const auto &mapper = mDataMaper[channelName];
	addToMap(channelInfo, jsonMap, mapper);
	if (getInfo(channelInfo, g_channelStatus, Error) == InValid) {
		channelInfo[g_channelStatus] = Valid;
	}
	channelInfo[g_profileThumbnailUrl] = getYoutubeImageUrl(channelInfo);
	channelInfo[g_nickName] = getYoutubeName(channelInfo);
	//ViewMapData(channelInfo);
}

void YouTubeDataHanbler::feedbackToServer()
{
	int status = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_channelStatus, InValid);
	switch (status) {
	case Expired: {
		PLSCHANNELS_API->channelExpired(mSrcUUID);
		return;
	} break;
	case UnInitialized: {
		bool isAsked = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUserAsked, false);
		if (!isAsked) {
			PLSCHANNELS_API->channelGoToInitialize(mSrcUUID);
		}
		return;
	} break;
	case Error: {
		bool isUpdated = PLSCHANNELS_API->getValueOfChannel(mSrcUUID, g_isUpdated, false);
		if (!isUpdated) {
			PLSCHANNELS_API->removeChannelInfo(mSrcUUID);
		}

		return;
	} break;
	case Waiting: {
		return;
	} break;
	default:
		break;
	}

	if (PLSCHANNELS_API->hasError()) {
		return;
	}
	QString loadUUID = createDownloadHandler(mSrcUUID);
	PLSCHANNELS_API->sigSendRequest(loadUUID);
}

void createYouTubeHandler(const QString &uuid)
{
	//PRE_LOG(youtube handler create, INFO);
	ChannelDataHandlerPtr handler(new YouTubeDataHanbler(uuid));
	handler->initData();
	PLSCHANNELS_API->addHandler(QVariant::fromValue(handler));
}

bool sendRefreshRequest(const QString &mSrcUUID)
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	auto refresh_token = getInfo(channelInfo, g_refreshToken);
	QVariantMap header = createDefaultHeaderMap();
	QVariantMap parameters;
	parameters[g_refreshToken] = refresh_token;
	parameters[HTTP_CLIENT_ID] = YOUTUBE_CLIENT_ID;
	parameters[HTTP_CLIENT_SECRET] = YOUTUBE_CLIENT_KEY;
	parameters[HTTP_GRANT_TYPE] = g_refreshToken;
	QString refresh_url = YOUTUBE_API_TOKEN;

	auto network = PLSCHANNELS_API->getNetWorkAPI();
	requestStartLog(refresh_url, "Post");
	QEventLoop loop;
	QObject::connect(convertToObejct(network), SIGNAL(replySyncData(const QString &)), &loop, SLOT(quit()), Qt::DirectConnection);
	auto &taskMap = network->createHttpRequest(QNetworkAccessManager::PostOperation, refresh_url, true, header, parameters, true);
	loop.exec();
	if (taskMap.isEmpty()) {
		return false;
	}
	TaskMapGuarder guarder(taskMap);
	taskMap.insert(g_handlerUUID, mSrcUUID);
	return refreshCallback(taskMap);
}

bool refreshCallback(const QVariantMap &taskData)
{
	auto reply = getInfo(taskData, REPLY_POINTER, ReplyPtrs());
	if (reply.isNull()) {
		return false;
	}
	auto Arraydatas = taskData.value(BODY_DATA).toByteArray();
	if (reply->error() != QNetworkReply::NoError) {
		ChannelsNetWorkPretestWithAlerts(reply, Arraydatas);
		return false;
	}

	if (Arraydatas.isEmpty()) {
		return false;
	}
	formatNetworkLogs(reply, Arraydatas);
	auto srcUUID = getInfo(taskData, g_handlerUUID);
	auto &srcInfo = PLSCHANNELS_API->getChanelInfoRef(srcUUID);
	if (Arraydatas.contains("error_description") && Arraydatas.contains("Token has been expired or revoked")) {
		srcInfo[g_channelStatus] = Expired;
		PLSCHANNELS_API->channelModified(srcUUID);
		return false;
	}

	auto jsonDoc = QJsonDocument::fromJson(Arraydatas);
	auto jsonMap = jsonDoc.toVariant().toMap();
	addToMap(srcInfo, jsonMap);
	srcInfo[g_channelStatus] = Valid;
	srcInfo[g_createTime] = QDateTime::currentDateTime();

	PLSCHANNELS_API->channelModified(srcUUID);
	return true;
}

bool isTokenValid(const QString &mSrcUUID)
{
	auto &channelInfo = PLSCHANNELS_API->getChanelInfoRef(mSrcUUID);
	auto lastTokenCreateTime = getInfo(channelInfo, g_createTime, QDateTime());
	int expiresSeconds = getInfo(channelInfo, g_expires_in, g_defaultExpiresSeconds);
	if (lastTokenCreateTime.isValid()) {
		int differ = lastTokenCreateTime.secsTo(QDateTime::currentDateTime());
		return (differ < expiresSeconds);
	}
	return false;
}

bool checkAndUpdateToken(const QString &mSrcUUID)
{
	if (isTokenValid(mSrcUUID)) {
		return true;
	}
	return sendRefreshRequest(mSrcUUID);
}

bool isReplyContainExpired(const QByteArray &body, const QStringList &keys)
{
	int matchCount = 0;
	for (const QString &key : keys) {
		if (body.contains(key.toUtf8())) {
			++matchCount;
		}
	}

	return matchCount = keys.count();
}

QJsonObject createJsonArrayFromInfo(const QString &uuid)
{
	QJsonObject obj;
	auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	if (!info.isEmpty()) {
		auto channelName = getInfo(info, g_channelName);

		channelName = channelName.remove("RTMP").remove(" ").toUpper();
		auto userID = getInfo(info, g_userID);
		auto disName = getInfo(info, g_nickName);
		auto rtmpUrl = getInfo(info, g_channelRtmpUrl);
		auto description = getInfo(info, g_otherInfo);
		auto streamKey = getInfo(info, g_streamKey);

		obj.insert("streamName", disName);
		obj.insert("rtmpUrl", rtmpUrl);
		obj.insert("streamKey", streamKey);
		obj.insert("username", userID);
		obj.insert(g_publishService, channelName);
		obj.insert("description", description);
		obj.insert("resolution", "720");
		obj.insert("bitrate", "2000");
		obj.insert("framerate", "30");
		obj.insert("interval", 2);
	}

	return obj;
};

QString &localizePlatName(QString &srcName)
{
	auto tmp = srcName;
	tmp = tmp.remove(" ").toUpper();
	const auto &rmtpNames = PLSCHANNELS_API->getRTMPsName();
	bool isFind = false;
	for (int i = 0; i < rmtpNames.size(); ++i) {
		auto name = rmtpNames[i];
		name = name.remove(" ").toUpper();
		if (name == tmp) {
			srcName = rmtpNames[i];
			isFind = true;
			break;
		}
	}
	if (!isFind) {
		srcName = CUSTOM_RTMP;
	}
	return srcName;
}
