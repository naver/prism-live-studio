#pragma once

#include <QFrame>
#include <QPointer>
#include <QTimer>
#include <util/platform.h>
#include <obs.h>
#include "PLSLabel.hpp"

class QLabel;
class QToolButton;
class PLSColorCircle;
class PLSBasicStatusBar;

class PLSBasicStatusBarFrameDropState : public QFrame {
	Q_OBJECT
	Q_PROPERTY(QColor stateColor READ getStateColor WRITE setStateColor)

public:
	PLSBasicStatusBarFrameDropState(QWidget *parent = nullptr);
	~PLSBasicStatusBarFrameDropState();

public:
	QColor getStateColor() const;
	void setStateColor(const QColor &color);

protected:
	virtual void paintEvent(QPaintEvent *) override;

private:
	QColor stateColor{1, 203, 6};
};

class PLSBasicStatusBarButtonFrame : public QFrame {
	Q_OBJECT
public:
	explicit PLSBasicStatusBarButtonFrame(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~PLSBasicStatusBarButtonFrame();

signals:
	void clicked();

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);

private:
	bool isLeftMouseButtonPress = false;
};

class PLSBasicStatusBar : public QFrame {
	Q_OBJECT

	struct RealBitrateHelper {
		uint64_t lastBytesSent = 0;
		uint64_t lastBytesSentTime = 0;
	};

private:
	PLSBasicStatusBarButtonFrame *encodes;
	QLabel *encoding;
	QLabel *encodesPt;
	QLabel *fps;
	QLabel *encodingSettingIcon;
	PLSBasicStatusBarButtonFrame *stats;
	PLSBasicStatusBarFrameDropState *frameDropState;
	QLabel *cpuUsage;
	QLabel *gpuUsage;
	QLabel *frameDrop;
	PLSLabel *bitrate;
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

	int startSkippedFrameCount = 0;
	int startTotalFrameCount = 0;
	int lastSkippedFrameCount = 0;

	int bitrateUpdateSeconds = 0;
	uint64_t lastBytesSent = 0;
	uint64_t lastBytesSentTime = 0;

	float lastCongestion = 0.0f;

	QPointer<QTimer> refreshTimer;
	QPointer<QTimer> uploadTimer;

	void Activate();
	void Deactivate();

	int GetRealTimeBitrate(OBSOutput output, RealBitrateHelper &helper);
	int GetExpectOutputBitrate(OBSOutput output);

	void UpdateBandwidth();
	void UpdateDroppedFrames();

	static void OBSOutputReconnect(void *data, calldata_t *params);
	static void OBSOutputReconnectSuccess(void *data, calldata_t *params);

private slots:
	void Reconnect(int seconds);
	void ReconnectSuccess();
	void UpdateStatusBar();
	void popupStats();
	void UploadStatus();

public:
	explicit PLSBasicStatusBar(QWidget *parent);
	virtual ~PLSBasicStatusBar();

	void StreamDelayStarting(int sec);
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
	void setEncodingEnabled(bool enabled);
	void UpdateDelayMsg();
	void StartStatusMonitor();
	void UpdateDropFrame(double dropFrame, double dropPercent);
	void UpdateRealBitrate(double birtare);
	void UpdateCPUUsage(double process, double total);
	void UpdateGPUUsage(double process, double total);
	void SetCalculateFixedWidth();

protected:
	bool eventFilter(QObject *watched, QEvent *event);
};
