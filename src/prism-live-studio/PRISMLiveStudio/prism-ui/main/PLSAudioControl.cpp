#include "PLSAudioControl.h"
#include "PLSSceneDataMgr.h"
#include "window-basic-main.hpp"
#include "PLSBasic.h"
#include "volume-control.hpp"
#include "log/module_names.h"
#include <obs-app.hpp>
#include <liblog.h>

using AudioStatus = PLSAudioControl::AudioStatus;

static inline void SetSourcePreviousMuted(obs_source_t *source, bool muted)
{
	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	obs_data_set_bool(priv_settings, "previous_muted", muted);
	obs_data_release(priv_settings);
}

static inline bool SourcePreviousMuted(obs_source_t *source, bool *exist = nullptr)
{
	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	if (nullptr != exist) {
		*exist = obs_data_has_user_value(priv_settings, "previous_muted");
	}
	bool mute = obs_data_get_bool(priv_settings, "previous_muted");
	obs_data_release(priv_settings);

	return mute;
}

PLSAudioControl::PLSAudioControl()
{
	InitObsSignals();
}

PLSAudioControl *PLSAudioControl::instance()
{
	static PLSAudioControl audioControl;
	return &audioControl;
}

void PLSAudioControl::MuteAll()
{
	UpdateMasterSwitch(true);

	auto enumAudioSource = [](void *param, obs_source_t *source) {
		Q_UNUSED(param)

		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO) {
			SetSourcePreviousMuted(source, obs_source_muted(source));
			obs_source_set_muted(source, true);
		}
		return true;
	};

	obs_enum_sources(enumAudioSource, nullptr);
}

void PLSAudioControl::UnmuteAll()
{
	UpdateMasterSwitch(false);

	auto masterStatus = GetMasterAudioStatusPrevious();

	// audio status should revert to previous
	auto enumAudioSource = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO) {
			bool previous_muted = SourcePreviousMuted(source);
			auto ref = (AudioStatus *)(param);
			if (!ref)
				return false;
			if (ref->isAllAudioMuted) {
				previous_muted = false;
				SetSourcePreviousMuted(source, previous_muted);
			}
			obs_source_set_muted(source, previous_muted);
		}
		return true;
	};

	obs_enum_sources(enumAudioSource, &masterStatus);
}

void PLSAudioControl::SetMuteState(bool mute)
{
	mute ? MuteAll() : UnmuteAll();
}

bool PLSAudioControl::GetMuteState() const
{
	return ConfigAllMuted();
}

void PLSAudioControl::TriggerMute()
{
	bool mute = ConfigAllMuted();
	if (mute) {
		UnmuteAll();
	} else {
		MuteAll();
	}
	PLS_INFO(AUDIO_MIXER, "All Mute Control: %s", mute ? "unchecked" : "checked");
}

void PLSAudioControl::InitControlStatus(obs_data_t *data, bool hasValue)
{
	if (!data)
		return;

	if (!hasValue) {
		auto masterStatus = GetMasterAudioStatusCurrent();
		bool allMuted = false;
		if (masterStatus.audioSourceCount > 0)
			allMuted = masterStatus.isAllAudioMuted;
		obs_data_set_bool(data, "all_muted", allMuted);
		UpdateMasterSwitch(allMuted);
	} else {
		bool allMuted = obs_data_get_bool(data, "all_muted");
		UpdateMasterSwitch(allMuted);
	}
}

void PLSAudioControl::InitSourceAudioStatus(obs_source_t *source) const
{
	bool hasValue = false;
	SourcePreviousMuted(source, &hasValue);

	if (muteAll.has_value()) {
		if (!hasValue) {
			if (muteAll.value()) {
				obs_source_set_muted(source, muteAll.value());
			} else {
				bool muted = obs_source_muted(source);
				SetSourcePreviousMuted(source, muted);
			}
		}
	} else {
		if (!hasValue) {
			SetSourcePreviousMuted(source, obs_source_muted(source));
		}
	}
}

void PLSAudioControl::InitObsSignals()
{
	signalHandlers.reserve(signalHandlers.size() + 2);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_create", PLSAudioControl::SourceCreated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_remove", PLSAudioControl::SourceRemoved, this);
}

void PLSAudioControl::ClearObsSignals()
{
	signalHandlers.clear();
}

