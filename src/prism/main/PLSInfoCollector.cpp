#include "PLSInfoCollector.h"

#include <Windows.h>
#include <QJsonArray>
#include <QJsonDocument>

#include "log.h"
#include "pls-common-define.hpp"
#include "pls-app.hpp"
#include "window-basic-main.hpp"

using namespace std;

static void logFilter(obs_source_t *, obs_source_t *filter, void *v_val)
{
	const char *name = obs_source_get_name(filter);
	const char *id = obs_source_get_id(filter);
	QJsonArray &filterData = *reinterpret_cast<QJsonArray *>(v_val);

	QString value = QString(name) + " (" + QString(id) + ")";
	filterData.push_back(QJsonValue(value));
}

static void logSource(obs_source_t *source, QJsonObject &sourceData)
{
	const char *id = obs_source_get_id(source);
	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		QJsonObject mixerData;
		mixerData.insert("mute", obs_source_muted(source));

		mixerData.insert("monitoring", "none");
		obs_monitoring_type monitoring_type = obs_source_get_monitoring_type(source);
		if (monitoring_type != OBS_MONITORING_TYPE_NONE) {
			const char *type = (monitoring_type == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "monitor only" : "monitor and output";
			mixerData.insert("monitoring", type);
		}

		obs_fader_t *fader = obs_fader_create(OBS_FADER_LOG);
		obs_fader_attach_source(fader, source);
		QString volume;
		float db = obs_fader_get_db(fader);
		if (db < -96.0f)
			volume = "-inf dB";
		else
			volume = QString::number(db, 'f', 1).append(" dB");

		mixerData.insert("volume", volume);
		obs_fader_destroy(fader);

		if (id && *id && (strcmp(AUDIO_INPUT_SOURCE_ID, id) == 0 || strcmp(PRISM_MOBILE_SOURCE_ID, id) == 0 || strcmp(DSHOW_SOURCE_ID, id) == 0 || 0 == strcmp(PRISM_NDI_SOURCE_ID, id))) {
			mixerData.insert("RNNoise", "off");
			obs_source_enum_filters(
				source,
				[](obs_source_t *, obs_source_t *filter, void *param) {
					if (filter) {
						const char *id = obs_source_get_id(filter);
						if (id && *id && strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE) == 0) {
							QJsonObject &mixerData = *reinterpret_cast<QJsonObject *>(param);
							const char *rnnoise = obs_source_enabled(filter) ? "on" : "off";
							mixerData.insert("RNNoise", rnnoise);
						}
					}
				},
				&mixerData);
		}

		sourceData.insert("mixer", mixerData);
	}
	obs_data_t *paramsData = obs_source_get_props_params(source);
	if (paramsData) {
		const char *params = obs_data_get_json(paramsData);
		QJsonObject props = QJsonDocument::fromJson(QByteArray(params)).object();
		sourceData.insert("props", props);
		obs_data_release(paramsData);
	}

	QJsonArray filters;
	obs_source_enum_filters(source, logFilter, &filters);
	if (!filters.isEmpty())
		sourceData.insert("filter[" + QString::number(filters.size()) + "]", filters);
}

static bool logSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *v_val)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	QJsonObject &sceneData = *reinterpret_cast<QJsonObject *>(v_val);
	QString key = QString(name) + " (" + QString(id) + ")";

	if (obs_sceneitem_is_group(item)) {
		QJsonObject groupData;
		obs_sceneitem_group_enum_items(item, logSceneItem, &groupData);
		sceneData.insert(key, groupData);
	} else {
		QJsonObject sourceData;
		logSource(source, sourceData);
		sceneData.insert(key, sourceData);
	}

	return true;
}

