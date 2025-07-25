#include "SocketServer.h"

#include <stdlib.h>
#include <time.h>
#include <string>
#include <random>

#include <QThreadPool>
#include <qtcpserver.h>
#include <qtcpsocket.h>
#include <quuid.h>
#include <qsysinfo.h>
#include <qcoreapplication.h>

#include <frontend-api.h>
//#include "pls-app.hpp"

#include "RemoteControlDefine.h"
#include "SocketSession.h"
#include "EventHandler.h"
#include "LogHelper.h"
#include "Utils.h"

using namespace rc;

// Dynamicand/orPrivatePorts [49152,65535]
const uint32_t RC_SERVICE_BASE_PORT = 49152;
const int RC_LISTEN_TRY_COUNT = 2;

SocketServer::SocketServer(QObject *parent) : QObject(parent), m_pSocketServer(pls_new<QTcpServer>(this))
{
	_uuid = QUuid::createUuid().toString().toStdString();
	std::string name = QSysInfo::machineHostName().toStdString();
	_userName = name;

	// clear
	pls_set_remote_control_client_info("", false);
	pls_set_remote_control_server_info(0);

	pls_frontend_add_event_callback(SocketServer::pls_frontend_event_received, this);
}

SocketServer::~SocketServer() noexcept(true)
{
	pls_frontend_remove_event_callback(SocketServer::pls_frontend_event_received, this);

	m_pSocketServer->close();
	m_pSocketServer->deleteLater();

	EventHandler::getInstance()->SetServer(nullptr);
	EventHandler::getInstance()->UnLoad();
}

void SocketServer::pls_frontend_event_received(pls_frontend_event event, const QVariantList &params, void *context)
{
	UNUSED_PARAMETER(params);

	auto session = (SocketServer *)context;
	if (!session)
		return;
	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_REMOTE_CONTROL_CLICK_CLOSE_CONNECT:
		session->closeAllConnections();
		break;

	default:
		break;
	}
}

void SocketServer::Start()
{
	LogHelper::getInstance()->writeMessage("", "SocketServer start...", false);

	if (m_pSocketServer->isListening() && EventHandler::getInstance()->IsLoaded()) {
		assert(m_port >= RC_SERVICE_BASE_PORT);
		return;
	}

	int tryCount = 0;
	qint16 newPort = generateNumber() + RC_SERVICE_BASE_PORT;
	while (!m_pSocketServer->isListening() && tryCount < RC_LISTEN_TRY_COUNT) {
		tryCount++;

		RC_LOG_INFO("Remote Control trying to listen Port: %d, retried count: %d", newPort, tryCount);

		if (!m_pSocketServer->listen(QHostAddress::Any, newPort)) {
			newPort = generateNumber() + RC_SERVICE_BASE_PORT;
			continue;
		}

		m_port = newPort;

		connect(m_pSocketServer, &QTcpServer::newConnection, this, &SocketServer::onNewConnection);
		connect(m_pSocketServer, &QTcpServer::acceptError, this, &SocketServer::onAcceptError);

		RC_LOG_INFO("Remote Control listens OK. Port: %d", newPort);

		pls_set_remote_control_server_info(newPort);

		ServiceRecord record{};
		record.id = _uuid.c_str();
		record.name = _userName.c_str();
		record.deviceType = "PC";
		record.connectType = "RemoteControl";
		record.version = CURRENT_API_VERSION;
#if _WIN32
		pls_register_mdns_service(_uuid.c_str(), m_port, record);
#else
		pls_register_mdns_service_ex(_uuid.c_str(), m_port, record, true, this, [](void *context, bool success) {

		});
#endif

		break;
	}

	if (!EventHandler::getInstance()->IsLoaded()) {
		EventHandler::getInstance()->Load();
		EventHandler::getInstance()->SetServer(this);
		RC_LOG_INFO("Remote Control EventHandler load OK");
	}
}

void SocketServer::Stop()
{
	RC_LOG_INFO("Remote Control Stopped");

	closeAllConnections();

	EventHandler::getInstance()->SetServer(nullptr);
	m_pSocketServer->close();
	EventHandler::getInstance()->UnLoad();

	unregisterService();
}

void SocketServer::Broadcast(const QJsonObject &json)
{
	std::unique_lock lock(m_sessionMutex);
	/// QT Socket sometime will trigger `close` when call `flush`.
	/// Add below line in order to avoid recursive calling.
	auto sessionMap = m_sessionMap;
	lock.unlock();

	for (auto it = sessionMap.begin(); it != sessionMap.end(); it++) {
		if (it->second->acceptedConnect()) {
			it->second->SendRequest(QJsonDocument(json).toJson(QJsonDocument::Compact));
		}
	}
}

