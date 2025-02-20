#include "PLSBasicStatusPanel.hpp"
#include "ui_PLSBasicStatusPanel.h"
#include "PLSBasicStatusBar.hpp"

#include <QStyle>

#include "utils-api.h"
#include "libui.h"
#include "liblog.h"
#include "PLSBasic.h"
#include "obs-app.hpp"
#include "log/module_names.h"
#include "pls/pls-dual-output.h"

using namespace std;

#define TIMER_INTERVAL (2000)

struct StreamingNoticeInfo {
	const char *tipsInfo;
	const char *fieldValue;
	bool notified = false;

	StreamingNoticeInfo(const char *_tipsInfo, const char *_fieldValue) : tipsInfo(_tipsInfo), fieldValue(_fieldValue) {}
};

static const int StreamingNoticeNum = 8;
struct LocalVars {
	static std::array<StreamingNoticeInfo, StreamingNoticeNum> streamingNoticeInfo;
};
std::array<StreamingNoticeInfo, StreamingNoticeNum> LocalVars::streamingNoticeInfo = {
	StreamingNoticeInfo("Basic.Stats.TransmissionBitrate.Warning", "TransmissionBitrate"), StreamingNoticeInfo("Basic.Stats.TransmissionBitrate.Error", "TransmissionBitrate"),
	StreamingNoticeInfo("Basic.Stats.DropNetworkFrame.Warning", "DropNetworkFrame"),       StreamingNoticeInfo("Basic.Stats.DropNetworkFrame.Error", "DropNetworkFrame"),
	StreamingNoticeInfo("Basic.Stats.DropRenderingFrame.Warning", "DropRenderingFrame"),   StreamingNoticeInfo("Basic.Stats.DropRenderingFrame.Error", "DropRenderingFrame"),
	StreamingNoticeInfo("Basic.Stats.DropEncodingFrame.Warning", "DropEncodingFrame"),     StreamingNoticeInfo("Basic.Stats.DropEncodingFrame.Error", "DropEncodingFrame")};

static QString MakeMissedFramesText(uint32_t total_lagged, uint32_t total_rendered, long double num)
{
	return QString("%1 / %2 (%3%)").arg(QString::number(total_lagged), QString::number(total_rendered), QString::number(num, 'f', 1));
}

PLSBasicStatusPanel::PLSBasicStatusPanel(QWidget *parent)
	: QDialog(parent, Qt::FramelessWindowHint)
{
	ui = pls_new<Ui::PLSBasicStatusPanel>();
	ui->setupUi(this);
	pls_set_css(this, {"PLSBasicStatusPanel"});
	setAttribute(Qt::WA_NativeWindow);

#ifndef _WIN32
	ui->gridLayout_5->removeWidget(ui->gpuUsageLabel);
	ui->gridLayout_5->removeWidget(ui->gpuUsage);
	ui->gridLayout_5->removeWidget(ui->memoryUsageLabel);
	ui->gridLayout_5->removeWidget(ui->memoryUsage);

	ui->gridLayout_5->addWidget(ui->memoryUsageLabel, 2, 0);
	ui->gridLayout_5->addWidget(ui->memoryUsage, 2, 1);
	ui->gridLayout_5->addWidget(ui->gpuUsageLabel, 3, 0);
	ui->gridLayout_5->addWidget(ui->gpuUsage, 3, 1);
	ui->gpuUsageLabel->setText(QString());
	ui->gpuUsage->setText(QString());
#endif

	OutputLabels outputStream = {};
	outputStream.networkStateImage = ui->networkStateImage;
	outputStream.droppedFramesState = ui->networkFramedropState;
	outputStream.droppedFramesLabel = ui->networkFramedropLabel;
	outputStream.droppedFrames = ui->networkFramedrop;
	outputStream.megabytesSentLabel = ui->StreamMegabytesLabel;
	outputStream.megabytesSent = ui->streamMegabytes;
	outputStream.bitrateState = ui->streamBitrateState;
	outputStream.bitrateLabel = ui->streamBitrateLabel;
	outputStream.bitrate = ui->streamBitrate;

	outputStream.networkStateImageV = ui->networkStateImageV;
	outputStream.droppedFramesStateV = ui->networkFramedropStateV;
	outputStream.droppedFramesLabelV = ui->networkFramedropLabelV;
	outputStream.droppedFramesV = ui->networkFramedropV;
	outputStream.megabytesSentLabelV = ui->StreamMegabytesLabelV;
	outputStream.megabytesSentV = ui->streamMegabytesV;
	outputStream.bitrateStateV = ui->streamBitrateStateV;
	outputStream.bitrateLabelV = ui->streamBitrateLabelV;
	outputStream.bitrateV = ui->streamBitrateV;
	outputLabels.append(outputStream);

	OutputLabels outputRecord = {};
	outputRecord.megabytesSentLabel = ui->recordedMegabytesLabel;
	outputRecord.megabytesSent = ui->recordedMegabytes;
	outputRecord.bitrateLabel = ui->recordingBitrateLabel;
	outputRecord.bitrate = ui->recordingBitrate;
	outputLabels.append(outputRecord);

	onDualOutputChanged(pls_is_dual_output_on());
	connect(PLSBasic::instance(), &PLSBasic::sigOpenDualOutput, this, &PLSBasicStatusPanel::onDualOutputChanged);
}

