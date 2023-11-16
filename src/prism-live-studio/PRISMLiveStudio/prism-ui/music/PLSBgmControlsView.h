#ifndef PLSBGMCONTROLSVIEW_H
#define PLSBGMCONTROLSVIEW_H

#include <QWidget>
#include <QTimer>
#include "obs.hpp"
#include "PLSBgmControlsBase.h"

class QRadioButton;
class QPushButton;
class MediaSlider;
class QLabel;
class PLSBgmControlsView : public PLSBgmControlsBase {
	Q_OBJECT

public:
	explicit PLSBgmControlsView(QWidget *parent = nullptr);
	~PLSBgmControlsView() = default;

	void UpdateUI();

public slots:
	void OnMediaLoopStateChanged(bool loop) override;

private slots:
	void SetSliderPos();
	void OnLoopButtonClicked(bool checked);
	void OnMediaSliderMoved(int val);
	void OnMediaSliderClicked();

private:
	virtual void SetDisabledState(bool disable) override;
	virtual void SetPlayingState() override;
	virtual void SetPauseState() override;
	void SetLoopState();
	void SeekTo(int);

private:
	QRadioButton *loopBtn = nullptr;
	QPushButton *preBtn = nullptr;
	QPushButton *playBtn = nullptr;
	QPushButton *nextBtn = nullptr;
	MediaSlider *slider = nullptr;
	QLabel *currentTimeLabel = nullptr;
	QLabel *durationLabel = nullptr;
};

#endif // PLSBGMCONTROLSVIEW_H
