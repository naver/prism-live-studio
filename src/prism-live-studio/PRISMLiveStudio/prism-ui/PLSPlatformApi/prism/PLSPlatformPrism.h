/*
* @file		PrismApi.h
* @brief	All api should send to Prism server
* @author	wu.longyue@navercorp.com
* @date		2020-01-08
*/

#pragma once

#include <string>
#include <list>
#include <memory>
#include <qobject.h>
#include "../PLSPlatformBase.hpp"
#include "libhttp-client.h"
#include "PLSDualOutputConst.h"

enum class PrismErrorPlatformType {
	NonePlatform,
	NCPPlatform,
};

using ApiErrorList = std::list<std::pair<std::list<PLSPlatformBase *>, PLSErrorHandler::RetData>>;

class PLSPlatformPrism : public QObject {
	Q_OBJECT
public:
	static PLSPlatformPrism *instance();
	PLSPlatformPrism();

	void onInactive(PLSPlatformBase *, bool);
	void stopLiveAndMqtt(PLSPlatformBase *);

	void onPrepareLive(bool value);
	void onPrepareFinish() const;
	void onLiveStopped() const;
	void onLiveEnded();

	int getVideoSeq(DualOutputType outputType) const { return m_iVideoSeq[outputType]; }
	std::string getCharVideoSeq() const;

	void bandRefreshTokenFinished(bool isRefresh, const PLSPlatformBase *platform, const QString &uuid, const std::function<void(bool)> &callback) const;

	void requestBandRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid, bool isForceUpdate = false) const;

	static std::string formatDateTime(time_t now = 0);

	void onTokenExpired(const PLSErrorHandler::RetData &retData) const;

	ApiErrorList &getApiErrorList() { return m_listApiError; }

	static QString getTableName() { return QStringLiteral("PRISM"); }
	static QString customErrorFailedToStartLive() { return QStringLiteral("FailedToStartLive"); }
	static QString customErrorTimeoutTryAgain() { return QStringLiteral("TimeoutTryAgain"); }
	static QString customErrorServerErrorTryAgain() { return QStringLiteral("ServerErrorTryAgain"); }
	static QString customErrorTempErrorTryAgain() { return QStringLiteral("TempErrorTryAgain"); }

private:
	//The address and StreamKey pushed to the Prism server
	std::string getStreamKey(DualOutputType outputType) const;
	std::string getStreamServer(DualOutputType outputType) const;

	void deactivateCallback(const PLSPlatformBase *, bool) const;

	void prepareLiveCallback(bool value);
	void prepareFinishCallback() const;
	void liveStoppedCallback() const;
	void liveEndedCallback() const;
	QString urlForPath(const QString &path) const;

	QString getPublishingTitle(std::list<PLSPlatformBase *>) const;

	void requestFailedCallback(const QString &url, int code, QByteArray data) const;

	void printStartLog() const;
	bool isAbpFlag() const;

	std::array<int, DualOutputType::All> m_iVideoSeq{};
	std::array<std::string, DualOutputType::All> m_strPublishUrl{};

	ApiErrorList m_listApiError;
};

#define PLS_PLATFORM_PRSIM PLSPlatformPrism::instance()