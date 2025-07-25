#pragma once

#include <string>
#include <optional>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <obs.h>

#include "Logger.h"

constexpr auto REQUIRED_MIN_API_VERSION = 1;
constexpr auto CURRENT_API_VERSION = 1;
constexpr auto NOT_SUPPORT_COMMAND_TYPE = -100;

namespace rc {

// MARK: - Types

enum class CommandType {
	Other = 0,
	StartBroadcast,
	FinishBroadcast,
	UpdateSource,
	SendAction,
	GetActionList,
	GetCurrentBroadcast,
	GetCurrentChatInfo,
	GetDeviceInfo,
	Connect,
	SubscribeSourceUpdated,
	UnsubscribeSourceUpdated,
	DeviceInfo,
	GetSupportedBroadcastTypeList,
	GetBroadcasterState,
	GetBroadcasterNetworkStatus,
	GetStreamingDuration,
	Success,
	Failure,
	Disconnect,
	Ping,
};
const QList<QString> CommandTypeStrings = {
	"Other",
	"startBroadcast",
	"finishBroadcast",
	"updateSource",
	"sendAction",
	"getActionList",
	"getCurrentBroadcast",
	"getCurrentChatInfo",
	"getDeviceInfo",
	"connect",
	"subscribeSourceUpdated",
	"unsubscribeSourceUpdated",
	"deviceInfo",
	"getSupportedBroadcastTypeList",
	"getBroadcasterState",
	"getBroadcasterNetworkStatus",
	"getStreamingDuration",
	"success",
	"failure",
	"disconnect",
	"ping",
};
CommandType parseCommand(const QByteArray &bytes);
std::string parseCommandString(const char *body);

enum class BroadcastType { Live = 0, Rehearsal, Recording };
const QList<QString> BroadcastTypeStrings = {"live", "rehearsal", "recording"};
BroadcastType parseBroadcastType(const QJsonObject &json);

enum class ConnectType { wifi = 0, usb, rc_wifi };
const QList<QString> ConnectTypeStrings = {"connect_wifi", "connect_usb", "remote_control_wifi"};

enum class DynamicActionType { sceneItem = 0, groupItem, scene, mute, rnnoise, volumeUp, volumeDown, muteAll, alertVisible, applyDraft, switchStudioMode, viewLive, viewChat };

// MARK: - Error

enum class RCError {
	success = 0,

	// [wiki](https://wiki.navercorp.com/pages/viewpage.action?pageId=1012807908)
	deniedByPeer = -1000,
	lessThanMinimumRequiredVersion = -1001,
	exceededConnectionLimit = -1002,
	notBroadcasting = -2000, // 방송 중이 아닙니다.

	inner = -4000,
	requestBeforeConn = -4001,
	obsReturnsError = -4002,
	errorSourceName = -4003,
	jsonFormatError = -4004,
	unknownCommandType = -4005,
	audioMuted = -4006,
	sceneNotExist = -4007,
	errorIdFormat = -4008,
	sceneItemNotExist = -4009,
	applyDraftWhenNotStudioMode = -4010,
	stopBroadcastStateError = -4011,
	alreadyConnected = -4012,
	errorConnectType = -4013,
	requestErrorCateSource = -4014,
	badParentSourceIdFormat = -4015,
	subscribeError = -4016,
	appCannotRespond = -4017,
	notSupportCommand = -4018,
	requestFailed = -4019,
};

void get_json_by_error(QJsonObject &json, RCError e, const std::string &errorInfo);
void get_success_json(QJsonObject &json, const QJsonObject &command, const QString &cmd);

// MARK: - Source

struct Source {
	~Source() noexcept(true) {}

	std::string id;
	std::string type;
	std::string name;
	bool isOn;
	int numberOfChildren;
};
constexpr auto RC_UNKNOWN_SOURCE_TYPE = "anonymous";

// MARK: -- json
void get_json_by_source(QJsonObject &json, const Source &source);
void get_result_json_by_source(QJsonObject &json, const Source &source);

// MARK: - Action

struct Action {
	~Action() noexcept {}
	std::string id;
	std::string behavior;
	std::string type;
	std::optional<Source> source;

	bool isInvalid() const { return id.empty() || behavior.empty(); }
};

// MARK: -- audio actions
void get_all_audio_actions(std::vector<Action> &actions);
int get_audio_action_count_for_source(const obs_source_t *s);
void get_all_audio_actions_for_source(obs_source_t *s, std::vector<Action> &actions);
Action get_volume_up_action(obs_source_t *s);
Action get_volume_down_action(obs_source_t *s);
Action get_mute_action(obs_source_t *s);
Action get_mute_all_action(bool muted);
Action get_rnnnoise_action(obs_source_t *s);

// MARK: -- scene/sceneitem
void get_all_scene_actions(std::vector<Action> &actions);
Action get_scene_action(obs_scene *scene);
Action get_group_item_action(obs_sceneitem_t *groupItem);
Action get_scene_item_action(obs_sceneitem_t *item);

// MARK: -- studio mode
void get_all_studio_mode_actions(std::vector<Action> &actions);
Action get_studio_mode_switch_action();
Action get_apply_draft_action();

// MARK: -- other
void get_all_other_actions(std::vector<Action> &actions);
Action get_view_live_action();
Action get_view_chat_action();
Action get_show_alert_action();

// MARK: -- json
void get_json_by_action_list(QJsonObject &json, const std::vector<Action> &actions);

struct BroadcastTarget {
	~BroadcastTarget() noexcept(true) {}
	QString platformName;
	std::string endURL;
};
void get_json_by_broadcast_target(QJsonObject &json, const BroadcastTarget &bt);

struct Broadcast {
	std::string id;
	BroadcastType type;
	std::optional<std::string> startedDate; // GMT +09:00: 2022-08-17T02:46:56.814+09:00
	std::vector<BroadcastTarget> targets;
};
void get_json_by_broadcast(QJsonObject &json, const Broadcast &b);

enum class BroadcasterState { unknown = 0, ready, preparing, streaming, finishing, finished, notReady, paused };
const QList<QString> BroadcasterStateStrs = {"unknown", "ready", "preparing", "streaming", "finishing", "finished", "notReady", "paused"};
BroadcasterState convert_broadcaster_state(const QString &inner);

void get_scene_item_source_type(const QString &sourceId, QString &sourceType);

QString getTextForType(int type, const QList<QString> &array);
int getTypeForText(const QVariant &v, const QList<QString> &array);

enum class DisconnectReason { none = 0, closeByPcUser, connectExists, closeByClient };
const QList<QString> DisconnectReasonStrs = {"tcp disconnect", "closed by pc user", "support only one connection", "closed by mobile"};
QString networkError(DisconnectReason reason, bool connectedBofore);
}
