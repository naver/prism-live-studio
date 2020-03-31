
#ifndef FREENETWORKREPLYGUARD_H
#define FREENETWORKREPLYGUARD_H

#include <QNetworkReply>
#include <QTimer>
#include <QHttpMultiPart>

class FreeNetworkReplyGuard {
public:
	explicit FreeNetworkReplyGuard(QNetworkReply *reply, QTimer *t, QHttpMultiPart *&multilPart) : m_reply(reply), m_timer(t), part(multilPart) {}

	~FreeNetworkReplyGuard()
	{
		m_reply->abort();
		m_reply->close();
		m_reply->deleteLater();
		m_timer->deleteLater();
		part = nullptr;
	}

private:
	QNetworkReply *m_reply;
	QTimer *m_timer;
	QHttpMultiPart *&part;
};

#endif // FREENETWORKREPLYGUARD_H
