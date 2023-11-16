#include "PLSRemoteChatWebSocketServer.h"
#include "log/log.h"
#include <QtNetwork/qnetworkinterface.h>
#include "PLSRemoteChatManage.h"
#include "utils-api.h"

constexpr auto Local_User_Add_Type = "localUserAdd";
constexpr auto Local_User_Data_Type = "localUserData";
constexpr auto Storage_Update_Data_Type = "storageUpdate";
constexpr auto Storage_Data_Type = "storageData";

PLSRemoteChatWebSocketServer::PLSRemoteChatWebSocketServer(QString serverName, QObject *parent) : QObject(parent), _serverName(serverName)
{
	RC_LOG("PLSRemoteChatWebSocketServer websocket server init");
	PLSRemoteChatManage::instance()->readCommonTermJson(m_commonTermObject);
}

bool PLSRemoteChatWebSocketServer::running() const
{
	return _running;
}

PLSRemoteChatWebSocketServer::~PLSRemoteChatWebSocketServer()
{
	RC_LOG("PLSRemoteChatWebSocketServer websocket server released");
}

void PLSRemoteChatWebSocketServer::sendData(const QString &message) const
{
	for (QWebSocket *socket : m_clients) {
		socket->sendTextMessage(message);
		RC_KR_LOG(QString("websocket server send data is") + message + QString("host is %1 , port is %2").arg(socket->peerAddress().toString()).arg(socket->peerPort()));
	}
}

void PLSRemoteChatWebSocketServer::sendGroupMemberInfo() const
{
	QJsonObject jsonObject;
	QJsonArray jsonArray;
	for (auto key : m_userClientList.keys()) {
		jsonArray.append(m_userClientList.value(key));
	}
	jsonObject.insert(g_remoteChatType, Local_User_Data_Type);
	jsonObject.insert("data", jsonArray);
	RC_LOG("websocket server send group member info");
	PLSRemoteChatManage::instance()->sendWebSocketEvent(jsonObject);
}

void PLSRemoteChatWebSocketServer::sendCommonTermsInfo() const
{
	QJsonObject jsonObject;
	jsonObject.insert(g_remoteChatType, Storage_Data_Type);
	jsonObject.insert("data", m_commonTermObject);
	RC_LOG("websocket server send common term info");
	PLSRemoteChatManage::instance()->sendWebSocketEvent(jsonObject);
}

bool PLSRemoteChatWebSocketServer::startWebsocketServer()
{
	if (_running) {
		RC_LOG("start websocket server failed, because it's already running");
		return false;
	}

	if (!_pWebSocketServer) {
		_pWebSocketServer = pls_new<QWebSocketServer>(_serverName, QWebSocketServer::NonSecureMode, this);
		connect(_pWebSocketServer, SIGNAL(newConnection()), this, SLOT(slot_newConnection()));
		connect(_pWebSocketServer, SIGNAL(closed()), this, SLOT(slot_closed()));
		connect(_pWebSocketServer, SIGNAL(serverError(QWebSocketProtocol::CloseCode)), this, SLOT(slot_serverError(QWebSocketProtocol::CloseCode)));
		RC_KR_LOG(QString("create websocket server object, server name is %1").arg(_serverName));
		RC_LOG("create websocket server object");
	}

	QHostAddress ipAddress;
	QList<QHostAddress> ipAddressList = QNetworkInterface::allAddresses();
	for (int i = 0; i < ipAddressList.size(); i++) {
		if (ipAddressList.at(i) != QHostAddress::LocalHost && ipAddressList.at(i).toIPv4Address()) {
			ipAddress = ipAddressList.at(i);
			break;
		}
	}

	if (ipAddressList.isEmpty()) {
		RC_LOG("create websocket server failed, because ip address is empty");
		return false;
	}

	if (_pWebSocketServer->listen(ipAddress)) {
		QUrl url(_pWebSocketServer->serverUrl());
		_listenHostHost = url.host();
		_listenPort = url.port();
		_running = true;
		RC_KR_LOG(QString("websocket server listen success, listen info { host:%1 , port:%2 }").arg(_listenHostHost).arg(_listenPort));
		RC_LOG("websocket server listen success");
		return true;
	}

	RC_LOG("websocket server listen failed");

	return false;
}

bool PLSRemoteChatWebSocketServer::stopWebsocketServer()
{
	if (!_running) {
		RC_LOG("stop websocket server failed, because it's not running");
		return false;
	}
	RC_LOG("call PLSRemoteChatWebSocketServer close websocket server method");
	_running = false;
	_pWebSocketServer->close();
	return true;
}

qint32 PLSRemoteChatWebSocketServer::getListenPort() const
{
	return _listenPort;
}

QString PLSRemoteChatWebSocketServer::getListenHostHost() const
{
	return _listenHostHost;
}

