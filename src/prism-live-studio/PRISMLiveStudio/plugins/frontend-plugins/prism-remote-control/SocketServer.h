#pragma once

#include <qobject.h>

#include <mutex>
#include <map>

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>

#include <obs.h>

#include "SocketSession.h"

QT_FORWARD_DECLARE_CLASS(QTcpSocket)
QT_FORWARD_DECLARE_CLASS(QTcpServer)
QT_FORWARD_DECLARE_CLASS(QThreadPool)
QT_FORWARD_DECLARE_CLASS(EventHandler)

class SocketServer : public QObject {
	Q_OBJECT
public:
	explicit SocketServer(QObject *parent = nullptr);
	~SocketServer() noexcept(true);

	void Start();
	void Stop();

	void Broadcast(const QJsonObject &json);

	size_t ValidConnectionCount();

Q_SIGNALS:
	void closed();

private Q_SLOTS:
	void onNewConnection();
	void onAcceptError(QAbstractSocket::SocketError socketError) const;
	void onDisconnected();

private:
	/// <summary>
	/// generate a random number in range of [0, 100]
	/// </summary>
	quint16 generateNumber() const;
	static void pls_frontend_event_received(pls_frontend_event event, const QVariantList &, void *context);
	void unregisterService() const;
	void closeAllConnections();

	std::string _uuid;
	std::string _userName;
	QTcpServer *m_pSocketServer;
	std::recursive_mutex m_sessionMutex;
	quint16 m_port;
	std::map<QTcpSocket *, SessionPtr> m_sessionMap;
};
