#pragma once

#include <obs.hpp>
#include <util/platform.h>
#include <QPointer>
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QList>
#include <atomic>

#include <dialog-view.hpp>

#include "ui_PLSBasicStats.h"
#include "PLSDpiHelper.h"

class QGridLayout;
class QCloseEvent;

class PLSBasicStats : public QWidget {
	Q_OBJECT

	Ui::PLSBasicStats *ui;
	QGridLayout *outputLayout = nullptr;

	QTimer timer;
	uint64_t num_bytes = 0;

	struct OutputLabels {
		uint64_t lastBytesSent = 0;
		uint64_t lastBytesSentTime = 0;

		int first_total = 0;
		int first_dropped = 0;

		void Update(PLSBasicStats *stats, Ui::PLSBasicStats *ui, obs_output_t *output, bool rec);
		void Reset(obs_output_t *output);

		long double kbps = 0.0l;
	};

	QList<OutputLabels> outputLabels;
	PLSDialogView *dialogView;

	void Update();
	void UpdatePerfStats();

	std::atomic_bool resetting = false;
	void ResetInternal();

public:
	explicit PLSBasicStats(PLSDialogView *dialogView, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSBasicStats();

	static void InitializeValues(bool ready = false);

	// add for popup notice when streaming state was warning or error
	// even number: warning, cardinal number : error
	enum class StreamingNoticeType {
		NoticeTransmissionBitrateWarning = 0,
		NoticeTransmissionBitrateError,
		NoticeBufferedDurationWarning,
		NoticeBufferedDurationError,
		NoticeDropNetworkFrameWarning,
		NoticeDropNetworkFrameError,
		NoticeDropRenderingFrameWarning,
		NoticeDropRenderingFrameError,
		NoticeDropEncodingFrameWarning,
		NoticeDropEncodingFrameError
	};
	Q_ENUM(StreamingNoticeType)
	void PopupNotice(StreamingNoticeType type, bool recoverNormal = false, int dropFrame = 0, double dropPercentFrame = 0);
	void SetNotified(StreamingNoticeType type, bool notified);

private:
	QPointer<QObject> shortcutFilter;

signals:
	void showSignal();
	void hideSignal();

public slots:
	void Reset();

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
};
