#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <Windows.h>
#include "pls-app.hpp"
#include "window-basic-main.hpp"
#include "window-basic-status-bar.hpp"
#include "window-basic-main-outputs.hpp"
#include "color-circle.hpp"

#include "log.h"
#include "action.h"
#include "log/module_names.h"
#include "frontend-api.h"
#include "main-view.hpp"

#include <Windows.h>

namespace {
QWidget *g_stats = nullptr;

class SystemCPUUsageInfo {
	uint64_t prevIdleTime, prevKernelTime, prevUserTime;

private:
	SystemCPUUsageInfo()
	{
		FILETIME curIdleFt, curKernelFt, curUserFt;
		GetSystemTimes(&curIdleFt, &curKernelFt, &curUserFt);

		prevIdleTime = ft2ui64(curIdleFt);
		prevKernelTime = ft2ui64(curKernelFt);
		prevUserTime = ft2ui64(curUserFt);
	}

public:
	static SystemCPUUsageInfo *instance()
	{
		static SystemCPUUsageInfo systemCPUUsageInfo;
		return &systemCPUUsageInfo;
	}

public:
	double getCPUUsage()
	{
		double percent = 0.0;

		FILETIME curIdleFt, curKernelFt, curUserFt;
		GetSystemTimes(&curIdleFt, &curKernelFt, &curUserFt);

		uint64_t curIdleTime = ft2ui64(curIdleFt);
		uint64_t curKernelTime = ft2ui64(curKernelFt);
		uint64_t curUserTime = ft2ui64(curUserFt);

		uint64_t idleTime = curIdleTime - prevIdleTime;
		uint64_t kernelTime = curKernelTime - prevKernelTime;
		uint64_t userTime = curUserTime - prevUserTime;

		percent = (kernelTime + userTime - idleTime) * 100.0 / (kernelTime + userTime);

		prevIdleTime = curIdleTime;
		prevKernelTime = curKernelTime;
		prevUserTime = curUserTime;

		return percent;
	}

	uint64_t ft2ui64(const FILETIME &ft)
	{
		uint64_t ui64 = ft.dwHighDateTime;
		ui64 = (ui64 << 32) | ft.dwLowDateTime;
		return ui64;
	}
};
}

PLSBasicStatusBarButtonFrame::PLSBasicStatusBarButtonFrame(QWidget *parent, Qt::WindowFlags f) : QFrame(parent, f)
{
	setMouseTracking(true);
}

PLSBasicStatusBarButtonFrame::~PLSBasicStatusBarButtonFrame() {}

void PLSBasicStatusBarButtonFrame::mousePressEvent(QMouseEvent *event)
{
	QFrame::mousePressEvent(event);

	if (event->button() != Qt::LeftButton) {
		return;
	}

	if (isEnabled()) {
		isLeftMouseButtonPress = true;
		pls_flush_style_recursive(this, "state", "pressed");
	}
}

void PLSBasicStatusBarButtonFrame::mouseReleaseEvent(QMouseEvent *event)
{
	QFrame::mouseReleaseEvent(event);

	if (event->button() != Qt::LeftButton) {
		return;
	}

	if (isEnabled()) {
		isLeftMouseButtonPress = false;
		if (QRect(mapToGlobal(QPoint(0, 0)), size()).contains(event->globalPos())) {
			pls_flush_style_recursive(this, "state", "hover");
			emit clicked();
		} else {
			pls_flush_style_recursive(this, "state", "");
		}
	}
}

void PLSBasicStatusBarButtonFrame::mouseMoveEvent(QMouseEvent *event)
{
	QFrame::mouseMoveEvent(event);

	isLeftMouseButtonPress = isLeftMouseButtonPress && (event->buttons() & Qt::LeftButton);

	if (isEnabled()) {
		if (QRect(mapToGlobal(QPoint(0, 0)), size()).contains(event->globalPos())) {
			pls_flush_style_recursive(this, "state", isLeftMouseButtonPress ? "pressed" : "hover");
		} else {
			pls_flush_style_recursive(this, "state", "");
		}
	}
}

void PLSBasicStatusBarButtonFrame::enterEvent(QEvent *event)
{
	QFrame::enterEvent(event);

	if (isEnabled()) {
		pls_flush_style_recursive(this, "state", "hover");
	}
}

void PLSBasicStatusBarButtonFrame::leaveEvent(QEvent *event)
{
	QFrame::leaveEvent(event);

	if (isEnabled()) {
		pls_flush_style_recursive(this, "state", "");
	}
}

