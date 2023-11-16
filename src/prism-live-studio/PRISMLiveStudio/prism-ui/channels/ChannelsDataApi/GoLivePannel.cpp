#include "GoLivePannel.h"
#include <qscopeguard.h>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSAddingFrame.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSPlatformApi.h"
#include "frontend-api.h"
#include "pls-channel-const.h"
#include "prism/PLSPlatformPrism.h"
#include "ui_GoLivePannel.h"

using namespace ChannelData;

GoLivePannel::GoLivePannel(QWidget *parent) : QFrame(parent), ui(new Ui::GoLivePannel)
{

	ui->setupUi(this);
	pls_add_css(this, {"GoLivePannel"});

	mBusyFrame = new PLSAddingFrame(ui->GoLiveShift);
	mBusyFrame->setObjectName("LoadingFrame");
	mBusyFrame->setContent("");
	mBusyFrame->setSourceFirstFile(g_goliveLoadingPixPath);
	auto *layout = new QHBoxLayout(ui->GoLiveShift);
	layout->addWidget(mBusyFrame);
	layout->setContentsMargins(0, 0, 0, 0);
	ui->GoLiveShift->setLayout(layout);
	mBusyFrame->hide();

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, this, &GoLivePannel::updateRecordButton);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this, &GoLivePannel::updateGoliveButton);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveTypeChanged, this, [this]() {
		auto state = PLSCHANNELS_API->currentBroadcastState();
		updateGoliveButton(state);
	});

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcast, ui->GoLiveShift, [this]() { toggleBroadcast(true); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopBroadcast, ui->GoLiveShift, [this]() { toggleBroadcast(false); });

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartRecord, ui->Record, [this]() { toggleRecord(true); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopRecord, ui->Record, [this]() { toggleRecord(false); });
}

GoLivePannel::~GoLivePannel()
{
	delete ui;
}

void GoLivePannel::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void GoLivePannel::on_Record_toggled(bool isCheck)
{
	PLS_LIVE_UI_STEP("channels", isCheck ? ("Reocrd Start") : ("Record Stop "), "clicked ");
	ui->Record->setDisabled(true);
	toggleRecord(isCheck);
}

void GoLivePannel::toggleRecord(bool isStart)
{
	if (isEnteredRecord) {
		PRE_LOG_MSG_STEP(" Wait for record state changing .... new event is ignored!", g_recordStep, INFO)
		return;
	}
	HolderReleaser holder(&GoLivePannel::setEnteredRecord, this);
	bool isRecording = PLSCHANNELS_API->isRecording();
	int state = PLSCHANNELS_API->currentReocrdState();

	//state has been shifting to

	auto ignoreChange = [&]() {
		PRE_LOG_MSG_STEP(QString(" Record state is going to be %1 ,new event is ignored!").arg(isStart ? " ON " : "OFF"), g_recordStep, INFO)
		updateRecordButton(state);
		return;
	};
	if (isRecording == isStart) {
		ignoreChange();
		return;
	}
	QString msg = QString("Try to %1 record,current record state: ").arg(isStart ? "start " : " stop ") + RecordStatesMap[state];
	PRE_LOG_MSG_STEP(msg, g_recordStep, INFO)

	switch (state) {
	case RecordReady:
	case RecordStopped:
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->sigTrySetRecordState(CanRecord);
		break;
	case RecordStarted:
		PLSCHANNELS_API->setIsClickToStopRecord(true);
		PLSCHANNELS_API->setIsOnlyStopRecord(true);
		PLSCHANNELS_API->sigTrySetRecordState(RecordStopGo);
		break;
	default:
		ignoreChange();
		break;
	}
}

void GoLivePannel::updateRecordButton(int state)
{
	switch (state) {
	case RecordStarted:
		if (!ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(true);
		}
		ui->Record->setText(CHANNELS_TR(STOP));
		ui->Record->setDisabled(false);
		break;
	case RecordReady:
	case CanRecord:
	case RecordStopped:
		if (ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(false);
		}
		ui->Record->setText(CHANNELS_TR(REC));
		ui->Record->setDisabled(false);
		break;

	case RecordStarting:
	case RecordStopping:
	case RecordStopGo:
		ui->Record->setDisabled(true);
		break;
	default:
		break;
	}
}

void GoLivePannel::holdOnAll(bool holdOn)
{
	mBusyFrame->setVisible(holdOn);
	if (holdOn) {
		mBusyFrame->start(200);

	} else {
		mBusyFrame->stop();
	}
}

