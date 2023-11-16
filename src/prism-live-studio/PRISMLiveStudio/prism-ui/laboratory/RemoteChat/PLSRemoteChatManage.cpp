#include "PLSRemoteChatManage.h"
#include <QTcpSocket>
#include <QtWebSockets/qwebsocketserver.h>
#include "utils-api.h"
#include "PLSRemoteChatView.h"
#include "PLSLaboratoryManage.h"
#include <QDir>
#include <QMimeDatabase>
#include <QtNetwork/qnetworkinterface.h>
#include "log/log.h"
#include <QJsonDocument>
#include <channels/ChannelsDataApi/PLSChannelDataAPI.h>
#include <PLSPlatformApi/PLSPlatformApi.h>
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSAlertView.h"
#include "PLSLaboratory.h"

constexpr auto guideHtml = "/guidePage.html";
constexpr auto indexHtml = "/index.html";
constexpr auto MODULE_WEBSERVER = "RemoteChatModule";
constexpr auto Prism_Local_Receive_Chat_Type = "localChatMessageSend";
constexpr auto Prism_Local_Send_Chat_Type = "localChatMessage";
constexpr auto Prism_Prism_Receive_Chat_Type = "prismChatMessageSend";
constexpr auto Prism_Prism_Send_Chat_Type = "prismChatMessage";

constexpr auto COMMON_TERMS_JSON_NAME = "commonTerms.json";

const QString g_remoteChatType = "type";

PLSLaboratory *g_laboratoryDialog = nullptr;
PLSRemoteChatView *g_remoteChatDialog = nullptr;

PLSRemoteChatManage::PLSRemoteChatManage(QObject *parent) : QObject(parent)
{
	printLog("PLSRemoteChatManage init");
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveStarted, this,
		[this](bool isSucceed) {
			if (!isSucceed) {
				return;
			}
			printLog("remote chat manage receive live started message");
			sendWebSocketIsLivingEvent(true);
		},
		Qt::QueuedConnection);
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this,
		[this]() {
			printLog("remote chat manage receive live end message");
			sendWebSocketIsLivingEvent(false);
		},
		Qt::QueuedConnection);
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::sendWebChatDataJsonObject, this,
		[this](const QJsonObject &data) {
			printLog("remote chat manage receive live chat message");
			QJsonObject root;
			root.insert("type", "prismChatMessage");
			root.insert("data", data);
			sendWebSocketEvent(root);
		},
		Qt::QueuedConnection);
}

PLSRemoteChatManage *PLSRemoteChatManage::instance()
{
	static PLSRemoteChatManage manage;
	return &manage;
}

PLSRemoteChatManage::~PLSRemoteChatManage()
{
	printLog("PLSRemoteChatManage release");
}

void PLSRemoteChatManage::printLog(const QString &subLog) const
{
	QString log = QString("remote chat status: ") + subLog;
	PLS_INFO(MODULE_WEBSERVER, "%s", log.toUtf8().constData());
}

void PLSRemoteChatManage::printKRLog(const QString &subLog) const
{
	QString log = QString("remote chat status: ") + subLog;
	PLS_INFO_KR(MODULE_WEBSERVER, "%s", log.toUtf8().constData());
}

void PLSRemoteChatManage::sendWebSocketEvent(const QJsonObject &data, const QString &type) const
{
	QJsonObject root;
	root.insert("type", type);
	root.insert("data", data);
	sendWebSocketEvent(root);
}

void PLSRemoteChatManage::sendWebSocketEvent(const QJsonObject &root) const
{
	printLog(QString("send websocket client event type is %1").arg(root.value(g_remoteChatType).toString()));
	if (m_websocketServer) {
		m_websocketServer->sendData(QJsonDocument(root).toJson().constData());
	}
}

void PLSRemoteChatManage::openRemoteChat()
{
	//First determine whether the current laboratory view is initialized
	if (g_remoteChatDialog) {
		printLog("duplicated open remote chat method");
		g_remoteChatDialog->activateWindow();
		return;
	}

	//Determine whether remoteChat is invalid
	if (!LabManage->isValidForLabFunc(LABORATORY_REMOTECHAT_ID)) {
		printLog("remote chat local dir invalid");
		pls_alert_error_message(g_laboratoryDialog, tr("Alert.Title"), tr("laboratory.item.open.other.reason.failed.text"));
		pls_set_laboratory_status(LABORATORY_REMOTECHAT_ID, false);
		return;
	}

	printLog("call open remote chat method");
	if (!startWebsocketServer()) {
		closeRemoteChatView();
		return;
	}

	if (!startWebServer()) {
		closeRemoteChatView();
		return;
	}

	if (!showRemoteChatView()) {
		closeRemoteChatView();
		return;
	}
}

