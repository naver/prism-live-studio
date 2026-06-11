#include "RemoteControlDefine.h"

#include <map>
#include <stdexcept>

#include <qfile.h>
#include <qobject.h>

#include <obs-frontend-api.h>

#include "frontend-api.h"

#include <pls-channel-const.h>
#include <pls-common-define.hpp>

#include "EventHandler.h"
#include "ObsUtils.h"
#include "json-data-handler.hpp"

namespace rc {

CommandType parseCommand(const QByteArray &bytes)
{
	QVariant v;
	if (!PLSJsonDataHandler::getValueFromByteArray(bytes, "commandType", v))
		return (CommandType)-1;
	return (CommandType)getTypeForText(v, CommandTypeStrings);
}

std::string parseCommandString(const char* body)
{
	if (!body)
		return std::string("");

	try {
		if (!PLSJsonDataHandler::isValidJsonFormat(body)) {
			assert(false);
			return std::string("");
		}

		QVariant v;
		QByteArray byteArray(body);
		if (!PLSJsonDataHandler::getValueFromByteArray(byteArray, "commandType", v)) {
			assert(false);
			return std::string("");
		}

		QString str = v.toString();
		return str.toStdString();

	} catch (...) {
		assert(false);
		return std::string("");
	}
}

BroadcastType parseBroadcastType(const QJsonObject &commandJson)
{
	QVariant v;
	if (!PLSJsonDataHandler::getValue(commandJson, "broadcastType", v))
		return BroadcastType(-1);
	return (BroadcastType)getTypeForText(v, BroadcastTypeStrings);
}

void get_json_by_error(QJsonObject &json, RCError e, const std::string &errorInfo)
{
	assert(e != RCError::success);
	auto errorDomain = (e == RCError::deniedByPeer || e == RCError::lessThanMinimumRequiredVersion) ? "com.navercorp.ConnectionError" : "com.navercorp.RemoteControlError";

	QJsonObject commandJson;
	commandJson["domain"] = errorDomain;
	commandJson["code"] = (int)e;
	commandJson["message"] = QString(errorInfo.c_str());
	json["commandType"] = QString("failure");
	json["command"] = commandJson;
}

void get_success_json(QJsonObject &json, const QJsonObject &command, const QString &cmd)
{
	json["commandType"] = cmd;
	json["command"] = command;
}

void get_json_by_source(QJsonObject &json, const Source &source)
{
	json["id"] = QString::fromStdString(source.id);
	json["name"] = QString::fromStdString(source.name);
	json["sourceType"] = QString::fromStdString(source.type);
	json["isOn"] = source.isOn;
	json["numberOfChildren"] = source.numberOfChildren < 0 ? 0 : source.numberOfChildren;
}

void get_result_json_by_source(QJsonObject &json, const Source &source)
{
	QJsonObject tmp;
	get_json_by_source(tmp, source);
	json["result"] = tmp;
}

int audio_value(const obs_source_t *s)
{
	auto desktop1 = obs_get_output_source(1);
	auto desktop2 = obs_get_output_source(2);
	int value = 0;
	if (s == desktop1)
		value = 3;
	else if (s == desktop2)
		value = 2;
	else
		value = 1;

	if (desktop1)
		obs_source_release(desktop1);

	if (desktop2)
		obs_source_release(desktop2);

	return value;
}

void get_all_audio_actions(std::vector<Action> &actions)
{
	Action a{};
	bool allmuted = true;
	std::vector<obs_source_t *> audio_sources = ObsUtils::get_all_audio_sources();

	std::sort(audio_sources.begin(), audio_sources.end(), [](const obs_source_t *lhs, const obs_source_t *rhs) { return audio_value(lhs) > audio_value(rhs); });

	for (size_t i = 0; i < audio_sources.size(); i++) {
		get_all_audio_actions_for_source(audio_sources[i], actions);
		if (!obs_source_muted(audio_sources[i]))
			allmuted = false;
	}
	UNUSED_PARAMETER(allmuted);
}

int get_audio_action_count_for_source(const obs_source_t *s)
{
	if (!s || !ObsUtils::is_valid_audio_source(s))
		return 0;

	return ObsUtils::is_support_rnnoise(s) ? 4 : 3;
}

void get_all_audio_actions_for_source(obs_source_t *s, std::vector<Action> &actions)
{
	if (!ObsUtils::is_valid_audio_source(s))
		return;

	Action a{};
	a = get_mute_action(s);
	if (!a.isInvalid())
		actions.push_back(a);
	a = get_volume_up_action(s);
	if (!a.isInvalid())
		actions.push_back(a);
	a = get_volume_down_action(s);
	if (!a.isInvalid())
		actions.push_back(a);
}

Action get_volume_up_action(obs_source_t *s)
{
	Action a{};
	SourceKey key;
	int channel = ObsUtils::get_audio_channel(s);
	key = SourceKey{channel <= 0 ? s : nullptr, DynamicActionType::volumeUp, channel};
	Source src{};

	if (!s)
		return a;

	a.type = "volumeUp";
	a.behavior = "send";
	src.name = std::string(obs_source_get_name(s));
	src.type = "audioDevice";
	src.isOn = true;
	src.numberOfChildren = 0;
	a.id = key.toKey().toStdString();
	src.id = a.id;
	a.source = std::make_optional<Source>(src);

	return a;
}

Action get_volume_down_action(obs_source_t *s)
{
	SourceKey key;
	int channel = ObsUtils::get_audio_channel(s);
	key = SourceKey{channel <= 0 ? s : nullptr, DynamicActionType::volumeDown, channel};
	auto a = get_volume_up_action(s);
	if (a.isInvalid())
		return a;

	a.type = "volumeDown";
	a.id = key.toKey().toStdString();
	if (a.source.has_value())
		a.source.value().id = a.id;

	return a;
}

Action get_mute_action(obs_source_t *s)
{
	Action a{};
	SourceKey key;
	int channel = ObsUtils::get_audio_channel(s);
	key = SourceKey{channel <= 0 ? s : nullptr, DynamicActionType::mute, channel};
	Source src{};
	const char *p = nullptr;

	if (!s)
		return a;

	a.type = "audioDeviceOnOff";
	a.behavior = "turnOnOff";
	src.type = "audioDevice";
	p = obs_source_get_name(s);
	if (p)
		src.name = std::string(p);
	src.isOn = !obs_source_muted(s);
	src.numberOfChildren = 0;
	a.id = key.toKey().toStdString();
	src.id = a.id;
	a.source = std::make_optional<Source>(src);

	return a;
}

Action get_mute_all_action(bool muted)
{
	Action a{};
	NoSourceKey key{DynamicActionType::muteAll};
	Source src{};

	a.type = "audioDeviceOnOff";
	a.behavior = "turnOnOff";
	src.type = "audioDevice";
	src.name = "Mute All";
	src.isOn = !muted;
	src.numberOfChildren = 0;
	a.id = key.toKey().toStdString();
	src.id = a.id;
	a.source = std::make_optional<Source>(src);

	return a;
}

Action get_rnnnoise_action(obs_source_t *s)
{
	Action a{};
	Source src{};
	SourceKey key;
	int channel = ObsUtils::get_audio_channel(s);
	key = SourceKey{channel <= 0 ? s : nullptr, DynamicActionType::rnnoise, channel};
	const char *p = nullptr;
	if (!s)
		return a;
	if (!ObsUtils::is_support_rnnoise(s))
		return a;

	a.type = "noiseCancellingOnOff";
	a.behavior = "turnOnOff";
	src.type = "audioDevice";
	p = obs_source_get_name(s);
	if (p)
		src.name = std::string(p);
	src.isOn = obs_source_enabled(ObsUtils::fetch_or_create_rnnoise_filter(s));
	src.numberOfChildren = 0;
	a.id = key.toKey().toStdString();
	src.id = a.id;
	a.source = std::make_optional<Source>(src);

	return a;
}

// MARK -- scene/sceneitem
void get_all_scene_actions(std::vector<Action> &actions)
{
	obs_frontend_source_list sourcelist{};
	obs_frontend_get_scenes(&sourcelist);
	for (size_t i = 0; i < sourcelist.sources.num; i++) {
		if (!sourcelist.sources.array[i])
			continue;

		auto scene = obs_scene_from_source(sourcelist.sources.array[i]);
		if (!scene)
			continue;

		auto a = get_scene_action(scene);
		if (a.isInvalid())
			continue;

		actions.push_back(a);
	}
	obs_frontend_source_list_free(&sourcelist);
}

struct ChildrenCountResult {
	int *pChildrenCount;
	obs_source_t *pSource;
	std::set<obs_source_t *> audioSourceSet;
};

Action get_scene_action(obs_scene *scene)
{
	Action a{};
	SceneKey key{};
	Source src{};
	const char *p = nullptr;
	ChildrenCountResult result{nullptr, nullptr};

	auto s = obs_scene_get_source(scene);
	if (!s)
		return a;

	key.sceneName = obs_source_get_name(s);

	auto curren_scene_source = obs_frontend_preview_program_mode_active() ? obs_frontend_get_current_preview_scene() : obs_frontend_get_current_scene();
	a.type = "scene";
	a.behavior = "send";
	src.type = "scene";
	p = obs_source_get_name(s);
	if (p)
		src.name = std::string(p);
	src.id = key.toKey().toStdString();
	a.id = src.id;
	src.isOn = s == curren_scene_source;
	src.numberOfChildren = 0;
	result.pChildrenCount = &src.numberOfChildren;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			if (!item || !param) {
				return true;
			}
			auto pResult = (ChildrenCountResult *)param;
			pResult->pSource = obs_sceneitem_get_source(item);
			if (pResult->pSource == nullptr)
				return true;

			*(pResult->pChildrenCount) = *(pResult->pChildrenCount) + 1;
			if (ObsUtils::is_valid_audio_source(pResult->pSource) && pResult->audioSourceSet.find(pResult->pSource) == pResult->audioSourceSet.end()) {
				pResult->audioSourceSet.insert(pResult->pSource);
				*(pResult->pChildrenCount) = *(pResult->pChildrenCount) + get_audio_action_count_for_source(pResult->pSource);
			}
			return true;
		},
		&result);

	a.source = std::make_optional<Source>(src);

	if (curren_scene_source)
		obs_source_release(curren_scene_source);
	return a;
}

