#include "PLSBgmControlsBase.h"
#include "libutils-api.h"
#include "libui.h"
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
#include "PLSBgmControlsBase.h"

PLSBgmControlsBase::PLSBgmControlsBase(QWidget *parent)
{
	sliderTimer = pls_new<QTimer>(this);
	connect(&seekTimer, &QTimer::timeout, this, &PLSBgmControlsBase::SeekTimerCallback, Qt::QueuedConnection);
}

OBSSource PLSBgmControlsBase::GetSource()
{
	return OBSGetStrongRef(weakSource);
}

void PLSBgmControlsBase::SetSource(OBSSource newSource)
{
	sigs.clear();

	if (newSource) {
		weakSource = OBSGetWeakRef(newSource);
		signal_handler_t *handler = obs_get_signal_handler();
		sigs.emplace_back(handler, "source_notify", OnSourceNotify, this);
	} else {
		weakSource = nullptr;
	}
}

void PLSBgmControlsBase::OnMediaLoopStateChanged(bool loop)
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_loop");
	obs_data_set_bool(settings, IS_LOOP, loop);
	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
}

void PLSBgmControlsBase::OnPreButtonClicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}
	obs_source_media_previous(source);
}

void PLSBgmControlsBase::OnNextButtonClicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}
	obs_source_media_next(source);
}

void PLSBgmControlsBase::OnSourceNotify(void *data, calldata_t *params)
{
	auto source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source) {
		return;
	}

	PLSBgmControlsBase *view = static_cast<PLSBgmControlsBase *>(data);
	if (!view) {
		return;
	}

	if (view->GetSource() != source) {
		return;
	}

	auto type = (int)calldata_int(params, "message");
	auto code = (int)calldata_int(params, "sub_code");
	if (type == OBS_SOURCE_MUSIC_STATE_CHANGED) {
		obs_media_state state = static_cast<obs_media_state>(code);
		QMetaObject::invokeMethod(view, "OnMediaStateChanged", Qt::QueuedConnection, Q_ARG(obs_media_state, state));
	} else if (type == OBS_SOURCE_MUSIC_LOOP_STATE_CHANGED) {
		QMetaObject::invokeMethod(view, "OnMediaLoopStateChanged", Qt::QueuedConnection, Q_ARG(bool, static_cast<bool>(code)));
	} else if (type == OBS_SOURCE_MUSIC_MODE_STATE_CHANGED) {
		QMetaObject::invokeMethod(view, "OnMediaModeStateChanged", Qt::QueuedConnection, Q_ARG(int, code));
	}
}

void PLSBgmControlsBase::OnMediaStateChanged(obs_media_state state)
{
	switch (state) {
	case OBS_MEDIA_STATE_PLAYING: {
		OBSSource source = OBSGetStrongRef(weakSource);
		if (!source) {
			SetDisabledState(true);
			return;
		}
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "method", "get_current_url");
		pls_source_get_private_data(source, settings);
		const char *url = obs_data_get_string(settings, BGM_URL);
		if (pls_is_empty(url)) {
			SetDisabledState(true);
		} else {
			SetPlayingState();
		}
		break;
	}
	case OBS_MEDIA_STATE_PAUSED:
		SetPauseState();
		break;
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ERROR:
	case OBS_MEDIA_STATE_NONE:
		SetDisabledState(true);
		break;
	default:
		break;
	}
}

void PLSBgmControlsBase::OnPlayButtonClicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}
	obs_media_state state = obs_source_media_get_state(source);
	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_NONE:
		obs_source_media_restart(source);
		break;
	case OBS_MEDIA_STATE_PLAYING:
		obs_source_media_play_pause(source, true);
		break;
	case OBS_MEDIA_STATE_PAUSED:
		obs_source_media_play_pause(source, false);
	default:
		break;
	}
}

void PLSBgmControlsBase::OnLoopButtonClicked(bool checked)
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_loop");
	obs_data_set_bool(settings, IS_LOOP, checked);
	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
}

void PLSBgmControlsBase::OnMediaSliderClicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}
	obs_media_state state = obs_source_media_get_state(source);
	if (state == OBS_MEDIA_STATE_PLAYING) {
		prevPaused = false;
		obs_source_media_play_pause(source, true);
		StopSliderPlayingTimer();
	} else if (state == OBS_MEDIA_STATE_PAUSED) {
		prevPaused = true;
	}

	seeking = true;
	seekTimer.start(100);
}

void PLSBgmControlsBase::OnMediaSliderReleased()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	if (seekTimer.isActive()) {
		seeking = false;
		seekTimer.stop();
		if (lastSeek != seek) {
			obs_source_media_set_time(source, GetSliderTime(seek));
		}

		seek = lastSeek = -1;
	}

	if (!prevPaused) {
		obs_source_media_play_pause(source, false);
		StartSliderPlayingTimer();
	}
}

void PLSBgmControlsBase::OnMediaSliderMoved(int val)
{
	if (seekTimer.isActive()) {
		seek = val;
	}
}

void PLSBgmControlsBase::SeekTimerCallback()
{
	if (lastSeek != seek) {
		OBSSource source = OBSGetStrongRef(weakSource);
		if (source) {
			obs_source_media_set_time(source, GetSliderTime(seek));
		}
		lastSeek = seek;
	}
}

void PLSBgmControlsBase::StartSliderPlayingTimer()
{
	if (!sliderTimer) {
		return;
	}

	if (!sliderTimer->isActive()) {
		sliderTimer->start(1000);
	}
}

void PLSBgmControlsBase::StopSliderPlayingTimer()
{
	if (!sliderTimer) {
		return;
	}

	if (sliderTimer->isActive()) {
		sliderTimer->stop();
	}
}

int64_t PLSBgmControlsBase::GetSliderTime(int val)
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return 0;
	}

	float percent = (float)val / (float)SLIDER_MAX;
	float duration = (float)obs_source_media_get_duration(source);
	int64_t seekTo = (int64_t)(percent * duration);

	return seekTo;
}
