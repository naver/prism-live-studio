#include "PLSInfoCollector.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <array>

#include "pls-common-define.hpp"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "pls/pls-properties.h"
#include "liblog.h"
#include "log/module_names.h"

using namespace std;
using namespace common;

struct StaticVars {
	static bool init_app;
};
bool StaticVars::init_app = true;

static void logFilter(obs_source_t *, obs_source_t *filter, void *v_val)
{
	const char *name = obs_source_get_name(filter);
	const char *id = obs_source_get_id(filter);
	QJsonArray &filterData = *static_cast<QJsonArray *>(v_val);

	std::array<char, 64> filterPtr;
	sprintf(filterPtr.data(), "%p", filter);
	QString value = QString(name) + " ( " + QString(id) + " : " + filterPtr.data() + " )";

	QJsonObject filterProbs;
	OBSData settings = pls_get_source_setting(filter);
	if (settings) {
		std::string params = obs_data_get_json(settings);
		if (strcmp(params.c_str(), "{}") == 0) {
			params = "{\n \"props\" : \"default value\" \n}";
		}
		QJsonObject props = QJsonDocument::fromJson(QByteArray(params.c_str())).object();
		filterProbs.insert(value, props);
	}

	filterData.push_back(filterProbs);
}

static void logRNNoise(obs_source_t *source, QJsonObject &mixerData_)
{
	const char *id = obs_source_get_id(source);
	if (id && *id && pls_source_support_rnnoise(id)) {
		mixerData_.insert("RNNoise", "off");
		obs_source_enum_filters(
			source,
			[](obs_source_t *, obs_source_t *filter, void *param) {
				if (filter) {
					const char *filterId = obs_source_get_id(filter);
					if (filterId && *filterId && strcmp(filterId, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE) == 0) {
						QJsonObject &mixerData = *static_cast<QJsonObject *>(param);
						const char *rnnoise = obs_source_enabled(filter) ? "on" : "off";
						mixerData.insert("RNNoise", rnnoise);
					}
				}
			},
			&mixerData_);
	}
	return;
}

static void logBalance(const obs_source_t *source, QJsonObject &mixerData)
{
	float value = obs_source_get_balance_value(source) * 100.0f;
	mixerData.insert("balance", (int)value);
}

static void logAudioVolume(obs_source_t *source, QJsonObject &mixerData)
{
	obs_fader_t *fader = obs_fader_create(OBS_FADER_LOG);
	obs_fader_attach_source(fader, source);
	QString volume;
	float db = obs_fader_get_db(fader);
	if (db < -96.0f)
		volume = "-inf dB";
	else
		volume = QString::number(db, 'f', 1).append(" dB");

	mixerData.insert("volume", volume);

	//volume in percent
	float percent = obs_fader_get_deflection(fader) * 100.0f;
	mixerData.insert("volume_percent", QString::number(percent, 'f', 0) + "%");

	obs_fader_destroy(fader);
	return;
}

static void logAudioMixer(obs_source_t *source, QJsonObject &sourceData)
{
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

		logAudioVolume(source, mixerData);
		logRNNoise(source, mixerData);
		logBalance(source, mixerData);

		sourceData.insert("mixer", mixerData);
	}
	return;
}

