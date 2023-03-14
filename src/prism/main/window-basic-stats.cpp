#include "frontend-api/frontend-api.h"

#include "window-basic-stats.hpp"
#include "window-basic-main.hpp"
#include "window-basic-status-bar.hpp"
#include "main-view.hpp"
#include "platform.hpp"
#include "pls-app.hpp"
#include "qt-wrappers.hpp"
#include "frontend-api.h"
#include "PLSPerformance/PLSPerfCounter.hpp"
#include "PLSPerformance/PLSPerfHelper.hpp"

#include <QDesktopWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMetaEnum>

#include <string>
#include "pls-gpop-data.hpp"

#define TIMER_INTERVAL 2000
#define REC_TIME_LEFT_INTERVAL 30000

extern std::pair<double, double> getCurrentCPUUsage();

struct StreamingNoticeInfo {
	QString tipsInfo;
	std::string fieldValue;
	bool notified{false};

	StreamingNoticeInfo(QString tips, std::string field) : tipsInfo(tips), fieldValue(field) {}
};

static const int StreamingNoticeNum = 10;
// the array order have to same with enum NoticeType
static StreamingNoticeInfo streamingNoticeInfo[StreamingNoticeNum] = {
	StreamingNoticeInfo("Basic.Stats.TransmissionBitrate.Warning", "TransmissionBitrate"), StreamingNoticeInfo("Basic.Stats.TransmissionBitrate.Error", "TransmissionBitrate"),
	StreamingNoticeInfo("Basic.Stats.BufferedDuration.Warning", "BufferedDuration"),       StreamingNoticeInfo("Basic.Stats.BufferedDuration.Error", "BufferedDuration"),
	StreamingNoticeInfo("Basic.Stats.DropNetworkFrame.Warning", "DropNetworkFrame"),       StreamingNoticeInfo("Basic.Stats.DropNetworkFrame.Error", "DropNetworkFrame"),
	StreamingNoticeInfo("Basic.Stats.DropRenderingFrame.Warning", "DropRenderingFrame"),   StreamingNoticeInfo("Basic.Stats.DropRenderingFrame.Error", "DropRenderingFrame"),
	StreamingNoticeInfo("Basic.Stats.DropEncodingFrame.Warning", "DropEncodingFrame"),     StreamingNoticeInfo("Basic.Stats.DropEncodingFrame.Error", "DropEncodingFrame")};

PLSBasicStats::PLSBasicStats(PLSDialogView *dialogView, QWidget *parent, PLSDpiHelper dpiHelper) : QWidget(parent), ui(new Ui::PLSBasicStats), timer(this)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSBasicStats});

	this->dialogView = dialogView;
	if (dialogView) {
		dialogView->installEventFilter(this);
		dialogView->setAttribute(Qt::WA_AlwaysShowToolTips, true);
	}

	ui->setupUi(this);

	ui->realtimeBitrate->setToolTip(QTStr("Basic.Stats.RealtimeBitrate.ToolTip"));
	ui->dynamicBitrate->setToolTip(QTStr("Basic.Stats.DynamicBitrate.ToolTip"));
	ui->desiredBitrate->setToolTip(QTStr("Basic.Stats.DesiredBitrate.ToolTip"));

	outputLabels.push_back(OutputLabels());
	outputLabels.push_back(OutputLabels());

	PLSPerf::PerfCounter::Instance();

	QObject::connect(&timer, &QTimer::timeout, this, [=]() {
		Update();
		if (dialogView)
			dialogView->sizeToContent();
	});

	timer.setInterval(TIMER_INTERVAL);
	timer.start();

	Update();
}

PLSBasicStats::~PLSBasicStats()
{
	delete shortcutFilter;
}

static uint32_t first_encoded = 0xFFFFFFFF;
static uint32_t first_skipped = 0xFFFFFFFF;
static uint32_t first_rendered = 0xFFFFFFFF;
static uint32_t first_lagged = 0xFFFFFFFF;
static bool basic_stats_ready = false;
static bool previous_active_status = false;
static uint32_t ignore_encoder_skipped_frames = 0;
static uint32_t ignore_render_skipped_frames = 0;

void PLSBasicStats::InitializeValues(bool ready)
{
	video_t *video = obs_get_video();
	first_encoded = video_output_get_total_frames(video);
	first_skipped = video_output_get_skipped_frames(video);
	first_rendered = obs_get_total_frames();
	first_lagged = obs_get_lagged_frames();
	ignore_render_skipped_frames = 0;
	ignore_encoder_skipped_frames = 0;
	if (ready)
		basic_stats_ready = ready;
}

