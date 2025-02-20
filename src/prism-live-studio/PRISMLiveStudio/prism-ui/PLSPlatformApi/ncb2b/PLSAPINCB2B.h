#pragma once

#include <QObject>
#include <libhttp-client.h>
#include "../common/PLSAPICommon.h"
#include "PLSPlatformNCB2B.h"

namespace PLSAPINCB2B {

void configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed,
			  const PLSAPICommon::errorCallback &onFailed, const QByteArray &logName = {}, bool isNeedCookie = true);
void addCommonCookieAndUserKey(const pls::http::Request &_request);

void requestChannelList(const QObject *receiver, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			PLSAPICommon::RefreshType refreshType);
void requestScheduleList(const QObject *receiver, const QString &channelID, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			 PLSAPICommon::RefreshType refreshType);

void requestCreateLive(const QObject *receiver, const PLSNCB2BLiveinfoData &data, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
		       PLSAPICommon::RefreshType refreshType);
void requestUpdateLive(const QObject *receiver, const PLSNCB2BLiveinfoData &data, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
		       PLSAPICommon::RefreshType refreshType);

void requestGetLiveInfo(const QObject *receiver, const QString &liveId, PLSPlatformNCB2B *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			PLSAPICommon::RefreshType refreshType);

void refreshTokenBeforeRequest(PLSPlatformNCB2B *platform, PLSAPICommon::RefreshType refreshType, const std::function<void()> &originNetworkReplay, const QObject *originReceiver,
			       const PLSAPICommon::dataCallback &originOnSucceed = nullptr, const PLSAPICommon::errorCallback &originOnFailed = nullptr);

void showFailedLog(const QString &logName, const pls::http::Reply &reply, PLSPlatformNCB2B *platform);

bool getErrCode(const QByteArray &data, int &subCode, QString &message, QString &exception);
};
