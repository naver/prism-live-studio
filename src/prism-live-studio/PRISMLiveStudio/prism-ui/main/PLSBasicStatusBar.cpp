#include "PLSBasicStatusBar.hpp"
#include "PLSBasicStatusPanel.hpp"
#include "GoLivePannel.h"
#include "PLSPreviewTitle.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>

#ifdef Q_OS_WIN
#include <Windows.h>
#else
#include <dns_sd.h>
#endif

#include <QPainterPath>

#include <util/platform.h>
#include <tuple>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include <libutils-api.h>
#include <liblog.h>
#include "log/module_names.h"

#include "../obs-ui/obs-app.hpp"
#include "../obs-ui/window-basic-main.hpp"
#include "PLSBasic.h"
#include "PLSPlatformPrism.h"
#include "frontend-api.h"
#include "login-user-info.hpp"
#ifdef _WIN32
#include "PLSPerformance/win32/PLSPerfCounter.hpp"
#include "pls/pls-vcam-host-name.hpp"
#elif defined(__APPLE__)

#endif
#include <pls/pls-dual-output.h>

using namespace std;

const auto UPLOAD_STATUS_INTERVAL = 3000;
static int nsec_to_ms(uint64_t nsec)
{
	return (int)(nsec / 1000000LL);
}

static std::string nsec_to_ms_s(uint64_t nsec)
{
	return std::to_string(nsec / 1000000LL);
}

PLSBasicStatusBarFrameDropState::PLSBasicStatusBarFrameDropState(QWidget *parent) : QFrame(parent) {}

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
	setAttribute(Qt::WA_NativeWindow);
	setProperty("ui-step.customButton", true);
	setProperty("showHandCursor", true);
	setMouseTracking(true);
}

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
		if (QRect(mapToGlobal(QPoint(0, 0)), size()).contains(event->globalPosition().toPoint())) {
			emit clicked();
		}
		pls_flush_style_recursive(this, "state", "");
	}
}

void PLSBasicStatusBarButtonFrame::mouseMoveEvent(QMouseEvent *event)
{
	QFrame::mouseMoveEvent(event);

	isLeftMouseButtonPress = isLeftMouseButtonPress && (event->buttons() & Qt::LeftButton);

	if (isEnabled()) {
		if (QRect(mapToGlobal(QPoint(0, 0)), size()).contains(event->globalPosition().toPoint())) {
			pls_flush_style_recursive(this, "state", isLeftMouseButtonPress ? "pressed" : "hover");
		} else {
			pls_flush_style_recursive(this, "state", "");
		}
	}
}

void PLSBasicStatusBarButtonFrame::enterEvent(QEnterEvent *event)
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