PLSBasicStatusBar::PLSBasicStatusBar(QWidget *parent)
	: QFrame(parent),
	  encodes(new PLSBasicStatusBarButtonFrame()),
	  encoding(new QLabel),
	  encodesPt(new QLabel),
	  fps(new QLabel),
	  encodingSettingIcon(new QLabel),
	  stats(new PLSBasicStatusBarButtonFrame()),
	  frameDropState(new QFrame),
	  cpuUsage(new QLabel(QString("CPU 0% (Total 0%)"))),
	  frameDrop(new QLabel(QTStr("DroppedFrames").arg(0).arg(0.0, 0, 'f', 1))),
	  bitrate(new QLabel(QTStr("Bitrate").arg(0))),
	  statsDropIcon(new QLabel)
{
	SystemCPUUsageInfo::instance();

	encodesPt->setFixedSize(2, 2);
	frameDropState->setVisible(false);

	encodesPt->setObjectName("encodesPt");
	encodingSettingIcon->setObjectName("encodingSettingIcon");
	frameDropState->setObjectName("frameDropState");
	statsDropIcon->setObjectName("statsDropIcon");

	encoding->setProperty("encodingText", true);
	fps->setProperty("encodingText", true);
	cpuUsage->setProperty("statsText", true);
	frameDrop->setProperty("statsText", true);
	bitrate->setProperty("statsText", true);

	encoding->setMargin(0);
	encoding->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	cpuUsage->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	frameDrop->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	bitrate->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	QHBoxLayout *layout1 = new QHBoxLayout(encodes);
	layout1->setContentsMargins(25, 0, 0, 0);
	layout1->setSpacing(5);

	QHBoxLayout *layout11 = new QHBoxLayout();
	layout11->setContentsMargins(0, 0, 0, 5);
	layout11->setSpacing(5);
	layout11->addWidget(encoding, 0, Qt::AlignVCenter);
	layout11->addWidget(encodesPt, 0, Qt::AlignVCenter);
	layout11->addWidget(fps, 0, Qt::AlignVCenter);

	layout1->addLayout(layout11);
	layout1->addWidget(encodingSettingIcon);

	QHBoxLayout *layout2 = new QHBoxLayout(stats);
	layout2->setMargin(0);
	layout2->setSpacing(10);
	layout2->addWidget(frameDropState);

	QHBoxLayout *layout21 = new QHBoxLayout();
	layout21->setMargin(0);
	layout21->setSpacing(0);
	layout21->addWidget(cpuUsage);
	layout21->addSpacing(20);
	layout21->addWidget(frameDrop);
	layout21->addSpacing(20);
	layout21->addWidget(bitrate);
	layout21->addSpacing(10);
	layout21->addWidget(statsDropIcon);
	layout2->addLayout(layout21);

	QHBoxLayout *layout3 = new QHBoxLayout(this);
	layout3->setMargin(0);
	layout3->setSpacing(0);
	layout3->addWidget(encodes, 0, Qt::AlignVCenter | Qt::AlignLeft);
	layout3->addSpacing(25);
	layout3->addWidget(stats, 0, Qt::AlignVCenter | Qt::AlignLeft);
	layout3->addStretch(1);

	stats->setCursor(Qt::ArrowCursor);
	encodes->setCursor(Qt::ArrowCursor);

	auto showSettings = []() {
		PLS_UI_STEP(STATUSBAR_MODULE, "Status Bar's Encoding Setting Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Q_ARG(QString, "Output"), Q_ARG(QString, ""));
	};
	connect(encodes, &PLSBasicStatusBarButtonFrame::clicked, this, showSettings);
	connect(stats, &PLSBasicStatusBarButtonFrame::clicked, this, &PLSBasicStatusBar::popupStats);
	setStatsOpen(false);
}

void PLSBasicStatusBar::Activate()
{
	if (!active) {
		refreshTimer = new QTimer(this);
		connect(refreshTimer, SIGNAL(timeout()), this, SLOT(UpdateStatusBar()));

		frameDropState->setVisible(true);

		int skipped = video_output_get_skipped_frames(obs_get_video());
		int total = video_output_get_total_frames(obs_get_video());

		lastSkippedFrameCount = 0;
		startSkippedFrameCount = skipped;
		startTotalFrameCount = total;

		refreshTimer->start(1000);
		active = true;
	}
}

void PLSBasicStatusBar::Deactivate()
{
	PLSBasic *main = PLSBasic::Get();

	if (active) {
		delete refreshTimer;

		frameDropState->setVisible(false);

		frameDrop->setText(QTStr("DroppedFrames").arg(0).arg(0.0, 0, 'f', 1));
		bitrate->setText(QTStr("Bitrate").arg(0));

		delaySecTotal = 0;
		delaySecStarting = 0;
		delaySecStopping = 0;
		reconnectTimeout = 0;
		active = false;
		overloadedNotify = true;
	}
}

#define BITRATE_UPDATE_SECONDS 2

void PLSBasicStatusBar::UpdateBandwidth()
{
	if (!streamOutput)
		return;

	if (++bitrateUpdateSeconds < BITRATE_UPDATE_SECONDS)
		return;

	uint64_t bytesSent = obs_output_get_total_bytes(streamOutput);
	uint64_t bytesSentTime = os_gettime_ns();

	if (bytesSent < lastBytesSent)
		bytesSent = 0;
	if (bytesSent == 0)
		lastBytesSent = 0;

	uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;
	double timePassed = double(bytesSentTime - lastBytesSentTime) / 1000000000.0;
	double kbitsPerSec = double(bitsBetween) / timePassed / 1000.0;

	bitrate->setText(QTStr("Bitrate").arg(kbitsPerSec, 0, 'f', 0));

	lastBytesSent = bytesSent;
	lastBytesSentTime = bytesSentTime;
	bitrateUpdateSeconds = 0;
}

void PLSBasicStatusBar::UpdateCPUUsage()
{
	cpuUsage->setText(QString("CPU %1% (Total %2%)").arg(PLSBasic::Get()->GetAppCPUUsage(), 0, 'f', 1).arg(SystemCPUUsageInfo::instance()->getCPUUsage(), 0, 'f', 1));
}

void PLSBasicStatusBar::popupStats()
{
	PLS_UI_STEP(STATUSBAR_MODULE, "Status Bar's State Stats Button", ACTION_CLICK);

	PLSBasic::Get()->popupStats(!isStatsOpen(), mapToGlobal(QPoint(0, 0)));
}

void PLSBasicStatusBar::UpdateDroppedFrames()
{
	if (!streamOutput)
		return;

	int totalDropped = obs_output_get_frames_dropped(streamOutput);
	int totalFrames = obs_output_get_total_frames(streamOutput);
	double percent = (double)totalDropped / (double)totalFrames * 100.0;

	if (!totalFrames)
		return;

	frameDrop->setText(QTStr("DroppedFrames").arg(totalDropped).arg(percent, 0, 'f', 1));

	float congestion = obs_output_get_congestion(streamOutput);
	float avgCongestion = (congestion + lastCongestion) * 0.5f;
	if (avgCongestion < congestion)
		avgCongestion = congestion;
	if (avgCongestion > 1.0f)
		avgCongestion = 1.0f;

	if (avgCongestion < EPSILON) {
		frameDropState->setStyleSheet("background-color: #00ff00");
	} else if (fabsf(avgCongestion - 1.0f) < EPSILON) {
		frameDropState->setStyleSheet("background-color: #ff0000");
	} else {
		float red = avgCongestion * 2.0f;
		if (red > 1.0f)
			red = 1.0f;
		red *= 255.0;

		float green = (1.0f - avgCongestion) * 2.0f;
		if (green > 1.0f)
			green = 1.0f;
		green *= 255.0;
		frameDropState->setStyleSheet(QString::asprintf("background-color: #%02x%02x00", int(red), int(green)));
	}

	lastCongestion = congestion;
}

void PLSBasicStatusBar::OBSOutputReconnect(void *data, calldata_t *params)
{
	PLSBasicStatusBar *statusBar = reinterpret_cast<PLSBasicStatusBar *>(data);

	int seconds = (int)calldata_int(params, "timeout_sec");
	QMetaObject::invokeMethod(statusBar, "Reconnect", Q_ARG(int, seconds));
	UNUSED_PARAMETER(params);
}

void PLSBasicStatusBar::OBSOutputReconnectSuccess(void *data, calldata_t *params)
{
	PLSBasicStatusBar *statusBar = reinterpret_cast<PLSBasicStatusBar *>(data);

	QMetaObject::invokeMethod(statusBar, "ReconnectSuccess");
	UNUSED_PARAMETER(params);
}

void PLSBasicStatusBar::Reconnect(int seconds)
{
	PLSBasic *main = PLSBasic::Get();

	if (!retries)
		main->SysTrayNotify(QTStr("Basic.SystemTray.Message.Reconnecting"), QSystemTrayIcon::Warning);

	reconnectTimeout = seconds;

	if (streamOutput) {
		delaySecTotal = obs_output_get_active_delay(streamOutput);
		UpdateDelayMsg();
		retries++;
	}
}

void PLSBasicStatusBar::ReconnectClear()
{
	retries = 0;
	reconnectTimeout = 0;
	bitrateUpdateSeconds = -1;
	lastBytesSent = 0;
	lastBytesSentTime = os_gettime_ns();
	delaySecTotal = 0;
	UpdateDelayMsg();
}

void PLSBasicStatusBar::setEncoding(int cx, int cy)
{
	this->encoding->setText(QString("%1p").arg(cy));
}

void PLSBasicStatusBar::setFps(const QString &fps)
{
	this->fps->setText(fps);
}

bool PLSBasicStatusBar::isStatsOpen() const
{
	return statsDropIcon->property("open").toBool();
}

void PLSBasicStatusBar::setStatsOpen(bool open)
{
	g_stats = open ? PLSBasic::Get()->stats : nullptr;
	pls_flush_style(statsDropIcon, "open", open);
}

void PLSBasicStatusBar::setEncodingEnabled(bool enabled)
{
	encodes->setEnabled(enabled);
	pls_flush_style_recursive(encodes, "active", enabled);
}

void PLSBasicStatusBar::ReconnectSuccess()
{
	PLSBasic *main = PLSBasic::Get();

	QString msg = QTStr("Basic.StatusBar.ReconnectSuccessful");
	//showMessage(msg, 4000);
	main->SysTrayNotify(msg, QSystemTrayIcon::Information);
	ReconnectClear();

	if (streamOutput) {
		delaySecTotal = obs_output_get_active_delay(streamOutput);
		UpdateDelayMsg();
	}
}

void PLSBasicStatusBar::UpdateStatusBar()
{
	PLSBasic *main = PLSBasic::Get();

	UpdateBandwidth();
	UpdateDroppedFrames();

	int skipped = video_output_get_skipped_frames(obs_get_video());
	int total = video_output_get_total_frames(obs_get_video());

	skipped -= startSkippedFrameCount;
	total -= startTotalFrameCount;

	int diff = skipped - lastSkippedFrameCount;
	double percentage = double(skipped) / double(total) * 100.0;

	if (diff > 10 && percentage >= 0.1f) {
		//showMessage(QTStr("HighResourceUsage"), 4000);
		if (!main->isVisible() && overloadedNotify) {
			main->SysTrayNotify(QTStr("HighResourceUsage"), QSystemTrayIcon::Warning);
			overloadedNotify = false;
		}
	}

	lastSkippedFrameCount = skipped;
}

void PLSBasicStatusBar::StreamDelayStarting(int sec)
{
	PLSBasic *main = PLSBasic::Get();
	if (!main->outputHandler)
		return;

	streamOutput = main->outputHandler->streamOutput;

	delaySecTotal = delaySecStarting = sec;
	UpdateDelayMsg();
	Activate();
}

void PLSBasicStatusBar::StreamDelayStopping(int sec)
{
	delaySecTotal = delaySecStopping = sec;
	UpdateDelayMsg();
}

void PLSBasicStatusBar::StreamStarted(obs_output_t *output)
{
	streamOutput = output;

	signal_handler_connect(obs_output_get_signal_handler(streamOutput), "reconnect", OBSOutputReconnect, this);
	signal_handler_connect(obs_output_get_signal_handler(streamOutput), "reconnect_success", OBSOutputReconnectSuccess, this);

	retries = 0;
	lastBytesSent = 0;
	lastBytesSentTime = os_gettime_ns();
	Activate();
}

void PLSBasicStatusBar::StreamStopped()
{
	if (streamOutput) {
		signal_handler_disconnect(obs_output_get_signal_handler(streamOutput), "reconnect", OBSOutputReconnect, this);
		signal_handler_disconnect(obs_output_get_signal_handler(streamOutput), "reconnect_success", OBSOutputReconnectSuccess, this);

		ReconnectClear();
		streamOutput = nullptr;
		//clearMessage();
		Deactivate();
	}
}

void PLSBasicStatusBar::RecordingStarted(obs_output_t *output)
{
	recordOutput = output;
	// Activate();
}

void PLSBasicStatusBar::RecordingStopped()
{
	recordOutput = nullptr;
	// Deactivate();
}

bool PLSBasicStatusBar::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == App()->getMainView()) {
		if (event->type() == QEvent::Move && g_stats && g_stats->isVisible()) {
			PLSBasic::Get()->popupStats(true, mapToGlobal(QPoint(0, 0)));
		} else if (event->type() == QEvent::Resize && g_stats && g_stats->isVisible()) {
			PLSBasic::Get()->popupStats(true, mapToGlobal(QPoint(0, 0)));
		}
	}
	return QWidget::eventFilter(watched, event);
}

void PLSBasicStatusBar::UpdateDelayMsg()
{
	QString msg;

	if (delaySecTotal) {
		if (delaySecStarting && !delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStartingIn");
			msg = msg.arg(QString::number(delaySecStarting));

		} else if (!delaySecStarting && delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStoppingIn");
			msg = msg.arg(QString::number(delaySecStopping));

		} else if (delaySecStarting && delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStartingStoppingIn");
			msg = msg.arg(QString::number(delaySecStopping), QString::number(delaySecStarting));
		} else {
			msg = QTStr("Basic.StatusBar.Delay");
			msg = msg.arg(QString::number(delaySecTotal));
		}

		PLS_INFO(STATUSBAR_MODULE, msg.toUtf8().constData());
	}
}
