#pragma once

#include <QObject>
#include "PLSPlatformYoutube.h"
#include "PLSAPICommon.h"

class PLSAPIYoutube : public QObject {
	Q_OBJECT
public:
	using UploadImageCallback = std::function<void(PLSPlatformApiResult result, const QString &imageUrl)>;

	explicit PLSAPIYoutube(QObject *parent = nullptr);

	//when live status is ready, then call test start api.
	static void requestTestLive(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);
	static void configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					 const QByteArray &logName = {}, bool isSetContentType = true);

	static void addCommonCookieAndUserKey(const pls::http::Request &_request);
	//get the live is stooped by remote.
	static void requestLiveBroadcastStatus(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					       PLSAPICommon::RefreshType refreshType);
	//get the viewer count by this live.
	static void requestVideoStatus(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);

	//below three api is to creat a new schedule.
	static void requestLiveBroadcastsInsert(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
						PLSAPICommon::RefreshType refreshType);
	static void requestLiveStreamsInsert(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);
	static void requestLiveBroadcastsBindOrUnbind(const QObject *receiver, const PLSYoutubeLiveinfoData &data, bool isBind, const PLSAPICommon::dataCallback &onSucceed,
						      const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);

	static void requestDeleteStream(const QObject *receiver, const QString &deleteID, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					PLSAPICommon::RefreshType refreshType);

	//to change schedule live to start and stop live by auto.
	static void requestLiveBroadcastsUpdate(const QObject *receiver, const PLSYoutubeStart &startData, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
						PLSAPICommon::RefreshType refreshType);

	static void requestCurrentSelectData(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);

	static void requestStopLive(const QObject *receiver, const std::function<void()> &onNext);

	static void requestCategoryID(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType,
				      const QString &searchID);
	static void requestCategoryList(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);
	static void requestScheduleList(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);
	static void requestLiveStream(const QStringList &ids, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
				      PLSAPICommon::RefreshType refreshType, const QString &queryKeys, const QByteArray &logName = "requestLiveStream");
	static void requestUpdateVideoData(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType,
					   const PLSYoutubeLiveinfoData &data);

	static void refreshYoutubeTokenBeforeRequest(PLSAPICommon::RefreshType refreshType, const std::function<void()> &originNetworkReplay, const QObject *originReceiver,
						     const PLSAPICommon::dataCallback &originOnSucceed = nullptr, const PLSAPICommon::errorCallback &originOnFailed = nullptr);

	static bool dealUploadImageSucceed(const QByteArray &data, QString &imgUrl);
	static void uploadImage(const QObject *receiver, const QString &imageFilePath, const PLSAPICommon::uploadImageCallback &callback, const PLSAPICommon::errorCallback &onFailed);

	static void setLatency(QJsonObject &object, PLSYoutubeLiveinfoData::Latency latency);
	static void getLatency(const QJsonObject &object, PLSYoutubeLiveinfoData::Latency &latency);

	static void showFailedLog(const QString &logName, const pls::http::Reply &reply);
};