void PLSRemoteChatManage::closeRemoteChat() const
{
	printLog("call close remote chat method");
	closeRemoteChatView();
}

void PLSRemoteChatManage::closeAllServer()
{
	printLog("start close all server method");
	stopWebsocketServer();
	stopWebServer();
}

bool PLSRemoteChatManage::startWebServer()
{
	printLog("start create webserver");
	m_tcpServer = pls_new<QTcpServer>();
	QHostAddress ipAddress;
	QList<QHostAddress> ipAddressList = QNetworkInterface::allAddresses();
	for (int i = 0; i < ipAddressList.size(); i++) {
		if (ipAddressList.at(i) != QHostAddress::LocalHost && ipAddressList.at(i).toIPv4Address()) {
			ipAddress = ipAddressList.at(i);
			break;
		}
	}

	if (ipAddressList.isEmpty()) {
		printLog("create webserver failed because the ip address is empty");
		return false;
	}

	if (!m_tcpServer->listen(ipAddress)) {
		printKRLog(QString("create webserver failed because listen { host:%1 , port:%2 } failed").arg(ipAddress.toString()).arg(m_tcpServer->serverPort()));
		printLog("webserver listen failed");
		return false;
	}
	printLog("webserver listen success");

	connect(m_tcpServer, &QTcpServer::destroyed, this, [this]() { printLog("webserver released"); });
	connect(m_tcpServer, &QTcpServer::acceptError, [this](QAbstractSocket::SocketError socketError) { printLog(QString("webserver status: webserver accept error %1").arg(socketError)); });
	connect(m_tcpServer, &QTcpServer::newConnection, [this] {
		while (m_tcpServer->hasPendingConnections()) {
			auto tcpClient = m_tcpServer->nextPendingConnection();
			if (nullptr == tcpClient) {
				continue;
			}
			receiveWebServerNewConnection(tcpClient);
		}
	});

	QNetworkProxy applicationProxy = QNetworkProxy::applicationProxy();
	QNetworkProxy serverProxy = m_tcpServer->proxy();
	printKRLog(QString("application proxy info {  hostname: %1 , port : %2 , proxyType : %3 }").arg(applicationProxy.hostName()).arg(applicationProxy.port()).arg(applicationProxy.type()));
	printKRLog(QString("webserver proxy info {  hostname: %1 , port : %2 , proxyType : %3}").arg(serverProxy.hostName()).arg(serverProxy.port()).arg(serverProxy.type()));
	printKRLog(QString("create webserver success, listen info { host:%1 , port:%2 }").arg(ipAddress.toString()).arg(m_tcpServer->serverPort()));
	printLog("create webserver success");
	return true;
}

void PLSRemoteChatManage::stopWebServer()
{
	if (m_tcpServer) {
		printLog("call PLSRemoteChatManage stop webserver method");
		m_tcpServer->close();
		pls_delete(m_tcpServer, nullptr);
	}
}

bool PLSRemoteChatManage::startWebsocketServer()
{
	printLog("start create websocket server");
	m_websocketServer = pls_new<PLSRemoteChatWebSocketServer>("RC_WebSocketServer", this);
	connect(m_websocketServer, &PLSRemoteChatWebSocketServer::signal_conncted, this, [this](QString, qint32) { sendWebSocketIsLivingEvent(PLSCHANNELS_API->isLiving()); });
	connect(m_websocketServer, &PLSRemoteChatWebSocketServer::signal_textMessageReceived, this, [this](QString, quint32, QString message) {
		QJsonObject jsonObject = stringToJson(message);
		QJsonObject dataJsonObject = jsonObject.value("data").toObject();
		QString type = jsonObject.value(g_remoteChatType).toString();
		if (type == Prism_Local_Receive_Chat_Type) {
			jsonObject.insert(g_remoteChatType, Prism_Local_Send_Chat_Type);
			sendWebSocketEvent(jsonObject);
		} else if (type == Prism_Prism_Receive_Chat_Type) {
			auto _actived = PLS_PLATFORM_ACTIVIED;
			if (_actived.empty()) {
				return;
			}
			auto platform = _actived.front();
			if (platform->getServiceType() == PLSServiceType::ST_NAVER_SHOPPING_LIVE) {
				sendNaverShoppingNotice(dataJsonObject);
				return;
			}
			PLS_PLATFORM_API->doWebSendChatRequest(jsonObject.value(name2str(data)).toObject());
		}
	});
	if (m_websocketServer->startWebsocketServer()) {
		printLog("create websocket server success");
		return true;
	}
	printLog("create websocket server failed");
	return false;
}