PLSBasicStatusBar::PLSBasicStatusBar(QWidget *parent) : PLSTransparentForMouseEvents<QFrame>(parent)
{
	encodes = pls_new<PLSBasicStatusBarButtonFrame>();
	encoding = pls_new<QLabel>();
	encodesPt = pls_new<QLabel>();
	fps = pls_new<QLabel>();
	encodingSettingIcon = pls_new<QLabel>();
	stats = pls_new<PLSBasicStatusBarButtonFrame>();
	frameDropState = pls_new<PLSBasicStatusBarFrameDropState>();
	cpuUsage = pls_new<QLabel>(QString("CPU 0% (Total 0%)"));
	renderTime = pls_new<QLabel>(QTStr("Basic.Stats.AverageTimeToRender") + QStringLiteral(" 0ms"));
#if _WIN32
	gpuUsage = pls_new<QLabel>(QString("GPU 0% (Total 0%)"));
#endif
	frameDrop = pls_new<QLabel>(QTStr("DroppedFrames").arg(0).arg(0.0, 0, 'f', 1));
	bitrate = pls_new<QLabel>(QTStr("Basic.Stats.BitrateValue").arg(0));
	statsDropIcon = pls_new<QLabel>();

	frameDropState->setVisible(false);

	encodesPt->setObjectName("encodesPt");
	encodingSettingIcon->setObjectName("encodingSettingIcon");
	frameDropState->setObjectName("frameDropState");
	statsDropIcon->setObjectName("statsDropIcon");

	encoding->setProperty("encodingText", true);
	fps->setProperty("encodingText", true);
	cpuUsage->setProperty("statsText", true);
	renderTime->setProperty("statsText", true);
#if _WIN32
	gpuUsage->setProperty("statsText", true);
#endif
	frameDrop->setProperty("statsText", true);
	bitrate->setProperty("statsText", true);

	encoding->setMargin(0);
	encoding->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	cpuUsage->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	renderTime->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
#if _WIN32
	gpuUsage->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
#endif
	frameDrop->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	bitrate->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	QHBoxLayout *layout1 = pls_new<QHBoxLayout>(encodes);
	layout1->setContentsMargins(25, 0, 0, 0);
	layout1->setSpacing(5);

	QHBoxLayout *layout11 = pls_new<QHBoxLayout>();
	layout11->setContentsMargins(0, 0, 0, 0);
	layout11->setSpacing(5);
	layout11->addWidget(encoding, 0, Qt::AlignVCenter);
	layout11->addWidget(encodesPt, 0, Qt::AlignVCenter);
	layout11->addWidget(fps, 0, Qt::AlignVCenter);

	layout1->addLayout(layout11);
	layout1->addWidget(encodingSettingIcon, 0, Qt::AlignVCenter);
	encodingSettingIcon->setStyleSheet("margin-top: 1px");

	QHBoxLayout *layout2 = pls_new<QHBoxLayout>(stats);
	layout2->setContentsMargins(0, 0, 0, 0);
	layout2->setSpacing(10);
	layout2->addWidget(frameDropState);

	QHBoxLayout *layout21 = pls_new<QHBoxLayout>();
	layout21->setContentsMargins(0, 0, 0, 0);
	layout21->setSpacing(0);
	layout21->addWidget(cpuUsage);
	// do not display in status bar
#if 0
	layout21->addSpacing(20);
	layout21->addWidget(renderTime);
	layout21->addSpacing(20);
	layout21->addWidget(gpuUsage);
	layout21->addSpacing(20);
	layout21->addWidget(frameDrop);
	layout21->addSpacing(20);
	layout21->addWidget(bitrate);
#endif
	layout21->addSpacing(9);
	layout21->addWidget(statsDropIcon);
	layout2->addLayout(layout21);

	QHBoxLayout *layout3 = pls_new<QHBoxLayout>(this);
	layout3->setContentsMargins(0, 0, 0, 0);
	layout3->setSpacing(0);
	layout3->addWidget(encodes, 0, Qt::AlignVCenter | Qt::AlignLeft);
	layout3->addSpacing(25);
	layout3->addWidget(stats, 0, Qt::AlignVCenter | Qt::AlignLeft);
	layout3->addStretch(1);

	liveArea = pls_new<PLSPreviewLiveLabel>(true, true, this);
	liveArea->SetSceneNameVisible(false);
	layout3->addWidget(liveArea);
	goliveWid = pls_new<GoLivePannel>();
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::holdOnGolive, goliveWid, &GoLivePannel::holdOnAll, Qt::QueuedConnection);
	layout3->addWidget(goliveWid);

	stats->setCursor(Qt::ArrowCursor);
	encodes->setCursor(Qt::ArrowCursor);

	pls_set_css(this, {"PLSBasicStatusBar"});

	setStatsOpen(false);
	OnLiveStatus(false);
	OnRecordStatus(false);

	auto showSettings = []() { QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Q_ARG(QString, "Output"), Q_ARG(QString, "")); };
	connect(encodes, &PLSBasicStatusBarButtonFrame::clicked, this, showSettings);
	connect(qobject_cast<PLSMainView *>(pls_get_toplevel_view(this)), &PLSMainView::onGolivePending, encodes, &QWidget::setDisabled);
	connect(stats, &PLSBasicStatusBarButtonFrame::clicked, this, [this] { PLSBasic::instance()->toggleStatusPanel(-1); });
}

PLSBasicStatusBar::~PLSBasicStatusBar()
{
#ifdef _WIN32
	PLSPerf::PerfCounter::Instance().Stop();
#elif defined(__APPLE__)

#endif

	if (uploadTimer) {
		uploadTimer->deleteLater();
		uploadTimer = nullptr;
	}
}

void PLSBasicStatusBar::Activate()
{
	if (!active) {
		frameDropState->setVisible(true);

		active = true;
	}
}

