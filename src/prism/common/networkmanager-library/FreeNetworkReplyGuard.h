

#ifndef FREENETWORKREPLYGUARD_H
#define FREENETWORKREPLYGUARD_H

#include <QNetworkReply>

class FreeNetworkReplyGuard {
public:
	explicit FreeNetworkReplyGuard(QNetworkReply *reply) : m_reply(reply) {}

	~FreeNetworkReplyGuard()
	{
		m_reply->abort();
		m_reply->close();
		delete m_reply;
	}

private:
	QNetworkReply *m_reply;
};

#endif // FREENETWORKREPLYGUARD_H