Action get_group_item_action(obs_sceneitem_t *groupItem)
{
	Action a{};
	GroupItemKey key{};
	Source src{};
	auto group_scene = obs_sceneitem_group_get_scene(groupItem);
	auto source = obs_sceneitem_get_source(groupItem);
	ChildrenCountResult result{nullptr, nullptr};
	const char *p = nullptr;

	if (!group_scene || !source)
		return a;

	key.group_scene = group_scene;
	key.item = groupItem;
	key.source = source;

	a.type = "groupOnOff";
	a.behavior = "turnOnOff";
	src.type = "group";
	p = obs_source_get_name(source);
	if (p)
		src.name = std::string(p);
	src.id = key.toKey().toStdString();
	a.id = src.id;
	src.isOn = obs_sceneitem_visible(groupItem);
	src.numberOfChildren = 0;
	result.pChildrenCount = &src.numberOfChildren;
	obs_sceneitem_group_enum_items(
		groupItem,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			if (!item || !param) {
				return true;
			}
			auto pResult = (ChildrenCountResult *)param;
			pResult->pSource = obs_sceneitem_get_source(item);
			if (pResult->pSource == nullptr)
				return true;

			*(pResult->pChildrenCount) = *(pResult->pChildrenCount) + 1;
			if (ObsUtils::is_valid_audio_source(pResult->pSource) && pResult->audioSourceSet.find(pResult->pSource) == pResult->audioSourceSet.end()) {
				pResult->audioSourceSet.insert(pResult->pSource);
				*(pResult->pChildrenCount) = *(pResult->pChildrenCount) + get_audio_action_count_for_source(pResult->pSource);
			}
			return true;
		},
		&result);
	a.source = std::make_optional<Source>(src);

	return a;
}

