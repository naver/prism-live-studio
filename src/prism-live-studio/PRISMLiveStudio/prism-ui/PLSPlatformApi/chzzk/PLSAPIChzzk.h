#pragma once

#include <QObject>
#include <libhttp-client.h>
#include "../common/PLSAPICommon.h"
#include "PLSPlatformChzzk.h"

namespace PLSAPIChzzk {

void configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed,
			  const PLSAPICommon::errorCallback &onFailed, const QByteArray &logName = {}, bool isSetContentType = true);
void addCommonCookieAndUserKey(const pls::http::Request &_request);

void requestChannelList(const QObject *receiver, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			PLSAPICommon::RefreshType refreshType);

void requestChannelOrLiveInfo(const QObject *receiver, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			      PLSAPICommon::RefreshType refreshType);
void requestUpdateChannelOrLiveInfo(const QObject *receiver, const PLSChzzkLiveinfoData &data, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed,
				    const PLSAPICommon::errorCallback &onFailed, PLSAPICommon::RefreshType refreshType);

void requestSearchCategory(const QObject *receiver, const QString &keyword, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
			   PLSAPICommon::RefreshType refreshType);

void requestCreateLive(const QObject *receiver, const PLSChzzkLiveinfoData &data, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
		       PLSAPICommon::RefreshType refreshType);
void uploadImage(const QObject *receiver, const PLSChzzkLiveinfoData &data, const QString &imageFilePath, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed,
		 const PLSAPICommon::errorCallback &onFailed);

void deleteImage(const QObject *receiver, const PLSChzzkLiveinfoData &data, PLSPlatformChzzk *platform, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);

void refreshTokenBeforeRequest(PLSPlatformChzzk *platform, PLSAPICommon::RefreshType refreshType, const std::function<void()> &originNetworkReplay, const QObject *originReceiver,
			       const PLSAPICommon::dataCallback &originOnSucceed = nullptr, const PLSAPICommon::errorCallback &originOnFailed = nullptr);

void showFailedLog(const QString &logName, const pls::http::Reply &reply, PLSPlatformChzzk *platform);

bool getErrCode(const QByteArray &data, int &subCode, QString &message, QString &exception);
};
