#ifndef CHANNELDATAHANDLER_H
#define CHANNELDATAHANDLER_H
#include <QString>
#include <QObject>
#include <QVariantMap>
#include <QSharedPointer>
#include "PLSChannelDataAPI.h"

class ChannelDataHandler {

public:
	ChannelDataHandler(const QString &uuid);
	virtual ~ChannelDataHandler();
	virtual void initData() = 0;
	virtual bool sendRequest() = 0;
	virtual void callback(const QVariantMap &retData) = 0;
	virtual void feedbackToServer() = 0;
	virtual const QString &name() const { return mHanderlID; }
	virtual void setName(const QString &newName) { mHanderlID = newName; }
	virtual void loadMaper();

protected:
	QString mHanderlID;
	QString mSrcUUID;
	static ChannelsMap mDataMaper;
};

using ChannelDataHandlerPtr = QSharedPointer<ChannelDataHandler>;
Q_DECLARE_METATYPE(ChannelDataHandlerPtr);

void createTwitchHandler(const QString &uuid);
void createYouTubeHandler(const QString &uuid);
QString createDownloadHandler(const QString &uuid);
QString createRrefreshHandler(const QString &srcChannelUUID);

using networkCallback = void (*)(const QVariantMap &);
Q_DECLARE_METATYPE(networkCallback)

bool sendRefreshRequest(const QString &mSrcUUID);
bool refreshCallback(const QVariantMap &taskData);
bool isTokenValid(const QString &mSrcUUID);
bool checkAndUpdateToken(const QString &mSrcUUID);
bool isReplyContainExpired(const QByteArray &body, const QStringList &keys);

#endif // ! CHANNELDATAHANDLER_H
