#include "PLSBasicStatusPanel.hpp"
#ifdef __APPLE__
#include "ui_PLSBasicStatusPanel_mac.h"
#else
#include "ui_PLSBasicStatusPanel.h"
#endif
#include "PLSBasicStatusBar.hpp"

#include <QStyle>

#include "utils-api.h"
#include "libui.h"
#include "liblog.h"
#include "PLSBasic.h"
#include "obs-app.hpp"
#include "log/module_names.h"

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

PLSBasicStatusPanel::PLSBasicStatusPanel(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSBasicStatusPanel>();
	ui->setupUi(this);
	pls_set_css(this, {"PLSBasicStatusPanel"});
	setAttribute(Qt::WA_NativeWindow);

	OutputLabels outputStream = {};
	outputStream.droppedFramesState = ui->networkFramedropState;
	outputStream.droppedFramesLabel = ui->networkFramedropLabel;
	outputStream.droppedFrames = ui->networkFramedrop;
	outputStream.megabytesSentLabel = ui->StreamMegabytesLabel;
	outputStream.megabytesSent = ui->streamMegabytes;
	outputStream.bitrateState = ui->streamBitrateState;
	outputStream.bitrateLabel = ui->streamBitrateLabel;
	outputStream.bitrate = ui->streamBitrate;
	outputLabels.append(outputStream);

	OutputLabels outputRecord = {};
	outputRecord.megabytesSentLabel = ui->recordedMegabytesLabel;
	outputRecord.megabytesSent = ui->recordedMegabytes;
	outputRecord.bitrateLabel = ui->recordingBitrateLabel;
	outputRecord.bitrate = ui->recordingBitrate;
	outputLabels.append(outputRecord);
}

static uint32_t first_encoded = 0xFFFFFFFF;
static uint32_t first_skipped = 0xFFFFFFFF;
static uint32_t first_rendered = 0xFFFFFFFF;
static uint32_t first_lagged = 0xFFFFFFFF;

void PLSBasicStatusPanel::InitializeValues()
{
	const video_t *video = obs_get_video();
	first_encoded = video_output_get_total_frames(video);
	first_skipped = video_output_get_skipped_frames(video);
	first_rendered = obs_get_total_frames();
	first_lagged = obs_get_lagged_frames();
}

void PLSBasicStatusPanel::setTextAndAlignment(PLSLabel *widget, const QString &text)
{
	widget->SetText(text);
	widget->setAlignment(Qt::AlignVCenter | (widget->toolTip().isEmpty() ? Qt::AlignRight : Qt::AlignLeft));
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

		if (num > 5.0) {
			if (State::Error != lastRenderingState) {
				pls_flush_style(ui->renderingFramedropState, "state", "error");
				PopupNotice(StreamingNoticeType::NoticeDropRenderingFrameError, false, total_lagged, num);

				lastRenderingState = State::Error;
			}
		} else if (num > 1.0) {
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
				pls_flush_style(ui->encodingFramedropLabel, "active", active);
				pls_flush_style(ui->encodingFramedrop, "active", active);

				lastActive = active;
			}
		}

		if (num > 5.0) {
			if (State::Error != lastEncodingState) {
				pls_flush_style(ui->encodingFramedropState, "state", "error");
				PopupNotice(StreamingNoticeType::NoticeDropEncodingFrameError, false, total_skipped, num);

				lastEncodingState = State::Error;
			}
		} else if (num > 1.0) {
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
	}

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
		setTextAndAlignment(megabytesSent, QString("%1 MB").arg(QString::number(num, 'f', 1)));
		setTextAndAlignment(bitrate,QString("%1 kb/s").arg(QString::number(kbps, 'f', 0)));
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