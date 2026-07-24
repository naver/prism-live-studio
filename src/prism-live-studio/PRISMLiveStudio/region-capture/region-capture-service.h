#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

class RegionCapture;

class RegionCaptureService : public QObject {
	Q_OBJECT

public:
	explicit RegionCaptureService(const QString &pipeName, QObject *parent = nullptr);
	~RegionCaptureService() override;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	void onNewConnection();
	void onClientData();
	void onClientDisconnected();
	void handleCommand(const QByteArray &line);
	void startCapture(uint64_t maxWidth, uint64_t maxHeight);
	void sendResponse(const QByteArray &response);
	void cleanupCapture();

	QLocalServer *m_server = nullptr;
	QLocalSocket *m_client = nullptr;
	RegionCapture *m_capture = nullptr;
	bool m_resultSent = false;
	QByteArray m_readBuffer;
};
