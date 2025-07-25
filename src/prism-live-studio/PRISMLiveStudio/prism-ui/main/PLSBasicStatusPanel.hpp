#pragma once

#include "obs.hpp"
#include "util/platform.h"
#include "frontend-api.h"

#include "PLSBasicStatusBar.hpp"

#include <QFrame>
#include <QLabel>
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSBasicStatusPanel;
}
QT_END_NAMESPACE

class PLSBasicStatusPanel : public QDialog {
	Q_OBJECT

	enum class State {
		Normal,
		Warning,
		Error,
	};

	struct OutputLabels {
		QPointer<QLabel> networkStateImage;
		QPointer<QLabel> droppedFramesState;
		QPointer<QLabel> droppedFramesLabel;
		QPointer<QLabel> droppedFrames;
		QPointer<QLabel> megabytesSentLabel;
		QPointer<QLabel> megabytesSent;
		QPointer<QLabel> bitrateState;
		QPointer<QLabel> bitrateLabel;
		QPointer<QLabel> bitrate;

		QPointer<QLabel> networkStateImageV;
		QPointer<QLabel> droppedFramesStateV;
		QPointer<QLabel> droppedFramesLabelV;
		QPointer<QLabel> droppedFramesV;
		QPointer<QLabel> megabytesSentLabelV;
		QPointer<QLabel> megabytesSentV;
		QPointer<QLabel> bitrateStateV;
		QPointer<QLabel> bitrateLabelV;
		QPointer<QLabel> bitrateV;

		bool lastActive = false;
		bool lastActiveV = false;
		State lastState = State::Normal;
		State lastStateV = State::Normal;

		uint64_t lastBytesSent = 0;
		uint64_t lastBytesSentTime = 0;
		uint64_t lastBytesSent_v = 0;
		uint64_t lastBytesSentTime_v = 0;

		int first_total = 0;
		int first_dropped = 0;
		int first_total_v = 0;
		int first_dropped_v = 0;

		void Update(const obs_output_t *output, bool rec, PLSBasicStatusData &dataStatus);
		void Reset(const obs_output_t *output);

		void Update_v(const obs_output_t *output_v, PLSBasicStatusData &dataStatus);
		void Reset_v(const obs_output_t *output_v);

		long double kbps = 0;
		long double kbps_v = 0;
	};

	QList<OutputLabels> outputLabels;

public:
	explicit PLSBasicStatusPanel(QWidget *parent = nullptr);

	static void InitializeValues();
	void Reset();

	enum class StreamingNoticeType {
		NoticeTransmissionBitrateWarning = 0,
		NoticeTransmissionBitrateError,
		NoticeDropNetworkFrameWarning,
		NoticeDropNetworkFrameError,
		NoticeDropRenderingFrameWarning,
		NoticeDropRenderingFrameError,
		NoticeDropEncodingFrameWarning,
		NoticeDropEncodingFrameError
	};
	Q_ENUM(StreamingNoticeType)
	void PopupNotice(StreamingNoticeType type, bool recoverNormal = false, int64_t dropFrame = 0, double dropPercentFrame = 0) const;
	void SetNotified(StreamingNoticeType type, bool notified) const;

	static void setTextAndAlignment(QLabel *, const QString &);

public slots:
	void updateStatusPanel(PLSBasicStatusData &dataStatus);
	void onDualOutputChanged(bool bDualOutput);

protected:
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSBasicStatusPanel *ui;

	bool lastActive = false;
	bool lastActiveV = false;

	State lastFramerateState = State::Normal;
	State lastRenderingState = State::Normal;
	State lastEncodingState = State::Normal;
	State lastEncodingStateV = State::Normal;
};