static void logSource(obs_source_t *source, QJsonObject &sourceData)
{
	logAudioMixer(source, sourceData);
	OBSData paramsData = pls_get_source_setting(source);
	if (paramsData) {
		const char *params = obs_data_get_json(paramsData);
		QJsonObject props = QJsonDocument::fromJson(QByteArray(params)).object();
		sourceData.insert("props", props);
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
	QJsonObject &sceneData = *static_cast<QJsonObject *>(v_val);
	std::array<char, 64> sourcePtr;
	sprintf(sourcePtr.data(), "%p", source);
	QString key = QString(name) + " (" + QString(id) + " : " + sourcePtr.data() + " )";

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
	obs_data_t *vParams = vEncoder ? obs_encoder_get_settings(vEncoder) : nullptr;
	if (vParams) {
		const char *params = obs_data_get_json(vParams);
		QJsonObject props = QJsonDocument::fromJson(QByteArray(params)).object();
		const char *name = obs_encoder_get_name(vEncoder);
		const char *id = obs_encoder_get_id(vEncoder);

		std::array<char, 64> encoderPtr;
		sprintf(encoderPtr.data(), "%p", vEncoder);

		QString key = QString(name) + " ( " + QString(id) + " : " + encoderPtr.data() + " )";
		encoders.insert(key, props);
		obs_data_release(vParams);
	}

	int num_tracks = 0;
	for (;;) {
		obs_encoder_t *aEncoder = obs_output_get_audio_encoder(output, num_tracks);
		if (!aEncoder)
			break;

		obs_data_t *aParams = obs_encoder_get_settings(aEncoder);
		if (aParams) {
			const char *params = obs_data_get_json(aParams);
			QJsonObject props = QJsonDocument::fromJson(QByteArray(params)).object();
			const char *name = obs_encoder_get_name(aEncoder);
			const char *id = obs_encoder_get_id(aEncoder);

			std::array<char, 64> encoderPtr;
			sprintf(encoderPtr.data(),"%p", aEncoder);

			QString key = QString(name) + " ( " + QString(id) + " : " + encoderPtr.data() + " )";
			encoders.insert(key, props);
			obs_data_release(aParams);
		}
		num_tracks++;
	}
}

bool enum_scenes_callback(void *param, obs_source_t *src)
{
	QJsonObject &scenes_ = *static_cast<QJsonObject *>(param);
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
		bool editScene = source == obs_scene_get_source(static_cast<OBSBasic *>(App()->GetMainWindow())->GetCurrentScene());
		QJsonObject sceneInfo;
		sceneInfo.insert("active", active);
		sceneInfo.insert("liveScene", liveScene);
		sceneInfo.insert("editScene", editScene);

		std::array<char, 64> scenePtr;
		sprintf(scenePtr.data(),"%p", source);

		QString key = QString(name) + " ( scene : " + scenePtr.data() + " )[" + QString::number(sceneData.size()) + "]";
		sceneData.insert("info", sceneInfo);
		scenes_.insert(key, sceneData);

		obs_source_release(curScene);
	}
	return true;
}

void PLSInfoCollector::logMsg(std::string type, OBSOutput output)
{
	QJsonObject json;
	QJsonObject scenes;
	obs_enum_scenes(enum_scenes_callback, &scenes);

	QJsonObject aDevices;
	auto EnumDefaultAudioSources = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		QJsonObject &aDevices_ = *static_cast<QJsonObject *>(param);
		if (obs_source_get_flags(source) & DEFAULT_AUDIO_DEVICE_FLAG) {
			const char *name = obs_source_get_name(source);
			const char *id = obs_source_get_id(source);

			std::array<char, 64> devicePtr;
			sprintf(devicePtr.data(), "%p", source);

			QString key = QString(name) + " (" + QString(id) + " : " + devicePtr.data() + " )";

			QJsonObject sourceData;
			logSource(source, sourceData);
			aDevices_.insert(key, sourceData);
		}
		return true;
	};

	obs_enum_sources(EnumDefaultAudioSources, &aDevices);

	json.insert("scenes[" + QString::number(scenes.size()) + "]", scenes);
	if (!aDevices.isEmpty())
		json.insert("audio devices[" + QString::number(aDevices.size()) + "]", aDevices);

	if (type.find("Stop") == std::string::npos) {
		QJsonObject encoders;
		logEncoder(output, encoders);
		if (!encoders.isEmpty())
			json.insert("encoders[" + QString::number(encoders.size()) + "]", encoders);
	}

	QByteArray jsonString = QJsonDocument(json).toJson(QJsonDocument::Indented);

	if (StaticVars::init_app) {
		type = "Init App";
		StaticVars::init_app = false;
	}

	PLS_INFO(INFO_COLLECTOE, "------------------------------------------------");
	PLS_LOGEX(PLS_LOG_INFO, INFO_COLLECTOE, {{"TraceMsg", type.c_str()}}, "[TRACE-MSG] %s Msg: %s", type.c_str(), jsonString.constData());
	PLS_INFO(INFO_COLLECTOE, "------------------------------------------------");
}

QString PLSInfoCollector::getFilterList(obs_source_t *source)
{
	QJsonArray filters;
	obs_source_enum_filters(source, logFilter, &filters);

	if (filters.isEmpty())
		return QString();

	QJsonObject json;
	json.insert("filter[" + QString::number(filters.size()) + "]", filters);

	QByteArray jsonString = QJsonDocument(json).toJson(QJsonDocument::Indented);
	return QString(jsonString.constData());
}

PLSInfoCollector::PLSInfoCollector(QObject *parent) : QObject(parent) {}

PLSInfoCollector::~PLSInfoCollector() = default;
