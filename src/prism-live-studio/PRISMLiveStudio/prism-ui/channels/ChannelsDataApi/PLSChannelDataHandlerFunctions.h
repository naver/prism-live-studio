#ifndef CHANNELDATAHANDLER_FUNCTIONS_H
#define CHANNELDATAHANDLER_FUNCTIONS_H
#include <qnetworkcookie.h>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVariantMap>

void registerAllPlatforms();

QString getYoutubeImageUrl(const QVariantMap &src);
QString getYoutubeName(const QVariantMap &src);
QString getYoutubeFirstID(const QVariantMap &src);
bool isItemsExists(const QVariantMap &src);
bool isChannelItemEmpty(const QVariantMap &src);
QString getYoutubePriacyStatus(const QVariantMap &src);

bool RTMPAddToPrism(const QString &uuid);
bool AddOrgDataToNewApi(const QString &uuid, bool bAddFlag);
bool RTMPUpdateToPrism(const QString &uuid);
bool RTMPDeleteToPrism(const QString &uuid);

QNetworkCookie createPrismCookie();

void updateAllRtmps();
void updateAllRtmpsV1();
void endRefresh();
void updateRTMPCallback(const QByteArray &retData, bool bNewAPIData);

bool isTokenValid(const QString &mSrcUUID);
bool isTokenValid(const QVariantMap &mSrc);

QJsonObject createJsonArrayFromInfo(const QString &uuid);

QVariantMap createPrismHeader();

#endif // ! CHANNELDATAHANDLER_FUNCTIONS_H
