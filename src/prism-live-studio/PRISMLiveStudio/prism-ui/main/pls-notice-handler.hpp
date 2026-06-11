#ifndef PLSNOTICEHANDLER_HPP
#define PLSNOTICEHANDLER_HPP

#include <QObject>
#include <qset.h>
#include <qmap.h>

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
	void getNoticeInfoFromRemote(const QObject *obj, const QString &url, const QString &hmac, const std::function<void(const QVariantMap &noticeInfo)> &noticeCallback);
	void saveNoticeInfo(const QByteArray &noticeData);

private:
	explicit PLSNoticeHandler(QObject *parent = nullptr);
	~PLSNoticeHandler() override = default;

	void getUsedNoticeSeqs();

	static uint m_currentIndex;
	QMap<int, QVariantMap> m_notices;
	QSet<int> m_usedNoticeSeqs;
};

#endif // PLSNOTICEHANDLER_HPP