void PLSBasicStatusBar::Deactivate()
{
	if (active) {
		frameDropState->setVisible(false);

		auto api = PLSBasic::instance()->getApi();
		if (api) {
			PrismStatus info{};
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE, {QVariant::fromValue<PrismStatus>(info)});
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP, {QVariant::fromValue<PrismStatus>(info)});
		}

		UpdateDropFrame(0, 0);
		UpdateRealBitrate(0);

		delaySecTotal = 0;
		delaySecStarting = 0;
		delaySecStopping = 0;
		reconnectTimeout = 0;
		active = false;
	}
}

void PLSBasicStatusBar::UpdateCPUUsage()
{
	auto num = OBSBasic::Get()->GetCPUUsage();
	m_dataStatus.cpu = make_tuple(num, num);

	if (cpuUsage) {
		cpuUsage->setText(QString("CPU %1%").arg(num, 0, 'f', 1));
	}

	auto api = PLSBasic::instance()->getApi();
	if (api) {
		PrismStatus info;
		info.cpuUsage = num;
		info.totalCpu = num;
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE, {QVariant::fromValue<PrismStatus>(info)});
	}

	num = (double)obs_get_average_frame_time_ns() / 1000000.0;
	m_dataStatus.renderTime = num;

	if (renderTime) {
		renderTime->setText(QString("%1 %2ms").arg(QTStr("Basic.Stats.AverageTimeToRender"), QString::number(num, 'f', 1)));
	}

	UpdateGPUUsage();

	if (active) {
		UpdateStatusBar();
	}

	PLSBasic::instance()->updateStatusPanel(m_dataStatus);
}

void PLSBasicStatusBar::UpdateGPUUsage()
{
#ifdef _WIN32
	double maxProcessUsage = 0.0;
	auto sysUsage = 0.0;

	PLSPerf::PerfStats perfStats;
	if (!PLSPerf::PerfCounter::Instance().GetPerfStats(perfStats)) {
		return;
	}

	int index = 0;
	for (int i = 0; i < (int)std::log2((double)PLSPerf::ENGINE_TYPE_COUNT); i++) {
		if (perfStats.process.gpu.engines[i].usage > maxProcessUsage) {
			maxProcessUsage = perfStats.process.gpu.engines[i].usage;
			index = i;
		}
	}

	for (int i = 0; i < perfStats.system.gpus.size(); i++)
		sysUsage += perfStats.system.gpus[i].engines[index].usage;
	if (sysUsage > 100.0)
		sysUsage = 100.0;

	m_dataStatus.gpu = make_tuple(maxProcessUsage, sysUsage);

	if (gpuUsage)
		gpuUsage->setText(QString("GPU %1%").arg(maxProcessUsage, 0, 'f', 1));
#endif
}

void PLSBasicStatusBar::OnLiveStatus(bool isStarted)
{
	pls_async_call_mt([this, isStarted]() { liveArea->SetLiveStatus(isStarted); });
}

void PLSBasicStatusBar::OnRecordStatus(bool isStarted)
{
	pls_async_call_mt([this, isStarted]() { liveArea->SetRecordStatus(isStarted); });
}

void PLSBasicStatusBar::OnRecordPaused(bool paused)
{
	return liveArea->OnRecordPaused(paused);
}

uint PLSBasicStatusBar::getStartTime() const
{
	return liveArea->GetStartTime();
}

int PLSBasicStatusBar::getRecordDuration() const
{
	return liveArea->GetRecordDuration();
}

void PLSBasicStatusBar::UploadStatus() const
{

}

void PLSBasicStatusBar::UpdateDroppedFrames()
{
	if (!streamOutput)
		return;

	auto totalDropped = get<0>(m_dataStatus.dropedNetwork);
	auto totalFrames = get<1>(m_dataStatus.dropedNetwork);
	auto percent = get<2>(m_dataStatus.dropedNetwork);

	if (!totalFrames)
		return;

	UpdateDropFrame(totalDropped, percent);

	/* ----------------------------------- *
	 * calculate congestion color          */

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
		QPixmap pixmap(20, 20);

		float red = avgCongestion * 2.0f;
		if (red > 1.0f)
			red = 1.0f;
		red *= 255;

		float green = (1.0f - avgCongestion) * 2.0f;
		if (green > 1.0f)
			green = 1.0f;
		green *= 255;

		frameDropState->setStateColor(QColor(int(red), int(green), 0));
	}

	lastCongestion = congestion;
}

