#pragma once

#include "context-bar-controls.hpp"
class QPushButton;

class RegionCaptureToolbar : public SourceToolbar {
	Q_OBJECT
public:
	RegionCaptureToolbar(QWidget *parent, OBSSource source);
	~RegionCaptureToolbar() final = default;

public slots:
	void OnSelectRegionClicked();
};

class TimerSourceToolbar : public SourceToolbar {
	Q_OBJECT
public:
	TimerSourceToolbar(QWidget *parent, OBSSource source);
	~TimerSourceToolbar() noexcept override;
	void updateBtnState();
	bool isSameSource(const obs_source_t *rSource);

	static void updatePropertiesButtonState(void *data, calldata_t *params);

public slots:
	void OnStartClicked();
	void OnStopClicked();

private:
	QPushButton *m_startBtn;
	QPushButton *m_stopBtn;
	OBSSignal updateButtonSignal;
};

class ChatTemplateSourceToolbar : public SourceToolbar {
	Q_OBJECT

public:
	ChatTemplateSourceToolbar(QWidget *parent, OBSSource source);
	~ChatTemplateSourceToolbar();
};
