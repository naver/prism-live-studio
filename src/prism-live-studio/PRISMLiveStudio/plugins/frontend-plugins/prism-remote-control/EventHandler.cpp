#include "EventHandler.h"

#include <iostream>
#include <set>

#include <qdatetime.h>
#include <qapplication.h>
#include <qobject.h>
#include <qwidget.h>

#include <util/util.hpp>
#include <frontend-api.h>
#include <pls-common-define.hpp>

#include "media-io/audio-math.h"

#include "LogHelper.h"
#include "Utils.h"
#include "ObsUtils.h"
#include "RemoteControlDefine.h"
#include "SocketSession.h"
#include "SocketServer.h"
#include "json-data-handler.hpp"

#define FORMAT_STARTED_TIME(dt) (dt.toString("yyyy-MM-ddThh:mm:ss.zzz") + Utils::utc_offset_description(dt))

EventHandler *EventHandler::_instance = nullptr;

using namespace rc;

EventHandler::EventHandler()
{
	auto current = QDateTime::currentDateTime();
	_streamStartedTime = current;
	_streamEndTime = current;
	_recordStartedTime = current;
	_recordEndTime = current;
}

EventHandler *EventHandler::getInstance()
{
	if (_instance)
		return _instance;
	_instance = pls_new<EventHandler>();
	return _instance;
}

// MARK: - Public

void EventHandler::Load()
{
	if (m_isLoaded)
		return;

	// Bind fontend event
	obs_frontend_add_event_callback(EventHandler::obs_frontend_event_received, this);
	pls_frontend_add_event_callback(EventHandler::pls_frontend_event_received, this);

	m_isLoaded = true;
}

void EventHandler::UnLoad()
{
	if (!m_isLoaded)
		return;

	obs_frontend_remove_event_callback(EventHandler::obs_frontend_event_received, this);
	pls_frontend_remove_event_callback(EventHandler::pls_frontend_event_received, this);

	m_isLoaded = false;
}

void EventHandler::SetServer(SocketServer *s)
{
	_server = s;
}