void PLSBasicStats::UpdatePerfStats()
{
	PLSPerf::PerfStats perfStats;
	bool ret = PLSPerf::PerfCounter::Instance().GetPerfStats(perfStats);
	if (ret) {
		double maxProcessUsage = 0.0;
		int index = 0;
		for (int i = 0; i < (int)std::log2((double)PLSPerf::ENGINE_TYPE_COUNT); i++) {
			if (perfStats.process.gpu.engines[i].usage > maxProcessUsage) {
				maxProcessUsage = perfStats.process.gpu.engines[i].usage;
				index = i;
			}
		}

		double sysUsage = 0.0;
		for (int i = 0; i < perfStats.system.gpus.size(); i++)
			sysUsage += perfStats.system.gpus[i].engines[index].usage;
		if (sysUsage > 100.0)
			sysUsage = 100.0;

		QString str;
		str = QString::number(maxProcessUsage, 'f', 1) + QStringLiteral("%");
		str = " " + str;
		str = PLSPerf::GetEngineName(perfStats.process.gpu.engines[index].type).c_str() + str;
		ui->gpuUsage->setText(str);
		PLSMainView *mainview = qobject_cast<PLSMainView *>(pls_get_main_view());
		if (mainview)
			mainview->statusBar()->UpdateGPUUsage(maxProcessUsage, sysUsage);
#ifdef ENABLE_FULL_FEATURE
		static const int CHECK_EVERY_10_TIMES = 10;
		static int checkTimes = 0;
		checkTimes++;
		if (0 == checkTimes % CHECK_EVERY_10_TIMES) {
			PLS_INFO(STATUSBAR_MODULE, "************************Performance Stats************************");
			PLS_INFO(STATUSBAR_MODULE, "=============================System=============================");
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------CPU-------------------------------");
			PLS_INFO(STATUSBAR_MODULE, ": CPU Cores           (Logical/Physical) : %d / %d", perfStats.system.cpu.logicalCoreNB, perfStats.system.cpu.physicalCoreNB);
			PLS_INFO(STATUSBAR_MODULE, ": CPU Usage                              : %.2f%% (%.2f%%)", perfStats.system.cpu.usage, perfStats.system.cpu.utility);
			PLS_INFO(STATUSBAR_MODULE, ": Base Frequency                         : %.2f GHz", perfStats.system.cpu.baseFrequencyGHz);
			PLS_INFO(STATUSBAR_MODULE, ": Performance                            : %.2f", perfStats.system.cpu.performance);
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------GPU-------------------------------");
			for (int i = 0; i < perfStats.system.gpus.size(); i++) {
				PLS_INFO(STATUSBAR_MODULE, ": GPU [%s]", perfStats.system.gpus[i].name.c_str());
				PLS_INFO(STATUSBAR_MODULE, "    :: Shared Memory    (Used/Total, MB) : %d / %d", perfStats.system.gpus[i].sharedMemory.usedMB,
					 perfStats.system.gpus[i].sharedMemory.totalMB);
				PLS_INFO(STATUSBAR_MODULE, "    :: Dedicated Memory (Used/Total, MB) : %d / %d", perfStats.system.gpus[i].dedicatedMemory.usedMB,
					 perfStats.system.gpus[i].dedicatedMemory.totalMB);
				PLS_INFO(STATUSBAR_MODULE, "    :: Engine Usage (%%) ");
				PLS_INFO(STATUSBAR_MODULE, "        ::: 3D                           : %.2f", perfStats.system.gpus[i].engines[0].usage);
				PLS_INFO(STATUSBAR_MODULE, "        ::: Video Decode                 : %.2f", perfStats.system.gpus[i].engines[1].usage);
				PLS_INFO(STATUSBAR_MODULE, "        ::: Video Encode                 : %.2f", perfStats.system.gpus[i].engines[2].usage);
				PLS_INFO(STATUSBAR_MODULE, "        ::: Copy                         : %.2f", perfStats.system.gpus[i].engines[3].usage);
				PLS_INFO(STATUSBAR_MODULE, "        ::: Compute                      : %.2f", perfStats.system.gpus[i].engines[4].usage);
			}
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------Memory----------------------------");
			PLS_INFO(STATUSBAR_MODULE, ": Memory                (Free/Total, MB) : %d / %d", perfStats.system.memory.physicalMemory.freeMB, perfStats.system.memory.physicalMemory.totalMB);
			PLS_INFO(STATUSBAR_MODULE, ": Committed/Commit Limit            (MB) : %d / %d", perfStats.system.memory.committedMB, perfStats.system.memory.commitLimitMB);
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------Network---------------------------");
			PLS_INFO(STATUSBAR_MODULE, ": Interface [%s]", perfStats.system.network.name.c_str());
			PLS_INFO(STATUSBAR_MODULE, ": Bandwidth                              : %d MB", perfStats.system.network.bandwidthMB);
			PLS_INFO(STATUSBAR_MODULE, ": Sent                                   : %.2f Kbps", perfStats.system.network.sentKbps);
			PLS_INFO(STATUSBAR_MODULE, ": Received                               : %.2f Kbps", perfStats.system.network.receivedKbps);
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------Disk------------------------------");
			for (int i = 0; i < perfStats.system.disk.physicalDisks.size(); i++) {
				PLS_INFO(STATUSBAR_MODULE, ": Disk [%d, %s]", perfStats.system.disk.physicalDisks[i].index, perfStats.system.disk.physicalDisks[i].name.c_str());
				PLS_INFO(STATUSBAR_MODULE, "    :: Total Storage                     : %d GB", perfStats.system.disk.physicalDisks[i].totalGB);
				PLS_INFO(STATUSBAR_MODULE, "    :: Read Speed                        : %.2f KBps", perfStats.system.disk.physicalDisks[i].readKBps);
				PLS_INFO(STATUSBAR_MODULE, "    :: Write Speed                       : %.2f KBps", perfStats.system.disk.physicalDisks[i].writeKBps);
			}
			PLS_INFO(STATUSBAR_MODULE, "=============================Process============================");
			PLS_INFO(STATUSBAR_MODULE, ": Process [%d, %s]", perfStats.process.pid, perfStats.process.name.c_str());
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------CPU-------------------------------");
			PLS_INFO(STATUSBAR_MODULE, ": Usage                                  : %.2f%%", perfStats.process.cpu.usage);
			PLS_INFO(STATUSBAR_MODULE, ": Handles                                : %d", perfStats.process.cpu.handleNB);
			PLS_INFO(STATUSBAR_MODULE, ": Threads                                : %d", perfStats.process.cpu.threadNB);
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------GPU-------------------------------");
			PLS_INFO(STATUSBAR_MODULE, ": Shared Memory                          : %d MB", perfStats.process.gpu.sharedMemoryMB);
			PLS_INFO(STATUSBAR_MODULE, ": Dedicated Memory                       : %d MB", perfStats.process.gpu.dedicatedMemoryMB);
			PLS_INFO(STATUSBAR_MODULE, ": Engine Usage (%%)");
			PLS_INFO(STATUSBAR_MODULE, "    :: 3D                                : %.2f", perfStats.process.gpu.engines[0].usage);
			PLS_INFO(STATUSBAR_MODULE, "    :: Video Decode                      : %.2f", perfStats.process.gpu.engines[1].usage);
			PLS_INFO(STATUSBAR_MODULE, "    :: Video Encode                      : %.2f", perfStats.process.gpu.engines[2].usage);
			PLS_INFO(STATUSBAR_MODULE, "    :: Copy                              : %.2f", perfStats.process.gpu.engines[3].usage);
			PLS_INFO(STATUSBAR_MODULE, "    :: Compute                           : %.2f", perfStats.process.gpu.engines[4].usage);
			PLS_INFO(STATUSBAR_MODULE, "-----------------------------Memory----------------------------");
			PLS_INFO(STATUSBAR_MODULE, ": Working Set - Private                  : %d MB", perfStats.process.memory.workingSetPrivateMB);
			PLS_INFO(STATUSBAR_MODULE, ": Committed                              : %d MB", perfStats.process.memory.committedMB);
		}
#endif
	}
}