void PLSAudioControl::UpdateSourceInMixer(const obs_source_t *source) const
{
	for (VolControl *vol : OBSBasic::Get()->volumes) {
		if (!vol)
			continue;
		if (vol->GetSource() == source) {
			vol->SetMuted(obs_source_muted(source));
			break;
		}
	}
}

void PLSAudioControl::SourceCreated(void *data, calldata_t *params)
{
	pls_unused(params);

	auto source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source)
		return;

	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		auto control = static_cast<PLSAudioControl *>(data);
		control->InitSourceAudioStatus(source);
		signal_handler_connect_ref(obs_source_get_signal_handler(source), "mute", PLSVolumeMuted, control);
	}
}

void PLSAudioControl::SourceRemoved(void *data, calldata_t *params)
{
	pls_unused(params);

	auto source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source)
		return;

	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		auto control = static_cast<PLSAudioControl *>(data);
		signal_handler_disconnect(obs_source_get_signal_handler(source), "mute", PLSVolumeMuted, control);
	}
}

void PLSAudioControl::PLSVolumeMuted(void *data, calldata_t *calldata)
{
	pls_unused(data, calldata);

	bool muted = calldata_bool(calldata, "muted");
	const obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");
	auto sourceName = obs_source_get_name(source);
	QMetaObject::invokeMethod(static_cast<PLSAudioControl *>(data), "VolumeMuted", Qt::QueuedConnection, Q_ARG(const QString &, sourceName), Q_ARG(bool, muted));
}

bool PLSAudioControl::CheckMasterAudioStatus(obs_data_t *data)
{
	if (muteAll.has_value()) {
		muteAll.reset();
	}
	bool hasValue = obs_data_has_user_value(data, "all_muted");
	if (hasValue) {
		bool allMuted = obs_data_get_bool(data, "all_muted");
		muteAll = allMuted;
	}
	return hasValue;
}

void PLSAudioControl::UpdateMasterSwitch(bool mute)
{
	muteAll = mute;
	for (QToolButton *btn : OBSBasic::Get()->ui->mixerDock->titleWidget()->getButtons()) {
		if (0 == btn->objectName().compare("audioMasterControl")) {
			btn->setProperty("muteAllAudio", mute);
			btn->setToolTip(mute ? QTStr("Basic.Main.UnmuteAllAudio") : QTStr("Basic.Main.MuteAllAudio"));
			pls_flush_style(btn);
			break;
		}
	}
	if (PLSBasic::instance()->getApi()) {
		PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_ALL_MUTE, {mute});
	}
}

bool PLSAudioControl::ConfigAllMuted() const
{
	if (muteAll.has_value())
		return muteAll.value();
	else
		return false;
}

AudioStatus PLSAudioControl::GetDeviceAudioStatusCurrent(const obs_source_t *ignoreDeviceSource) const
{
	AudioStatus deviceAudioStatus;
	//tranverse all the audio device sources to judge if all the audio device sources are muted.
	auto allSourceMute = [&deviceAudioStatus, ignoreDeviceSource](const obs_source_t *source) {
		if (!source)
			return true;

		if (ignoreDeviceSource && source == ignoreDeviceSource)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO) {
			if (0 == strcmp(obs_source_get_id(source), App()->InputAudioSource()) || (0 == strcmp(obs_source_get_id(source), App()->OutputAudioSource()))) {
				deviceAudioStatus.audioSourceCount++;
				if (!obs_source_muted(source)) {
					deviceAudioStatus.isAllAudioMuted = false;
					return false;
				}
			}
		}
		return true;
	};

	using allSourceMute_t = decltype(allSourceMute);

	auto preEnum = [](void *param, obs_source_t *source) {
		auto ref = (allSourceMute_t *)(param);
		if (!ref)
			return false;
		return (*ref)(source);
	};

	obs_enum_sources(preEnum, &allSourceMute);

	return deviceAudioStatus;
}

void PLSAudioControl::DeactivateAudioSource(OBSSource source)
{
	const char *id = obs_source_get_id(source);
	if (id && *id) {
		// when deactive a audio device(in setting->Audio->device->disable), judge all the audio are muted at present.
		if (0 == strcmp(id, App()->InputAudioSource()) || (0 == strcmp(id, App()->OutputAudioSource()))) {
			// ignore disabled device,we do not count disabled one in enum loop.
			auto currentStatus = GetMasterAudioStatusCurrent(nullptr, source);
			if (currentStatus.audioSourceCount > 0) {
				bool allMute = currentStatus.isAllAudioMuted;
				UpdateMasterSwitch(allMute);
			}
		}
	}
}