void EventHandler::_handleDynamicCommand(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const
{
	auto strKey = getKeyFromJson(commandJson);
	auto dynamicType = parseDynamicAction(strKey);
	auto apiIdJson = parseKeyByString(strKey);
	QVariant v;

	if (QApplication::activeModalWidget()) {
		sendFailure(requestHeader, session, RCError::appCannotRespond, "PC is in modal.");
		return;
	}

	if ((int)dynamicType < 0)
		throw std::invalid_argument("invalid actionId or id");

	bool isOn = false;
	if (PLSJsonDataHandler::getValue(commandJson, "isOn", v))
		isOn = v.toBool();

	switch (dynamicType) {
	case DynamicActionType::sceneItem:
		this->SetSceneItemVisible(apiIdJson, isOn, requestHeader, session);
		break;

	case DynamicActionType::groupItem:
		this->SetGroupItemVisible(apiIdJson, isOn, requestHeader, session);
		break;

	case DynamicActionType::scene:
		this->SetCurrentScene(apiIdJson, requestHeader, session);
		break;

	case DynamicActionType::mute:
		this->SetSourceMute(apiIdJson, isOn, requestHeader, session);
		break;

	case DynamicActionType::rnnoise:
		this->SetRNNoiseActive(apiIdJson, isOn, requestHeader, session);
		break;

	case DynamicActionType::volumeUp:
		this->SetSourceVolume(apiIdJson, true, requestHeader, session);
		break;

	case DynamicActionType::volumeDown:
		this->SetSourceVolume(apiIdJson, false, requestHeader, session);
		break;

	case DynamicActionType::muteAll:
		this->SetAllSourceMute(isOn, requestHeader, session);
		break;

	case DynamicActionType::alertVisible:
		this->SetAlertMessageVisible(isOn, requestHeader, session);
		break;

	case DynamicActionType::applyDraft:
		this->ApplyDraftToLive(requestHeader, session);
		break;

	case DynamicActionType::switchStudioMode:
		this->SetStudioModeActive(isOn, requestHeader, session);
		break;

	default:
		assert(false);
		break;
	}
}

bool EventHandler::_handleConnectCommand(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const
{
	QVariant v;
	if (!PLSJsonDataHandler::getValue(commandJson, "connectType", v))
		throw std::invalid_argument("lack of connectType");
	auto enumType = getTypeForText(v, ConnectTypeStrings);
	bool ok = false;
	if (enumType < 0)
		throw std::invalid_argument(QString("connectType error %1").arg(v.toString()).toStdString().c_str());
	auto cnnType = (ConnectType)enumType;

	if (!PLSJsonDataHandler::getValue(commandJson, "version", v))
		throw std::invalid_argument("lack of version");
	enumType = v.toInt(&ok);
	if (!ok)
		throw std::invalid_argument(QString("version error %1").arg(v.toString()).toStdString().c_str());
	return this->Connect(cnnType, enumType, requestHeader, session);
}

void EventHandler::_handleActionListCommand(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const
{
	auto apiIdJson = parseKeyByJson(commandJson);
	QVariant v;
	bool ok = false;

	if (!PLSJsonDataHandler::getValue(apiIdJson, "action", v)) {
		LogHelper::getInstance()->writeMessage(session->GetName(), QString("Request all actions"), false);
		GetAllAction(requestHeader, session);
	} else {
		v.toLongLong(&ok);
		if (!ok)
			throw std::invalid_argument("invalid actionId or id");
		auto dynamicType = (DynamicActionType)v.toLongLong();

		if (dynamicType == DynamicActionType::scene) {
			LogHelper::getInstance()->writeMessage(session->GetName(), QString("Request actions of scene"), false);
			auto key = SceneKey::parse(apiIdJson);
			GetActionsOfScene(key, requestHeader, session);
		} else if (dynamicType == DynamicActionType::groupItem) {
			LogHelper::getInstance()->writeMessage(session->GetName(), QString("Request actions of group item"), false);
			auto key = GroupItemKey::parse(apiIdJson);
			GetActionsOfGroupItem(key, requestHeader, session);
		} else {
			assert(false);
		}
	}
}

bool EventHandler::Receive(const DataPackage &pkg, SocketSession *session) const
{
	if (!m_isLoaded)
		return false;

	auto ret = true;

	try {
		if (!PLSJsonDataHandler::isValidJsonFormat(pkg.body)) {
			throw std::invalid_argument("Invalid json format");
		}

		QVariant v;
		CommandType cmd;
		QJsonObject commandJson;

		cmd = parseCommand(pkg.body);
		if ((int)cmd < 0) {
			if ((int)cmd != NOT_SUPPORT_COMMAND_TYPE) {
				Utils::rc_log_warning("X-RC-WARN-LOG", "Unsupport command type", QString("Unsupport command type %1").arg(QString(pkg.body)));
			}
			this->sendFailure(pkg.header, session, RCError::notSupportCommand, "not support command");
			return ret;
		}

		PLSJsonDataHandler::getValuesFromByteArray(pkg.body, "command", commandJson);

		switch (cmd) {
		case CommandType::StartBroadcast:
			this->StartBroadcast(commandJson, pkg.header, session);
			break;

		case CommandType::FinishBroadcast:
			this->StopBroadcast(pkg.header, session, commandJson);
			break;

		case CommandType::GetCurrentBroadcast:
			this->GetCurrentBroadcast(pkg.header, session, parseBroadcastType(commandJson));
			break;

		case CommandType::GetCurrentChatInfo:
			this->GetCurrentChatInfo(pkg.header, session);
			break;

		case CommandType::GetDeviceInfo:
			LogHelper::getInstance()->writeMessage(session->GetName(), QString("Request device info"), false);
			this->GetDeviceInfo(pkg.header, session);
			break;

		case CommandType::UpdateSource:
		case CommandType::SendAction:
			_handleDynamicCommand(commandJson, pkg.header, session);
			break;

		case CommandType::GetActionList:
			_handleActionListCommand(commandJson, pkg.header, session);
			break;

		case CommandType::Connect:
			ret = _handleConnectCommand(commandJson, pkg.header, session);
			break;

		case CommandType::SubscribeSourceUpdated:
			this->Subscribe(commandJson["sourceIds"].toArray(), pkg.header, session);
			break;

		case CommandType::UnsubscribeSourceUpdated:
			this->UnSubscribe(commandJson["sourceIds"].toArray(), pkg.header, session);
			break;

		case CommandType::DeviceInfo:
			session->SetPeerDeviceInfo(commandJson);
			break;

		case CommandType::GetSupportedBroadcastTypeList:
			this->GetSupportedBroadcastTypeList(pkg.header, session);
			break;

		case CommandType::GetBroadcasterState:
			this->GetBroadcasterState(pkg.header, session, parseBroadcastType(commandJson));
			break;

		case CommandType::GetBroadcasterNetworkStatus:
			this->GetBroadcasterNetworkStatus(pkg.header, session);
			break;

		case CommandType::GetStreamingDuration:
			this->GetStreamingDuration(pkg.header, session, parseBroadcastType(commandJson));
			break;

		case CommandType::Success:
			break;
		case CommandType::Failure:
			Utils::rc_log_warning("X-RC-WARN-LOG", "Receive failure command", QString("Receive failure command %1").arg(QString(pkg.body)));
			RC_LOG_WARN("Receive failture command: %s", QString(pkg.body).toStdString().c_str());
			break;

		case CommandType::Disconnect:
			session->SetDisconnectReason(DisconnectReason::closeByClient);
			this->sendSuccess(pkg.header, session, QJsonObject());
			break;

		case CommandType::Ping:
			session->SendReply(pkg.header, QByteArray());
			break;

		default:
			assert(false);
			Utils::rc_log_warning("X-RC-WARN-LOG", "Unhandle command type", QString("Unhandle command type %1").arg(QString(pkg.body)));
			this->sendFailure(pkg.header, session, RCError::notSupportCommand, "not support command");
			break;
		}
	} catch (const std::exception &e) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Parse json error", QString("Parse json error %1").arg(QString(getTextOfDataPackage(pkg))));
		sendFailure(pkg.header, session, RCError::jsonFormatError, e.what());
	}
	return ret;
}

void EventHandler::StartBroadcast(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const
{
	auto type = parseBroadcastType(commandJson);
	LogHelper::getInstance()->writeMessage(session->GetName(), QString("Click start broadcast %1").arg(getTextForType((int)type, BroadcastTypeStrings)));

	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject paramJson;
	QJsonObject successCmdJson;
	auto state = BroadcasterState::unknown;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	switch (type) {
	case BroadcastType::Live:
		state = convert_broadcaster_state(pls_get_stream_state());
		if (state != BroadcasterState::ready && state != BroadcasterState::finished)
			break;

		if (pls_is_rehearsal_info_display()) {
			pls_start_broadcast_in_info(true);
		} else if (QApplication::activeModalWidget()) {
			sendFailure(requestHeader, session, RCError::appCannotRespond, "PC is in modal.");
			return;
		} else if (pls_get_current_selected_channel_count() <= 0) {
			sendFailure(requestHeader, session, RCError::requestFailed, "No selected channel");
			return;
		} else {
			QMetaObject::invokeMethod(qApp, []() { pls_start_broadcast(true, ControlSrcType::RemoteControl); }, Qt::QueuedConnection);
		}
		break;
	case BroadcastType::Rehearsal:
		if (pls_is_rehearsaling())
			break;

		if (pls_is_rehearsal_info_display()) {
			QMetaObject::invokeMethod(qApp, []() { pls_start_rehearsal(true); }, Qt::QueuedConnection);
		} else if (QApplication::activeModalWidget()) {
			sendFailure(requestHeader, session, RCError::appCannotRespond, "PC is in modal.");
			return;
		}
		break;
	case BroadcastType::Recording:
		if (pls_is_recording())
			break;
		if (QApplication::activeModalWidget()) {
			sendFailure(requestHeader, session, RCError::appCannotRespond, "PC is in modal.");
			return;
		} else {
			QMetaObject::invokeMethod(qApp, []() { pls_start_record(true); }, Qt::QueuedConnection);
		}
		break;
	default:
		break;
	}

	auto b = get_current_broadcast(type);
	get_json_by_broadcast(paramJson, b);
	successCmdJson["result"] = paramJson;
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::StopBroadcast(const CommandHeader &requestHeader, SocketSession *session, const QJsonObject &commandJson) const
{
	auto broadcastType = parseBroadcastType(commandJson);
	LogHelper::getInstance()->writeMessage(session->GetName(), QString("Click stop broadcast %1").arg(getTextForType((int)broadcastType, BroadcastTypeStrings)));
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (QApplication::activeModalWidget()) {
		sendFailure(requestHeader, session, RCError::appCannotRespond, "PC is in modal.");
		return;
	}

	switch (broadcastType) {
	case BroadcastType::Live:
	case BroadcastType::Rehearsal:
		QMetaObject::invokeMethod(qApp, []() { pls_start_broadcast(false, ControlSrcType::RemoteControl); }, Qt::QueuedConnection);
		break;
	case BroadcastType::Recording:
		QMetaObject::invokeMethod(qApp, []() { pls_start_record(false, ControlSrcType::RemoteControl); }, Qt::QueuedConnection);
		break;
	default:
		break;
	}

	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetCurrentScene(const QJsonObject &apiIdJson, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), QString("Set current scene"));
	auto key = SceneKey::parse(apiIdJson);
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};

	auto scene = key.getScene();
	auto scene_source = obs_scene_get_source(scene);

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (!scene_source || !scene) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Fail to find scene", QString("Fail to find scene by key %1").arg(key.toKey()));
		errorInfo = QString("Cannot find scene source by name: %1").arg(key.toKey()).toStdString();
		e = RCError::sceneNotExist;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(scene_source);
	else
		obs_frontend_set_current_scene(scene_source);

	a = get_scene_action(scene);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetGroupItemVisible(const QJsonObject &apiIdJson, bool visible, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), QString("Set scene item visible"));
	auto key = GroupItemKey::parse(apiIdJson);
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};

	auto groupItem = pls_get_sceneitem_by_pointer_address(key.item);

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (!groupItem) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Fail to find scene item", QString("Fail to find scene item by key %1").arg(key.toKey()));
		errorInfo = QString("Cannot find scene item by key %1").arg(key.toKey()).toStdString();
		e = RCError::sceneItemNotExist;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	obs_sceneitem_set_visible(groupItem, visible);
	a = get_group_item_action(groupItem);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetSceneItemVisible(const QJsonObject &apiIdJson, bool visible, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), QString("Set scene item visible"));
	auto key = SceneItemKey::parse(apiIdJson);
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};
	auto sceneItem = pls_get_sceneitem_by_pointer_address(key.item);

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (!sceneItem) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Fail to find scene item", QString("Fail to find scene item by key %1").arg(key.toKey()));
		errorInfo = QString("Cannot find scene item by key %1").arg(key.toKey()).toStdString();
		e = RCError::sceneItemNotExist;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	obs_sceneitem_set_visible(sceneItem, visible);
	a = get_scene_item_action(sceneItem);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetAllSourceMute(bool isOn, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), QString("Set mute all"));
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (!QMetaObject::invokeMethod(qApp, [isOn]() { pls_mixer_mute_all(!isOn); }, Qt::QueuedConnection)) {
		errorInfo = "obs pls_mixer_mute_all error";
		e = RCError::obsReturnsError;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	a = get_mute_all_action(!isOn);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetSourceMute(const QJsonObject &apiIdJson, bool isOn, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), "Set mute");
	auto key = SourceKey::parse(apiIdJson);
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};
	OBSSource src{};

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	src = key.getOBSSource();
	if (!src) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Fail to find audio source", QString("Fail to find audio source by key %1").arg(key.toKey()));
		errorInfo = QString("Cannot find source by name: %1").arg(key.toKey()).toStdString();
		e = RCError::errorSourceName;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	obs_source_set_muted(src, !isOn);

	a = get_mute_action(src);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}

	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetSourceVolume(const QJsonObject &apiIdJson, bool volumeUp, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), volumeUp ? "Set volume up" : "Set volume down");
	auto key = SourceKey::parse(apiIdJson);
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	OBSSource src{};
	Action a{};

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	src = key.getOBSSource();
	if (!src) {
		errorInfo = QString("Cannot find source by name: %1").arg(key.toKey()).toStdString();
		e = RCError::errorSourceName;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (obs_source_muted(src)) {
		errorInfo = QString("Cannot set volume when muted").arg(key.toKey()).toStdString();
		e = RCError::audioMuted;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	float db = mul_to_db(obs_source_get_volume(src));
	db = ObsUtils::change_volume(db, volumeUp);

	obs_source_set_volume(src, db_to_mul(db));

	a = volumeUp ? get_volume_up_action(src) : get_volume_down_action(src);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetRNNoiseActive(const QJsonObject &apiIdJson, bool active, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), "Set rnnoise");
	auto key = SourceKey::parse(apiIdJson);
	RCError e = RCError::success;
	std::string errorInfo;
	Action a{};
	QJsonObject successCmdJson;
	OBSSource src{};
	obs_source_t *rnnoisesource = nullptr;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	src = key.getOBSSource();
	rnnoisesource = ObsUtils::fetch_or_create_rnnoise_filter(src, true);
	if (!rnnoisesource) {
		Utils::rc_log_warning("X-RC-WARN-LOG", "Fail to find rnnoise audio source", QString("Fail to find rnnoise audio source by key %1").arg(key.toKey()));
		errorInfo = QString("Cannot find source by name: %1").arg(key.toKey()).toStdString();
		e = RCError::errorSourceName;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	obs_source_set_enabled(rnnoisesource, active);

	a = get_rnnnoise_action(src);
	if (a.source.has_value()) {
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetStudioModeActive(bool active, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), "Set studio mode");
	RCError e = RCError::success;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (active == obs_frontend_preview_program_mode_active()) {
		RC_LOG_INFO("No need set studio mode");

		QJsonObject successCmdJson;
		Action a = get_studio_mode_switch_action();

		if (a.source.has_value())
			get_result_json_by_source(successCmdJson, a.source.value());
		sendSuccess(requestHeader, session, successCmdJson);
		return;
	}

	std::weak_ptr<SocketSession> weakSession = session->getWeakPointer();
	auto callback = [active, requestHeader, weakSession]() {
		bool successed = pls_frontend_set_preview_program_mode(active);

		auto session = weakSession.lock();
		if (!session)
			return; // session has been destroyed, never need to response

		if (successed) {
			QJsonObject successCmdJson;
			Action a = get_studio_mode_switch_action();
			if (a.source.has_value()) {
				a.source.value().isOn = obs_frontend_preview_program_mode_active();
				get_result_json_by_source(successCmdJson, a.source.value());
			}
			EventHandler::getInstance()->sendSuccess(requestHeader, session.get(), successCmdJson);
		} else {
			std::string errorInfo = "pls_frontend_set_preview_program_mode returns false";
			EventHandler::getInstance()->sendFailure(requestHeader, session.get(), RCError::obsReturnsError, errorInfo);
		}
	};

	if (!QMetaObject::invokeMethod(qApp, callback, Qt::QueuedConnection)) {
		errorInfo = "pls_frontend_set_preview_program_mode returns false";
		sendFailure(requestHeader, session, RCError::obsReturnsError, errorInfo);
	}
}

void EventHandler::ApplyDraftToLive(const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), "Adding to Live");
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (!obs_frontend_preview_program_mode_active()) {
		errorInfo = "Studio mode is not active";
		e = RCError::applyDraftWhenNotStudioMode;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	obs_frontend_preview_program_trigger_transition();

	a = get_apply_draft_action();
	if (a.source.has_value())
		get_result_json_by_source(successCmdJson, a.source.value());
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::SetAlertMessageVisible(bool visible, const CommandHeader &requestHeader, SocketSession *session) const
{
	LogHelper::getInstance()->writeMessage(session->GetName(), "Set alert visible");
	RCError e = RCError::success;
	std::string errorInfo;
	QJsonObject successCmdJson;
	Action a{};

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	if (pls_alert_message_visible() == visible) {
		RC_LOG_INFO("No need set alert");
		a = get_show_alert_action();
		if (a.source.has_value())
			get_result_json_by_source(successCmdJson, a.source.value());
		sendSuccess(requestHeader, session, successCmdJson);
		return;
	}

	if (!QMetaObject::invokeMethod(qApp, []() { pls_click_alert_message(); }, Qt::QueuedConnection)) {
		errorInfo = "pls_click_alert_message executes error";
		e = RCError::obsReturnsError;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	a = get_show_alert_action();
	if (a.source.has_value()) {
		a.source.value().isOn = visible;
		get_result_json_by_source(successCmdJson, a.source.value());
	}
	sendSuccess(requestHeader, session, successCmdJson);
}

void EventHandler::GetAllAction(const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::vector<Action> actionList;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	get_all_studio_mode_actions(actionList);
	get_all_scene_actions(actionList);
	get_all_audio_actions(actionList);
	get_all_other_actions(actionList);

	get_json_by_action_list(cmdJson, actionList);

	sendSuccess(requestHeader, session, cmdJson);
}

void EventHandler::GetActionsOfGroupItem(const GroupItemKey &key, const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::vector<Action> actionList;
	std::string errorInfo;

	auto group = pls_get_sceneitem_by_pointer_address(key.item);
	auto scene_items = ObsUtils::get_scene_items_from_group(group);
	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	std::set<obs_source_t *> audioSourceSet;
	for (size_t i = 0; i < scene_items.size(); i++) {
		actionList.push_back(get_scene_item_action(scene_items[i]));
		auto source = obs_sceneitem_get_source(scene_items[i]);
		if (ObsUtils::is_valid_audio_source(source) && audioSourceSet.find(source) == audioSourceSet.end()) {
			audioSourceSet.insert(source);
			get_all_audio_actions_for_source(source, actionList);
		}
	}
	get_json_by_action_list(cmdJson, actionList);

	sendSuccess(requestHeader, session, cmdJson);
}

void EventHandler::GetActionsOfScene(const SceneKey &key, const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::vector<Action> actionList;
	std::string errorInfo;

	auto scene = key.getScene();
	auto scene_items = ObsUtils::get_scene_items_from_scene(scene);

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}
	std::set<obs_source_t *> audioSourceSet;
	for (size_t i = 0; i < scene_items.size(); i++) {
		if (obs_sceneitem_is_group(scene_items[i])) {
			actionList.push_back(get_group_item_action(scene_items[i]));
		} else {
			actionList.push_back(get_scene_item_action(scene_items[i]));
		}
		auto source = obs_sceneitem_get_source(scene_items[i]);
		if (ObsUtils::is_valid_audio_source(source) && audioSourceSet.find(source) == audioSourceSet.end()) {
			audioSourceSet.insert(source);
			get_all_audio_actions_for_source(source, actionList);
		}
	}
	get_json_by_action_list(cmdJson, actionList);

	sendSuccess(requestHeader, session, cmdJson);
}

void EventHandler::GetStreamingDuration(const CommandHeader &requestHeader, SocketSession *session, rc::BroadcastType broadcastType) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}
	double duration = 0.0;
	BroadcasterState state = ObsUtils::get_broadcaster_state(broadcastType);
	switch (broadcastType) {
	case BroadcastType::Live:
	case BroadcastType::Rehearsal:
		if (state == BroadcasterState::streaming)
			duration = Utils::calc_duration(true, _streamStartedTime, _streamEndTime);
		else if (state == BroadcasterState::finished || state == BroadcasterState::finishing)
			duration = Utils::calc_duration(false, _streamStartedTime, _streamEndTime);
		break;
	case BroadcastType::Recording:
		duration = pls_get_record_duration();
		break;
	default:
		break;
	}
	cmdJson["result"] = duration;
	sendSuccess(requestHeader, session, cmdJson);
}