Action get_scene_item_action(obs_sceneitem_t *item)
{
	Action a{};
	SceneItemKey key{};
	const char *p = nullptr;
	Source src{};
	QString sourceType;
	QString actionType;
	auto source = obs_sceneitem_get_source(item);
	if (!item || !source)
		return a;

	if (auto id = obs_source_get_id(source); id != nullptr) {
		get_scene_item_source_type(id, sourceType);
		if (sourceType == "scene")
			actionType = sourceType;
		else if (sourceType == RC_UNKNOWN_SOURCE_TYPE)
			actionType = "";
		else
			actionType = sourceType + "OnOff";
	}

	key.item = item;
	key.source = source;

	a.type = actionType.toStdString();
	a.behavior = "turnOnOff";
	src.type = sourceType.toStdString();
	p = obs_source_get_name(source);
	if (p)
		src.name = std::string(p);
	src.id = key.toKey().toStdString();
	a.id = src.id;
	src.isOn = obs_sceneitem_visible(item);
	src.numberOfChildren = 0;
	a.source = std::make_optional<Source>(src);

	return a;
}

// MARK: -- studio

void get_all_studio_mode_actions(std::vector<Action> &actions)
{
	Action a{};
	a = get_studio_mode_switch_action();
	if (!a.isInvalid())
		actions.push_back(a);
	a = get_apply_draft_action();
	if (!a.isInvalid())
		actions.push_back(a);
}