void PLSBasicStatusBar::OBSOutputReconnect(void *data, calldata_t *params)
{
	auto statusBar = reinterpret_cast<OBSBasicStatusBar *>(data);

	auto seconds = (int)calldata_int(params, "timeout_sec");
	auto name = calldata_string(params, "name");
	auto id = calldata_string(params, "id");
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, {{"outputStats", "Reconnect"}}, "[Output] %s:'%s' reconnecting in %d seconds.", id, name, seconds);

	QMetaObject::invokeMethod(statusBar, "Reconnect", Q_ARG(int, seconds));
}

void PLSBasicStatusBar::OBSOutputReconnectSuccess(void *data, calldata_t *params)
{
	auto statusBar = reinterpret_cast<OBSBasicStatusBar *>(data);

	auto name = calldata_string(params, "name");
	auto id = calldata_string(params, "id");
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, {{"outputStats", "Reconnect Success"}}, "[Output] %s:'%s' successfully reconnected.", id, name);

	QMetaObject::invokeMethod(statusBar, "ReconnectSuccess");
}

void PLSBasicStatusBar::Reconnect(int seconds)
{
	OBSBasic *main = OBSBasic::Get();

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
	delaySecTotal = 0;
	UpdateDelayMsg();
}

void PLSBasicStatusBar::setEncoding(int cx, int cy)
{
	this->encoding->setText(QString("%1x%2").arg(cx).arg(cy));
}

void PLSBasicStatusBar::setFps(const QString &fps_)
{
	this->fps->setText(fps_);
}

bool PLSBasicStatusBar::isStatsOpen() const
{
	return statsDropIcon->property("open").toBool();
}

void PLSBasicStatusBar::setStatsOpen(bool open)
{
	pls_flush_style(statsDropIcon, "open", open);
}

void PLSBasicStatusBar::ReconnectSuccess()
{
	OBSBasic *main = OBSBasic::Get();

	QString msg = QTStr("Basic.StatusBar.ReconnectSuccessful");
	main->SysTrayNotify(msg, QSystemTrayIcon::Information);
	ReconnectClear();

	if (streamOutput) {
		delaySecTotal = obs_output_get_active_delay(streamOutput);
		UpdateDelayMsg();
	}
}

void PLSBasicStatusBar::UpdateStatusBar()
{
	UpdateRealBitrate(get<0>(m_dataStatus.streaming));
	UpdateDroppedFrames();
}

void PLSBasicStatusBar::StreamDelayStarting(int) const {}

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
	Activate();
}

void PLSBasicStatusBar::StreamStopped()
{
	if (streamOutput) {
		signal_handler_disconnect(obs_output_get_signal_handler(streamOutput), "reconnect", OBSOutputReconnect, this);
		signal_handler_disconnect(obs_output_get_signal_handler(streamOutput), "reconnect_success", OBSOutputReconnectSuccess, this);

		ReconnectClear();
		streamOutput = nullptr;
		Deactivate();
	}
}

void PLSBasicStatusBar::RecordingStarted(obs_output_t *output)
{
	recordOutput = output;
}

void PLSBasicStatusBar::RecordingStopped()
{
	recordOutput = nullptr;
}

void PLSBasicStatusBar::UpdateDelayMsg() const
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
		PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "Status upload timer: id=%d, self=%p", uploadTimer->timerId(), uploadTimer.data());
	}
}

void PLSBasicStatusBar::UpdateDropFrame(double dropFrame, double dropPercent)
{
	if (frameDrop) {
		frameDrop->setText(QTStr("DroppedFrames").arg(dropFrame).arg(dropPercent, 0, 'f', 1));
	}

	auto api = PLSBasic::instance()->getApi();
	if (api) {
		PrismStatus info;
		info.totalDrop = dropFrame;
		info.dropPercent = dropPercent;
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP, {QVariant::fromValue<PrismStatus>(info)});
	}
}

void PLSBasicStatusBar::UpdateRealBitrate(double birtare)
{
	if (bitrate) {
		bitrate->setText(QTStr("Basic.Stats.BitrateValue").arg(birtare, 0, 'f', 0));
	}

	auto api = PLSBasic::instance()->getApi();
	if (api) {
		PrismStatus info;
		info.kbitsPerSec = birtare;
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE, {QVariant::fromValue<PrismStatus>(info)});
	}
}