size_t SocketServer::ValidConnectionCount()
{
	size_t count = 0;
	std::unique_lock lock(m_sessionMutex);
	for (auto iter = m_sessionMap.begin(); iter != m_sessionMap.end(); ++iter)
		if (iter->second->GetDisconnectReason() == DisconnectReason::none && iter->second->acceptedConnect())
			count++;
	lock.unlock();

	return count;
}

void SocketServer::onNewConnection()
{
	auto pSocket = m_pSocketServer->nextPendingConnection();
	if (!pSocket)
		return;

	RC_LOG_INFO("new connecton. local port: %d. peer port: %d", pSocket->localPort(), pSocket->peerPort());

	size_t count = ValidConnectionCount();
	connect(pSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

	auto session = std::make_shared<SocketSession>(pSocket, std::to_string((long long)pSocket));
	if (count > 0) {
		session->SetDisconnectReason(DisconnectReason::connectExists);
		RC_LOG_INFO("There have been other connect. Refuse this connection");
	}

	std::unique_lock lock(m_sessionMutex);
	m_sessionMap[pSocket] = session;
	lock.unlock();
}

void SocketServer::onAcceptError(QAbstractSocket::SocketError socketError) const
{
	auto server = qobject_cast<QTcpServer *>(sender());
	if (!server) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Socket error", QString("Socket error: %1").arg(QString::number(socketError)));
	} else {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Socket error", QString("Socket error: %1, info: %2").arg(QString::number(socketError), server->errorString()));
	}
}

void SocketServer::onDisconnected()
{
	auto pClient = qobject_cast<QTcpSocket *>(sender());
	if (!pClient)
		return;

	QString name;
	bool connectedBefore = false;
	int connectedCount = 0;
	RC_LOG_INFO("Socket %lld disconnected with error=%ld", (long long)pClient, (long)pClient->error());
	std::unique_lock lock(m_sessionMutex);
	auto iter = m_sessionMap.find(pClient);
	if (iter != m_sessionMap.end()) {
		name = iter->second->GetName();
		auto reason = iter->second->GetDisconnectReason();
		RC_LOG_INFO("Name:%s has disconnected, by: %d", name.toStdString().c_str(), (int)reason);

		connectedBefore = iter->second->GetConnectedBefore();

		if (!pls_get_app_exiting()) {
			QMetaObject::invokeMethod(
				qApp,
				[reason, name, connectedBefore]() {
					if (pls_get_app_exiting())
						return;
					pls_sys_tray_notify(networkError(reason, connectedBefore), QSystemTrayIcon::MessageIcon::Warning);
				},
				Qt::QueuedConnection);
		}

		iter->second->blockSignals(true);
		iter->second->OnDisconnect();

		QPointer<SocketServer> serverPointer = this;
		QPointer<QTcpSocket> clientPointer = pClient;
		QMetaObject::invokeMethod(
			qApp,
			[serverPointer, this, clientPointer]() {
				if (serverPointer.isNull() || !serverPointer || clientPointer.isNull() || !clientPointer) {
					return;
				}
				std::lock_guard lock(m_sessionMutex);
				m_sessionMap.erase(clientPointer);
				clientPointer->deleteLater();
			},
			Qt::QueuedConnection);
	}

	for (auto iter_session = m_sessionMap.begin(); iter_session != m_sessionMap.end(); ++iter_session) {
		if (iter_session->second->acceptedConnect())
			connectedCount += 1;
	}

	lock.unlock();

	if (connectedCount <= 0)
		pls_set_remote_control_client_info(name, false);
}

void SocketServer::unregisterService() const
{
	RC_LOG_INFO("Remote Control unregisterService");

	ServiceRecord record{};
	record.id = _uuid.c_str();
	record.name = _userName.c_str();
	record.deviceType = "PC";
	record.connectType = "RemoteControl";
	record.version = CURRENT_API_VERSION;

#if _WIN32
	pls_register_mdns_service(_uuid.c_str(), m_port, record, false);
#else
	pls_register_mdns_service_ex(_uuid.c_str(), m_port, record, false, nullptr, [](void *context, bool success) {

	});
#endif
}

void SocketServer::closeAllConnections()
{
	RC_LOG_INFO("User click close connect");

	std::vector<SessionPtr> sessions;
	std::unique_lock lock(m_sessionMutex);
	for (auto iter = m_sessionMap.begin(); iter != m_sessionMap.end(); ++iter) {
		sessions.push_back(iter->second);
		iter->second->SetDisconnectReason(DisconnectReason::closeByPcUser);
	}
	lock.unlock();

	for (auto iter = sessions.begin(); iter != sessions.end(); iter++) {
		QJsonObject json;
		json["commandType"] = "disconnect";
		json["command"] = QJsonObject();
		(*iter)->SendRequest(QJsonDocument(json).toJson(QJsonDocument::Compact));
		(*iter)->Disconnect();
	}
}

quint16 SocketServer::generateNumber() const
{
	std::minstd_rand simple_rand;
	simple_rand.seed((unsigned int)time(nullptr));
	return simple_rand() % 15000;
}
