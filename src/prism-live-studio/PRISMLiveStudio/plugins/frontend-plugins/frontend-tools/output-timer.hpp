#pragma once

#include <QDialog>
#include <memory>

#include "ui_output-timer.h"
#include "PLSDialogView.h"

class QCloseEvent;

class OutputTimer : public PLSDialogView {
	Q_OBJECT

public:
	std::unique_ptr<Ui_OutputTimer> ui;
	OutputTimer(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;
	void PauseRecordingTimer();
	void UnpauseRecordingTimer();

public slots:
	void StreamingTimerButton();
	void RecordingTimerButton();
	void StreamTimerStart();
	void RecordTimerStart();
	void StreamTimerStop();
	void RecordTimerStop();
	void UpdateStreamTimerDisplay();
	void UpdateRecordTimerDisplay();
	void ShowHideDialog();
	void EventStopStreaming();
	void EventStopRecording();

protected:
	void showEvent(QShowEvent *event) override;

private:
	bool streamingAlreadyActive = false;
	bool recordingAlreadyActive = false;

	QTimer *streamingTimer;
	QTimer *recordingTimer;
	QTimer *streamingTimerDisplay;
	QTimer *recordingTimerDisplay;

	int recordingTimeLeft;
};
