#pragma once

#include <vector>

#include "ApiKey.h"
#include "SocketSession.h"
#include "RemoteControlDefine.h"

class SocketServer;
class QDateTime;

class EventHandler {
public:
	static EventHandler *getInstance();
	EventHandler();
	~EventHandler() noexcept { UnLoad(); };

	EventHandler(EventHandler const &) = delete;
	void operator=(EventHandler const &) = delete;

	void Load();
	void UnLoad();
	void SetServer(SocketServer *s);
	inline bool IsLoaded() const { return m_isLoaded; }
	bool Receive(const DataPackage &pkg, SocketSession *s) const;

private:
	// MARK: -- remote control
	void StartBroadcast(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const;
	void StopBroadcast(const CommandHeader &requestHeader, SocketSession *session, const QJsonObject &commandJson) const;

	void SetCurrentScene(const QJsonObject &apiIdJson, const CommandHeader &requestHeader, SocketSession *session) const;
	void SetGroupItemVisible(const QJsonObject &apiIdJson, bool visible, const CommandHeader &requestHeader, SocketSession *session) const;
	void SetSceneItemVisible(const QJsonObject &apiIdJson, bool visible, const CommandHeader &requestHeader, SocketSession *session) const;

	void SetAllSourceMute(bool isOn, const CommandHeader &requestHeader, SocketSession *session) const;

	void SetSourceMute(const QJsonObject &apiIdJson, bool isOn, const CommandHeader &requestHeader, SocketSession *session) const;
	void SetSourceVolume(const QJsonObject &apiIdJson, bool volumeUp, const CommandHeader &requestHeader, SocketSession *session) const;
	void SetRNNoiseActive(const QJsonObject &apiIdJson, bool active, const CommandHeader &requestHeader, SocketSession *session) const;

	void SetStudioModeActive(bool active, const CommandHeader &requestHeader, SocketSession *session) const;
	void ApplyDraftToLive(const CommandHeader &requestHeader, SocketSession *session) const;
	void SetAlertMessageVisible(bool visible, const CommandHeader &requestHeader, SocketSession *session) const;

	void GetAllAction(const CommandHeader &requestHeader, SocketSession *session) const;
	void GetActionsOfScene(const SceneKey &key, const CommandHeader &requestHeader, SocketSession *session) const;
	void GetActionsOfGroupItem(const GroupItemKey &key, const CommandHeader &requestHeader, SocketSession *session) const;

	void GetStreamingDuration(const CommandHeader &requestHeader, SocketSession *session, rc::BroadcastType) const;
	void GetBroadcasterNetworkStatus(const CommandHeader &requestHeader, SocketSession *session) const;
	void GetBroadcasterState(const CommandHeader &requestHeader, SocketSession *session, rc::BroadcastType) const;
	void GetSupportedBroadcastTypeList(const CommandHeader &requestHeader, SocketSession *session) const;
	void GetCurrentBroadcast(const CommandHeader &requestHeader, SocketSession *session, rc::BroadcastType) const;
	void GetCurrentChatInfo(const CommandHeader &requestHeader, SocketSession *session) const;
	void GetDeviceInfo(const CommandHeader &requestHeader, SocketSession *session) const;
	bool Connect(rc::ConnectType cnn, int version, const CommandHeader &requestHeader, SocketSession *session) const;

	void Subscribe(const QJsonArray &sourceIds, const CommandHeader &requestHeader, SocketSession *session) const;
	void UnSubscribe(const QJsonArray &sourceIds, const CommandHeader &requestHeader, SocketSession *session) const;

	// MARK: -- obs, pls callbacks
	static void obs_frontend_event_received(enum obs_frontend_event event, void *context);
	static void pls_frontend_event_received(pls_frontend_event event, const QVariantList &, void *context);

	rc::RCError common_check(rc::RCError &e, const SocketSession *session, std::string &errorInfo) const;
	void sendSuccess(const CommandHeader &requestHeader, SocketSession *s, const QJsonObject &successCmdJson, const QString &cmd = "success") const;
	void sendFailure(const CommandHeader &requestHeader, SocketSession *s, rc::RCError e, const std::string &errorInfo) const;

	rc::Broadcast get_current_broadcast(rc::BroadcastType type) const;
	void broadcast_broadcaster_state_changed(rc::BroadcastType type, rc::BroadcasterState state) const;

	void _handleDynamicCommand(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const;
	bool _handleConnectCommand(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const;
	void _handleActionListCommand(const QJsonObject &commandJson, const CommandHeader &requestHeader, SocketSession *session) const;

	void _handleStreamingState(const QString &state);
	void _handleRecordingState(const QString &state);
	static EventHandler *_instance;

	bool m_isLoaded{false};

	QDateTime _streamStartedTime;
	QDateTime _streamEndTime;
	QDateTime _recordStartedTime;
	QDateTime _recordEndTime;

	SocketServer *_server{nullptr};
};
