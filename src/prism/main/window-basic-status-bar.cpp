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
#include "prism/PLSPlatformPrism.h"
#include "PLSNetworkMonitor.h"

#include <Windows.h>
#include <util/platform.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include "PLSPerformance/PLSPerfCounter.hpp"
#include "PLSPerformance/PLSPerfHelper.hpp"

#define UPLOAD_STATUS_INTERVAL 3000     // milliseconds
#define CPU_USAGE_REFRESH_INTERVAL 1000 // milliseconds

namespace {
QWidget *g_stats = nullptr;
static std::mutex mtx;

class CalcCPUUsage {
	class SimpleTimer {
		std::atomic_bool abort;
		CalcCPUUsage *user;
		std::thread timerThread;

	public:
		SimpleTimer(CalcCPUUsage *user) : user(user) {}
		void start(int interval)
		{
			abort = false;
			user->Refresh();
			std::thread t([=]() {
				while (true) {
					if (abort)
						return;
					std::this_thread::sleep_for(std::chrono::milliseconds(interval));
					if (abort)
						return;
					user->Refresh();
				}
			});

			timerThread.swap(t);
		}
		void stop()
		{
			abort = true;
			if (timerThread.joinable())
				timerThread.join();
		}
	};
	double processCPUUsage = 0.0;
	double overallCPUUsage = 0.0;
#ifdef WIN32
	os_cpu_usage_info_ex_t *
#else
	os_cpu_usage_info_t *
#endif
		cpuUsageInfo = nullptr;
	SimpleTimer timer;

private:
	CalcCPUUsage()
		: timer(this),
#ifdef WIN32
		  cpuUsageInfo(os_cpu_usage_info_start_ex())
#else
		  cpuUsageInfo(os_cpu_usage_info_start())
#endif
	{
		timer.start(CPU_USAGE_REFRESH_INTERVAL);
	}
	~CalcCPUUsage()
	{
		timer.stop();
#ifdef WIN32
		os_cpu_usage_info_destroy_ex(cpuUsageInfo);
#else
		os_cpu_usage_info_destroy(cpuUsageInfo);
#endif
	}

	void Refresh()
	{
		if (cpuUsageInfo) {
			double process_usage = 0.0;
			double overall_usage = 0.0;
#ifdef WIN32
			os_cpu_usage_info_query_ex(cpuUsageInfo, &process_usage, &overall_usage);
#else
			process_usage = os_cpu_usage_info_query(cpu_info);
			// TODO:
			// No interface to retrieve overall cpu usage for non-Windows OS
			// Modify here!
			// overall_usage = ???;
#endif
			mtx.lock();
			processCPUUsage = process_usage;
			overallCPUUsage = overall_usage;
			mtx.unlock();
		}
	};

public:
	static CalcCPUUsage *instance()
	{
		static CalcCPUUsage calcCPUUsage;
		return &calcCPUUsage;
	}

	double getSysCPUUsage() const
	{
		double usage = 0.0;
		mtx.lock();
		usage = overallCPUUsage;
		mtx.unlock();
		return usage;
	}
	double getProcessCPUUsage() const
	{
		double usage = 0.0;
		mtx.lock();
		usage = processCPUUsage;
		mtx.unlock();
		return usage;
	}

	std::pair<double, double> getCPUUsage()
	{
		double overall = 0.0;
		double prococess = 0.0;
		mtx.lock();
		prococess = processCPUUsage;
		overall = overallCPUUsage;
		mtx.unlock();
		return {overall, prococess};
	}
};
}

double getProcessCurrentCPUUsage()
{
	return CalcCPUUsage::instance()->getProcessCPUUsage();
}

std::pair<double, double> getCurrentCPUUsage()
{
	return CalcCPUUsage::instance()->getCPUUsage();
}

extern bool s_isExistInstance;

PLSBasicStatusBarFrameDropState::PLSBasicStatusBarFrameDropState(QWidget *parent) : QFrame(parent) {}

PLSBasicStatusBarFrameDropState::~PLSBasicStatusBarFrameDropState() {}

QColor PLSBasicStatusBarFrameDropState::getStateColor() const
{
	return stateColor;
}

void PLSBasicStatusBarFrameDropState::setStateColor(const QColor &color)
{
	stateColor = color;
	update();
}

