/*
* @file		PrismApi.h
* @brief	All api should send to Prism server
* @author	wu.longyue@navercorp.com
* @date		2020-01-08
*/

#pragma once

#include <string>
#include <list>
#include <qobject.h>
#include "..\PLSPlatformBase.hpp"
#include "PLSHttpApi\PLSHttpHelper.h"

using namespace std;
class PLSHmacNetworkReplyBuilder;

class PLSPlatformPrism : public QObject {
	Q_OBJECT
public:
	static PLSPlatformPrism *instance();
	PLSPlatformPrism();

public:
	string getStreamKey() const;
	string getStreamServer() const;

	void onActive(PLSPlatformBase *, bool);
	void onInactive(PLSPlatformBase *, bool);

	void onPrepareLive(bool value);
	void onLiveStarted(bool value);
	void onPrepareFinish();
	void onLiveStopped();
	void onLiveEnded();

	int getVideoSeq() { return m_iVideoSeq; }
	std::string getCharVideoSeq();
	void setVideoSeq(int seq) { m_iVideoSeq = seq; }

	void mqttRequestRefreshToken(PLSPlatformBase *, function<void(bool)>);

	static string formatDateTime(time_t now = 0);

	void getSendThumAPIJson(QJsonObject &jsonData);

	void onTokenExpired();
	//prism refresh one accesstoken
	void requestRefrshAccessToken(PLSPlatformBase *platform, function<void(bool)> onNext);

private:
	void activateCallback(PLSPlatformBase *, bool);
	void deactivateCallback(PLSPlatformBase *, bool);

	void prepareLiveCallback(bool value);
	void liveStartedCallback(bool value);
	void prepareFinishCallback();
	void liveStoppedCallback();
	void liveEndedCallback();
	void setCommonReplyBuilderCookie(PLSHmacNetworkReplyBuilder &builder);
	void postJson(const QUrl &url, const QJsonObject &jsonObjectRoot, dataFunction onSucceed, dataErrorFunction onFailed);
	const QString urlForPath(const QString &path);

	string getPublishingTitle();

	void requestStartSimulcastLive(bool);
	void requestStopSimulcastLive(bool);
	void requestHeartbeat();
	void requestFailedCallback(const QString &url, int code, QByteArray data);

	void requestLiveDirectStart();
	void requestLiveDirectEnd();

	void requestStopSingleLive(PLSPlatformBase *platform);

	void showWarningAlertWithMsg(const QString &msg);

	void printStartLog();

	int m_iVideoSeq;
	string m_strPublishUrl;

	int m_iHistorySeq;

	QTimer m_timerHeartbeat;
};

#define PLS_PLATFORM_PRSIM PLSPlatformPrism::instance()
