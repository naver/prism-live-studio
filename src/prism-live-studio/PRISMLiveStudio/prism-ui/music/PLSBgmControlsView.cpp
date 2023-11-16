#include "PLSBgmControlsView.h"
#include "libutils-api.h"
#include "libui.h"
#include "media-slider.hpp"
#include "pls/pls-obs-api.h"
#include "pls/pls-source.h"
#include "pls-common-define.hpp"
#include "PLSBgmDataManager.h"
#include "obs-app.hpp"
using namespace common;

#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QLabel>

PLSBgmControlsView::PLSBgmControlsView(QWidget *parent) : PLSBgmControlsBase(parent)
{
	pls_add_css(this, {"PLSBgmControlsView"});
	QHBoxLayout *hLayout = pls_new<QHBoxLayout>(this);
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(5);
	hLayout->setAlignment(Qt::AlignLeft | Qt::AlignHCenter);

	loopBtn = pls_new<QRadioButton>(this);
	loopBtn->setObjectName("loopBtn");
	loopBtn->setToolTip(QTStr("Bgm.Repeat"));
	connect(loopBtn, &QRadioButton::clicked, this, &PLSBgmControlsView::OnLoopButtonClicked);
	preBtn = pls_new<QPushButton>(this);
	preBtn->setObjectName("preBtn");
	preBtn->setToolTip(QTStr("Bgm.Previous"));

	connect(preBtn, &QPushButton::clicked, this, &PLSBgmControlsView::OnPreButtonClicked);
	playBtn = pls_new<QPushButton>(this);
	playBtn->setObjectName("playBtn");
	connect(playBtn, &QPushButton::clicked, this, &PLSBgmControlsView::OnPlayButtonClicked);
	nextBtn = pls_new<QPushButton>(this);
	nextBtn->setObjectName("nextBtn");
	nextBtn->setToolTip(QTStr("Bgm.Next"));
	connect(nextBtn, &QPushButton::clicked, this, &PLSBgmControlsView::OnNextButtonClicked);

	QWidget *timerLabel = pls_new<QWidget>(this);
	QHBoxLayout *labelLayout = pls_new<QHBoxLayout>(timerLabel);
	labelLayout->setContentsMargins(0, 0, 0, 0);
	labelLayout->setSpacing(5);

	currentTimeLabel = pls_new<QLabel>(this);
	currentTimeLabel->setObjectName("currentTimeLabel");
	currentTimeLabel->setText("00:00");

	durationLabel = pls_new<QLabel>(this);
	durationLabel->setObjectName("durationLabel");
	durationLabel->setText("00:00");

	QLabel *label = pls_new<QLabel>("/", this);
	label->setObjectName("timeLabel");

	labelLayout->addWidget(currentTimeLabel);
	labelLayout->addWidget(label);
	labelLayout->addWidget(durationLabel);
	timerLabel->setLayout(labelLayout);

	slider = pls_new<MediaSlider>(this);
	slider->setOrientation(Qt::Horizontal);
	slider->setObjectName("playingSlider");
	slider->setMaximum(SLIDER_MAX);
	connect(slider, &MediaSlider::sliderPressed, this, &PLSBgmControlsView::OnMediaSliderClicked);
	connect(slider, &MediaSlider::sliderReleased, this, &PLSBgmControlsView::OnMediaSliderReleased);
	connect(slider, &MediaSlider::sliderMoved, this, &PLSBgmControlsView::OnMediaSliderMoved);

	hLayout->addWidget(preBtn);
	hLayout->addWidget(playBtn);
	hLayout->addWidget(nextBtn);
	hLayout->addWidget(loopBtn);
	hLayout->addSpacing(13);
	hLayout->addWidget(slider);
	hLayout->addSpacing(5);
	hLayout->addWidget(timerLabel);
	hLayout->addSpacing(15);

	sliderTimer = pls_new<QTimer>(this);
	connect(sliderTimer, &QTimer::timeout, this, &PLSBgmControlsView::SetSliderPos, Qt::QueuedConnection);
	connect(&seekTimer, &QTimer::timeout, this, &PLSBgmControlsView::SeekTimerCallback, Qt::QueuedConnection);
}

void PLSBgmControlsView::OnMediaLoopStateChanged(bool loop)
{
	SetLoopState();
}

void PLSBgmControlsView::SetSliderPos()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	float time = (float)obs_source_media_get_time(source);
	if (time < 1) {
		return;
	}
	float duration = (float)obs_source_media_get_duration(source);

	currentTimeLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(time / 1000.0f)));
	durationLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(duration / 1000.0f)));
	float sliderPosition = (time / duration) * (float)slider->maximum();
	slider->setValue((int)sliderPosition);
}

void PLSBgmControlsView::OnLoopButtonClicked(bool checked)
{
	pls_flush_style(loopBtn, STATUS_PRESSED, checked);
	PLSBgmControlsBase::OnLoopButtonClicked(checked);
}

void PLSBgmControlsView::OnMediaSliderMoved(int val)
{
	PLSBgmControlsBase::OnMediaSliderMoved(val);
	slider->setValue(val);
}

void PLSBgmControlsView::OnMediaSliderClicked()
{
	seek = slider->value();
	PLSBgmControlsBase::OnMediaSliderClicked();
}

void PLSBgmControlsView::UpdateUI()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		SetDisabledState(true);
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);
	OnMediaStateChanged(state);
}

void PLSBgmControlsView::SetDisabledState(bool disable)
{
	if (disable) {
		StopSliderPlayingTimer();
		currentTimeLabel->setText("00:00");
		durationLabel->setText("00:00");
		slider->setValue(0);
		pls_flush_style(currentTimeLabel, "playing", false);
	}

	playBtn->setDisabled(disable);
	loopBtn->setDisabled(disable);
	preBtn->setDisabled(disable);
	nextBtn->setDisabled(disable);
	slider->setDisabled(disable);
	QString status = disable ? STATUS_DISABLE : STATUS_ENABLE;
	pls_flush_style(slider, STATUS_ENTER, !disable);
	pls_flush_style(playBtn, STATUS_STATE, status);
	pls_flush_style(loopBtn, STATUS, status);
	pls_flush_style(preBtn, STATUS, status);
	pls_flush_style(nextBtn, STATUS, status);
}

void PLSBgmControlsView::SetPlayingState()
{
	SetLoopState();
	StartSliderPlayingTimer();
	SetDisabledState(false);
	prevPaused = false;

	pls_flush_style(playBtn, STATUS_STATE, STATUS_PAUSE);
	pls_flush_style(currentTimeLabel, "playing", true);
	playBtn->setToolTip(QTStr("Bgm.Pause"));
}

void PLSBgmControlsView::SetPauseState()
{
	if (seeking) {
		return;
	}
	StopSliderPlayingTimer();

	pls_flush_style(playBtn, STATUS_STATE, STATUS_PLAY);
	playBtn->setToolTip(QTStr("Bgm.Play"));
}

void PLSBgmControlsView::SetLoopState()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	OBSData settings = obs_source_get_private_settings(source);
	bool loop = obs_data_get_bool(settings, IS_LOOP);
	if (loopBtn->isChecked() == loop) {
		return;
	}

	loopBtn->setChecked(loop);
	pls_flush_style(loopBtn, STATUS_PRESSED, loop);
}

void PLSBgmControlsView::SeekTo(int val)
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	int64_t duration = obs_source_media_get_duration(source);
	if (duration > 0) {
		auto seekTo = GetSliderTime(val);
		obs_source_media_set_time(source, seekTo);
		slider->setValue(val);
		currentTimeLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(static_cast<float>(seekTo) / 1000.0f)));
	}
}
