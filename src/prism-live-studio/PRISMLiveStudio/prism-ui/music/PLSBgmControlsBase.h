#ifndef PLSBGMCONTROLSCOMMON_H
#define PLSBGMCONTROLSCOMMON_H

#include <QWidget>
#include <QTimer>
#include "obs.hpp"

const int SLIDER_MAX = 1024;

class QRadioButton;
class QPushButton;
class PLSMediaSlider;
class QLabel;
class PLSBgmControlsBase : public QWidget {
	Q_OBJECT

public:
	explicit PLSBgmControlsBase(QWidget *parent = nullptr);
	~PLSBgmControlsBase() = default;

	OBSSource GetSource();
	void SetSource(OBSSource newSource);
	int64_t GetSliderTime(int val);

public slots:
	void OnLoopButtonClicked(bool checked);
	void OnPreButtonClicked();
	void OnNextButtonClicked();

protected:
	virtual void OnMediaLoopStateChanged(bool loop);
	virtual void SetDisabledState(bool disable) {}
	virtual void SetPlayingState() {}
	virtual void SetPauseState() {}

protected slots:
	static void OnSourceNotify(void *data, calldata_t *calldata);
	void OnMediaStateChanged(obs_media_state state);

	void OnPlayButtonClicked();
	void OnMediaSliderClicked();
	void OnMediaSliderReleased();
	void OnMediaSliderMoved(int val);
	void SeekTimerCallback();

	void StartSliderPlayingTimer();
	void StopSliderPlayingTimer();

protected:
	OBSWeakSource weakSource = nullptr;
	std::vector<OBSSignal> sigs;

	QTimer *sliderTimer = nullptr;
	QTimer seekTimer;

	int seek = -1;
	int lastSeek = -1;
	bool seeking = false;
	bool prevPaused = false;
};

#endif // PLSBGMCONTROLSCOMMON_H
