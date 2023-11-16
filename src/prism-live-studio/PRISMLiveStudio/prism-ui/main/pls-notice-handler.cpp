#include "pls-notice-handler.hpp"
#include "pls-common-define.hpp"
#include <qfile.h>
#include <qdatetime.h>
#include <qjsondocument.h>
#include <QVariantMap>
#include <qjsonarray.h>
#include "libutils-api.h"
#include "libhttp-client.h"
#include <qeventloop.h>
#include "liblog.h"

constexpr auto PLS_NOTICE_PATH = "PRISMLiveStudio/user";
constexpr auto NOTICE_NOTICE_SEQ = "noticeSeq";
constexpr auto TIMEOUT = 15000;
constexpr auto NOTICE_VAILD_UNTIL_AT = "validUntilAt";
constexpr auto PLS_LOGIN_MODULE = "launcher/login";

PLSNoticeHandler *PLSNoticeHandler::getInstance()
{
	static PLSNoticeHandler noticeHandler;
	return &noticeHandler;
}

PLSNoticeHandler::PLSNoticeHandler(QObject *parent) : QObject(parent) {}

void PLSNoticeHandler::getNoticeInfoFromRemote(const QObject *obj, const QString &url, const QString &hmac, const std::function<void(const QVariantMap &noticeInfo)> &noticeCallback)
{
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)          //
				   .hmacUrl(url, hmac.toUtf8().constData()) //
				   .jsonContentType()
				   .rawHeader("gcc", pls_prism_get_locale().section('-', 1, 1))
				   .rawHeader("X-prism-apptype", QString("LIVE_STUDIO"))
				   .withLog()
				   .workInMainThread()
				   .receiver(obj)
				   .timeout(TIMEOUT)
				   .okResult([obj, this, noticeCallback](const pls::http::Reply &reply) {
					   PLS_INFO(PLS_LOGIN_MODULE, "get notice info success.");
					   saveNoticeInfo(reply.data());
					   pls_async_call_mt(obj, [noticeCallback]() { noticeCallback(PLSNoticeHandler::getInstance()->getNewNotice()); });
				   })
				   .failResult([obj, noticeCallback](const pls::http::Reply &reply) {
					   if (reply.statusCode() == 404) {
						   PLS_INFO(PLS_LOGIN_MODULE, "get notice is empty.");

					   } else {
						   PLS_ERROR(PLS_LOGIN_MODULE, "get notice info failed.");
					   }
					   pls_async_call_mt(obj, [noticeCallback]() { noticeCallback(PLSNoticeHandler::getInstance()->getNewNotice()); });
				   }));
}

void PLSNoticeHandler::saveNoticeInfo(const QByteArray &noticeData)
{
	for (auto item : QJsonDocument::fromJson(noticeData).array().toVariantList()) {
		QVariantMap notice = item.value<QVariantMap>();
		m_notices.insert(notice.value(NOTICE_NOTICE_SEQ).toInt(), notice);
	}
}

void PLSNoticeHandler::getUsedNoticeSeqs()
{
	QFile file(pls_get_app_data_dir(PLS_NOTICE_PATH) + "/notice.bin");
	if (file.open(QFile::ReadOnly)) {
		QByteArray array = file.readAll();
		for (auto seq : QStringList(QString(array).split(' '))) {
			if (!seq.isEmpty()) {
				m_usedNoticeSeqs.insert(seq.toInt());
			}
		}
		file.close();
	}
}

QVariantMap PLSNoticeHandler::getNewNotice()
{
	QString saveingSeq;
	QVariantMap currentNoticeMap;
	//get used seq into QVector container
	getUsedNoticeSeqs();
	for (auto index = m_notices.size(); index > 0; --index) {
		int lastKey = m_notices.keys().value(index - 1);
		if (m_usedNoticeSeqs.find(lastKey) == m_usedNoticeSeqs.end()) {
			long long currentDataTime = QDateTime::currentMSecsSinceEpoch();
			QVariant vaildUntilAt = m_notices.value(lastKey).value(NOTICE_VAILD_UNTIL_AT);
			if (!vaildUntilAt.isValid() || vaildUntilAt.toLongLong() >= currentDataTime) {
				saveingSeq += " " + QString::number(lastKey);
				currentNoticeMap = m_notices.value(lastKey);
				break;
			} else {
				//save current seq to file
				saveingSeq += " " + QString::number(lastKey);
				continue;
			}
		}
	}

	QFile file(pls_get_app_data_dir(PLS_NOTICE_PATH) + "/notice.bin");
	if (file.open(QFile::WriteOnly | QIODevice::Append)) {
		file.write(saveingSeq.toUtf8());
		file.close();
	}
	return currentNoticeMap;
}
