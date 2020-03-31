#ifndef PLSNOTICEHANDLER_HPP
#define PLSNOTICEHANDLER_HPP

#include <QObject>
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

	bool requestNoticeInfo();
signals:

private:
	explicit PLSNoticeHandler(QObject *parent = nullptr);
	~PLSNoticeHandler();

	void getUsedNoticeSeqs();

private:
	static uint m_currentIndex;
	QSet<int> m_usedNoticeSeqs;

};

#endif // PLSNOTICEHANDLER_HPP
