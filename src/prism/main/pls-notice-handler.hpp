#ifndef PLSNOTICEHANDLER_HPP
#define PLSNOTICEHANDLER_HPP

#include <QObject>
#include "login-web-handler.hpp"
#include <qset.h>

class PLSNoticeHandler : public QObject {
	Q_OBJECT

	struct NoticeInfo {
		int noticeSeq;
		QString countryCode;
		QString os;
		QString osVersion;
		QString appVersion;
		QString title;
		QString content;
		QString detailLink;
		QString contentType;
		QString createdTime;
		QString invaildTime;
	};

public:
	static PLSNoticeHandler *getInstance();
	QVariantMap getNewNotice();
	void saveNoticeInfo(const QByteArray &noticeData);
	QString getLocale();

signals:

private:
	explicit PLSNoticeHandler(QObject *parent = nullptr);
	~PLSNoticeHandler();

	void getUsedNoticeSeqs();

private:
	static uint m_currentIndex;
	QMap<int, QVariantMap> m_notices;
	QSet<int> m_usedNoticeSeqs;
	LoginWebHandler m_loginWebHanler;
};

#endif // PLSNOTICEHANDLER_HPP