static uint32_t first_encoded = 0xFFFFFFFF;
static uint32_t first_skipped = 0xFFFFFFFF;
static uint32_t first_rendered = 0xFFFFFFFF;
static uint32_t first_lagged = 0xFFFFFFFF;
static uint32_t first_encoded_v = 0xFFFFFFFF;
static uint32_t first_skipped_v = 0xFFFFFFFF;

void PLSBasicStatusPanel::InitializeValues()
{
	const video_t *video = obs_get_video();
	first_encoded = video_output_get_total_frames(video);
	first_skipped = video_output_get_skipped_frames(video);
	first_rendered = obs_get_total_frames();
	first_lagged = obs_get_lagged_frames();

	const video_t *video_v = pls_get_vertical_video_t();
	if (video_v) {
		first_encoded_v = video_output_get_total_frames(video_v);
		first_skipped_v = video_output_get_skipped_frames(video_v);
	}
}

void PLSBasicStatusPanel::setTextAndAlignment(QLabel *widget, const QString &text)
{
	widget->setText(text);
}

void PLSBasicStatusPanel::updateStatusPanel(PLSBasicStatusData &dataStatus)
{
	if (isVisible()) {
		setTextAndAlignment(ui->cpuUsage, QString::number(get<0>(dataStatus.cpu), 'g', 2) + QStringLiteral("%"));
#if _WIN32
		setTextAndAlignment(ui->gpuUsage, QString::number(get<0>(dataStatus.gpu), 'g', 2) + QStringLiteral("%"));
#endif
	}
	auto num = (double)os_get_proc_resident_size() / (1024.0 * 1024.0);
	dataStatus.memory = num;
	if (isVisible()) {
		setTextAndAlignment(ui->memoryUsage, QString::number(num, 'f', 1) + QStringLiteral(" MB"));
	}

	if (isVisible()) {
		const char *path = OBSBasic::Get()->GetCurrentOutputPath();
#define GBYTE (1024ULL * 1024ULL * 1024ULL)
#define TBYTE (1024ULL * 1024ULL * 1024ULL * 1024ULL)
		auto num_bytes = os_get_free_disk_space(path);
		auto abrv = QStringLiteral(" MB");

		num = (long double)num_bytes / (1024.0 * 1024.0);
		dataStatus.disk = num;
		if (num_bytes > TBYTE) {
			num /= 1024.0 * 1024.0;
			abrv = QStringLiteral(" TB");
		} else if (num_bytes > GBYTE) {
			num /= 1024.0;
			abrv = QStringLiteral(" GB");
		}

		setTextAndAlignment(ui->diskAvailable, QString::number(num, 'f', 1) + abrv);
	}

	struct obs_video_info ovi = {};
	obs_get_video_info(&ovi);

	double curFPS = obs_get_active_fps();
	double plsFPS = (double)ovi.fps_num / (double)ovi.fps_den;

	dataStatus.renderFPS = make_tuple(curFPS, plsFPS);

	dataStatus.streamOutputFPS = OBSBasic::Get()->GetStreamingOutputFPS();
	dataStatus.recordOutputFPS = OBSBasic::Get()->GetRecordingOutputFPS();
	dataStatus.streamOutputFPS_v = pls_is_dual_output_on() ? OBSBasic::Get()->GetStreamingOutputFPS(true) : 0.0;

	if (isVisible()) {
		setTextAndAlignment(ui->realtimeFramerate, QString("%1 / %2 fps").arg(QString::number(curFPS, 'f', 2), QString::number(plsFPS, 'f', 2)));

		if (curFPS < (plsFPS * 0.8)) {
			if (State::Error != lastFramerateState) {
				pls_flush_style(ui->realtimeFramerateState, "state", "error");
				lastFramerateState = State::Error;
			}
		} else if (curFPS < (plsFPS * 0.95)) {
			if (State::Warning != lastFramerateState) {
				pls_flush_style(ui->realtimeFramerateState, "state", "warning");
				lastFramerateState = State::Warning;
			}
		} else {
			if (State::Normal != lastFramerateState) {
				pls_flush_style(ui->realtimeFramerateState, "state", "");
				lastFramerateState = State::Normal;
			}
		}
	}

	{
		uint32_t total_rendered = obs_get_total_frames();
		uint32_t total_lagged = obs_get_lagged_frames();

		if (total_rendered < first_rendered || total_lagged < first_lagged) {
			first_rendered = total_rendered;
			first_lagged = total_lagged;
		}
		total_rendered -= first_rendered;
		total_lagged -= first_lagged;

		num = total_rendered ? (double)total_lagged / (double)total_rendered : 0;
		num *= 100;

		dataStatus.dropedRendering = make_tuple(total_lagged, total_rendered, num);

		if (isVisible()) {
			setTextAndAlignment(ui->renderingFramedrop, MakeMissedFramesText(total_lagged, total_rendered, num));
		}

		if (num > 5.0 && total_rendered >= plsFPS * 5) {
			if (State::Error != lastRenderingState) {
				pls_flush_style(ui->renderingFramedropState, "state", "error");
				PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameError, false, total_lagged, num);

				lastRenderingState = State::Error;
			}
		} else if (num > 1.0 && total_rendered >= plsFPS * 5) {
			if (State::Warning != lastRenderingState) {
				pls_flush_style(ui->renderingFramedropState, "state", "warning");
				PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameWarning, false, total_lagged, num);

				lastRenderingState = State::Warning;
			}
		} else {
			if (State::Normal != lastRenderingState) {
				pls_flush_style(ui->renderingFramedropState, "state", "");
				PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameWarning, true);
				PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameError, true);

				lastRenderingState = State::Normal;
			}
		}
	}

	{
		OBSOutputAutoRelease strOutput = obs_frontend_get_streaming_output();
		OBSOutputAutoRelease recOutput = obs_frontend_get_recording_output();

		if (!strOutput && !recOutput)
			return;

		const auto active = obs_output_active(strOutput) || obs_output_active(recOutput);

		const video_t *video = obs_get_video();
		if (!video)
			return;

		uint32_t total_encoded = video_output_get_total_frames(video);
		uint32_t total_skipped = video_output_get_skipped_frames(video);

		if (total_encoded < first_encoded || total_skipped < first_skipped) {
			first_encoded = total_encoded;
			first_skipped = total_skipped;
		}
		total_encoded -= first_encoded;
		total_skipped -= first_skipped;

		num = total_encoded ? (double)total_skipped / (double)total_encoded : 0;
		num *= 100;

		dataStatus.dropedEncoding = make_tuple(total_skipped, total_encoded, num);

		if (isVisible()) {
			setTextAndAlignment(ui->encodingFramedrop, QString("%1 / %2 (%3%)").arg(QString::number(total_skipped), QString::number(total_encoded), QString::number(num, 'f', 1)));

			if (active != lastActive) {
				pls_flush_style(ui->encodingFramedropImage, "active", active);
				pls_flush_style(ui->encodingFramedropLabel, "active", active);
				pls_flush_style(ui->encodingFramedrop, "active", active);
				pls_flush_style(ui->encodingFramedropState, "active", active);

				lastActive = active;
			}
		}

		if (num > 5.0 && total_encoded >= plsFPS * 5) {
			if (State::Error != lastEncodingState) {
				pls_flush_style(ui->encodingFramedropState, "state", "error");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, false, total_skipped, num);

				lastEncodingState = State::Error;
			}
		} else if (num > 1.0 && total_encoded >= plsFPS * 5) {
			if (State::Warning != lastEncodingState) {
				pls_flush_style(ui->encodingFramedropState, "state", "warning");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, false, total_skipped, num);

				lastEncodingState = State::Warning;
			}
		} else {
			if (State::Normal != lastEncodingState) {
				pls_flush_style(ui->encodingFramedropState, "state", "");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, true);
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, true);

				lastEncodingState = State::Normal;
			}
		}

		outputLabels[0].Update(strOutput, false, dataStatus);
		outputLabels[1].Update(recOutput, true, dataStatus);
		// get the rtmp latency statistics
		if (active && strOutput)
			pls_statistics_get_dump(strOutput, &dataStatus.latencyInfo);
		else
			dataStatus.latencyInfo = {0};
	}

	do {
		if (!pls_is_dual_output_on()) {
			break;
		}

		OBSOutputAutoRelease strOutput_v = pls_frontend_get_streaming_output_v();
		if (!strOutput_v)
			break;

		const video_t *video_v = pls_get_vertical_video_t();
		if (!video_v)
			break;

		const auto active = obs_output_active(strOutput_v);

		uint32_t total_encoded = video_output_get_total_frames(video_v);
		uint32_t total_skipped = video_output_get_skipped_frames(video_v);

		if (total_encoded < first_encoded_v || total_skipped < first_skipped_v) {
			first_encoded_v = total_encoded;
			first_skipped_v = total_skipped;
		}
		total_encoded -= first_encoded_v;
		total_skipped -= first_skipped_v;

		num = total_encoded ? (double)total_skipped / (double)total_encoded : 0;
		num *= 100;

		if (isVisible()) {
			setTextAndAlignment(ui->encodingFramedropV, QString("%1 / %2 (%3%)").arg(QString::number(total_skipped), QString::number(total_encoded), QString::number(num, 'f', 1)));

			if (active != lastActiveV) {
				pls_flush_style(ui->encodingFramedropImageV, "active", active);
				pls_flush_style(ui->encodingFramedropLabelV, "active", active);
				pls_flush_style(ui->encodingFramedropV, "active", active);
				pls_flush_style(ui->encodingFramedropStateV, "active", active);

				lastActiveV = active;
			}
		}

		if (num > 5.0 && total_encoded >= plsFPS * 5) {
			if (State::Error != lastEncodingStateV) {
				pls_flush_style(ui->encodingFramedropStateV, "state", "error");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, false, total_skipped, num);

				lastEncodingStateV = State::Error;
			}
		} else if (num > 1.0 && total_encoded >= plsFPS * 5) {
			if (State::Warning != lastEncodingStateV) {
				pls_flush_style(ui->encodingFramedropStateV, "state", "warning");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, false, total_skipped, num);

				lastEncodingStateV = State::Warning;
			}
		} else {
			if (State::Normal != lastEncodingStateV) {
				pls_flush_style(ui->encodingFramedropStateV, "state", "");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameWarning, true);
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, true);

				lastEncodingStateV = State::Normal;
			}
		}
	} while (false);

	// for dual output
	do{
		if (!pls_is_dual_output_on()) {
			dataStatus.dropedEncoding_v = {0, 0, 0.0};
			dataStatus.dropedNetwork_v = {0, 0, 0.0};
			dataStatus.streaming_v = {0.0, 0};
			dataStatus.latencyInfo_v = {0};
			first_encoded_v = 0;
			first_skipped_v = 0;
			outputLabels[0].Reset_v(nullptr);
			break;
		}

		OBSOutputAutoRelease strOutput_v = pls_frontend_get_streaming_output_v();
		if (!strOutput_v)
			break;

		const video_t *video_v = pls_get_vertical_video_t();
		if (!video_v)
			break;

		uint32_t total_encoded = video_output_get_total_frames(video_v);
		uint32_t total_skipped = video_output_get_skipped_frames(video_v);

		if (total_encoded < first_encoded_v || total_skipped < first_skipped_v) {
			first_encoded_v = total_encoded;
			first_skipped_v = total_skipped;
		}
		total_encoded -= first_encoded_v;
		total_skipped -= first_skipped_v;

		num = total_encoded ? (double)total_skipped / (double)total_encoded : 0;
		num *= 100;

		dataStatus.dropedEncoding_v = make_tuple(total_skipped, total_encoded, num);

		outputLabels[0].Update_v(strOutput_v, dataStatus);
		// get the rtmp latency statistics
		const auto active = obs_output_active(strOutput_v);
		if (active)
			pls_statistics_get_dump(strOutput_v, &dataStatus.latencyInfo_v);
	} while (false);

	auto numSkiped = get<1>(dataStatus.dropedNetwork);
	auto numPercent = get<2>(dataStatus.dropedNetwork);
	if (numPercent > 5) {
		PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameError, false, numSkiped, numPercent);
	} else if (numPercent > 1) {
		PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameWarning, false, numSkiped, numPercent);
	} else {
		PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameWarning, true);
		PopupNotice(StreamingNoticeType::NoticeDropNetworkFrameError, true);
	}
}