void EventHandler::GetBroadcasterNetworkStatus(const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	cmdJson["result"] = "good";

	sendSuccess(requestHeader, session, cmdJson);
}
void EventHandler::GetBroadcasterState(const CommandHeader &requestHeader, SocketSession *session, rc::BroadcastType broadcastType) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	QString outState = getTextForType((int)ObsUtils::get_broadcaster_state(broadcastType), BroadcasterStateStrs);

	cmdJson["result"] = outState;
	sendSuccess(requestHeader, session, cmdJson);
}
void EventHandler::GetSupportedBroadcastTypeList(const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	QJsonArray array;
	array.append("broadcast");
	array.append("recording");
	cmdJson["result"] = array;
	sendSuccess(requestHeader, session, cmdJson);
}

void EventHandler::GetCurrentBroadcast(const CommandHeader &requestHeader, SocketSession *session, rc::BroadcastType broadcastType) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	QJsonObject paramJson;
	std::string errorInfo;

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	bool playing = false;
	auto state = ObsUtils::get_broadcaster_state(broadcastType);
	switch (state) {
	case rc::BroadcasterState::streaming:
	case rc::BroadcasterState::finishing:
	case rc::BroadcasterState::finished:
		playing = true;
		break;
	default:
		break;
	}

	if (playing) {
		auto b = get_current_broadcast(broadcastType);
		get_json_by_broadcast(paramJson, b);
		cmdJson["result"] = paramJson;
		sendSuccess(requestHeader, session, cmdJson);
	} else {
		e = RCError::notBroadcasting;
		errorInfo = "방송 중이 아닙니다.";
		sendFailure(requestHeader, session, e, errorInfo);
	}
}

