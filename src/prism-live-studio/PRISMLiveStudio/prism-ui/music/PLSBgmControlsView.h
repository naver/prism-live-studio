#ifndef PLSBGMCONTROLSVIEW_H
#define PLSBGMCONTROLSVIEW_H

#include <QWidget>
#include <QTimer>
#include "obs.hpp"
#include "PLSBgmControlsBase.h"
#include "PLSBackgroundMusicView.h"

class QRadioButton;
class QPushButton;
class AbsoluteSlider;
class QLabel;
class PLSDelayResponseButton;
class PLSBgmControlsView : public PLSBgmControlsBase {
	Q_OBJECT

public:
	explicit PLSBgmControlsView(QWidget *parent = nullptr);
	~PLSBgmControlsView() = default;

	void UpdateUI();

public slots:
	void OnMediaLoopStateChanged(bool loop) override;
	void OnMediaModeStateChanged(int mode);

private slots:
	void SetSliderPos();
	void OnLoopButtonClicked(bool checked);
	void OnModeButtonClicked();
	void OnMediaSliderMoved(int val);
	void OnMediaSliderClicked();

private:
	virtual void SetDisabledState(bool disable) override;
	virtual void SetPlayingState() override;
	virtual void SetPauseState() override;
	void SetLoopState();
	void SetModeState();
	void SeekTo(int);

private:
	QRadioButton *loopBtn = nullptr;
	QPushButton *modeBtn = nullptr;
	PLSDelayResponseButton *preBtn = nullptr;
	QPushButton *playBtn = nullptr;
	PLSDelayResponseButton *nextBtn = nullptr;
	AbsoluteSlider *slider = nullptr;
	QLabel *currentTimeLabel = nullptr;
	QLabel *durationLabel = nullptr;
	PLSBackgroundMusicView::PlayMode mode = PLSBackgroundMusicView::PlayMode::RandomMode;
};

#endif // PLSBGMCONTROLSVIEW_H