void PLSBasicStatusBarFrameDropState::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);

	QPainterPath path;
	path.addRoundedRect(rect, rect.height() / 2, rect.height() / 2);
	painter.setClipPath(path);

	painter.fillRect(rect, stateColor);
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
	  frameDropState(new PLSBasicStatusBarFrameDropState),
	  cpuUsage(new QLabel(QString("CPU 0% (Total 0%)"))),
	  gpuUsage(new QLabel(QString("GPU 0% (Total 0%)"))),
	  frameDrop(new QLabel(QTStr("DroppedFrames").arg(0).arg(0.0, 0, 'f', 1))),
	  bitrate(new PLSLabel(QTStr("Bitrate").arg(0), false)),
	  statsDropIcon(new QLabel)
{
	CalcCPUUsage::instance();
	installEventFilter(this);
	frameDropState->setVisible(false);

	encodesPt->setObjectName("encodesPt");
	encodingSettingIcon->setObjectName("encodingSettingIcon");
	frameDropState->setObjectName("frameDropState");
	statsDropIcon->setObjectName("statsDropIcon");

	encoding->setProperty("encodingText", true);
	fps->setProperty("encodingText", true);
	cpuUsage->setProperty("statsText", true);
	gpuUsage->setProperty("statsText", true);
	frameDrop->setProperty("statsText", true);
	bitrate->setProperty("statsText", true);

	encoding->setMargin(0);
	encoding->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	cpuUsage->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	gpuUsage->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
	layout21->addWidget(gpuUsage);
	layout21->addSpacing(20);
	layout21->addWidget(frameDrop);
	layout21->addSpacing(20);
	layout21->addWidget(bitrate);
	layout21->addSpacing(9);
	layout21->addWidget(statsDropIcon);
	layout2->addLayout(layout21);

	QHBoxLayout *layout3 = new QHBoxLayout(this);
	layout3->setMargin(0);
	layout3->setSpacing(0);
	layout3->addWidget(encodes, 0, Qt::AlignVCenter | Qt::AlignLeft);
	layout3->addSpacing(25);
	layout3->addWidget(stats, 0, Qt::AlignVCenter | Qt::AlignLeft);
	layout3->addStretch(1);
	layout3->addSpacing(25);

	SetCalculateFixedWidth();

	stats->setCursor(Qt::ArrowCursor);
	encodes->setCursor(Qt::ArrowCursor);

	auto showSettings = []() {
		PLS_UI_STEP(STATUSBAR_MODULE, "Status Bar's Encoding Setting Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Q_ARG(QString, "Output"), Q_ARG(QString, ""));
	};
	connect(encodes, &PLSBasicStatusBarButtonFrame::clicked, this, showSettings);
	connect(stats, &PLSBasicStatusBarButtonFrame::clicked, this, &PLSBasicStatusBar::popupStats);
	setStatsOpen(false);

	PLSDpiHelper dpiHelper;
	dpiHelper.notifyDpiChanged(this, [this]() {
		QMetaObject::invokeMethod(
			this,
			[this] {
				if (g_stats && g_stats->isVisible()) {
					PLSBasic::Get()->popupStats(true, mapToGlobal(QPoint(5, -5)));
				}
			},
			Qt::QueuedConnection);
	});
	dpiHelper.notifyScreenAvailableGeometryChanged(this, [this](const QRect &) {
		QMetaObject::invokeMethod(
			this,
			[this] {
				if (g_stats && g_stats->isVisible()) {
					PLSBasic::Get()->popupStats(true, mapToGlobal(QPoint(5, -5)));
				}
			},
			Qt::QueuedConnection);
	});
}

PLSBasicStatusBar::~PLSBasicStatusBar()
{
	if (uploadTimer) {
		delete uploadTimer;
		uploadTimer = NULL;
	}
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

	if (active) {
		delete refreshTimer;

		frameDropState->setVisible(false);

		auto api = PLSBasic::Get()->getApi();
		if (api) {
			PrismStatus info{};
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE, {QVariant::fromValue<PrismStatus>(info)});
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP, {QVariant::fromValue<PrismStatus>(info)});
		}

		//frameDrop->setText(QTStr("DroppedFrames").arg(0).arg(0.0, 0, 'f', 1));
		//bitrate->setText(QTStr("Bitrate").arg(0));

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

	auto api = PLSBasic::Get()->getApi();
	if (api) {
		PrismStatus info;
		info.kbitsPerSec = kbitsPerSec;
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE, {QVariant::fromValue<PrismStatus>(info)});
	}
	//bitrate->setText(QTStr("Bitrate").arg(kbitsPerSec, 0, 'f', 0));

	lastBytesSent = bytesSent;
	lastBytesSentTime = bytesSentTime;
	bitrateUpdateSeconds = 0;
}