void PLSAudioControl::ActivateAudioSource(OBSSource source) const
{
	// when active a audio source, the muted status of it depends on the master muted status.
	bool mute = ConfigAllMuted();
	if (mute) {
		SetSourcePreviousMuted(source, false);
		obs_source_set_muted(source, true);
	}
}

void PLSAudioControl::VolumeMuted(const QString &sourceName, bool muted)
{
	OBSSource source = pls_get_source_by_name(sourceName.toUtf8().constData());
	if (!source)
		return;
	if (!muted) {
		UpdateMasterSwitch(false);
		SetSourcePreviousMuted(source, muted);
		return;
	}

	bool allMuted = ConfigAllMuted();
	if (!allMuted)
		SetSourcePreviousMuted(source, muted);

	auto muteStatus = GetMasterAudioStatusCurrent();
	if (muteStatus.audioSourceCount > 0 && muteStatus.isAllAudioMuted) {
		UpdateMasterSwitch(true);
	}
}

void PLSAudioControl::OnSourceItemVisibleChanged(OBSSceneItem item, bool visible)
{
	uint32_t flags = obs_source_get_output_flags(obs_sceneitem_get_source(item));
	if (flags & OBS_SOURCE_AUDIO) {
		if (!visible) {
			auto muteStatus = GetMasterAudioStatusCurrent();
			if (muteStatus.audioSourceCount > 0) {
				bool allMute = muteStatus.isAllAudioMuted;
				UpdateMasterSwitch(allMute);
			}
		} else {
			bool allMute = ConfigAllMuted();
			obs_source_t *source = obs_sceneitem_get_source(item);
			if (allMute) {
				obs_source_set_muted(source, allMute);
				SetSourcePreviousMuted(source, allMute);
			}
			UpdateSourceInMixer(source);
		}
	}
}

void PLSAudioControl::OnSourceItemRemoved(OBSSceneItem item)
{
	const obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		auto muteStatus = GetMasterAudioStatusCurrent(source);
		if (muteStatus.audioSourceCount > 0 && muteStatus.isAllAudioMuted) {
			UpdateMasterSwitch(true);
		}
	}
}

void PLSAudioControl::OnSceneRemoved(OBSSource source)
{
	pls_unused(source);
	auto muteStatus = GetMasterAudioStatusCurrent();
	if (muteStatus.audioSourceCount > 0 && muteStatus.isAllAudioMuted) {
		UpdateMasterSwitch(true);
	}
}

//judge if all the audio are muted at present(include audio sources, audio devices)
//@param ignoreAudioSource.
//@describe: ignore the audio source when counting the  audio sources in enum loop(when counting the audio sources,we do not count the removed one).
//@ignoreDeviceSource.
//@describe: ignor the audio device when counting the device sources in enum loop(when counting the device sources,we do not count the removed one).
AudioStatus PLSAudioControl::GetMasterAudioStatusCurrent(const obs_source_t *ignoreAudioSource, const obs_source_t *ignoreDeviceSource) const
{
	AudioStatus masterAudioStatus;
	AudioStatus deviceAudioStatus = GetDeviceAudioStatusCurrent(ignoreDeviceSource);
	AudioStatus audioSourceStatus = GetSourceAudioStatusCurrent(ignoreAudioSource);

	masterAudioStatus.audioSourceCount = deviceAudioStatus.audioSourceCount + audioSourceStatus.audioSourceCount;
	masterAudioStatus.isAllAudioMuted = deviceAudioStatus.isAllAudioMuted && audioSourceStatus.isAllAudioMuted;
	return masterAudioStatus;
}

static bool enumItemToVector(obs_scene_t *t, obs_sceneitem_t *item, void *ptr)
{
	pls_unused(t);
	auto ref = (QVector<OBSSceneItem> *)(ptr);
	if (!ref)
		return false;

	QVector<OBSSceneItem> &items = *ref;

	if (obs_sceneitem_is_group(item)) {
		obs_data_t *data = obs_sceneitem_get_private_settings(item);

		bool collapse = obs_data_get_bool(data, "collapsed");
		if (!collapse) {
			obs_scene_t *scene = obs_sceneitem_group_get_scene(item);

			obs_scene_enum_items(scene, enumItemToVector, &items);
		}

		obs_data_release(data);
	}

	items.insert(0, item);
	return true;
}

