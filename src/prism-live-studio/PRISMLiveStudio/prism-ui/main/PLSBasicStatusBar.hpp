#pragma once

#include <QFrame>
#include <QPointer>
#include <QTimer>
#include <util/platform.h>
#include <obs.h>
#include <obs.hpp>
#include "PLSLabel.h"
#include "PLSTransparentForMouseEvents.h"
#include <pls/pls-statistics.h>

#include <atomic>
#if defined(_WIN32)
#include <QThreadPool>
#endif

class QLabel;
class QToolButton;
class PLSColorCircle;
class PLSBasicStatusBar;
class GoLivePannel;
class PLSPreviewLiveLabel;
class QWebSocket;

struct PLSBasicStatusData {
	std::tuple<double, double> cpu{0.0, 0.0}; //PRISM/System
	std::tuple<double, double> gpu{0.0, 0.0}; //PRISM/System

	double memory{0.0};
	double disk{0.0};

	double renderTime{0.0};
	std::tuple<double, double> renderFPS{0.0, 0.0}; //Current/Setting
	double streamOutputFPS{0.0};                     //output
	double recordOutputFPS{0.0};
	double streamOutputFPS_v{0.0};        //vertical output
	double streamNetworkMilliTime{0.0};   // in ms
	double streamNetworkMilliTime_v{0.0}; // in ms

	//Droped frames, Total frames, Droped percent
	std::tuple<uint32_t, uint32_t, double> dropedRendering{0, 0, 0.0};
	std::tuple<uint32_t, uint32_t, double> dropedEncoding{0, 0, 0.0};
	std::tuple<uint32_t, uint32_t, double> dropedEncoding_v{0, 0, 0.0};
	std::tuple<uint32_t, uint32_t, double> dropedNetwork{0, 0, 0.0};
	std::tuple<uint32_t, uint32_t, double> dropedNetwork_v{0, 0, 0.0};

	//bitrate, bytes
	std::tuple<double, uint64_t> streaming{0.0, 0};
	std::tuple<double, uint64_t> streaming_v{0.0, 0};
	std::tuple<double, uint64_t> recording{0.0, 0};

	//output latency info
	pls_statistics_dump latencyInfo{0};
	pls_statistics_dump latencyInfo_v{0};
};

class PLSBasicStatusBarFrameDropState : public QFrame {
	Q_OBJECT
	Q_PROPERTY(QColor stateColor READ getStateColor WRITE setStateColor)

public:
	explicit PLSBasicStatusBarFrameDropState(QWidget *parent = nullptr);
	~PLSBasicStatusBarFrameDropState() override = default;

	QColor getStateColor() const;
	void setStateColor(const QColor &color);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	QColor stateColor{1, 203, 6};
};

class PLSBasicStatusBarButtonFrame : public QFrame {
	Q_OBJECT
public:
	explicit PLSBasicStatusBarButtonFrame(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

signals:
	void clicked();

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	bool isLeftMouseButtonPress = false;
};

class PLSBasicStatusBar : public PLSTransparentForMouseEvents<QFrame> {
	Q_OBJECT

private:
	PLSBasicStatusBarButtonFrame *encodes;
	QLabel *encoding;
	QLabel *encodesPt;
	QLabel *fps;
	QLabel *encodingSettingIcon;
	PLSBasicStatusBarButtonFrame *stats;
	PLSBasicStatusBarFrameDropState *frameDropState;
	QLabel *cpuUsage;
	QLabel *renderTime;
#if _WIN32
	QLabel *gpuUsage;
#endif
	QLabel *frameDrop;
	QLabel *bitrate;
	QLabel *statsDropIcon;

	obs_output_t *streamOutput = nullptr;
	obs_output_t *recordOutput = nullptr;
	bool active = false;
	bool overloadedNotify = true;

	int retries = 0;

	int reconnectTimeout = 0;

	int delaySecTotal = 0;
	int delaySecStarting = 0;
	int delaySecStopping = 0;

	float lastCongestion = 0.0f;

	QPointer<QTimer> refreshTimer;
	QPointer<QTimer> uploadTimer;

	GoLivePannel *goliveWid = nullptr;
	PLSPreviewLiveLabel *liveArea = nullptr;

#if defined(_WIN32)
	mutable QThreadPool vcamHostThreadPool;
	mutable std::atomic<bool> vcamHostScanRunning{false};
#endif

	void Activate();
	void Deactivate();

	void UpdateDroppedFrames();

	static void OBSOutputReconnect(void *data, calldata_t *params);
	static void OBSOutputReconnectSuccess(void *data, calldata_t *params);

	// MARK: -- Analog
#ifdef __APPLE__
	static void RequestExtensionClientInfo(std::function<void(int pid, const std::string &signingId, const std::string &clientId, const std::string &time)> closure);
	QWebSocket *extensionClientSocket = nullptr;
#endif

public slots:
	void UpdateCPUUsage();

private slots:
	void Reconnect(int seconds);
	void ReconnectSuccess();
	void UpdateStatusBar();
	void UploadStatus() const;

public:
	explicit PLSBasicStatusBar(QWidget *parent);
	~PLSBasicStatusBar() override;

	PLSBasicStatusBar(const PLSBasicStatusBar &) = delete;
	PLSBasicStatusBar &operator=(const PLSBasicStatusBar &) = delete;
	PLSBasicStatusBar(PLSBasicStatusBar &&) = delete;
	PLSBasicStatusBar &operator=(PLSBasicStatusBar &&) = delete;

	void StreamDelayStarting(int) const;
	void StreamDelayStopping(int sec);
	void StreamStarted(obs_output_t *output);
	void StreamStopped();
	void RecordingStarted(obs_output_t *output);
	void RecordingStopped();

	void ReconnectClear();

	void setEncoding(int cx, int cy);
	void setFps(const QString &fps);
	bool isStatsOpen() const;
	void setStatsOpen(bool open);
	void UpdateDelayMsg() const;
	void StartStatusMonitor();
	void UpdateDropFrame(double dropFrame, double dropPercent);
	void UpdateRealBitrate(double birtare);
	void UpdateGPUUsage();

	void OnLiveStatus(bool isStarted);
	void OnRecordStatus(bool isStarted);
	void OnRecordPaused(bool paused);

	uint getStartTime() const;
	int getRecordDuration() const;

	GoLivePannel *getGoLivePannel() { return goliveWid; }
	PLSBasicStatusBarButtonFrame *getStatus() { return stats; }

	PLSBasicStatusData m_dataStatus;
};