void PLSBasicStats::Update()
{
	if (!basic_stats_ready)
		return;

	if (resetting) {
		resetting = false;
		ResetInternal();
	}

	PLSBasic *main = PLSBasic::Get();

	OBSOutput streamingOutput = obs_frontend_get_streaming_output();
	OBSOutput recordingOutput = obs_frontend_get_recording_output();
	obs_output_release(streamingOutput);
	obs_output_release(recordingOutput);

	if (!streamingOutput && !recordingOutput)
		return;

	QString str;
	auto usage = getCurrentCPUUsage();
	str = QString::number(usage.second, 'f', 1) + QStringLiteral("%");
	ui->cpuUsage->setText(str);
	PLSMainView *mainview = qobject_cast<PLSMainView *>(pls_get_main_view());
	if (mainview)
		mainview->statusBar()->UpdateCPUUsage(usage.second, usage.first);

	UpdatePerfStats();

	const char *path = main->GetCurrentOutputPath();

#define MBYTE (1024ULL * 1024ULL)
#define GBYTE (1024ULL * 1024ULL * 1024ULL)
#define TBYTE (1024ULL * 1024ULL * 1024ULL * 1024ULL)
	num_bytes = os_get_free_disk_space(path);
	QString abrv = QStringLiteral(" MB");
	long double num;

	num = (long double)num_bytes / (1024.0l * 1024.0l);
	if (num_bytes > TBYTE) {
		num /= 1024.0l * 1024.0l;
		abrv = QStringLiteral(" TB");
	} else if (num_bytes > GBYTE) {
		num /= 1024.0l;
		abrv = QStringLiteral(" GB");
	}

	str = QString::number(num, 'f', 1) + abrv;
	ui->diskAvailable->setText(str);

	num = (long double)os_get_proc_resident_size() / (1024.0l * 1024.0l);

	str = QString::number(num, 'f', 1) + QStringLiteral(" MB");
	ui->memoryUsage->setText(str);

	// drop encoding frame
	bool active = obs_output_active(streamingOutput) || obs_output_active(recordingOutput);
	video_t *video = obs_get_video();
	uint32_t total_encoded = video_output_get_total_frames(video);
	uint32_t total_skipped = video_output_get_skipped_frames(video);

	if (total_encoded < first_encoded || total_skipped < first_skipped) {
		first_encoded = total_encoded;
		first_skipped = total_skipped;
	}
	total_encoded -= first_encoded;
	total_skipped -= first_skipped;

	/* When output(s) changes from inactive to active, the first three "total_skipped" may be a non-zero value when using some hardware encoders.
	 * It should be ignored when calculating whether prompt is needed, otherwise, the "error"/"warning" may be prompted as soon as it turned active. */
	static const int IGNORE_TIMES = 5;
	static int ignore_skipped_encoder = 0;
	if (!previous_active_status && active) {
		ignore_skipped_encoder = IGNORE_TIMES;
	} else if (previous_active_status && !active) {
		// reset encoder_skipped_frames_ignore and ignore_skipped when it turns inactive from active.
		ignore_encoder_skipped_frames = 0;
		ignore_skipped_encoder = 0;
	} else if (total_skipped < ignore_encoder_skipped_frames) {
		// encoder and it's statistics may be reset during the streaming, if that happen, we should reset the encoder skipped frames
		ignore_skipped_encoder = IGNORE_TIMES;
	}

	if (ignore_skipped_encoder && --ignore_skipped_encoder >= 0) {
		// save ignore frame counts to ignore_encoder_skipped_frames.
		ignore_encoder_skipped_frames = total_skipped;
		if (!ignore_skipped_encoder)
			PLS_INFO(STATUSBAR_MODULE, "[Stats] Ignore %d encoder skipped frames, total encoded %d.", ignore_encoder_skipped_frames, total_encoded);
	}

	previous_active_status = active;

	// ignore ignore_encoder_skipped_frames when calculating drop percentage.
	if (ignore_encoder_skipped_frames > 0 && total_skipped >= ignore_encoder_skipped_frames)
		total_skipped -= ignore_encoder_skipped_frames;

	num = total_encoded ? (long double)total_skipped / (long double)total_encoded : 0.0l;
	num *= 100.0l;

	pls_flush_style(ui->encodingFramedropLabel, "active", active);
	str = QString("%1 / %2 (%3%)").arg(QString::number(total_skipped), QString::number(total_encoded), QString::number(num, 'f', 1));
	ui->encodingFramedrop->setText(str);
	pls_flush_style(ui->encodingFramedrop, "active", active);
	long double error_threshold = PLSGpopData::instance()->getDropEncodingFramePrecentThreshold() * 100.0l;
	if (!active) {
		pls_flush_style(ui->encodingFramedropState, "state", "disabled");
		PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, true);
		PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, true);
	} else if (num > error_threshold) {
		pls_flush_style(ui->encodingFramedropState, "state", "error");
		PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, false, total_skipped, num);
	} else if (num > 30.0l) {
		pls_flush_style(ui->encodingFramedropState, "state", "warning");
		PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, false, total_skipped, num);
	} else {
		pls_flush_style(ui->encodingFramedropState, "state", "");
		PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, true);
		PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, true);
	}

	// rendering frame drop
	uint32_t total_rendered = obs_get_total_frames();
	uint32_t total_lagged = obs_get_lagged_frames();

	if (total_rendered < first_rendered || total_lagged < first_lagged) {
		first_rendered = total_rendered;
		first_lagged = total_lagged;
	}
	total_rendered -= first_rendered;
	total_lagged -= first_lagged;

	/* The render frame drop may have the same problem with encoder frame drop when app is first starting,
	 * so we ignore these dropped frames when calculating whether prompt is needed. */
	static int ignore_skipped_render = IGNORE_TIMES;
	if (ignore_skipped_render && --ignore_skipped_render >= 0) {
		// save ignore frame counts to ignore_render_skipped_frames.
		ignore_render_skipped_frames = total_lagged;
		if (!ignore_skipped_render)
			PLS_INFO(STATUSBAR_MODULE, "[Stats] Ignore %d render skipped frames, total rendered %d.", ignore_render_skipped_frames, total_rendered);
	}

	// ignore ignore_render_skipped_frames when calculating drop percentage.
	if (ignore_render_skipped_frames > 0 && total_lagged >= ignore_render_skipped_frames)
		total_lagged -= ignore_render_skipped_frames;

	num = total_rendered ? (long double)total_lagged / (long double)total_rendered : 0.0l;
	num *= 100.0l;

	str = QString("%1 / %2 (%3%)").arg(QString::number(total_lagged), QString::number(total_rendered), QString::number(num, 'f', 1));
	ui->renderingFramedrop->setText(str);
	error_threshold = PLSGpopData::instance()->getDropRenderingFramePrecentThreshold() * 100.0l;
	if (num > error_threshold) {
		pls_flush_style(ui->renderingFramedropState, "state", "error");
		PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameError, false, total_lagged, num);
	} else if (num > 30.0l) {
		pls_flush_style(ui->renderingFramedropState, "state", "warning");
		PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameWarning, false, total_lagged, num);
	} else {
		pls_flush_style(ui->renderingFramedropState, "state", "");
		PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameWarning, true);
		PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameError, true);
	}

	// realtime fps
	struct obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	double renderFPS = (ovi.fps_den > 0) ? ((double)ovi.fps_num / (double)ovi.fps_den) : 0;
	double realRenderFPS = obs_get_active_fps();
	str = QString("%1 / %2 fps").arg(QString::number(realRenderFPS, 'f', 2), QString::number(renderFPS, 'f', 2));
	ui->realtimeFramerate->setText(str);
	num = renderFPS ? (long double)realRenderFPS / (long double)renderFPS : 0.0l;
	if (num < 0.95l)
		pls_flush_style(ui->realtimeFramerateState, "state", "error");
	else if (num < 0.99l)
		pls_flush_style(ui->realtimeFramerateState, "state", "warning");
	else
		pls_flush_style(ui->realtimeFramerateState, "state", "");

	/* recording/streaming stats */
	outputLabels[0].Update(this, ui, streamingOutput, false);
	outputLabels[1].Update(this, ui, recordingOutput, true);
}

