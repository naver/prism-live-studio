#include "region-capture-service.h"
#include "region-capture.h"
#include "libutils-api.h"
#include <QApplication>
#include <QEvent>
#include <QThread>
#include <QTimer>
#include <iostream>

RegionCaptureService::RegionCaptureService(const QString &pipeName, QObject *parent) : QObject(parent)
{
	m_server = pls_new<QLocalServer>(this);
	m_server->setSocketOptions(QLocalServer::WorldAccessOption);

	// Retry listen: when parent restarts quickly, old region-capture may still be releasing the pipe
	const int listenRetries = 3;
	const int retryDelayMs = 500;
	bool listenOk = false;
	for (int i = 0; i < listenRetries; ++i) {
		QLocalServer::removeServer(pipeName);
		if (m_server->listen(pipeName)) {
			listenOk = true;
			break;
		}
		if (i < listenRetries - 1) {
			QThread::msleep(retryDelayMs);
		}
	}
	if (!listenOk) {
		qWarning() << "Failed to start region capture service:" << m_server->errorString();
		QTimer::singleShot(0, qApp, &QApplication::quit);
		return;
	}

	QObject::connect(m_server, &QLocalServer::newConnection, this, &RegionCaptureService::onNewConnection);

	std::cout << "READY" << std::endl;
	std::cout.flush();
}

RegionCaptureService::~RegionCaptureService()
{
	cleanupCapture();
}

bool RegionCaptureService::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == m_capture && event->type() == QEvent::Close) {
		if (!m_resultSent) {
			sendResponse("CANCEL");
		}
		QMetaObject::invokeMethod(this, &RegionCaptureService::cleanupCapture, Qt::QueuedConnection);
	}
	return QObject::eventFilter(obj, event);
}

void RegionCaptureService::onNewConnection()
{
	auto *socket = m_server->nextPendingConnection();
	if (!socket)
		return;

	if (m_client) {
		m_client->close();
		m_client->deleteLater();
		m_client = nullptr;
	}
	cleanupCapture();

	m_client = socket;
	m_readBuffer.clear();
	QObject::connect(m_client, &QLocalSocket::readyRead, this, &RegionCaptureService::onClientData);
	QObject::connect(m_client, &QLocalSocket::disconnected, this, &RegionCaptureService::onClientDisconnected);
}

void RegionCaptureService::onClientData()
{
	if (!m_client)
		return;

	m_readBuffer.append(m_client->readAll());
	while (true) {
		int idx = m_readBuffer.indexOf('\n');
		if (idx < 0)
			break;
		QByteArray line = m_readBuffer.left(idx).trimmed();
		m_readBuffer.remove(0, idx + 1);
		if (!line.isEmpty())
			handleCommand(line);
	}
}

void RegionCaptureService::onClientDisconnected()
{
	if (m_client) {
		m_client->deleteLater();
		m_client = nullptr;
	}
	cleanupCapture();
}

void RegionCaptureService::handleCommand(const QByteArray &line)
{
	auto parts = line.split('|');
	if (parts.isEmpty())
		return;

	QByteArray cmd = parts[0];
	if (cmd == "START" && parts.size() == 3) {
		startCapture(parts[1].toULongLong(), parts[2].toULongLong());
	} else if (cmd == "EXIT") {
		cleanupCapture();
		QApplication::quit();
	}
}

void RegionCaptureService::startCapture(uint64_t maxWidth, uint64_t maxHeight)
{
	cleanupCapture();
	m_resultSent = false;

	m_capture = pls_new<RegionCapture>();
	m_capture->setAttribute(Qt::WA_DeleteOnClose, false);
	m_capture->setWindowModality(Qt::ApplicationModal);

	QObject::connect(m_capture, &RegionCapture::selectedRegion, this, [this](const QRect &rect) {
		m_resultSent = true;
		sendResponse(QByteArray::number(rect.x()) + "|" + QByteArray::number(rect.y()) + "|" + QByteArray::number(rect.width()) + "|" +
			     QByteArray::number(rect.height()));
	});

	m_capture->installEventFilter(this);
	m_capture->StartCapture(maxWidth, maxHeight);
}

void RegionCaptureService::sendResponse(const QByteArray &response)
{
	if (!m_client)
		return;
	m_client->write(response + "\n");
	m_client->flush();
}

void RegionCaptureService::cleanupCapture()
{
	if (m_capture) {
		m_capture->removeEventFilter(this);
		m_capture->hide();
		m_capture->deleteLater();
		m_capture = nullptr;
	}
}
