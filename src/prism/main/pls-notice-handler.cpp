#include "pls-notice-handler.hpp"
#include "pls-net-url.hpp"
#include "pls-app.hpp"
#include "network-access-manager.hpp"
#include "pls-common-define.hpp"
#include "json-data-handler.hpp"
#include <qfile.h>
#include <qdatetime.h>
#define PLS_NOTICE_PATH "PRISMLiveStudio/user/notice.bin"
PLSNoticeHandler *PLSNoticeHandler::getInstance()
{
	static PLSNoticeHandler noticeHandler;
	return &noticeHandler;
}

PLSNoticeHandler::PLSNoticeHandler(QObject *parent) : QObject(parent) {}

PLSNoticeHandler::~PLSNoticeHandler() {}

QString PLSNoticeHandler::getLocale()
{
	const char *lang = App()->GetLocale();
	QString currentLangure(lang);
	return currentLangure.split('-').last();
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
	QFile file(pls_get_user_path(PLS_NOTICE_PATH));
	if (file.open(QFile::ReadOnly | QFile::Truncate)) {
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

	QFile file(pls_get_user_path(PLS_NOTICE_PATH));
	if (file.open(QFile::WriteOnly | QIODevice::Append)) {
		file.write(saveingSeq.toUtf8());
		file.close();
	}
	return currentNoticeMap;
}
