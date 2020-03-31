#include "GoLivePannel.h"
#include "ui_GoLivePannel.h"
#include "PLSChannelDataAPI.h"
#include "ChannelConst.h"
#include "frontend-api.h"
#include "PLSChannelsVirualAPI.h"
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSAddingFrame.h"
#include "PLSPlatformApi.h"

using namespace ChannelData;

GoLivePannel::GoLivePannel(QWidget *parent) : QFrame(parent), ui(new Ui::GoLivePannel)
{
	ui->setupUi(this);
	auto goliveState = new QState;
	goliveState->assignProperty(ui->GoLiveShift, "text", CHANNELS_TR(GoLive));
	goliveState->assignProperty(ui->GoLiveShift, "checked", false);

	auto inLive = new QState;
	inLive->assignProperty(ui->GoLiveShift, "text", CHANNELS_TR(FinishLive));
	inLive->assignProperty(ui->GoLiveShift, "checked", true);

	goliveState->addTransition(PLSCHANNELS_API, &PLSChannelDataAPI::broadcasting, inLive);
	inLive->addTransition(PLSCHANNELS_API, &PLSChannelDataAPI::broadCastEnded, goliveState);

	mMachine.addState(goliveState);
	mMachine.addState(inLive);

	mMachine.setInitialState(goliveState);
	mMachine.start();

	mBusyFrame = new PLSAddingFrame(ui->GoLiveShift);

	mBusyFrame->setObjectName("LoadingFrame");
	mBusyFrame->setContent("");
	mBusyFrame->setSourceFirstFile(g_goliveLoadingPixPath);
	QHBoxLayout *layout = new QHBoxLayout(ui->GoLiveShift);
	layout->addWidget(mBusyFrame);
	layout->setMargin(0);
	ui->GoLiveShift->setLayout(layout);
	mBusyFrame->hide();
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, this, &GoLivePannel::shitftRecordState);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcast, ui->GoLiveShift, &QPushButton::click);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopBroadcast, ui->GoLiveShift, &QPushButton::click);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartRecord, ui->Record, &QPushButton::click);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopRecord, ui->Record, &QPushButton::click);
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
	QSignalBlocker blocker(ui->Record);
	ui->Record->setDisabled(true);
	PRE_LOG_UI_MSG(QString(ui->Record->text() + " clicked ").toStdString().c_str(), GoLivePannel);
	QString disTxt = isCheck ? (CHANNELS_TR(STOP)) : (CHANNELS_TR(REC));
	ui->Record->setText(disTxt);

	int state = PLSCHANNELS_API->currentReocrdState();
	switch (state) {
	case RecordReady:
	case RecordStopped: {
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->setRecordState(CanRecord);
	} break;
	case RecordStarted: {
		PLSCHANNELS_API->setIsClickToStopRecord(true);
		PLSCHANNELS_API->setIsOnlyStopRecord(true);
		PLSCHANNELS_API->setRecordState(RecordStopGo);
	} break;
	default:
		break;
	}
	ui->Record->setDisabled(false);
}

void GoLivePannel::shitftRecordState(int state)
{
	switch (state) {
	case RecordStarted: {
		if (!ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(true);
			ui->Record->setText(CHANNELS_TR(STOP));
		}
	} break;
	case RecordReady:
	case CanRecord:
	case RecordStopped:
		if (ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(false);
			ui->Record->setText(CHANNELS_TR(REC));
		}
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

void GoLivePannel::on_GoLiveShift_clicked()
{
	QSignalBlocker blocker(ui->GoLiveShift);
	ui->GoLiveShift->setDisabled(true);
	PRE_LOG_UI_MSG(QString(ui->GoLiveShift->text() + " clicked ").toStdString().c_str(), GoLivePannel);
	int state = PLSCHANNELS_API->currentBroadcastState();
	switch (state) {
	case StreamStopped:
	case ReadyState: {
		PLSCHANNELS_API->setBroadcastState(BroadcastGo);
	} break;

	case StreamStarted: {
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->setBroadcastState(StopBroadcastGo);
	} break;
	default:
		break;
	}
	ui->GoLiveShift->setDisabled(false);
}