//judge if all aduio sources are muted at present.
PLSAudioControl::AudioStatus PLSAudioControl::GetSourceAudioStatusCurrent(const obs_source_t *ignoreSource) const
{
	AudioStatus allAudioSourceCurrentStatus;

	SceneDisplayVector displayVector = PLSSceneDataMgr::Instance()->GetDisplayVector();
	auto iter = displayVector.begin();
	while (iter != displayVector.end()) {
		if (!iter->second) {
			iter++;
			continue;
		}

		OBSScene scene = iter->second->GetData();
		QVector<OBSSceneItem> items;
		obs_scene_enum_items(scene, enumItemToVector, &items);
		for (auto scene_item : items) {
			if (!scene_item)
				continue;
			const obs_source_t *source = obs_sceneitem_get_source(scene_item);
			if (ignoreSource && ignoreSource == source)
				continue;
			uint32_t flags = obs_source_get_output_flags(source);
			bool visible = obs_sceneitem_visible(scene_item);
			if (!(flags & OBS_SOURCE_AUDIO) || !visible)
				continue;
			allAudioSourceCurrentStatus.audioSourceCount++;
			if (!obs_source_muted(source)) {
				allAudioSourceCurrentStatus.isAllAudioMuted = false;
				return allAudioSourceCurrentStatus;
			}
		}
		iter++;
	}
	return allAudioSourceCurrentStatus;
}

// judge if all the previous muted status of audio devices are muted.
AudioStatus PLSAudioControl::GetDeviceAudioStatusPrevious() const
{
	AudioStatus allDeviceAudioStatusPrevious;

	//tranverse all the audio device sources to judge if all the device sources' previous muted status are muted.
	auto allSourceMute = [&allDeviceAudioStatusPrevious](obs_source_t *source) {
		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO) {
			if (0 == strcmp(obs_source_get_id(source), App()->InputAudioSource()) || (0 == strcmp(obs_source_get_id(source), App()->OutputAudioSource()))) {
				allDeviceAudioStatusPrevious.audioSourceCount++;
				if (!SourcePreviousMuted(source)) {
					allDeviceAudioStatusPrevious.isAllAudioMuted = false;
					return false;
				}
			}
		}
		return true;
	};

	using allSourceMute_t = decltype(allSourceMute);

	auto preEnum = [](void *param, obs_source_t *source) {
		auto ref = (allSourceMute_t *)(param);
		if (!ref)
			return false;
		return (*ref)(source);
	};

	obs_enum_sources(preEnum, &allSourceMute);

	return allDeviceAudioStatusPrevious;
}

// judge if all the previous muted status of audio sources are muted.
AudioStatus PLSAudioControl::GetSourceAudioStatusPrevious() const
{
	AudioStatus allSourceAudioStatusPrevious;

	//tranverse all the scenes and sources to judge if all the audio sources' previous muted status are muted.
	SceneDisplayVector displayVector = PLSSceneDataMgr::Instance()->GetDisplayVector();
	auto iter = displayVector.begin();
	while (iter != displayVector.end()) {
		if (!iter->second) {
			iter++;
			continue;
		}

		OBSScene scene = iter->second->GetData();
		QVector<OBSSceneItem> items;
		obs_scene_enum_items(scene, enumItemToVector, &items);
		for (auto scene_item : items) {
			if (!scene_item)
				continue;
			obs_source_t *source = obs_sceneitem_get_source(scene_item);
			uint32_t flags = obs_source_get_output_flags(source);
			bool visible = obs_sceneitem_visible(scene_item);
			if (!(flags & OBS_SOURCE_AUDIO) || !visible)
				continue;
			allSourceAudioStatusPrevious.audioSourceCount++;
			if (!SourcePreviousMuted(source)) {
				allSourceAudioStatusPrevious.isAllAudioMuted = false;
				return allSourceAudioStatusPrevious;
			}
		}
		iter++;
	}
	return allSourceAudioStatusPrevious;
}

AudioStatus PLSAudioControl::GetMasterAudioStatusPrevious() const
{
	AudioStatus masterAudioStatus;

	AudioStatus deviceAudio = GetDeviceAudioStatusPrevious();
	AudioStatus sourceAudio = GetSourceAudioStatusPrevious();

	masterAudioStatus.audioSourceCount += deviceAudio.audioSourceCount + sourceAudio.audioSourceCount;
	masterAudioStatus.isAllAudioMuted = deviceAudio.isAllAudioMuted && sourceAudio.isAllAudioMuted;

	return masterAudioStatus;
}