Action get_studio_mode_switch_action()
{
	Action a{};
	Source src{};
	NoSourceKey key{DynamicActionType::switchStudioMode};

	a.type = "studioModeOnOff";
	a.behavior = "turnOnOff";
	a.id = key.toKey().toStdString();
	src.id = a.id;
	src.isOn = obs_frontend_preview_program_mode_active();
	src.name = "Open Studio Mode";
	src.numberOfChildren = 0;
	src.type = "studioMode";
	a.source = std::make_optional<Source>(src);

	return a;
}

Action get_apply_draft_action()
{
	Action a{};
	NoSourceKey key{DynamicActionType::applyDraft};

	a.type = "applyDraftToLive";
	a.behavior = "send";
	a.id = key.toKey().toStdString();
	a.source = std::nullopt;
	return a;
}

// MARK: -- other
void get_all_other_actions(std::vector<Action> &actions)
{
	Action a{};
	a = get_view_live_action();
	if (!a.isInvalid())
		actions.push_back(a);
	a = get_view_chat_action();
	if (!a.isInvalid())
		actions.push_back(a);
	a = get_show_alert_action();
	if (!a.isInvalid())
		actions.push_back(a);
}

Action get_view_live_action()
{
	Action a{};
	NoSourceKey key{DynamicActionType::viewLive};
	a.type = "viewLive";
	a.behavior = "noBehavior";
	a.id = key.toKey().toStdString();
	a.source = std::nullopt;
	return a;
}

Action get_view_chat_action()
{
	Action a{};
	NoSourceKey key{DynamicActionType::viewChat};
	a.type = "viewChat";
	a.behavior = "noBehavior";
	a.id = key.toKey().toStdString();
	a.source = std::nullopt;
	return a;
}

Action get_show_alert_action()
{
	Action a{};
	NoSourceKey key{DynamicActionType::alertVisible};
	Source s{};

	a.type = "alertOnOff";
	a.behavior = "turnOnOff";
	a.id = key.toKey().toStdString();
	s.id = a.id;
	s.type = "alert";
	s.isOn = pls_alert_message_visible();
	s.name = "alertMessage";
	s.numberOfChildren = pls_alert_message_count();
	a.source = std::make_optional<Source>(s);
	return a;
}