void PLSRemoteChatWebSocketServer::slot_newConnection()
{
	QWebSocket *pWebSocket = _pWebSocketServer->nextPendingConnection();
	connect(pWebSocket, SIGNAL(disconnected()), this, SLOT(slot_disconnected()));
	connect(pWebSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(slot_error(QAbstractSocket::SocketError)));
	connect(pWebSocket, SIGNAL(textMessageReceived(QString)), this, SLOT(slot_textMessageReceived(QString)));
	RC_KR_LOG(QString("websocket server receive new client connection, client info { host:%1 , port:%2 }").arg(pWebSocket->peerAddress().toString()).arg(pWebSocket->peerPort()));
	RC_LOG("websocket server receive new client connection");
	QJsonObject jsonObject;
	jsonObject.insert(g_remoteChatType, Storage_Data_Type);
	jsonObject.insert("data", m_commonTermObject);
	QString message = QJsonDocument(jsonObject).toJson().constData();
	RC_LOG("websocket server receive new client connection , send common term message");
	pWebSocket->sendTextMessage(message);
	m_clients.append(pWebSocket);
	emit signal_conncted(pWebSocket->peerAddress().toString(), pWebSocket->peerPort());
}

void PLSRemoteChatWebSocketServer::slot_serverError(QWebSocketProtocol::CloseCode)
{
	//This signal is emitted when an error occurs during the setup of a WebSocket connection. The closeCode parameter describes the type of error that occurred
	auto pWebSocket = dynamic_cast<QWebSocket *>(sender());
	if (!pWebSocket) {
		return;
	}
	RC_KR_LOG(QString("websocket server receive error %1 , host is %2 , port is %3 ").arg(_pWebSocketServer->errorString()).arg(pWebSocket->peerAddress().toString()).arg(pWebSocket->peerPort()));
	RC_LOG(QString("websocket server receive error %1").arg(_pWebSocketServer->errorString()));
	emit signal_error(pWebSocket->peerAddress().toString(), pWebSocket->peerPort(), _pWebSocketServer->errorString());
}

void PLSRemoteChatWebSocketServer::slot_closed()
{
	RC_LOG("websocket server receive closed message");
	for (int index = 0; index < m_clients.size(); index++) {
		m_clients.at(index)->close();
	}
	m_clients.clear();
	m_userClientList.clear();
	emit signal_close();
}

void PLSRemoteChatWebSocketServer::slot_disconnected()
{
	auto pWebSocket = dynamic_cast<QWebSocket *>(sender());
	if (!pWebSocket) {
		return;
	}
	RC_LOG("client websocket disconnected");
	RC_KR_LOG(QString("client websocket disconnected , host is %1 , port is %2").arg(pWebSocket->peerAddress().toString()).arg(pWebSocket->peerPort()));
	m_clients.removeOne(pWebSocket);
	m_userClientList.remove(pWebSocket);
	sendGroupMemberInfo();
	pWebSocket->deleteLater();
	emit signal_disconncted(pWebSocket->peerAddress().toString(), pWebSocket->peerPort());
}

void PLSRemoteChatWebSocketServer::slot_error(QAbstractSocket::SocketError error)
{
	auto pWebSocket = dynamic_cast<QWebSocket *>(sender());
	if (!pWebSocket) {
		return;
	}
	RC_LOG(QString("client websocket receive error , socket error string is %1 , socket error code is %2 ").arg(pWebSocket->errorString()).arg(error));
	RC_KR_LOG(QString("client websocket receive error , socket error string is %1 , socket error code is %2 , host is %3 , port is %4")
			  .arg(pWebSocket->errorString())
			  .arg(error)
			  .arg(pWebSocket->peerAddress().toString())
			  .arg(pWebSocket->peerPort()));
	emit signal_error(pWebSocket->peerAddress().toString(), pWebSocket->peerPort(), pWebSocket->errorString());
}

void PLSRemoteChatWebSocketServer::slot_textMessageReceived(const QString &message)
{
	auto pWebSocket = dynamic_cast<QWebSocket *>(sender());
	if (!pWebSocket) {
		return;
	}
	QJsonObject jsonObject = PLSRemoteChatManage::instance()->stringToJson(message);
	QString type = jsonObject.value(g_remoteChatType).toString();
	QJsonObject dataJsonObject = jsonObject.value("data").toObject();
	if (type == Local_User_Add_Type) {
		m_userClientList.insert(pWebSocket, dataJsonObject);
		sendGroupMemberInfo();
	} else if (type == Storage_Update_Data_Type) {
		m_commonTermObject = dataJsonObject;
		PLSRemoteChatManage::instance()->saveCommonTermJson(m_commonTermObject);
		sendCommonTermsInfo();
	}
	RC_KR_LOG(QString("websocket server receive client send text is ") + message + QString("host is %1 , port is %2").arg(pWebSocket->peerAddress().toString()).arg(pWebSocket->peerPort()));
	RC_LOG("websocket server receive client send text");
	emit signal_textMessageReceived(pWebSocket->peerAddress().toString(), pWebSocket->peerPort(), message);
}