void EventHandler::GetCurrentChatInfo(const CommandHeader &requestHeader, SocketSession *session) const
{
	QString id;
	QString cookie;
	bool isSinglePlatform = false;
	int seqHorizontal;
	int seqVertical;
	bool localRet = pls_get_chat_info(id, seqHorizontal, seqVertical, cookie, isSinglePlatform);

	if (localRet) {
		QJsonObject cmdJson;
		QJsonObject paramJson;
		QJsonObject cookieJson;

		cookieJson["NEO_SES"] = cookie;
		paramJson["id"] = id;
		if (seqVertical > 0) {
			paramJson["ids"] = QJsonArray({seqHorizontal, seqVertical});
		} else {
			paramJson["ids"] = QJsonArray({seqHorizontal});
		}
		paramJson["cookieDic"] = cookieJson;
		paramJson["singlePlatform"] = isSinglePlatform;
		cmdJson["result"] = paramJson;
		sendSuccess(requestHeader, session, cmdJson);
	} else {
		Utils::rc_log_warning("X-RC-WARN-LOG", "pls_get_chat_info error", "pls_get_chat_info error");
		this->sendFailure(requestHeader, session, RCError::requestFailed, "getCurrentChatInfo failed");
	}
}

void EventHandler::GetDeviceInfo(const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;

	if (!IsLoaded()) {
		errorInfo = "Call before load";
		e = RCError::inner;
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	cmdJson["id"] = QSysInfo::machineUniqueId().constData();
	cmdJson["name"] = QSysInfo::machineHostName();
#if WIN32
	cmdJson["os"] = "windows";
#else
	cmdJson["os"] = "mac";
#endif // WIN32
	cmdJson["osVersion"] = QSysInfo::productVersion();

	sendSuccess(requestHeader, session, cmdJson, "deviceInfo");
}

bool EventHandler::Connect(ConnectType cnn, int version, const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	QJsonObject resultObj;
	std::string errorInfo;

	if (!IsLoaded()) {
		errorInfo = "Call before load";
		e = RCError::inner;
		sendFailure(requestHeader, session, e, errorInfo);
		return false;
	}
	if (cnn != ConnectType::rc_wifi) {
		PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-DISCONNECT-REASON", "Wrong connect type"}}, "Wrong connect type %d", (int)cnn);
		errorInfo = QString("errorConnectType %1").arg((int)cnn).toStdString();
		e = RCError::errorConnectType;
		sendFailure(requestHeader, session, e, errorInfo);
		session->Disconnect();
		return false;
	}
	if (version < REQUIRED_MIN_API_VERSION) {
		PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-DISCONNECT-REASON", "Wrong version"}}, "Wrong version %d", version);
		errorInfo = QString("lessThanMinimumRequiredVersion %1").arg(version).toStdString();
		e = RCError::lessThanMinimumRequiredVersion;
		sendFailure(requestHeader, session, e, errorInfo);
		session->Disconnect();
		return false;
	}

	if (session->GetDisconnectReason() == DisconnectReason::connectExists) {
		e = RCError::exceededConnectionLimit;
		errorInfo = "exceeded connection limit";
		sendFailure(requestHeader, session, e, errorInfo);
		session->Disconnect();
		return false;
	}

	if (_server && !session->acceptedConnect() && _server->ValidConnectionCount() > 0) {
		session->SetDisconnectReason(DisconnectReason::connectExists);
		e = RCError::exceededConnectionLimit;
		errorInfo = "exceeded connection limit";
		sendFailure(requestHeader, session, e, errorInfo);
		session->Disconnect();
		return false;
	}

	resultObj["version"] = CURRENT_API_VERSION;
	resultObj["sessionKey"] = session->SessionKey().c_str();
	cmdJson["result"] = resultObj;
	session->SetConnected();
	sendSuccess(requestHeader, session, cmdJson);

	QJsonObject json;
	json["commandType"] = CommandTypeStrings[(int)CommandType::GetDeviceInfo];
	json["command"] = QJsonObject();
	session->SendRequest(QJsonDocument(json).toJson(QJsonDocument::Compact));
	return true;
}

void EventHandler::Subscribe(const QJsonArray &sourceIds, const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;
	auto vList = sourceIds.toVariantList();

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	std::for_each(vList.begin(), vList.end(), [session](const QVariant &v) {
		if (!v.toString().isEmpty()) {
			session->SubscribeKey(v.toString());
		}
	});

	sendSuccess(requestHeader, session, cmdJson);
}

void EventHandler::UnSubscribe(const QJsonArray &sourceIds, const CommandHeader &requestHeader, SocketSession *session) const
{
	RCError e = RCError::success;
	QJsonObject cmdJson;
	std::string errorInfo;
	auto vList = sourceIds.toVariantList();

	if (common_check(e, session, errorInfo) != RCError::success) {
		sendFailure(requestHeader, session, e, errorInfo);
		return;
	}

	std::for_each(vList.begin(), vList.end(), [session](const QVariant &v) {
		if (!v.toString().isEmpty()) {
			session->UnsubscribeKey(v.toString());
		}
	});

	sendSuccess(requestHeader, session, cmdJson);
}

// MARK: - pls callbacks

void EventHandler::_handleStreamingState(const QString &strState)
{
	auto state = convert_broadcaster_state(strState);
	bool isExclude = true;

	switch (state) {
	case rc::BroadcasterState::preparing:
		if (pls_is_rehearsal_info_display() && !pls_is_rehearsaling()) {
			isExclude = false;
			broadcast_broadcaster_state_changed(BroadcastType::Live, state);
			broadcast_broadcaster_state_changed(BroadcastType::Rehearsal, state);
		} else if (pls_is_rehearsaling()) {
			broadcast_broadcaster_state_changed(BroadcastType::Rehearsal, state);
		} else {
			broadcast_broadcaster_state_changed(BroadcastType::Live, state);
		}
		break;

	case rc::BroadcasterState::ready:
		broadcast_broadcaster_state_changed(pls_is_rehearsaling() ? BroadcastType::Rehearsal : BroadcastType::Live, state);
		break;
	case rc::BroadcasterState::finishing:
	case rc::BroadcasterState::finished:
		_streamEndTime = QDateTime::currentDateTime();
		broadcast_broadcaster_state_changed(pls_is_rehearsaling() ? BroadcastType::Rehearsal : BroadcastType::Live, state);
		break;

	case rc::BroadcasterState::streaming:
		_streamStartedTime = QDateTime::currentDateTime();
		broadcast_broadcaster_state_changed(pls_is_rehearsaling() ? BroadcastType::Rehearsal : BroadcastType::Live, state);
		break;

	default:
		break;
	}

	if (isExclude) {
		if (pls_is_rehearsaling())
			broadcast_broadcaster_state_changed(BroadcastType::Live, BroadcasterState::notReady);
		else
			broadcast_broadcaster_state_changed(BroadcastType::Rehearsal, BroadcasterState::notReady);
	}
}
void EventHandler::_handleRecordingState(const QString &strState)
{
	auto state = convert_broadcaster_state(strState);
	switch (state) {
	case rc::BroadcasterState::streaming:
		_recordStartedTime = QDateTime::currentDateTime();
		if (obs_frontend_recording_paused()) {
			state = rc::BroadcasterState::paused;
		}
		break;
	case rc::BroadcasterState::finishing:
	case rc::BroadcasterState::finished:
		_recordEndTime = QDateTime::currentDateTime();
		break;
	default:
		break;
	}
	broadcast_broadcaster_state_changed(BroadcastType::Recording, state);
}
void EventHandler::obs_frontend_event_received(obs_frontend_event event, void *context)
{
	auto h = (EventHandler *)context;

	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		h->_handleRecordingState(pls_get_record_state());
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		h->_handleRecordingState(pls_get_record_state());
		break;
	default:
		break;
	}
}
void EventHandler::pls_frontend_event_received(pls_frontend_event event, const QVariantList &params, void *context)
{
	auto h = (EventHandler *)context;

	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_STREAM_STATE_CHANGED:
		if (params.size() > 0)
			h->_handleStreamingState(params.at(0).toString());
		break;

	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_RECORD_STATE_CHANGED:
		if (params.size() > 0)
			h->_handleRecordingState(params.at(0).toString());
		break;

	default:
		break;
	}
}