void PLSBasicStatusPanel::PopupNotice(StreamingNoticeType type, bool recoverNormal, int64_t dropFrame, double dropPercentFrame) const
{
	auto noticeNumber = static_cast<int>(type);
	if (noticeNumber < 0 || noticeNumber >= StreamingNoticeNum) {
		return;
	}

	StreamingNoticeInfo &info = LocalVars::streamingNoticeInfo[noticeNumber];
	QString dropStr;
	if (type == StreamingNoticeType::NoticeDropNetworkFrameError || type == StreamingNoticeType::NoticeDropNetworkFrameWarning || type == StreamingNoticeType::NoticeDropRenderingFrameError ||
	    type == StreamingNoticeType::NoticeDropRenderingFrameWarning || type == StreamingNoticeType::NoticeDropEncodingFrameWarning || type == StreamingNoticeType::NoticeDropEncodingFrameError) {
		dropStr = QTStr(info.tipsInfo).arg(QString::number(dropFrame), QString::number(dropPercentFrame, 'f', 1));
	}

	if (recoverNormal) {
		if (info.notified) {
			PLS_INFO(STATUSBAR_MODULE, "[Stats] %s state recoverd.", info.tipsInfo);
		}
		info.notified = false;
		return;
	}

	if (info.notified) {
		return;
	}

	QString noticeStr = !dropStr.isEmpty() ? dropStr : QTStr(info.tipsInfo);
	if (0 == noticeNumber % 2) {
		info.notified = true;
		PLS_INFO(STATUSBAR_MODULE, "[Stats] Notify : %s <%s>.", info.tipsInfo, noticeStr.toStdString().c_str());
		return;
	}

	info.notified = true;

	PLS_LOGEX(PLS_LOG_INFO, STATUSBAR_MODULE, {{"dropFrameStats", info.fieldValue}}, "[Stats] Toast : %s <%s>.", info.tipsInfo, noticeStr.toStdString().c_str());
	pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, noticeStr);

	switch (type) {
	case StreamingNoticeType::NoticeTransmissionBitrateError:
		SetNotified(StreamingNoticeType::NoticeTransmissionBitrateWarning, true);
		break;
	case StreamingNoticeType::NoticeDropNetworkFrameError:
		SetNotified(StreamingNoticeType::NoticeDropNetworkFrameWarning, true);
		break;
	case StreamingNoticeType::NoticeDropRenderingFrameError:
		SetNotified(StreamingNoticeType::NoticeDropRenderingFrameWarning, true);
		break;
	case StreamingNoticeType::NoticeDropEncodingFrameError:
		SetNotified(StreamingNoticeType::NoticeDropEncodingFrameWarning, true);
		break;
	default:
		break;
	}
}

