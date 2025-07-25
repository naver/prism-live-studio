#include "ObsUtils.h"

#include <frontend-api.h>
#include <pls-common-define.hpp>

using namespace rc;

std::vector<obs_sceneitem_t *> ObsUtils::get_scene_items_from_scene(obs_scene_t *scene)
{
	std::vector<obs_sceneitem_t *> result;
	if (!scene)
		return result;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *s, obs_sceneitem_t *item, void *param) {
			if (!s || !item || !param)
				return true;

			auto p_result = (std::vector<obs_sceneitem_t *> *)param;
			p_result->insert(p_result->begin(), item);
			return true;
		},
		&result);

	return result;
}

std::vector<obs_sceneitem_t *> ObsUtils::get_scene_items_from_group(obs_sceneitem_t *group)
{
	std::vector<obs_sceneitem_t *> result;
	if (!group)
		return result;

	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *s, obs_sceneitem_t *item, void *param) {
			if (!s || !item || !param)
				return true;

			auto p_result = (std::vector<obs_sceneitem_t *> *)param;
			p_result->insert(p_result->begin(), item);
			return true;
		},
		&result);
	return result;
}

struct RNNoiseResult {
	obs_source_t *s;
	obs_source_t *f;
};
obs_source_t *ObsUtils::fetch_or_create_rnnoise_filter(obs_source_t *source, bool createWhenNotExist)
{
	if (!source)
		return nullptr;

	if (!is_support_rnnoise(source))
		return nullptr;

	RNNoiseResult result{};
	obs_source_enum_filters(
		source,
		[](obs_source_t *s, obs_source_t *filter, void *param) {
			if (!filter || !s)
				return;

			const char *id = obs_source_get_id(filter);
			if (id && *id && strcmp(id, "noise_suppress_filter_rnnoise") == 0) {
				auto filterData = (RNNoiseResult *)(param);
				filterData->f = filter;
				filterData->s = s;
			}
		},
		&result);

	if (!result.f && createWhenNotExist) {
		QString strFilterName = QString::asprintf("%s_%lld", common::FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE, QDateTime::currentMSecsSinceEpoch());
		obs_source_t *filter = obs_source_create(common::FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE, strFilterName.toUtf8().constData(), nullptr, nullptr);
		if (filter) {
			obs_source_filter_add(source, filter);
			obs_source_release(filter);
		}
		return filter;
	} else {
		return result.f;
	}
}

bool ObsUtils::is_valid_audio_source(const obs_source_t *source)
{
	if (!source)
		return false;

	uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_AUDIO) && obs_source_enabled(source) && obs_source_audio_active(source) && obs_source_active(source);
}

bool ObsUtils::is_support_rnnoise(const obs_source_t *source)
{
	auto aid = obs_source_get_id(source);
	if (!aid)
		return false;

	return pls_source_support_rnnoise(aid);
}

struct FindAudioSource {
	~FindAudioSource() noexcept {}
	obs_scene_t *scene;
	std::vector<obs_source_t *> result;
};
std::vector<obs_source_t *> ObsUtils::get_all_audio_sources()
{
	auto scene_source = obs_frontend_get_current_scene();
	FindAudioSource result{obs_scene_from_source(scene_source)};
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			auto pResult = (FindAudioSource *)data;
			if (!source || !pResult)
				return true;
			if (ObsUtils::is_valid_audio_source(source) && !find_source_recursive(pResult->scene, source))
				pResult->result.push_back(source);
			return true;
		},
		&result);
	if (scene_source)
		obs_source_release(scene_source);
	return result.result;
}

struct FindSceneItemSource {
	obs_sceneitem_t *sceneItem;
	obs_source_t *source;
};
bool ObsUtils::find_source_in_group(obs_sceneitem_t *group, void *param)
{
	if (!group || !param)
		return false;

	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *, obs_sceneitem_t *i, void *p) {
			auto pResult = (FindSceneItemSource *)p;
			if (obs_sceneitem_get_source(i) == pResult->source) {
				pResult->sceneItem = i;
				return false;
			} else {
				return true;
			}
		},
		param);
	auto pResult = (FindSceneItemSource *)param;
	return pResult->sceneItem != nullptr;
}
obs_sceneitem_t *ObsUtils::find_source_recursive(obs_scene_t *scene, obs_source_t *source)
{
	FindSceneItemSource result{nullptr, source};
	if (!scene || !source)
		return nullptr;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *i, void *param) {
			auto pResult = (FindSceneItemSource *)param;
			if (obs_sceneitem_get_source(i) == pResult->source) {
				pResult->sceneItem = i;
				return false;
			}

			if (obs_sceneitem_is_group(i)) {
				return !ObsUtils::find_source_in_group(i, param);
			}

			return true;
		},
		&result);

	return result.sceneItem;
}
bool ObsUtils::is_mixer_hidden(obs_source_t *source)
{
	if (!source)
		return true;

	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	bool hidden = obs_data_get_bool(priv_settings, "mixer_hidden");
	obs_data_release(priv_settings);

	return hidden;
}
int ObsUtils::get_audio_channel(const obs_source_t *source)
{
	if (!source)
		return 0;

	for (int i = 1; i <= 6; i++) {
		auto audioChannelSource = obs_get_output_source(i);
		if (audioChannelSource == source) {
			obs_source_release(audioChannelSource);
			return i;
		} else if (audioChannelSource) {
			obs_source_release(audioChannelSource);
		}
	}
	return 0;
}
float ObsUtils::change_volume(float db, bool isVolumeUp)
{
	clap_db(db);
	db += isVolumeUp ? 10 : -10;
	clap_db(db);
	if (db <= -96.0f && !isVolumeUp)
		db = db - 0.1f;
	return db;
}
rc::BroadcasterState ObsUtils::get_broadcaster_state(rc::BroadcastType broadcastType)
{
	QString outState;
	QString innerState;
	switch (broadcastType) {
	case BroadcastType::Live:
		if (pls_is_rehearsaling())
			innerState = "notReady";
		else
			innerState = pls_get_stream_state();
		break;
	case BroadcastType::Rehearsal:
		if (!pls_is_rehearsaling() && pls_is_rehearsal_info_display())
			innerState = pls_get_stream_state();
		else if (pls_is_rehearsaling())
			innerState = pls_get_stream_state();
		else
			innerState = "notReady";
		break;
	case BroadcastType::Recording:
		innerState = get_fixed_innerrecord_state();
		break;
	default:
		break;
	}
	return convert_broadcaster_state(innerState);
}
QString ObsUtils::get_fixed_innerrecord_state()
{
	if (obs_frontend_recording_paused()) {
		return "paused";
	} else {
		return pls_get_record_state();
	}
}

void ObsUtils::clap_db(float &db)
{
	if (db < -96.0f)
		db = -96.0f;
	else if (db > 0.0f)
		db = 0.0f;
}