RCError EventHandler::common_check(RCError &e, const SocketSession *session, std::string &errorInfo) const
{
	e = RCError::success;

	if (!IsLoaded()) {
		errorInfo = "Call before load";
		e = RCError::inner;
	}
	if (!session->acceptedConnect()) {
		errorInfo = "Request before connect";
		e = RCError::requestBeforeConn;
	}
	return e;
}

void EventHandler::sendSuccess(const CommandHeader &requestHeader, SocketSession *s, const QJsonObject &successCmdJson, const QString &cmd) const
{
	QJsonObject json;
	get_success_json(json, successCmdJson, cmd);
	s->SendReply(requestHeader, QJsonDocument(json).toJson(QJsonDocument::Compact));
}
void EventHandler::sendFailure(const CommandHeader &requestHeader, SocketSession *s, rc::RCError e, const std::string &errorInfo) const
{
	QJsonObject json;
	get_json_by_error(json, e, errorInfo);
	RC_LOG_WARN("%s", errorInfo.c_str());
	s->SendReply(requestHeader, QJsonDocument(json).toJson(QJsonDocument::Compact));
}

rc::Broadcast EventHandler::get_current_broadcast(rc::BroadcastType type) const
{
	auto channelInfos = pls_get_user_active_channles_info();
	std::vector<BroadcastTarget> targets;
	std::transform(channelInfos.begin(), channelInfos.end(), std::back_insert_iterator(targets), [](std::tuple<QString, QString> info) {
		BroadcastTarget t{};
		t.platformName = std::get<0>(info);
		t.endURL = std::get<1>(info).toStdString();
		return t;
	});
	Broadcast b{};
	switch (type) {
	case rc::BroadcastType::Live:
		b.id = "broadcast-living";
		b.type = BroadcastType::Live;
		if (pls_is_streaming() && !pls_is_rehearsaling())
			b.startedDate = std::make_optional<std::string>(FORMAT_STARTED_TIME(_streamStartedTime).toStdString());
		else
			b.startedDate = std::nullopt;
		b.targets = targets;
		break;
	case rc::BroadcastType::Rehearsal:
		b.id = "broadcast-rehearsaling";
		b.type = BroadcastType::Rehearsal;
		if (pls_is_streaming() && pls_is_rehearsaling())
			b.startedDate = std::make_optional<std::string>(FORMAT_STARTED_TIME(_streamStartedTime).toStdString());
		else
			b.startedDate = std::nullopt;
		b.targets = targets;
		break;
	case rc::BroadcastType::Recording:
		b.id = "broadcast-recording";
		b.type = BroadcastType::Recording;
		if (pls_is_recording())
			b.startedDate = std::make_optional<std::string>(FORMAT_STARTED_TIME(_recordStartedTime).toStdString());
		else
			b.startedDate = std::nullopt;
		b.targets = targets;
		break;
	default:
		break;
	}
	return b;
}

void EventHandler::broadcast_broadcaster_state_changed(BroadcastType type, rc::BroadcasterState state) const
{
	if (!_server)
		return;

	UNUSED_PARAMETER(type);

	QJsonObject json;
	QJsonObject param;
	QString outState = getTextForType((int)state, BroadcasterStateStrs);
	auto server = _server;

	json["commandType"] = "changedBroadcasterState";
	param["broadcastType"] = getTextForType((int)type, BroadcastTypeStrings);
	param["broadcasterState"] = outState;
	json["command"] = param;

	pls_async_call(server, [server, json]() { server->Broadcast(json); });
}