void PLSBasicStatusBar::UpdateCPUUsage(double process, double total)
{
	if (cpuUsage)
		cpuUsage->setText(QString("CPU %1% (Total %2%)").arg(process, 0, 'f', 1).arg(total, 0, 'f', 1));
	auto api = PLSBasic::Get()->getApi();
	if (api) {
		PrismStatus info;
		info.cpuUsage = process;
		info.totalCpu = total;
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE, {QVariant::fromValue<PrismStatus>(info)});
	}
}

void PLSBasicStatusBar::UpdateGPUUsage(double process, double total)
{
	if (gpuUsage)
		gpuUsage->setText(QString("GPU %1% (Total %2%)").arg(process, 0, 'f', 1).arg(total, 0, 'f', 1));
	//auto api = PLSBasic::Get()->getApi();
	//if (api) {
	//	PrismStatus info;
	//	info.cpuUsage = process;
	//	info.totalCpu = total;
	//	api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE, {QVariant::fromValue<PrismStatus>(info)});
	//}
}

void PLSBasicStatusBar::popupStats()
{
	PLS_UI_STEP(STATUSBAR_MODULE, "Status Bar's State Stats Button", ACTION_CLICK);

	PLSBasic::Get()->popupStats(!isStatsOpen(), mapToGlobal(QPoint(5, -5)));
}

int PLSBasicStatusBar::GetRealTimeBitrate(OBSOutput output, RealBitrateHelper &helper)
{
	uint64_t totalBytes = obs_output_get_total_bytes(output);
	uint64_t curTime = os_gettime_ns();
	double kbps = 0;

	if (helper.lastBytesSentTime > 0) {
		uint64_t bitsBetween = (totalBytes - helper.lastBytesSent) * 8;
		long double timePassed = (long double)(curTime - helper.lastBytesSentTime) / 1000000000.0l;
		if (timePassed > 0) {
			kbps = (double)bitsBetween / timePassed / 1000.0l;
		}
	}

	helper.lastBytesSent = totalBytes;
	helper.lastBytesSentTime = curTime;

	return (int)kbps;
}

int PLSBasicStatusBar::GetExpectOutputBitrate(OBSOutput output)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(output);
	if (vencoder) {
		obs_data_t *vsettings = obs_encoder_get_settings(vencoder);
		if (vsettings) {
			int bitrate = obs_data_get_int(vsettings, "bitrate");
			obs_data_release(vsettings);
			return bitrate;
		}
	}
	return 0;
}

void PLSBasicStatusBar::UploadStatus()
{
}

void PLSBasicStatusBar::UpdateDroppedFrames()
{
	if (!streamOutput)
		return;

	int totalFrames = obs_output_get_total_frames(streamOutput);
	if (!totalFrames)
		return;

	int totalDropped = obs_output_get_frames_dropped(streamOutput);
	double percent = (double)totalDropped / (double)totalFrames * 100.0;

	auto api = PLSBasic::Get()->getApi();
	if (api) {
		PrismStatus info;
		info.totalDrop = totalDropped;
		info.dropPercent = percent;
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP, {QVariant::fromValue<PrismStatus>(info)});
	}
	//frameDrop->setText(QTStr("DroppedFrames").arg(totalDropped).arg(percent, 0, 'f', 1));

	float congestion = obs_output_get_congestion(streamOutput);
	float avgCongestion = (congestion + lastCongestion) * 0.5f;
	if (avgCongestion < congestion)
		avgCongestion = congestion;
	if (avgCongestion > 1.0f)
		avgCongestion = 1.0f;

	if (avgCongestion < EPSILON) {
		frameDropState->setStateColor(QColor(0, 0xff, 0));
	} else if (fabsf(avgCongestion - 1.0f) < EPSILON) {
		frameDropState->setStateColor(QColor(0xff, 0, 0));
	} else {
		float red = avgCongestion * 2.0f;
		if (red > 1.0f)
			red = 1.0f;
		red *= 255.0;

		float green = (1.0f - avgCongestion) * 2.0f;
		if (green > 1.0f)
			green = 1.0f;
		green *= 255.0;
		frameDropState->setStateColor(QColor(int(red), int(green), 0));
	}

	lastCongestion = congestion;
}

