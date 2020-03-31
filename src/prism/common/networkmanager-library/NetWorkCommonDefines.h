
#ifndef NETWORKCOMMONDEFINES_H
#define NETWORKCOMMONDEFINES_H

#include <QMetaType>
#include <QVariantMap>
#include <QSharedPointer>
#include <QNetworkReply>

/********other *****************/
const QString UUID = "uuid";
const char UUID_C[] = "uuid";
const QString REPLY_POINTER = "replyPointer";
const QString BODY_DATA = "bodyData";
const QString STATUS_CODE = "statusCode";

using TasksMap = QMap<QString, QVariantMap>;
class QNetworkReply;
using ReplyPtrs = QSharedPointer<QNetworkReply>;
Q_DECLARE_METATYPE(ReplyPtrs)

template<typename QtObj> void deleteQtObject(QtObj *taget)
{
	taget->deleteLater();
}

#endif // !NETWORKCOMMONDEFINES_H