bool GoLivePannel::confirmToContinue() const
{
	bool isClosing = pls_is_main_window_closing();
	if (isClosing) {
		return true;
	}
	if (pls_previous_broadcast_control_by() == ControlSrcType::RemoteControl) {
		return true;
	}
	//only check naver shopping
	auto selected = PLSCHANNELS_API->getCurrentSelectedPlatformChannels(NAVER_SHOPPING_LIVE, NoType);
	if (selected.isEmpty()) {
		return true;
	}
	auto islastMuted = pls_mixer_is_all_mute();
	if (!islastMuted) {
		pls_mixer_mute_all(true);
	}

	quint64 time = 10 * 1000;
	auto msg = PLSCHANNELS_API->isRehearsaling() ? tr("golive.endrehearsal.confirm") : tr("golive.endlive.confirm");
	auto toEndLive = PLSCHANNELS_API->isRehearsaling() ? tr("yes.endRehearsal") : tr("yes.endlive");
	auto notEnd = PLSCHANNELS_API->isRehearsaling() ? tr("no.continue.rehearsal") : tr("no.continue.live");
	//according ux, continue left,end right,so use cancel and apply .

	auto ret = PLSAlertView::questionWithCountdownView(pls_get_main_view(), tr("Confirm"), msg, "", {{PLSAlertView::Button::Cancel, notEnd}, {PLSAlertView::Button::Apply, toEndLive}},
							   PLSAlertView::Button::Apply, time, 170);

	if (islastMuted != pls_mixer_is_all_mute()) {
		pls_mixer_mute_all(islastMuted);
	}
	if (ret.button == PLSAlertView::Button::Apply) {
		return true;
	}
	return false;
}

void GoLivePannel::on_GoLiveShift_toggled(bool isGolive)
{
	QString finishStr = PLSCHANNELS_API->isRehearsaling() ? "Finish rehearsal " : "Finish live ";
	PLS_LIVE_UI_STEP("channels", isGolive ? ("Golive") : finishStr.toUtf8().constData(), "clicked ");
	if (!isGolive) {
		PLS_PLATFORM_API->sendLiveAnalog(true);
	}
	ui->GoLiveShift->setDisabled(true);
	holdOnAll(true);
	toggleBroadcast(isGolive);
}

void GoLivePannel::toggleBroadcast(bool toStart)
{
	auto cleanup = qScopeGuard([]() { pls_set_broadcast_control(ControlSrcType::None); });
	if (isEnteredGolive) {
		PRE_LOG_MSG_STEP("ignore toggle broadcasting,it has entered ", g_LiveStep, INFO)
		return;
	}

	HolderReleaser holder(&GoLivePannel::setEnteredGolive, this);

	int state = PLSCHANNELS_API->currentBroadcastState();
	// to end
	if (!toStart && !confirmToContinue()) {
		updateGoliveButton(state);
		holdOnAll(false);
		return;
	}

	auto ignoreChange = [&]() {
		PRE_LOG_MSG_STEP("ignore toggle broadcasting,state is going to be ", g_LiveStep, INFO)
		updateGoliveButton(state);
		return;
	};

	//state has been shifting to
	if (toStart == PLSCHANNELS_API->isLiving()) {
		ignoreChange();
		return;
	}

	QString msg = QString(" try to change broadcast ,now state is %1 ").arg(LiveStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_LiveStep, INFO)

	switch (state) {
	case ReadyState:
		PLSCHANNELS_API->sigTrySetBroadcastState(BroadcastGo);
		break;

	case StreamStarted:
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->sigTrySetBroadcastState(StopBroadcastGo);
		break;
	default:
		ignoreChange();
		break;
	}
}

void GoLivePannel::updateGoliveButton(int state)
{
	QString msg = QString(" try to update GoliveButton ,now state is %1 ").arg(LiveStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_LiveStep, INFO);
	switch (state) {
	case StreamStarted:
		if (!ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(true);
		}
		ui->GoLiveShift->setText(PLSCHANNELS_API->isRehearsaling() ? finisheRehearsalText : finishLiveText);
		ui->GoLiveShift->setDisabled(false);
		break;
	case ReadyState:
		if (ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(false);
		}
		ui->GoLiveShift->setText(goliveText);
		ui->GoLiveShift->setDisabled(false);

		break;
	case BroadcastGo:
	case CanBroadcastState:
	case StreamStarting:
	case StopBroadcastGo:
	case CanBroadcastStop:
	case StreamStopping:
		ui->GoLiveShift->setDisabled(true);
		break;
	default:
		break;
	}
}