void get_json_by_action_list(QJsonObject &json, const std::vector<Action> &actions)
{
	if (actions.empty())
		return;

	QJsonArray array;
	for (size_t i = 0; i < actions.size(); i++) {
		QJsonObject actionObj;
		actionObj["id"] = QString::fromStdString(actions[i].id);
		actionObj["behavior"] = QString::fromStdString(actions[i].behavior);
		actionObj["type"] = actions[i].type.empty() ? QJsonValue() : QString::fromStdString(actions[i].type);

		if (actions[i].source.has_value()) {
			QJsonObject sourceObj;
			get_json_by_source(sourceObj, actions[i].source.value());
			actionObj["source"] = sourceObj;
		} else {
			actionObj["source"] = QJsonValue();
		}
		actionObj["thumbnailId"] = QJsonValue();
		actionObj["thumbnailURL"] = QJsonValue();
		array.append(actionObj);
	}
	json["result"] = array;
}

// MARK: -- private

BroadcasterState convert_broadcaster_state(const QString &inner)
{
	if (inner == "readyState" || inner == "recordReady")
		return BroadcasterState::ready;
	else if (inner == "broadcastGo" || inner == "canRecord" || inner == "canBroadcastState" || inner == "streamStarting" || inner == "recordStarting")
		return BroadcasterState::preparing;
	else if (inner == "streamStarted" || inner == "recordStarted")
		return BroadcasterState::streaming;
	else if (inner == "stopBroadcastGo" || inner == "canBroadcastStop" || inner == "streamStopping" || inner == "recordStopping" || inner == "recordStopGo")
		return BroadcasterState::finishing;
	else if (inner == "streamStopped" || inner == "streamEnd" || inner == "recordStopped")
		return BroadcasterState::finished;
	else if (inner == "notReady")
		return BroadcasterState::notReady;
	else if (inner == "paused")
		return BroadcasterState::paused;
	else
		return BroadcasterState::unknown;
}

void get_scene_item_source_type(const QString &sourceId, QString &sourceType)
{
	sourceType = "";
	std::map<QString, QString> sourceIds;
	sourceIds[common::SCENE_SOURCE_ID] = "scene";
	sourceIds[common::GROUP_SOURCE_ID] = "group";
	sourceIds[common::OBS_DSHOW_SOURCE_ID] = "webcamVideoCaptureDevice";
#ifdef __APPLE__
	sourceIds[common::OBS_DSHOW_SOURCE_ID_V2] = "webcamVideoCaptureDevice";
	sourceIds[common::OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID] = "macOSScreenCapture";
#endif
	sourceIds[common::WAVEFORM_SOURCE_ID] = "waveFormVisualizer";
	sourceIds[common::AUDIO_INPUT_SOURCE_ID] = "micAudioCaptureDevice";
	sourceIds[common::AUDIO_OUTPUT_SOURCE_ID] = "audioOutputCaptureDevice";
	sourceIds[common::AUDIO_OUTPUT_SOURCE_ID_V2] = "applicationAudioCapture";
	sourceIds[common::OBS_APP_AUDIO_CAPTURE_ID] = "applicationAudioCapture";
	sourceIds[common::GDIP_TEXT_SOURCE_ID] = "text";
	sourceIds[common::GDIP_TEXT_SOURCE_ID_V2] = "text";
	sourceIds[common::GAME_SOURCE_ID] = "game";
	sourceIds[common::WINDOW_SOURCE_ID] = "windowCapture";
	sourceIds[common::PRISM_MONITOR_SOURCE_ID] = "monitorCaptureFull";
	sourceIds[common::PRISM_REGION_SOURCE_ID] = "monitorCapturePart";
	sourceIds[common::BROWSER_SOURCE_ID] = "web";
	sourceIds[common::MEDIA_SOURCE_ID] = "videoMusic";
	sourceIds[common::IMAGE_SOURCE_ID] = "image";
	sourceIds[common::SLIDESHOW_SOURCE_ID] = "imageSlideShow";
	sourceIds[common::COLOR_SOURCE_ID] = "colorPanel";
	sourceIds[common::COLOR_SOURCE_ID_V3] = "colorPanel";
	sourceIds[common::VLC_SOURCE_ID] = "vlcVideoSource";
	sourceIds[common::BGM_SOURCE_ID] = "musicPlaylist";
	sourceIds[common::PRISM_GIPHY_STICKER_SOURCE_ID] = "giphySticker";
	sourceIds[common::PRISM_STICKER_SOURCE_ID] = "prismSticker";
	sourceIds[common::PRISM_CHAT_SOURCE_ID] = "prismChat";
	sourceIds[common::PRISM_CHATV2_SOURCE_ID] = "prismChat";
	sourceIds[common::PRISM_TEXT_TEMPLATE_ID] = "textTemplate";
	sourceIds[common::PRISM_NDI_SOURCE_ID] = "ndiSource";
	sourceIds[common::PRISM_SPECTRALIZER_SOURCE_ID] = "audioVisualizer";
	sourceIds[common::PRISM_BACKGROUND_TEMPLATE_SOURCE_ID] = "backgroundTemplate";
	sourceIds[common::PRISM_TIMER_SOURCE_ID] = "clockWidget";
	sourceIds[common::PRISM_APP_AUDIO_SOURCE_ID] = "applicationAudioCapture";
	sourceIds[common::PRISM_VIEWER_COUNT_SOURCE_ID] = "viewCountWidget";
	sourceIds[common::PRISM_LENS_SOURCE_ID] = "prismLens";
	sourceIds[common::PRISM_LENS_MOBILE_SOURCE_ID] = "prismMobile";

	auto it_find = sourceIds.find(sourceId);
	if (it_find != sourceIds.end())
		sourceType = it_find->second;
	else
		sourceType = RC_UNKNOWN_SOURCE_TYPE;
}

