#pragma once

#include <string>
#include <mutex>
#include <set>

#include <QTcpSocket>
#include <qtimer.h>

#include <obs-frontend-api.h>

#include "frontend-api.h"

#include "ApiKey.h"

class SocketSession;

using SessionPtr = std::shared_ptr<SocketSession>;

enum class ProtocolType { Json = 1, Binary = 2 };
enum class RoleType { Server = 0, Client = 1 };
struct CommandHeader {
	uint32_t length = 0;
	uint32_t version = 1;
	uint32_t payloadType = (uint32_t)ProtocolType::Json;
	uint32_t identifier = 0; // when request, server uses 0, client uses 1.
	uint32_t tag = 1;
};
#define CommandHeaderLength 20

struct DataPackage {
	~DataPackage() noexcept {}
	CommandHeader header;
	QByteArray body;
};

QString getTextOfDataPackage(const DataPackage &pkg);

class SocketSession : public QObject, public std::enable_shared_from_this<SocketSession> {
	Q_OBJECT
Q_SIGNALS:
	void peerDeviceInfoSignal(QJsonObject &);

public:
	SocketSession(QTcpSocket *socket, const std::string &sessionKey, QObject *parent = nullptr);
	~SocketSession() override;

	std::weak_ptr<SocketSession> getWeakPointer() { return shared_from_this(); }

	void SetConnected();
	bool acceptedConnect() const { return m_acceptsConn; }

	std::string SessionKey() const { return m_sessionKey; }

	void SetPeerDeviceInfo(const QJsonObject &json);

	bool SubscribeKey(const QString &key);
	bool UnsubscribeKey(const QString &key);
	bool RemoveSubscribeKey(const QString &key);

	void SendReply(const CommandHeader &header, const QByteArray &body);
	void SendRequest(const QByteArray &body);

	QString GetName() const;
	rc::DisconnectReason GetDisconnectReason() const { return _disconnectReason; }
	void SetDisconnectReason(rc::DisconnectReason r);
	bool GetConnectedBefore() const { return _connectedBefore; }

	void OnDisconnect();
	void Disconnect();

private:
	// heart beat
	void startHeartBeat();
	void stopHeartBeat();

	void sendUpdated(const rc::Source &s);
	void sendDeleted(const QString &id);
	bool doSubscribe(const QString &strKey, rc::DynamicActionType a, const QJsonObject &json);
	bool doUnsubscribe(const QString &strKey, rc::DynamicActionType a, const QJsonObject &json);
	bool find(const NoSourceKey &key) const;
	void currentSceneDidChange();
	void currentCollectionDidChange();
	void studioModeDidChange(bool enabled);
	void muteAllDidChange(bool isOn);
	void alertWindowDidChange(bool isOn, int count = -1);
	void rnnoiseDidChange(long long sourceAddr);
	static void obs_frontend_event_received(enum obs_frontend_event event, void *context);
	static void pls_frontend_event_received(pls_frontend_event event, const QVariantList &, void *context);

	static void obs_source_deleted(void *data, calldata_t *calldata);
	static void obs_source_added(void *data, calldata_t *calldata);
	static void obs_source_renamed(void *data, calldata_t *calldata);
	static void obs_source_active_changed(void *data, calldata_t *calldata);
	void obsSourceActiveDidChange(obs_source_t *source);
	void obsSourceDidRename(obs_source_t *source, const QString &prevName);
	rc::Action getActionByJson(const QJsonObject &json, rc::DynamicActionType a) const;

	// audio
	static void audioMute(void *data, calldata_t *cd);

	// MARK: -- items
	static void itemVisible(void *data, calldata_t *cd);
	enum class ItemOption { add, remove, other };
	static void itemAdd(void *data, calldata_t *cd);
	static void itemRemove(void *data, calldata_t *cd);
	static void reorder(void *data, calldata_t *cd);
	static void _refreshByItemChange(SocketSession *session, obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option);
	static void _handleItemTopic(SocketSession *session, const QJsonObject &json, const obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option);
	static void _handelGroupTopic(SocketSession *session, const QJsonObject &json, const obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option);
	static void _handleSceneTopic(SocketSession *session, const QJsonObject &json, obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option);

	bool _subscribeSceneItem(const QJsonObject &json);
	bool _subscribeGroupItem(const QJsonObject &json);
	bool _subscribeScene(const QJsonObject &json);
	bool _subscribeMute(const QJsonObject &json);
	bool _subscribeOther(const QJsonObject &json) const;

	bool _unsubscribeSceneItem(const QJsonObject &json);
	bool _unsubscribeGroupItem(const QJsonObject &json);
	bool _unsubscribeScene(const QJsonObject &json);
	bool _unsubscribeMute(const QJsonObject &json);
	bool _unsubscribeOther(const QJsonObject &json) const;

private Q_SLOTS:

	void onReadReady();

private:
	bool m_acceptsConn{false};
	QTcpSocket *m_pSocket;
	const std::string m_sessionKey;
	/// <summary>
	/// key: id, name, os, osVersion
	/// </summary>
	QJsonObject _peerDeviceInfo;
	rc::DisconnectReason _disconnectReason{rc::DisconnectReason::none};

	CommandHeader _header{};
	bool _readHeader{false};

	mutable std::recursive_mutex _mutex;
	std::map<QString, int> _subscription;
	static volatile long gRequestID;

	enum class AllMuteSet { none = -1, notAllMute = 0, allMute = 1 };
	AllMuteSet _isAllMute{AllMuteSet::none};

	QTimer *_heartBeatTimer{nullptr};
	int _heartBeatTimeOutMiliseconds{10000}; // 10 seconds

	// for statistics
	bool _isWriteClientInfo{false};
	bool _connectedBefore{false};
};