void PLSBasicStats::PopupNotice(StreamingNoticeType type, bool recoverNormal, int dropFrame, double dropPercentFrame)
{
	int noticeNumber = static_cast<int>(type);
	if (noticeNumber < 0 || noticeNumber >= StreamingNoticeNum) {
		return;
	}

	StreamingNoticeInfo &info = streamingNoticeInfo[noticeNumber];
	QString dropStr = "";
	if (type == StreamingNoticeType::NoticeDropNetworkFrameError || type == StreamingNoticeType::NoticeDropNetworkFrameWarning || type == StreamingNoticeType::NoticeDropRenderingFrameError ||
	    type == StreamingNoticeType::NoticeDropRenderingFrameWarning || type == StreamingNoticeType::NoticeDropEncodingFrameWarning || type == StreamingNoticeType::NoticeDropEncodingFrameError) {
		dropStr = QTStr(info.tipsInfo.toStdString().c_str()).arg(QString::number(dropFrame), QString::number(dropPercentFrame, 'f', 1)).toStdString().c_str();
	} else if (type == StreamingNoticeType::NoticeBufferedDurationWarning || type == StreamingNoticeType::NoticeBufferedDurationError) {
		dropStr = QTStr(info.tipsInfo.toStdString().c_str()).arg(QString::number(dropFrame)).toStdString().c_str();
	}

	// recover to normal
	if (recoverNormal) {
		if (info.notified) {
			PLS_INFO(STATUSBAR_MODULE, "[Stats] %s state recoverd.", info.tipsInfo.toStdString().c_str());
		}
		info.notified = false;
		return;
	}

	// already notified
	if (info.notified) {
		return;
	}

	// warning
	QString noticeStr = !dropStr.isEmpty() ? dropStr : QTStr(info.tipsInfo.toStdString().c_str());
	if (0 == noticeNumber % 2) {
		info.notified = true;
		PLS_INFO(STATUSBAR_MODULE, "[Stats] Notify : %s <%s>.", info.tipsInfo.toStdString().c_str(), noticeStr.toStdString().c_str());
		noticeStr.replace("%", "%%");
		return;
	}

	// error
	info.notified = true;

	const char *fields[][2] = {{"dropFrameStats", info.fieldValue.c_str()}};
	PLS_LOGEX(PLS_LOG_INFO, STATUSBAR_MODULE, fields, 1, "[Stats] Toast : %s <%s>.", info.tipsInfo.toStdString().c_str(), noticeStr.toStdString().c_str());
	pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, noticeStr);

	// not notice warning info when error happened.
	SetNotified(static_cast<StreamingNoticeType>(noticeNumber - 1), true);
}

