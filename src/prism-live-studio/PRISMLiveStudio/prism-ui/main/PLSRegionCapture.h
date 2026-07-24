#ifndef PLSREGIONCAPTURE_H
#define PLSREGIONCAPTURE_H

#include <atomic>
#include <qobject.h>
#include <QProcess>
#include <QLocalSocket>
#include <QRect>

class QThread;

class PLSRegionCapture : public QObject {
	Q_OBJECT

public:
	explicit PLSRegionCapture(QObject *parent = nullptr);
	~PLSRegionCapture() noexcept final;
	void StartCapture(uint64_t maxRegionWidth = 0, uint64_t maxRegionHeight = 0);
	QRect GetSelectedRect() const;

	static void PrewarmService();
	static void ShutdownService();

signals:
	void selectedRegion(const QRect &rect);

private:
	void onServiceResponse();
	void handleResponse(const QByteArray &line);
	void startLegacy(uint64_t maxRegionWidth, uint64_t maxRegionHeight);

	static bool isServiceReadyForCapture();
	static void serviceWatchdogTick();
	static void ensureWatchdogRunning();
	static bool startRegionCaptureServiceSync();
	static void tearDownServiceProcess();
	static void onServiceProcessStdout();

	bool tryAcquireCaptureSlot();
	void releaseCaptureIfHolding();

	static QProcess *s_serviceProcess;
	static QString s_pipeName;
	static bool s_shutdownConnected;
	static bool s_captureInProgress;
	static bool s_serviceReady;
	static QThread *s_watchdogThread;
	static std::atomic<bool> s_watchdogStop;

	QLocalSocket *m_socket = nullptr;
	QByteArray m_readBuffer;
	QRect m_rectSelected;
	bool m_finished = false;
	/** This instance set s_captureInProgress; must clear via releaseCaptureIfHolding or destructor. */
	bool m_holdCaptureToken = false;
};

#endif // PLSREGIONCAPTURE_H