void PLSRemoteChatManage::stopWebsocketServer()
{
	if (m_websocketServer) {
		printLog("call PLSRemoteChatManage stop websocket server method");
		m_websocketServer->stopWebsocketServer();
		pls_delete(m_websocketServer, nullptr);
	}
}

bool PLSRemoteChatManage::showRemoteChatView()
{
	printLog("start show remote chat view");
	if (g_remoteChatDialog) {
		g_remoteChatDialog->show();
		printLog("show remote chat view again success");
		return true;
	}

	QString htmlFilePath = getAbsolutePath(guideHtml);
	QFileInfo fileInfo(htmlFilePath);
	if (!fileInfo.exists()) {
		printLog("remote chat view celf show local guide html file is not existed");
		return false;
	}

	DialogInfo info;
	info.configId = ConfigId::LaboratoryConfig;
	info.defaultWidth = 820;
	info.defaultOffset = 5;
	info.defaultHeight = 600;
	g_remoteChatDialog = pls_new<PLSRemoteChatView>(info, pls_get_main_view());
	connect(g_remoteChatDialog, &PLSRemoteChatView::destroyed, this, [this]() {
		printLog("receive remote chat view destroyed signal,call close all server method and global chat view pointer is nullptr");
		closeAllServer();
		g_remoteChatDialog = nullptr;
	});

	QString chatPageUrl = QString("http://%1:%2%3").arg(m_tcpServer->serverAddress().toString()).arg(m_tcpServer->serverPort()).arg(indexHtml);
	QString cefLoadUrl = QString("http://%1:%2%3?chatPageUrl=%4&wsip=%5&wsport=%6&lang=%7")
				     .arg(m_tcpServer->serverAddress().toString())
				     .arg(m_tcpServer->serverPort())
				     .arg(guideHtml)
				     .arg(chatPageUrl)
				     .arg(m_websocketServer->getListenHostHost())
				     .arg(m_websocketServer->getListenPort())
				     .arg(pls_get_current_language_short_str());
	if (!g_remoteChatDialog) {
		return false;
	}

	g_remoteChatDialog->setURL(cefLoadUrl);
	printKRLog(QString("create cef open url : %1").arg(cefLoadUrl));
	g_remoteChatDialog->show();
	printLog("show remote chat view first success");
	return true;
}

void PLSRemoteChatManage::closeRemoteChatView() const
{
	if (!g_remoteChatDialog) {
		return;
	}
	g_remoteChatDialog->close();
	g_remoteChatDialog = nullptr;
	printLog("close remote chat view and global chat view pointer is nullptr");
}

QString PLSRemoteChatManage::getAbsolutePath(const QString &subPath) const
{
	return QString("%1%2").arg(PLSLaboratoryManage::instance()->getLabZipUnzipFolderPath(LABORATORY_REMOTECHAT_ID)).arg(subPath);
}

QJsonObject PLSRemoteChatManage::stringToJson(const QString &str) const
{
	QJsonObject l_ret;
	QJsonParseError l_err;
	QJsonDocument l_doc = QJsonDocument::fromJson(str.toUtf8(), &l_err);
	if (l_err.error == QJsonParseError::NoError) {
		if (l_doc.isObject()) {
			l_ret = l_doc.object();
		}
	}
	return l_ret;
}

void PLSRemoteChatManage::readCommonTermJson(QJsonObject &data) const
{
	printLog("start reading common term json");
	QFileInfo fi(getDownloadRemoteChatCommonTermJsonPath());
	QString filePath = fi.absoluteFilePath();
	if (!checkCommonTersmJsonValid(filePath)) {
		printLog("get the commonTerms.json file failed");
		return;
	}

	QByteArray byteArray;
	PLSJsonDataHandler::getJsonArrayFromFile(byteArray, filePath);
	if (byteArray.size() == 0) {
		printLog("get the data under the file path is empty");
	}

	PLSJsonDataHandler::jsonTo(byteArray, data);
}