void PLSBasicStats::SetNotified(StreamingNoticeType type, bool notified)
{
	int noticeNumber = static_cast<int>(type);
	if (noticeNumber < 0 || noticeNumber >= StreamingNoticeNum) {
		return;
	}

	StreamingNoticeInfo &info = streamingNoticeInfo[noticeNumber];
	info.notified = notified;
}

void PLSBasicStats::Reset()
{
	if (!timer.isActive()) {
		timer.start();
	}

	resetting = true;
}

void PLSBasicStats::ResetInternal()
{
	first_encoded = 0xFFFFFFFF;
	first_skipped = 0xFFFFFFFF;
	first_rendered = 0xFFFFFFFF;
	first_lagged = 0xFFFFFFFF;

	ignore_render_skipped_frames = 0;
	ignore_encoder_skipped_frames = 0;

	OBSOutput strOutput = obs_frontend_get_streaming_output();
	OBSOutput recOutput = obs_frontend_get_recording_output();
	obs_output_release(strOutput);
	obs_output_release(recOutput);

	outputLabels[0].Reset(strOutput);
	outputLabels[1].Reset(recOutput);

	PLS_INFO(STATUSBAR_MODULE, "[Stats] Status reset.");
}

static int GetExpectBitrate(OBSOutput output)
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

