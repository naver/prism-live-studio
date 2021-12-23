#include "PLSAPIVLive.h"
#include <qfileinfo.h>
#include <quuid.h>
#include <QNetworkCookieJar>
#include <vector>
#include "../PLSPlatformBase.hpp"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSPlatformApi.h"
#include "PLSPlatformVLive.h"
#include "PLSServerStreamHandler.hpp"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "PLSPlatformBase.hpp"

using namespace std;
const static QString strVersion = "1";
static const QString IMAGE_FILE_NAME_PREFIX = "";
const static QString vlive_app_key = "";

PLSAPIVLive::PLSAPIVLive(QObject *parent) : QObject(parent) {}

void PLSAPIVLive::vliveRequestGccAndLanguage(const QObject *receiver, dataErrorFunction onFinish)
{

}

void PLSAPIVLive::vliveRequestCountryCodes(const QObject *receiver, dataErrorFunction onFinish)
{

}

void PLSAPIVLive::vliveRequestUsersInfoAndChannel(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed)
{

}

void PLSAPIVLive::vliveRequestScheduleList(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, void *const context)
{

}

void PLSAPIVLive::vliveRequestBoardList(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, void *const context)
{
}

void PLSAPIVLive::vliveRequestBoardDetail(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed)
{

}
void PLSAPIVLive::vliveRequestProfileList(const QString &channelUuid, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, void *const context)
{
}

void PLSAPIVLive::vliveRequestStartLive(const QString &channelUuid, const QObject *receiver, dataErrorFunction onFinish)
{
}

void PLSAPIVLive::vliveRequestStartLiveToPostBoard(const QString &, const QObject *receiver, dataErrorFunction onFinish)
{
}

void PLSAPIVLive::vliveRequestStatistics(const QObject *receiver, dataErrorFunction onFinish)
{
}

void PLSAPIVLive::vliveRequestStopLive(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed)
{
}

void PLSAPIVLive::uploadImage(const QObject *receiver, const QString &imageFilePath, UploadImageCallback callback)
{
}

void PLSAPIVLive::downloadImageAsync(const QObject *receiver, const QString &imageUrl, ImageCallback callback, void *const context, const QString &prefix, bool ingoreCache)
{
}

QString PLSAPIVLive::getLocalImageFile(const QString &imageUrl, const QString &prefix)
{
	return QString();
}

QPair<bool, QString> PLSAPIVLive::downloadImageSync(const QObject *receive, const QString &url, const QString &prefix, bool ingoreCache)
{
	return {};
}

void PLSAPIVLive::addCommenQuery(PLSNetworkReplyBuilder &builder)
{
}

void PLSAPIVLive::addCommenCookie(PLSNetworkReplyBuilder &builder)
{
}

void PLSAPIVLive::addMacAddress(PLSNetworkReplyBuilder &builder)
{
}

void PLSAPIVLive::addUserAgent(PLSNetworkReplyBuilder &builder)
{
}

void PLSAPIVLive::addDebugMessage(PLSNetworkReplyBuilder &builder)
{
}

bool PLSAPIVLive::isVliveFanship()
{
	return false;
}

QString PLSAPIVLive::getVliveHost()
{
	return {};
}

QString PLSAPIVLive::getStreamName()
{
	return {};
}

void PLSAPIVLive::maskingUrlKeys(PLSNetworkReplyBuilder &builder, QNetworkReply *reply, const QStringList &keys)
{
}