void PLSBasicStatusPanel::SetNotified(StreamingNoticeType type, bool notified) const
{
	auto noticeNumber = static_cast<int>(type);
	if (noticeNumber < 0 || noticeNumber >= StreamingNoticeNum) {
		return;
	}

	StreamingNoticeInfo &info = LocalVars::streamingNoticeInfo[noticeNumber];
	info.notified = notified;
}

void PLSBasicStatusPanel::OutputLabels::Update(const obs_output_t *output, bool rec, PLSBasicStatusData &dataStatus)
{
	auto active = output ? obs_output_active(output) : false;
	auto totalBytes = active ? obs_output_get_total_bytes(output) : 0;
	auto curTime = os_gettime_ns();
	auto bytesSent = totalBytes;

	if (bytesSent < lastBytesSent)
		bytesSent = 0;
	if (bytesSent == 0)
		lastBytesSent = 0;

	auto bitsBetween = (bytesSent - lastBytesSent) * 8;
	auto timePassed = (double)(curTime - lastBytesSentTime) / 1000000000.0;
	kbps = (double)bitsBetween / timePassed / 1000.0;

	if (timePassed < 0.01)
		kbps = 0.0;

	auto num = (double)totalBytes / (1024 * 1024);

	if (rec) {
		dataStatus.recording = make_tuple(kbps, num);
	} else {
		dataStatus.streaming = make_tuple(kbps, num);
	}

	if (active != lastActive) {
		pls_flush_style(megabytesSentLabel, "active", active);
		pls_flush_style(megabytesSent, "active", active);
		if (bitrateState) {
			pls_flush_style(bitrateState, "active", active);
		}
		pls_flush_style(bitrateLabel, "active", active);
		pls_flush_style(bitrate, "active", active);
	}

	if (megabytesSentLabel->isVisible()) {
		const char *unit = "MiB";
		if (num > 1024) {
			num /= 1024;
			unit = "GiB";
		}
		setTextAndAlignment(megabytesSent, QString("%1 %2").arg(num, 0, 'f', 1).arg(unit));

		num = kbps;
		unit = "kb/s";
		if (num >= 10'000) {
			num /= 1000;
			unit = "Mb/s";
		}
		setTextAndAlignment(bitrate, QString("%1 %2").arg(num, 0, 'f', 0).arg(unit));
	}

	if (!rec) {
		int total = active ? obs_output_get_total_frames(output) : 0;
		int dropped = active ? obs_output_get_frames_dropped(output) : 0;

		if (total < first_total || dropped < first_dropped) {
			first_total = 0;
			first_dropped = 0;
		}

		total -= first_total;
		dropped -= first_dropped;

		num = total ? (double)dropped / (double)total * 100 : 0;

		dataStatus.dropedNetwork = make_tuple(dropped, total, num);

		if (active != lastActive) {
			pls_flush_style(networkStateImage, "active", active);
			pls_flush_style(droppedFramesState, "active", active);
			pls_flush_style(droppedFramesLabel, "active", active);
			pls_flush_style(droppedFrames, "active", active);
		}

		if (droppedFrames->isVisible()) {
			auto str = QString("%1 / %2 (%3%)").arg(QString::number(dropped), QString::number(total), QString::number(num, 'f', 1));
			setTextAndAlignment(droppedFrames, str);

			if (num > 5.0) {
				if (State::Error != lastState) {
					pls_flush_style(droppedFramesState, "state", "error");
					lastState = State::Error;
				}
			} else if (num > 1.0) {
				if (State::Warning != lastState) {
					pls_flush_style(droppedFramesState, "state", "warning");
					lastState = State::Warning;
				}
			} else {
				if (State::Normal != lastState) {
					pls_flush_style(droppedFramesState, "state", "");
					lastState = State::Normal;
				}
			}
		}
	}

	lastActive = active;

	lastBytesSent = bytesSent;
	lastBytesSentTime = curTime;
}