void PLSRemoteChatManage::saveCommonTermJson(const QJsonObject &data) const
{
	QJsonDocument doc(data);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), getDownloadRemoteChatCommonTermJsonPath())) {
		printLog("commonTerms.json save download lab cache failed.");
		return;
	}
	printLog("commonTerms.json save download lab cache success.");
}

void PLSRemoteChatManage::sendNaverShoppingNotice(const QJsonObject &data) const
{
	printLog("send navershopping notice api request");
	PLSPlatformNaverShoppingLIVE *platform = PLS_PLATFORM_API->getPlatformNaverShoppingLIVE();
	QJsonObject requestJsonObject = data;
	PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo liveInfo = platform->getLivingInfo();
	requestJsonObject.insert("broadcastId", liveInfo.id);
	PLSNaverShoppingLIVEAPI::sendNotice(
		platform, requestJsonObject, this, [](const QJsonDocument &) {},
		[](PLSAPINaverShoppingType) {

		});
}

void PLSRemoteChatManage::sendWebSocketIsLivingEvent(bool isLiving) const
{
	QJsonObject data = PLS_PLATFORM_API->getWebPrismInit().value("data").toObject();
	data.insert("isLiving", isLiving);
	sendWebSocketEvent(data, "liveStatus");
}

bool PLSRemoteChatManage::checkCommonTersmJsonValid(const QString &destPath) const
{
	//common term json dir not exist, then create laboratory dir
	printLog("start checking remote chat the commonTerms.json file.");
	QFileInfo fileInfo(destPath);
	if (!fileInfo.dir().exists()) {
		printLog("The laboratory folder in the user directory does not exist and needs to be created in the user directory.");
		bool ok = fileInfo.dir().mkpath(fileInfo.absolutePath());
		if (!ok) {
			printLog("check laboratory json file failed because failed to create laboratory folder in user directory.");
			return false;
		}
	}
	printLog("The laboratory folder in the user directory exists.");

	//laboratory json file not exist, then copy json file to dest path from app cache file
	if (!fileInfo.exists()) {

		//user local laboratory.json is not existed
		printLog("The commonTerms.json file does not exist in the user laboratory folder");
		return false;
	}

	printLog("check commonTerms.json file success, the commonTerms.json file exists in the laboratory folder in the user directory");
	return true;
}

QString PLSRemoteChatManage::getDownloadRemoteChatCommonTermJsonPath() const
{
	return pls_get_user_path(QString("PRISMLiveStudio/laboratory/%1").arg(COMMON_TERMS_JSON_NAME));
}

void PLSRemoteChatManage::receiveWebServerNewConnection(QTcpSocket *tcpClient) const
{
	QObject::connect(tcpClient, &QTcpSocket::readyRead, m_tcpServer, [this, tcpClient] { writeDataToTcpClient(tcpClient); });
}

void PLSRemoteChatManage::writeDataToTcpClient(QTcpSocket *tcpClient) const
{
	for (;;) {
		auto data = tcpClient->readLine();
		if (data.isEmpty()) {
			break;
		}
		auto line = QString(data.trimmed());
		if (line.isEmpty() || !(line.startsWith("POST") || line.startsWith("GET"))) {
			continue;
		}
		auto urls = line.split(" ");
		if (urls.length() != 3) {
			break;
		}
		QString path = urls[1].section('?', 0, 0);
		if (path.isEmpty()) {
			path = urls[1];
		}
		printKRLog(QString("webserver receive new connection, receive data is %1").arg(line));

		QString filePath = getAbsolutePath(path);
		tcpClient->write("HTTP/1.1 200 OK\n");
		QFileInfo fileInfo(filePath);
		printKRLog(QString("webserver file path is %1 , file is %2").arg(filePath).arg(fileInfo.exists() ? "existed" : "not existed"));

		QMimeDatabase db;
		QMimeType subFileMimeType = db.mimeTypeForFile(fileInfo);
		QString contentType = QString("Content-Type: %1;\n").arg(subFileMimeType.name());
		tcpClient->write(contentType.toUtf8().constData());
		tcpClient->write("\n");

		printKRLog(QString("webserver response %1").arg(contentType));

		QByteArray byteArray;
		QFile file(filePath);
		if (file.open(QIODevice::ReadOnly)) {
			byteArray = file.readAll();
		}
		file.close();
		tcpClient->write(byteArray);

		tcpClient->flush();
		tcpClient->close();
		tcpClient->deleteLater();

		break;
	}
}