void PLSBasicStatusBar::SetCalculateFixedWidth()
{
	if (!cpuUsage || !gpuUsage || !frameDrop || !encodes || !frameDropState || !frameDrop || !statsDropIcon) {
		return;
	}
	QFont fontTab = this->font();
	fontTab.setPixelSize(PLSDpiHelper::calculate(this, 12));
	QFontMetrics fontWidth(fontTab);
	cpuUsage->setFixedWidth(fontWidth.width(cpuUsage->text()));
	gpuUsage->setFixedWidth(fontWidth.width(gpuUsage->text()));
	frameDrop->setFixedWidth(fontWidth.width(frameDrop->text()));

	int statsWidth = this->width() - encodes->width() - PLSDpiHelper::calculate(this, 50);
	int bitrateWidth = statsWidth - PLSDpiHelper::calculate(this, 69) - frameDropState->width() - cpuUsage->width() - gpuUsage->width() - frameDrop->width() - statsDropIcon->width();

	if (fontWidth.width(bitrate->Text()) > bitrateWidth) {
		bitrate->setFixedWidth(bitrateWidth);
	} else {
		bitrate->setFixedWidth(fontWidth.width(bitrate->Text()));
	}
}

void PLSBasicStatusBar::OBSOutputReconnect(void *data, calldata_t *params)
{
	PLSBasicStatusBar *statusBar = reinterpret_cast<PLSBasicStatusBar *>(data);

	int seconds = (int)calldata_int(params, "timeout_sec");

	const char *name = calldata_string(params, "name");
	const char *id = calldata_string(params, "id");
	const char *fields[][2] = {{"outputStats", "Reconnect"}};
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s:'%s' reconnecting in %d seconds.", id, name, seconds);

	QMetaObject::invokeMethod(statusBar, "Reconnect", Q_ARG(int, seconds));
	UNUSED_PARAMETER(params);
}

void PLSBasicStatusBar::OBSOutputReconnectSuccess(void *data, calldata_t *params)
{
	PLSBasicStatusBar *statusBar = reinterpret_cast<PLSBasicStatusBar *>(data);

	const char *name = calldata_string(params, "name");
	const char *id = calldata_string(params, "id");
	const char *fields[][2] = {{"outputStats", "Reconnect Success"}};
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s:'%s' successfully reconnected.", id, name);

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
	this->encoding->setText(QString("%1x%2").arg(cx).arg(cy));
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
	pls_flush_style(statsDropIcon, "open", open);

	if (!PLSBasic::Get()) {
		return;
	}
	g_stats = PLSBasic::Get()->stats;
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
	double percentage = total ? double(skipped) / double(total) * 100.0 : 0.0;

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
			PLSBasic::Get()->popupStats(true, mapToGlobal(QPoint(5, -5)));
		} else if (event->type() == QEvent::Resize && g_stats && g_stats->isVisible()) {
			PLSBasic::Get()->popupStats(true, mapToGlobal(QPoint(5, -5)));
		}
	} else if (watched == this) {
		if (event->type() == QEvent::Resize) {
			SetCalculateFixedWidth();
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

void PLSBasicStatusBar::StartStatusMonitor()
{
	if (!uploadTimer) {
		uploadTimer = new QTimer(this);
		connect(uploadTimer, SIGNAL(timeout()), this, SLOT(UploadStatus()));
		uploadTimer->start(UPLOAD_STATUS_INTERVAL);
	}
}

void PLSBasicStatusBar::UpdateDropFrame(double dropFrame, double dropPercent)
{
	if (frameDrop)
		frameDrop->setText(QTStr("DroppedFrames").arg(dropFrame).arg(dropPercent, 0, 'f', 1));
}

void PLSBasicStatusBar::UpdateRealBitrate(double birtare)
{
	if (bitrate)
		bitrate->SetText(QTStr("Bitrate").arg(birtare, 0, 'f', 0));
}