void PLSBasicStatusPanel::OutputLabels::Reset(const obs_output_t *output)
{
	if (!output)
		return;

	first_total = obs_output_get_total_frames(output);
	first_dropped = obs_output_get_frames_dropped(output);
}

void PLSBasicStatusPanel::OutputLabels::Update_v(const obs_output_t *output_v, PLSBasicStatusData &dataStatus) 
{
	auto active = output_v ? obs_output_active(output_v) : false;
	auto totalBytes = active ? obs_output_get_total_bytes(output_v) : 0;
	auto curTime = os_gettime_ns();
	auto bytesSent = totalBytes;

	if (bytesSent < lastBytesSent_v)
		bytesSent = 0;
	if (bytesSent == 0)
		lastBytesSent_v = 0;

	auto bitsBetween = (bytesSent - lastBytesSent_v) * 8;
	auto timePassed = (double)(curTime - lastBytesSentTime_v) / 1000000000.0;
	kbps_v = (double)bitsBetween / timePassed / 1000.0;

	if (timePassed < 0.01)
		kbps_v = 0.0;

	auto num = (double)totalBytes / (1024 * 1024);

	dataStatus.streaming_v = make_tuple(kbps_v, num);

	if (active != lastActiveV) {
		pls_flush_style(megabytesSentLabelV, "active", active);
		pls_flush_style(megabytesSentV, "active", active);
		if (bitrateStateV) {
			pls_flush_style(bitrateStateV, "active", active);
		}
		pls_flush_style(bitrateLabelV, "active", active);
		pls_flush_style(bitrateV, "active", active);
	}

	if (megabytesSentLabelV->isVisible()) {
		const char *unit = "MiB";
		if (num > 1024) {
			num /= 1024;
			unit = "GiB";
		}
		setTextAndAlignment(megabytesSentV, QString("%1 %2").arg(num, 0, 'f', 1).arg(unit));

		num = kbps_v;
		unit = "kb/s";
		if (num >= 10'000) {
			num /= 1000;
			unit = "Mb/s";
		}
		setTextAndAlignment(bitrateV, QString("%1 %2").arg(num, 0, 'f', 0).arg(unit));
	}

	int total = active ? obs_output_get_total_frames(output_v) : 0;
	int dropped = active ? obs_output_get_frames_dropped(output_v) : 0;

	if (total < first_total_v || dropped < first_dropped_v) {
		first_total_v = 0;
		first_dropped_v = 0;
	}

	total -= first_total_v;
	dropped -= first_dropped_v;

	num = total ? (double)dropped / (double)total * 100 : 0;

	dataStatus.dropedNetwork_v = make_tuple(dropped, total, num);

	if (active != lastActiveV) {
		pls_flush_style(networkStateImageV, "active", active);
		pls_flush_style(droppedFramesStateV, "active", active);
		pls_flush_style(droppedFramesLabelV, "active", active);
		pls_flush_style(droppedFramesV, "active", active);
	}

	if (droppedFramesV->isVisible()) {
		auto str = QString("%1 / %2 (%3%)").arg(QString::number(dropped), QString::number(total), QString::number(num, 'f', 1));
		setTextAndAlignment(droppedFramesV, str);

		if (num > 5.0) {
			if (State::Error != lastState) {
				pls_flush_style(droppedFramesStateV, "state", "error");
				lastState = State::Error;
			}
		} else if (num > 1.0) {
			if (State::Warning != lastState) {
				pls_flush_style(droppedFramesStateV, "state", "warning");
				lastState = State::Warning;
			}
		} else {
			if (State::Normal != lastState) {
				pls_flush_style(droppedFramesStateV, "state", "");
				lastState = State::Normal;
			}
		}
	}

	lastActiveV = active;

	lastBytesSent_v = bytesSent;
	lastBytesSentTime_v = curTime;
}

