#ifndef PLSREMOTECHATMANAGE_H
#define PLSREMOTECHATMANAGE_H

#include <QObject>
#include <functional>
#include <QTcpServer>
#include "PLSRemoteChatWebSocketServer.h"

extern const QString g_remoteChatType;

class PLSRemoteChatManage : public QObject {
	Q_OBJECT
public:
	explicit PLSRemoteChatManage(QObject *parent = nullptr);
	static PLSRemoteChatManage *instance();
	~PLSRemoteChatManage() override;
	void openRemoteChat();
	void closeRemoteChat() const;
	void printLog(const QString &subLog) const;
	void printKRLog(const QString &subLog) const;
	void sendWebSocketEvent(const QJsonObject &data, const QString &type) const;
	void sendWebSocketEvent(const QJsonObject &root) const;
	QJsonObject stringToJson(const QString &str) const;
	void readCommonTermJson(QJsonObject &data) const;
	void saveCommonTermJson(const QJsonObject &data) const;
	void sendNaverShoppingNotice(const QJsonObject &data) const;

private:
	void closeAllServer();
	bool startWebServer();
	void stopWebServer();
	bool startWebsocketServer();
	void stopWebsocketServer();
	bool showRemoteChatView();
	void closeRemoteChatView() const;
	QString getAbsolutePath(const QString &subPath) const;
	void sendWebSocketIsLivingEvent(bool isLiving) const;
	bool checkCommonTersmJsonValid(const QString &destPath) const;
	QString getDownloadRemoteChatCommonTermJsonPath() const;
	void receiveWebServerNewConnection(QTcpSocket *tcpClient) const;
	void writeDataToTcpClient(QTcpSocket *tcpClient) const;

	PLSRemoteChatWebSocketServer *m_websocketServer{nullptr};
	QTcpServer *m_tcpServer{nullptr};
};

#define RC_LOG(sublog) PLSRemoteChatManage::instance()->printLog(sublog)
#define RC_KR_LOG(sublog) PLSRemoteChatManage::instance()->printKRLog(sublog)

#endif // PLSREMOTECHATMANAGE_H