QString getTextForType(int type, const QList<QString> &array)
{
	if (type < 0 || type >= array.size()) {
		return "";
	}

	return array[type];
}

int getTypeForText(const QVariant &v, const QList<QString> &array)
{
	QString qstr = v.toString();
	for (int i = 0; i < array.size(); i++) {
		if (qstr.toLower() == array[i].toLower())
			return i;
	}

	if (qstr.toLower() == "ping" || qstr.toLower() == "pong") {
		return NOT_SUPPORT_COMMAND_TYPE;
	} else {
		return -1;
	}
}

void get_json_by_broadcast_target(QJsonObject &json, const BroadcastTarget &bt)
{
	QString mobilePlatformName = pls_get_remote_control_mobile_name(bt.platformName);
	if (mobilePlatformName.isEmpty()) {
		mobilePlatformName = "unknown";
	}
	json["platform"] = mobilePlatformName.toUtf8().constData();
	json["endURL"] = bt.endURL.c_str();
}

void get_json_by_broadcast(QJsonObject &json, const Broadcast &b)
{
	json["id"] = b.id.c_str();
	json["type"] = getTextForType((int)b.type, BroadcastTypeStrings);
	json["startedDate"] = b.startedDate.has_value() ? b.startedDate.value().c_str() : QJsonValue();
	QJsonArray arr;
	for (size_t i = 0; i < b.targets.size(); i++) {
		QJsonObject o;
		get_json_by_broadcast_target(o, b.targets[i]);
		arr.push_back(o);
	}
	json["targets"] = arr;
}

QString networkError(DisconnectReason reason, bool connectedBofore)
{
	QString hint;

	switch (reason) {
	case DisconnectReason::none:
		if (connectedBofore) {
			hint.append(QObject::tr("remotecontrol.connect.fail.disconnect"));
		} else {
			hint.append(QObject::tr("remotecontrol.connect.fail.connect"));
		}
		hint.append("\n");
		hint.append(QObject::tr("remotecontrol.connect.check.wifi"));
		break;

	case DisconnectReason::connectExists:
		hint.append(QObject::tr("remotecontrol.connect.fail.repeat"));
		break;

	case DisconnectReason::closeByPcUser:
	case DisconnectReason::closeByClient:
		hint.append(QObject::tr("remotecontrol.connect.fail.disconnect"));
		break;
	default:
		break;
	}
	return hint;
}
}