void PLSBasicStatusPanel::OutputLabels::Reset_v(const obs_output_t *output_v)
{
	first_total_v = output_v ? obs_output_get_total_frames(output_v) : 0;
	first_dropped_v = output_v ? obs_output_get_frames_dropped(output_v) : 0;
}

void PLSBasicStatusPanel::showEvent(QShowEvent *event)
{
#if defined(Q_OS_MACOS)
    PLSCustomMacWindow::addCurrentWindowToParentWindow(this);
#endif
    QDialog::showEvent(event);
}

void PLSBasicStatusPanel::onDualOutputChanged(bool bDualOutput) {
	setFixedHeight(bDualOutput ? 240 : 180);

	ui->encodingFramedropImage->setVisible(bDualOutput);
	ui->encodingFramedropStateV->setVisible(bDualOutput);
	ui->encodingFramedropImageV->setVisible(bDualOutput);
	ui->encodingFramedropLabelV->setVisible(bDualOutput);
	ui->encodingFramedropV->setVisible(bDualOutput);

	ui->networkStateImage->setVisible(bDualOutput);
	ui->networkStateImageV->setVisible(bDualOutput);

	ui->networkFramedropStateV->setVisible(bDualOutput);
	ui->networkFramedropLabelV->setVisible(bDualOutput);
	ui->networkFramedropV->setVisible(bDualOutput);

	ui->streamBitrateStateV->setVisible(bDualOutput);
	ui->streamBitrateLabelV->setVisible(bDualOutput);
	ui->streamBitrateV->setVisible(bDualOutput);

	ui->StreamMegabytesLabelV->setVisible(bDualOutput);
	ui->streamMegabytesV->setVisible(bDualOutput);

	if (bDualOutput) {
		ui->verticalSpacer_3->changeSize(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
		ui->verticalSpacer_4->changeSize(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
		ui->verticalSpacer_5->changeSize(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
	} else {
		ui->verticalSpacer_3->changeSize(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
		ui->verticalSpacer_4->changeSize(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
		ui->verticalSpacer_5->changeSize(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
	}

	PLSBasic::instance()->moveStatusPanel();
}