void PLSBasicStats::OutputLabels::Update(PLSBasicStats *stats, Ui::PLSBasicStats *ui, obs_output_t *output, bool rec)
{
	if (!stats) {
		return;
	}

	PLSMainView *main = qobject_cast<PLSMainView *>(pls_get_main_view());
	if (main) {
		main->statusBar()->SetCalculateFixedWidth();
	}

	if (!output) {
		if (!rec) {
			pls_flush_style(ui->streamBitrateLabel, "active", false);
			pls_flush_style(ui->streamBitrate, "active", false);
			pls_flush_style(ui->realtimeBitrate, "active", false);
			pls_flush_style(ui->dynamicBitrate, "active", false);
			pls_flush_style(ui->desiredBitrate, "active", false);
			pls_flush_style(ui->streamBitrateState, "state", "disabled");
			stats->PopupNotice(StreamingNoticeType::NoticeTransmissionBitrateWarning, true);

			pls_flush_style(ui->networkFramedropLabel, "active", false);
			pls_flush_style(ui->networkFramedrop, "active", false);
			pls_flush_style(ui->networkFramedropState, "state", "disabled");
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameError, true);
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameWarning, true);

			pls_flush_style(ui->bufferedDurationLabel, "active", false);
			pls_flush_style(ui->bufferedDuration, "active", false);
			pls_flush_style(ui->bufferedDurationState, "state", "disabled");
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationError, true);
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationWarning, true);
		} else {
			pls_flush_style(ui->recordingBitrateLabel, "active", false);
			pls_flush_style(ui->recordingBitrate, "active", false);

			pls_flush_style(ui->recordedMegabytesLabel, "active", false);
			pls_flush_style(ui->recordedMegabytes, "active", false);
		}
		ui->realtimeBitrate->setText("0 / ");
		ui->dynamicBitrate->setText("0 / ");
		ui->desiredBitrate->setText("0");
		if (main) {
			main->statusBar()->UpdateRealBitrate(0);
		}
		return;
	}

	bool active = obs_output_active(output);
	uint64_t totalBytes = output ? obs_output_get_total_bytes(output) : 0;
	uint64_t curTime = os_gettime_ns();
	uint64_t bytesSent = totalBytes;

	if (bytesSent < lastBytesSent)
		bytesSent = 0;
	if (bytesSent == 0)
		lastBytesSent = 0;

	uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;
	long double timePassed = (long double)(curTime - lastBytesSentTime) / 1000000000.0l;
	kbps = (long double)bitsBetween / timePassed / 1000.0l;

	if (timePassed < 0.01l)
		kbps = 0.0l;

	if (!rec) {
		// stream bitrate
		pls_flush_style(ui->streamBitrateLabel, "active", active);
		int dbrStreamBitrate = obs_output_get_dbr_bitrate(output);
		bool rtmpStreaming = false;
		const char *steamingOutputID = obs_output_get_id(output);
		if (steamingOutputID && steamingOutputID[0])
			rtmpStreaming = !strcmp(steamingOutputID, "rtmp_output");
		int expectBitrate = rtmpStreaming ? obs_output_get_orig_bitrate(output) : GetExpectBitrate(output);
		if (main) {
			main->statusBar()->UpdateRealBitrate(kbps);
		}
		ui->realtimeBitrate->setText(QString::number(kbps, 'f', 0) + " / ");
		ui->dynamicBitrate->setText(QString::number(dbrStreamBitrate) + " / ");
		ui->desiredBitrate->setText(QString::number(expectBitrate));
		pls_flush_style(ui->streamBitrate, "active", active);
		pls_flush_style(ui->realtimeBitrate, "active", active);
		pls_flush_style(ui->dynamicBitrate, "active", active);
		pls_flush_style(ui->desiredBitrate, "active", active);

		int refBitrate = dbrStreamBitrate < expectBitrate ? dbrStreamBitrate : expectBitrate;
		long double num = refBitrate ? (long double)kbps / (long double)refBitrate : 0.0l;
		if (ui->streamBitrateState) {
			if (!active) {
				pls_flush_style(ui->streamBitrateState, "state", "disabled");
			} else if (num < 0.8l) {
				pls_flush_style(ui->streamBitrateState, "state", "error");
			} else if (num < 0.95l) {
				pls_flush_style(ui->streamBitrateState, "state", "warning");
			} else {
				// if dbr bitrate < expected bitrate, it should always be warning or worse ?
				if (dbrStreamBitrate < expectBitrate) {
					pls_flush_style(ui->streamBitrateState, "state", "warning");
				} else {
					pls_flush_style(ui->streamBitrateState, "state", "");
				}
			}

			if (active) {
				if (dbrStreamBitrate < expectBitrate)
					stats->PopupNotice(StreamingNoticeType::NoticeTransmissionBitrateWarning);
				else
					stats->PopupNotice(StreamingNoticeType::NoticeTransmissionBitrateWarning, true);
			} else
				stats->PopupNotice(StreamingNoticeType::NoticeTransmissionBitrateWarning, true);
		}

		// network drop frame
		int total = output ? obs_output_get_total_frames(output) : 0;
		int dropped = output ? obs_output_get_frames_dropped(output) : 0;

		if (total < first_total || dropped < first_dropped) {
			first_total = 0;
			first_dropped = 0;
		}

		total -= first_total;
		dropped -= first_dropped;

		num = total ? (long double)dropped / (long double)total * 100.0l : 0.0l;

		pls_flush_style(ui->networkFramedropLabel, "active", active);
		if (main) {
			main->statusBar()->UpdateDropFrame(dropped, num);
		}
		ui->networkFramedrop->setText(QString("%1 / %2 (%3%)").arg(QString::number(dropped), QString::number(total), QString::number(num, 'f', 1)));
		pls_flush_style(ui->networkFramedrop, "active", active);

		long double error_threshold = PLSGpopData::instance()->getDropNetworkFramePrecentThreshold() * 100.0l;
		if (!active) {
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameError, true);
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameWarning, true);
			pls_flush_style(ui->networkFramedropState, "state", "disabled");
		} else if (num > error_threshold) {
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameError, false, dropped, num);
			pls_flush_style(ui->networkFramedropState, "state", "error");
		} else if (num > 30.0l) {
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameWarning, false, dropped, num);
			pls_flush_style(ui->networkFramedropState, "state", "warning");
		} else {
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameError, true);
			stats->PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameWarning, true);
			pls_flush_style(ui->networkFramedropState, "state", "");
		}

		// buffered duration
		pls_flush_style(ui->bufferedDurationLabel, "active", active);
		int64_t bufferedVideoDuration = 0;
		int64_t bufferedAudioDuration = 0;
		obs_output_get_buffered_duration_usec(output, &bufferedVideoDuration, &bufferedAudioDuration);
		bufferedVideoDuration /= 1000; // to MS
		ui->bufferedDuration->setText(QString("%1 ms").arg(QString::number(bufferedVideoDuration)));
		pls_flush_style(ui->bufferedDuration, "active", active);

		struct obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		long double fpsFrameTime = (ovi.fps_num > 0) ? ((long double)ovi.fps_den * 1000.0l / (long double)ovi.fps_num) : 0;

		/* 200ms is the default point that will trigger dbr,
		 * and if the buffered frame count is less than 5, the network should be treated as normal,
		 * so we set "warn threshold" according to them like blow : */
		long double warnTime = fpsFrameTime ? 5 * fpsFrameTime : 200;

		/* 700ms is the default point that will trigger frame drop,
		 * and 30 is the default frame rate of prism,
		 * so we set "error threshold" according to them like blow : */
		//long double errorTime = fpsFrameTime ? 30 * fpsFrameTime : 700;
		int64_t error_threshold_ms = PLSGpopData::instance()->getBufferedDurationMs();

		if (!active) {
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationError, true);
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationWarning, true);
			pls_flush_style(ui->bufferedDurationState, "state", "disabled");
		} else if (bufferedVideoDuration > error_threshold_ms) {
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationError, false, bufferedVideoDuration);
			pls_flush_style(ui->bufferedDurationState, "state", "error");
		} else if (bufferedVideoDuration > warnTime) {
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationWarning, false, bufferedVideoDuration);
			pls_flush_style(ui->bufferedDurationState, "state", "warning");
		} else {
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationError, true);
			stats->PopupNotice(StreamingNoticeType::NoticeBufferedDurationWarning, true);
			pls_flush_style(ui->bufferedDurationState, "state", "");
		}
	} else {
		pls_flush_style(ui->recordingBitrateLabel, "active", active);
		ui->recordingBitrate->setText(QString("%1 kb/s").arg(QString::number(kbps, 'f', 0)));
		pls_flush_style(ui->recordingBitrate, "active", active);

		pls_flush_style(ui->recordedMegabytesLabel, "active", active);
		long double num = (long double)totalBytes / (1024.0l * 1024.0l);
		ui->recordedMegabytes->setText(QString("%1 MB").arg(QString::number(num, 'f', 1)));
		pls_flush_style(ui->recordedMegabytes, "active", active);
	}

	lastBytesSent = bytesSent;
	lastBytesSentTime = curTime;
}

void PLSBasicStats::OutputLabels::Reset(obs_output_t *output)
{
	if (!output)
		return;

	first_total = obs_output_get_total_frames(output);
	first_dropped = obs_output_get_frames_dropped(output);
	kbps = 0.0;
}

bool PLSBasicStats::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == dialogView) {
		switch (event->type()) {
		case QEvent::Show:
			//timer.start(TIMER_INTERVAL);
			emit showSignal();
			break;
		case QEvent::Hide:
			//timer.stop();// do not need stop timer because of window alarm
			emit hideSignal();
			break;
		}
	}
	return QWidget::eventFilter(watched, event);
}

void PLSBasicStats::mousePressEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);

	if (event->button() == Qt::LeftButton) {
		grabMouse();
	}
}

void PLSBasicStats::mouseReleaseEvent(QMouseEvent *event)
{
	QWidget::mousePressEvent(event);

	if (event->button() == Qt::LeftButton) {
		releaseMouse();

		if (dialogView->geometry().contains(event->globalPos())) {
			dialogView->hide();
		}
	}
}
