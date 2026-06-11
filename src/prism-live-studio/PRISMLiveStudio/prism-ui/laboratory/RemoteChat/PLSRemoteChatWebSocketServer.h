#ifndef PLSREMOTECHATWEBSOCKETSERVER_H
#define PLSREMOTECHATWEBSOCKETSERVER_H

#include <QObject>
#include <QtWebSockets/qwebsocketserver.h>
#include <QtWebSockets/qwebsocket.h>
#include <QJsonObject>
#include <QJsonArray>

class PLSRemoteChatWebSocketServer : public QObject {

	Q_OBJECT
public:
	explicit PLSRemoteChatWebSocketServer(QString serverName, QObject *parent = nullptr);
	~PLSRemoteChatWebSocketServer() override;
	void sendData(const QString &message) const;
	void sendGroupMemberInfo() const;
	void sendCommonTermsInfo() const;

signals:
	void signal_conncted(QString ip, qint32 port);
	void signal_disconncted(QString ip, qint32 port);
	void signal_error(QString ip, quint32 port, QString errorString);
	void signal_close();
	void signal_textMessageReceived(QString ip, quint32 port, QString message);

public:
	bool running() const;
	bool startWebsocketServer();
	bool stopWebsocketServer();
	qint32 getListenPort() const;
	QString getListenHostHost() const;

protected slots:
	void slot_newConnection();
	void slot_serverError(QWebSocketProtocol::CloseCode closeCode);
	void slot_closed();
	void slot_disconnected();
	void slot_error(QAbstractSocket::SocketError error);
	void slot_textMessageReceived(const QString &message);

private:
	QString _serverName;
	QWebSocketServer *_pWebSocketServer{nullptr};
	bool _running{false};
	QString _listenHostHost;
	qint32 _listenPort{0};
	QList<QWebSocket *> m_clients;
	QMap<QWebSocket *, QJsonObject> m_userClientList;
	QJsonObject m_commonTermObject;
};

#endif // PLSREMOTECHATWEBSOCKETSERVER_H
