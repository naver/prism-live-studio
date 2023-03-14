#include "GoLivePannel.h"
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "LogPredefine.h"
#include "PLSAddingFrame.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSPlatformApi.h"
#include "frontend-api.h"
#include "prism/PLSPlatformPrism.h"
#include "ui_GoLivePannel.h"

using namespace ChannelData;

GoLivePannel::GoLivePannel(QWidget *parent)
	: QFrame(parent),
	  ui(new Ui::GoLivePannel),
	  goliveText(CHANNELS_TR(GoLive)),
	  finishLiveText(CHANNELS_TR(FinishLive)),
	  finisheRehearsalText(CHANNELS_TR(FinishRehearsal)),
	  isEnteredGolive(false),
	  isEnteredRecord(false)
{

	ui->setupUi(this);

	mBusyFrame = new PLSAddingFrame(ui->GoLiveShift);

	mBusyFrame->setObjectName("LoadingFrame");
	mBusyFrame->setContent("");
	mBusyFrame->setSourceFirstFile(g_goliveLoadingPixPath);
	QHBoxLayout *layout = new QHBoxLayout(ui->GoLiveShift);
	layout->addWidget(mBusyFrame);
	layout->setMargin(0);
	ui->GoLiveShift->setLayout(layout);
	mBusyFrame->hide();

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, this, &GoLivePannel::updateRecordButton);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this, &GoLivePannel::updateGoliveButton);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcast, ui->GoLiveShift, [=]() { toggleBroadcast(true); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopBroadcast, ui->GoLiveShift, [=]() { toggleBroadcast(false); });

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartRecord, ui->Record, [=]() { toggleRecord(true); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopRecord, ui->Record, [=]() { toggleRecord(false); });
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
	//PRE_LOG_UI_MSG(QString(ui->Record->text() + " clicked ").toStdString().c_str(), GoLivePannel);
	PLS_LIVE_UI_STEP("channels", isCheck ? ("Reocrd Start") : ("Record Stop "), "clicked ");
	toggleRecord(isCheck);
}

void GoLivePannel::toggleRecord(bool isStart)
{
	if (isEnteredRecord) {
		PRE_LOG_MSG_STEP(" Wait for record state changing .... new event is ignored!", g_recordStep, INFO);
		return;
	}
	HolderReleaser holder(&GoLivePannel::setEnteredRecord, this);
	bool isRecording = PLSCHANNELS_API->isRecording();
	int state = PLSCHANNELS_API->currentReocrdState();

	//state has been shifting to

	auto ignore = [&]() {
		PRE_LOG_MSG_STEP(QString(" Record state is going to be %1 ,new event is ignored!").arg(isStart ? " ON " : "OFF"), g_recordStep, INFO);
		updateRecordButton(state);
		return;
	};
	if (isRecording == isStart) {
		ignore();
		return;
	}
	QString msg = QString("Try to %1 record,current record state: ").arg(isStart ? "start " : " stop ") + RecordStatesMap[state];
	PRE_LOG_MSG_STEP(msg, g_recordStep, INFO);

	switch (state) {
	case RecordReady:
	case RecordStopped: {
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->sigTrySetRecordState(CanRecord);
	} break;
	case RecordStarted: {
		PLSCHANNELS_API->setIsClickToStopRecord(true);
		PLSCHANNELS_API->setIsOnlyStopRecord(true);
		PLSCHANNELS_API->sigTrySetRecordState(RecordStopGo);
	} break;
	default:
		ignore();
		break;
	}
}

void GoLivePannel::updateRecordButton(int state)
{
	switch (state) {
	case RecordStarted: {
		if (!ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(true);
		}
		ui->Record->setText(CHANNELS_TR(STOP));
		ui->Record->setDisabled(false);
	} break;
	case RecordReady:
	case CanRecord:
	case RecordStopped: {
		if (ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(false);
		}
		ui->Record->setText(CHANNELS_TR(REC));
		ui->Record->setDisabled(false);
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

void GoLivePannel::on_GoLiveShift_toggled(bool isGolive)
{
	//PRE_LOG_UI_MSG(QString(ui->GoLiveShift->text() + " clicked ").toStdString().c_str(), GoLivePannel);
	QString finishStr = PLSCHANNELS_API->isRehearsaling() ? ("Finish rehearsal ") : ("Finish live ");
	PLS_LIVE_UI_STEP("channels", isGolive ? ("Golive") : finishStr.toUtf8().constData(), "clicked ");
	toggleBroadcast(isGolive);
}

void GoLivePannel::toggleBroadcast(bool toStart)
{
	if (isEnteredGolive) {
		PRE_LOG_MSG_STEP("ignore toggle broadcasting,it has entered ", g_LiveStep, INFO);
		return;
	}
	HolderReleaser holder(&GoLivePannel::setEnteredGolive, this);
	int state = PLSCHANNELS_API->currentBroadcastState();

	auto ignore = [&]() {
		PRE_LOG_MSG_STEP("ignore toggle broadcasting,state is going to be ", g_LiveStep, INFO);
		updateGoliveButton(state);
		return;
	};

	//state has been shifting to
	if (toStart == PLSCHANNELS_API->isLiving()) {
		ignore();
		return;
	}

	QString msg = QString(" try to change broadcast ,now state is %1 ").arg(LiveStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_LiveStep, INFO);

	switch (state) {
	case StreamStopped:
	case ReadyState: {
		PLSCHANNELS_API->sigTrySetBroadcastState(BroadcastGo);
	} break;

	case StreamStarted: {
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->sigTrySetBroadcastState(StopBroadcastGo);
	} break;
	default:
		ignore();
		break;
	}
}

void GoLivePannel::updateGoliveButton(int state)
{
	switch (state) {
	case StreamStarted: {
		if (!ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(true);
		}
		ui->GoLiveShift->setText(PLSCHANNELS_API->isRehearsaling() ? finisheRehearsalText : finishLiveText);
		ui->GoLiveShift->setDisabled(false);
	} break;
	case ReadyState:
	case StreamStopped: {
		if (ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(false);
		}
		ui->GoLiveShift->setText(goliveText);
		ui->GoLiveShift->setDisabled(false);

	} break;
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
