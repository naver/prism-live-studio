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
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopBroadcast, ui->GoLiveShift, [this](DualOutputType type) {
		if (type == DualOutputType::All) {
			toggleBroadcast(false);
		} else {
			PLSBasic::instance()->StopStreaming(type);
		}
	});

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

void GoLivePannel::showEvent(QShowEvent *event)
{
	setRecTooltip();
	QFrame::showEvent(event);
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

	bool bSame = PLSBasic::instance()->checkRecEncoder();
	if (bSame && mBusyFrame->isVisible()) {
		int state = PLSCHANNELS_API->currentBroadcastState();
		PLS_INFO("GoLivePannel", "current BroadcastState is %d", state);
		QString errMessage;
		if (PLSCHANNELS_API->isRehearsaling()) {
			if (state == BroadcastGo || state == CanBroadcastState || state == StreamStarting) {
				errMessage = tr("StartingRehearsal.GoLiveBtnIsBusy.Text");
			} else if (state == StopBroadcastGo || state == CanBroadcastStop || state == StreamStopping || state == StreamStopped) {
				errMessage = tr("EndingRehearsal.GoLiveBtnIsBusy.Text");
			}
		} else {
			if (state == BroadcastGo || state == CanBroadcastState || state == StreamStarting) {
				errMessage = tr("StartingStream.GoLiveBtnIsBusy.Text");
			} else if (state == StopBroadcastGo || state == CanBroadcastStop || state == StreamStopping || state == StreamStopped) {
				errMessage = tr("EndingStream.GoLiveBtnIsBusy.Text");
			}
		}
		if (!errMessage.isEmpty()) {
			PLSAlertView::warning(this, tr("Alert.Title"), errMessage);
		}

		ui->Record->setDisabled(false);
		QSignalBlocker blocker(ui->Record);
		bool bCheck = ui->Record->isChecked();
		ui->Record->setChecked(!bCheck);
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
	QString msg = QString(" try to update RECButton ,now state is %1 ").arg(RecordStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_recordStep, INFO);
	switch (state) {
	case RecordStarted:
		if (!ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(true);
		}
		ui->Record->setText(QTStr("Channels.STOP"));
		ui->Record->setDisabled(false);
		ui->Record->setToolTip(QTStr("Channels.stopRec.tooltip"));
		break;
	case RecordReady:
	case CanRecord:
	case RecordStopped: {
		if (ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(false);
		}
		ui->Record->setText(QTStr("Channels.REC"));
		ui->Record->setDisabled(false);
		setRecTooltip();
	} break;

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

	if (PLS_PLATFORM_API->getLiveEndType() == EndLiveType::MQTT_END_LIVE) {
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

	bool bSame = PLSBasic::instance()->checkRecEncoder();
	if (bSame && !ui->Record->isEnabled()) {
		int state = PLSCHANNELS_API->currentReocrdState();
		PLS_INFO("GoLivePannel", "current Reocrd is %d", state);
		if (state == RecordStarting) {
			PLSAlertView::warning(this, tr("Alert.Title"), tr("StartingRec.RECBtnIsBusy.Text"));
		} else if (state == RecordStopGo || state == RecordStopping) {
			PLSAlertView::warning(this, tr("Alert.Title"), tr("EndingRec.RECBtnIsBusy.Text"));
		}

		ui->GoLiveShift->setDisabled(false);
		QSignalBlocker blocker(ui->GoLiveShift);
		auto bCheck = ui->GoLiveShift->isChecked();
		ui->GoLiveShift->setChecked(!bCheck);
		holdOnAll(false);
		return;
	}

	HolderReleaser holder(&GoLivePannel::setEnteredGolive, this);

	int state = PLSCHANNELS_API->currentBroadcastState();

	if (state == StreamStopping) {
		auto ret = PLSAlertView::question(pls_get_main_view(), CHANNELS_TR(Confirm), CHANNELS_TR(NeedForceStop),
						  {{PLSAlertView::Button::Yes, QObject::tr("OK")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}}, PLSAlertView::Button::Cancel);
		ui->GoLiveShift->setDisabled(false);

		if (ret == PLSAlertView::Button::Yes && PLSCHANNELS_API->currentBroadcastState() == StreamStopping) {
			PLSBasic::instance()->ForceStopStreaming();
			QSignalBlocker blocker(ui->GoLiveShift);
			auto bCheck = ui->GoLiveShift->isChecked();
			ui->GoLiveShift->setChecked(!bCheck);
		}
		return;
	}

	// to end
	if (!toStart && !confirmToContinue()) {
		//issue3760 navershopping click the finsh button to end end,wait 10s,BroadcastState maybe modify when mqtt force end
		state = PLSCHANNELS_API->currentBroadcastState();
		updateGoliveButton(state);
		holdOnAll(false);
		return;
	}
	state = PLSCHANNELS_API->currentBroadcastState();

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
		emit PLSMainView::instance()->onGolivePending(false);
		break;
	case ReadyState:
		if (ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(false);
		}
		ui->GoLiveShift->setText(goliveText);
		ui->GoLiveShift->setDisabled(false);
		emit PLSMainView::instance()->onGolivePending(false);
		break;
	case BroadcastGo:
	case CanBroadcastState:
	case StreamStarting:
	case StopBroadcastGo:
	case CanBroadcastStop:
		ui->GoLiveShift->setDisabled(true);
		emit PLSMainView::instance()->onGolivePending(true);
		break;
	case StreamStopping:
		ui->GoLiveShift->setDisabled(false);
		emit PLSMainView::instance()->onGolivePending(true);
		break;
	default:
		break;
	}
}

void GoLivePannel::setRecTooltip()
{
	pls_check_app_exiting();
	int state = PLSCHANNELS_API->currentReocrdState();
	if (state == RecordStarted) {
		ui->Record->setToolTip(QTStr("Channels.stopRec.tooltip"));
	} else {
		bool bSame = PLSBasic::instance()->checkRecEncoder();
		if (bSame) {
			ui->Record->setToolTip(QTStr("Channels.pauseRec.tooltip"));
		} else {
			ui->Record->setToolTip("");
		}
	}
}