static void logEncoder(OBSOutput output, QJsonObject &encoders)
{
	obs_encoder_t *vEncoder = obs_output_get_video_encoder(output);
	obs_data_t *vParams = vEncoder ? obs_encoder_get_props_params(vEncoder) : nullptr;
	if (vParams) {
		const char *params = obs_data_get_json(vParams);
		QJsonObject props = QJsonDocument::fromJson(QByteArray(params)).object();
		const char *name = obs_encoder_get_name(vEncoder);
		const char *id = obs_encoder_get_id(vEncoder);
		QString key = QString(name) + " (" + QString(id) + ")";
		encoders.insert(key, props);
		obs_data_release(vParams);
	}

	int num_tracks = 0;
	for (;;) {
		obs_encoder_t *aEncoder = obs_output_get_audio_encoder(output, num_tracks);
		if (!aEncoder)
			break;

		obs_data_t *aParams = obs_encoder_get_props_params(aEncoder);
		if (aParams) {
			const char *params = obs_data_get_json(aParams);
			QJsonObject props = QJsonDocument::fromJson(QByteArray(params)).object();
			const char *name = obs_encoder_get_name(aEncoder);
			const char *id = obs_encoder_get_id(aEncoder);
			QString key = QString(name) + " (" + QString(id) + ")";
			encoders.insert(key, props);
			obs_data_release(aParams);
		}
		num_tracks++;
	}
}

void PLSInfoCollector::logMsg(std::string type, OBSOutput output)
{
	QJsonObject json;
	QJsonObject scenes;

	auto cb = [](void *param, obs_source_t *src) {
		QJsonObject &scenes = *reinterpret_cast<QJsonObject *>(param);
		obs_scene_t *scene = obs_scene_from_source(src);
		if (scene) {
			obs_source_t *source = obs_scene_get_source(scene);
			const char *name = obs_source_get_name(source);

			QJsonObject sceneData;
			obs_scene_enum_items(scene, logSceneItem, &sceneData);
			QJsonArray filters;
			obs_source_enum_filters(source, logFilter, &filters);
			if (!filters.isEmpty())
				sceneData.insert("filter[" + QString::number(filters.size()) + "]", filters);

			bool active = obs_source_showing(source);
			obs_source_t *curScene = obs_frontend_get_current_scene();
			bool liveScene = source == curScene;
			bool editScene = source == obs_scene_get_source(reinterpret_cast<PLSBasic *>(App()->GetMainWindow())->GetCurrentScene());
			QJsonObject sceneInfo;
			sceneInfo.insert("active", active);
			sceneInfo.insert("liveScene", liveScene);
			sceneInfo.insert("editScene", editScene);

			QString key = QString(name) + " (scene)[" + QString::number(sceneData.size()) + "]";
			sceneData.insert("info", sceneInfo);
			scenes.insert(key, sceneData);

			obs_source_release(curScene);
		}
		return true;
	};
	obs_enum_scenes(cb, &scenes);

	QJsonObject aDevices;
	auto EnumDefaultAudioSources = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		QJsonObject &aDevices = *reinterpret_cast<QJsonObject *>(param);
		if (obs_source_get_flags(source) & DEFAULT_AUDIO_DEVICE_FLAG) {
			const char *name = obs_source_get_name(source);
			const char *id = obs_source_get_id(source);
			QString key = QString(name) + " (" + QString(id) + ")";

			QJsonObject sourceData;
			logSource(source, sourceData);
			aDevices.insert(key, sourceData);
		}
		return true;
	};

	obs_enum_sources(EnumDefaultAudioSources, &aDevices);

	json.insert("scenes[" + QString::number(scenes.size()) + "]", scenes);
	if (!aDevices.isEmpty())
		json.insert("audio devices[" + QString::number(aDevices.size()) + "]", aDevices);

	QJsonObject encoders;
	logEncoder(output, encoders);
	if (!encoders.isEmpty())
		json.insert("encoders[" + QString::number(encoders.size()) + "]", encoders);
	QByteArray jsonString = QJsonDocument(json).toJson(QJsonDocument::Indented);

	PLS_INFO(INFO_COLLECTOE, "------------------------------------------------");
	const char *fields[][2] = {{"TraceMsg", type.c_str()}};
	PLS_LOGEX(PLS_LOG_INFO, INFO_COLLECTOE, fields, 1, "[TRACE-MSG] %s Msg: %s", type.c_str(), jsonString.constData());
	PLS_INFO(INFO_COLLECTOE, "------------------------------------------------");
}

PLSInfoCollector::PLSInfoCollector(QObject *parent) : QObject(parent) {}

PLSInfoCollector::~PLSInfoCollector() {}
