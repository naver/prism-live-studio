#include "SocketSession.h"

#include <QDataStream>

#include <obs-frontend-api.h>
#include <util/threading.h>

#include "RemoteControlDefine.h"
#include "EventHandler.h"
#include "ObsUtils.h"
#include "LogHelper.h"

using namespace rc;

volatile long SocketSession::gRequestID = 0;

QString getTextOfDataPackage(const DataPackage &pkg)
{
	auto text = QString("length:%1, version:%2, payloadType:%3, identifier: %4, tag: %5, body:%6")
			    .arg(QString::number(pkg.header.length), QString::number(pkg.header.version), QString::number(pkg.header.payloadType), QString::number(pkg.header.identifier),
				 QString::number(pkg.header.tag), QString(pkg.body));
	return text.left(1000);
}

SocketSession::SocketSession(QTcpSocket *socket, const std::string &sessionKey, QObject *parent) : QObject(parent), m_pSocket(socket), m_sessionKey(sessionKey)
{
	PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-SHORTCUT", "SocketSessionInit"}}, "SocketSession init. Socket name: %s", sessionKey.c_str());

	// socket
	connect(m_pSocket, SIGNAL(readyRead()), this, SLOT(onReadReady()));

	// frontend event
	obs_frontend_add_event_callback(SocketSession::obs_frontend_event_received, this);
	pls_frontend_add_event_callback(SocketSession::pls_frontend_event_received, this);

	// source event
	signal_handler_connect(obs_get_signal_handler(), "source_create", obs_source_added, (void *)this);
	signal_handler_connect(obs_get_signal_handler(), "source_remove", obs_source_deleted, (void *)this);
	signal_handler_connect(obs_get_signal_handler(), "source_destroy", obs_source_deleted, (void *)this);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", obs_source_renamed, (void *)this);

	signal_handler_connect(obs_get_signal_handler(), "source_activate", obs_source_active_changed, this);
	signal_handler_connect(obs_get_signal_handler(), "source_deactivate", obs_source_active_changed, this);
	signal_handler_connect(obs_get_signal_handler(), "source_audio_activate", obs_source_active_changed, this);
	signal_handler_connect(obs_get_signal_handler(), "source_audio_deactivate", obs_source_active_changed, this);
}

SocketSession::~SocketSession()
{
	RC_LOG_INFO("SocketSession deinit. Socket name: %s", m_sessionKey.c_str());
	obs_frontend_remove_event_callback(SocketSession::obs_frontend_event_received, this);
	pls_frontend_remove_event_callback(SocketSession::pls_frontend_event_received, this);

	signal_handler_disconnect(obs_get_signal_handler(), "source_create", obs_source_added, (void *)this);
	signal_handler_disconnect(obs_get_signal_handler(), "source_remove", obs_source_deleted, (void *)this);
	signal_handler_disconnect(obs_get_signal_handler(), "source_destroy", obs_source_deleted, (void *)this);
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename", obs_source_renamed, (void *)this);

	signal_handler_disconnect(obs_get_signal_handler(), "source_activate", obs_source_active_changed, this);
	signal_handler_disconnect(obs_get_signal_handler(), "source_deactivate", obs_source_active_changed, this);
	signal_handler_disconnect(obs_get_signal_handler(), "source_audio_activate", obs_source_active_changed, this);
	signal_handler_disconnect(obs_get_signal_handler(), "source_audio_deactivate", obs_source_active_changed, this);
}

void SocketSession::SetConnected()
{
	auto name = GetName();
	m_acceptsConn = true;
	_connectedBefore = true;

	pls_async_call_mt([name]() { pls_set_remote_control_client_info(name, true); });

	startHeartBeat();
	RC_LOG_INFO("Session statistics: session start");
}

void SocketSession::SetPeerDeviceInfo(const QJsonObject &json)
{
	_peerDeviceInfo = json;
	if (!m_acceptsConn)
		return;
	auto name = GetName();

	if (!_isWriteClientInfo) {
		_isWriteClientInfo = true;

		auto os = _peerDeviceInfo["os"].toString().toStdString();
		auto osVersion = os + _peerDeviceInfo["osVersion"].toString().toStdString();
		PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-OS", os.c_str()}}, "Client OS Information: Client os: %s", os.c_str());
		PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-OSVERSION", osVersion.c_str()}}, "Client OS Information: Client os version: %s", osVersion.c_str());
	}

	pls_async_call_mt([name]() { pls_set_remote_control_client_info(name, true); });
}

void SocketSession::Disconnect()
{
	stopHeartBeat();

	if (m_pSocket->state() != QTcpSocket::ConnectedState) {
		return;
	}

	m_acceptsConn = false;
	disconnect(m_pSocket, SIGNAL(readyRead()), this, SLOT(onReadReady()));
	m_pSocket->disconnectFromHost();
}

void SocketSession::OnDisconnect()
{
	stopHeartBeat();

	m_acceptsConn = false;

	std::map<QString, int> subscriptions;
	std::unique_lock lock(_mutex);
	subscriptions = _subscription;
	_subscription.clear();
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
		auto a = parseDynamicAction(iter->first);
		auto json = parseKeyByString(iter->first);
		for (size_t i = 0; i < iter->second; i++) {
			doUnsubscribe(iter->first, a, json);
		}
	}
	RC_LOG_INFO("Session statistics: session end");
}

void SocketSession::onReadReady()
{
	DataPackage data{};
	QDataStream in(m_pSocket);
	in.setVersion(QDataStream::Qt_6_0);
	in.setByteOrder(QDataStream::LittleEndian);

	if (m_pSocket->bytesAvailable() < CommandHeaderLength)
		return;

	bool ret = true;
	while (ret && m_pSocket->bytesAvailable()) {
		if (m_pSocket->bytesAvailable() >= CommandHeaderLength && !_readHeader) { // read header
			in >> _header.length;
			in >> _header.version;
			in >> _header.payloadType;
			in >> _header.identifier;
			in >> _header.tag;
			_readHeader = _header.length > CommandHeaderLength;
		} else if (_readHeader && m_pSocket->bytesAvailable() >= (_header.length - CommandHeaderLength)) { // read body
			data.header = _header;
			_readHeader = false;
			auto len = static_cast<unsigned long>(data.header.length - CommandHeaderLength + 1);
			auto temp = pls_new_array<char>(len);
			memset(temp, 0, len);
			in.readRawData(temp, data.header.length - CommandHeaderLength);
			QByteArray buffer(temp, data.header.length - CommandHeaderLength);
			pls_delete_array(temp);
			data.body = buffer;

			LogHelper::printMessageIO(this, __FUNCTION__, _header.length, _header.tag, data.body.data());

			ret = EventHandler::getInstance()->Receive(data, this);
		} else {
			break;
		}
	}
}

bool SocketSession::SubscribeKey(const QString &key)
{
	auto a = parseDynamicAction(key);
	auto json = parseKeyByString(key);
	bool ret = true;
	if ((int)a < 0 || json.isEmpty())
		return false;

	std::unique_lock lock(_mutex);
	auto iter = _subscription.find(key);
	if (iter != _subscription.end())
		iter->second += 1;
	else if (doSubscribe(key, a, json))
		_subscription[key] = 1;
	else
		ret = false;
	lock.unlock();
	return ret;
}

bool SocketSession::doSubscribe(const QString &strKey, DynamicActionType a, const QJsonObject &json)
{
	UNUSED_PARAMETER(strKey);
	switch (a) {
	case DynamicActionType::sceneItem:
		return _subscribeSceneItem(json);

	case DynamicActionType::groupItem:
		return _subscribeGroupItem(json);

	case DynamicActionType::scene:
		return _subscribeScene(json);

	case DynamicActionType::mute:
		return _subscribeMute(json);

	case DynamicActionType::rnnoise:
	case DynamicActionType::muteAll:
	case DynamicActionType::alertVisible:
	case DynamicActionType::switchStudioMode:
	case DynamicActionType::volumeUp:
	case DynamicActionType::volumeDown:
		return _subscribeOther(json);

	default:
		return false;
	}
}

bool SocketSession::_subscribeSceneItem(const QJsonObject &json)
{
	auto key = SceneItemKey::parse(json);
	if (!key.isValid())
		return false;

	auto item = pls_get_sceneitem_by_pointer_address(key.item);
	auto parentscene = obs_sceneitem_get_scene(item);
	auto parentsource = obs_scene_get_source(parentscene);
	signal_handler_t *parenthandler = obs_source_get_signal_handler(parentsource);
	if (!parenthandler)
		return false;

	signal_handler_connect_ref(parenthandler, "item_visible", itemVisible, this);
	signal_handler_connect_ref(parenthandler, "item_remove", itemRemove, this);
	return true;
}
bool SocketSession::_subscribeGroupItem(const QJsonObject &json)
{
	auto key = GroupItemKey::parse(json);
	if (!key.isValid())
		return false;

	auto item = pls_get_sceneitem_by_pointer_address(key.item);
	auto groupscene = obs_sceneitem_group_get_scene(item);
	auto groupsource = obs_scene_get_source(groupscene);
	signal_handler_t *grouphandler = obs_source_get_signal_handler(groupsource);
	auto parentscene = obs_sceneitem_get_scene(item);
	auto parentsource = obs_scene_get_source(parentscene);
	auto parenthandler = obs_source_get_signal_handler(parentsource);
	if (!grouphandler || !parenthandler)
		return false;

	// observe change of group item itself
	signal_handler_connect_ref(parenthandler, "item_visible", itemVisible, this);
	signal_handler_connect_ref(parenthandler, "item_remove", itemRemove, this);
	// observe change of group item's children
	signal_handler_connect_ref(grouphandler, "item_add", itemAdd, this);
	signal_handler_connect_ref(grouphandler, "item_remove", itemRemove, this);
	signal_handler_connect_ref(grouphandler, "reorder", reorder, this);
	return true;
}
bool SocketSession::_subscribeScene(const QJsonObject &json)
{
	auto key = SceneKey::parse(json);
	auto scene = key.getScene();
	auto source = obs_scene_get_source(scene);
	signal_handler_t *handler = obs_source_get_signal_handler(source);
	if (!handler)
		return false;

	signal_handler_connect_ref(handler, "item_add", itemAdd, this);
	signal_handler_connect_ref(handler, "item_remove", itemRemove, this);
	signal_handler_connect_ref(handler, "reorder", reorder, this);
	return true;
}
bool SocketSession::_subscribeMute(const QJsonObject &json)
{
	auto key = SourceKey::parse(json);
	if (!key.isValid())
		return false;
	auto source = key.getOBSSource();
	auto handler = obs_source_get_signal_handler(source);
	if (!handler)
		return false;

	signal_handler_connect_ref(handler, "mute", SocketSession::audioMute, this);
	return true;
}
bool SocketSession::_subscribeOther(const QJsonObject &json) const
{
	auto key = NoSourceKey::parse(json);
	return key.isValid();
}

bool SocketSession::doUnsubscribe(const QString &strKey, DynamicActionType a, const QJsonObject &json)
{
	UNUSED_PARAMETER(strKey);
	switch (a) {
	case DynamicActionType::sceneItem:
		return _unsubscribeSceneItem(json);

	case DynamicActionType::groupItem:
		return _unsubscribeGroupItem(json);

	case DynamicActionType::scene:
		return _unsubscribeScene(json);

	case DynamicActionType::mute:
		return _unsubscribeMute(json);

	case DynamicActionType::rnnoise:
	case DynamicActionType::muteAll:
	case DynamicActionType::alertVisible:
	case DynamicActionType::switchStudioMode:
	case DynamicActionType::volumeUp:
	case DynamicActionType::volumeDown:
		return _unsubscribeOther(json);

	default:
		return false;
	}
}

bool SocketSession::_unsubscribeSceneItem(const QJsonObject &json)
{
	auto key = SceneItemKey::parse(json);
	if (!key.isValid())
		return false;

	auto item = pls_get_sceneitem_by_pointer_address(key.item);
	auto parentscene = obs_sceneitem_get_scene(item);
	auto parentsource = obs_scene_get_source(parentscene);
	signal_handler_t *parenthandler = obs_source_get_signal_handler(parentsource);
	if (!parenthandler)
		return false;

	signal_handler_disconnect(parenthandler, "item_visible", itemVisible, this);
	signal_handler_disconnect(parenthandler, "item_remove", itemRemove, this);

	return true;
}
bool SocketSession::_unsubscribeGroupItem(const QJsonObject &json)
{
	auto key = GroupItemKey::parse(json);
	if (!key.isValid())
		return false;

	auto item = pls_get_sceneitem_by_pointer_address(key.item);
	auto groupscene = obs_sceneitem_group_get_scene(item);
	auto groupsource = obs_scene_get_source(groupscene);
	signal_handler_t *grouphandler = obs_source_get_signal_handler(groupsource);
	auto parentscene = obs_sceneitem_get_scene(item);
	auto parentsource = obs_scene_get_source(parentscene);
	auto parenthandler = obs_source_get_signal_handler(parentsource);
	if (!grouphandler || !parenthandler)
		return false;

	// observe change of group item itself
	signal_handler_disconnect(parenthandler, "item_visible", itemVisible, this);
	signal_handler_disconnect(parenthandler, "item_remove", itemRemove, this);
	// observe change of group item's children
	signal_handler_disconnect(grouphandler, "item_add", itemAdd, this);
	signal_handler_disconnect(grouphandler, "item_remove", itemRemove, this);
	signal_handler_disconnect(grouphandler, "reorder", reorder, this);
	return true;
}
bool SocketSession::_unsubscribeScene(const QJsonObject &json)
{
	auto key = SceneKey::parse(json);
	auto scene = key.getScene();
	auto source = obs_scene_get_source(scene);
	signal_handler_t *handler = obs_source_get_signal_handler(source);
	if (!handler)
		return false;

	signal_handler_disconnect(handler, "item_add", itemAdd, this);
	signal_handler_disconnect(handler, "item_remove", itemRemove, this);
	signal_handler_disconnect(handler, "reorder", reorder, this);
	return true;
}
bool SocketSession::_unsubscribeMute(const QJsonObject &json)
{
	auto key = SourceKey::parse(json);
	if (!key.isValid())
		return false;
	auto source = key.getOBSSource();
	auto handler = obs_source_get_signal_handler(source);
	if (!handler)
		return false;

	signal_handler_disconnect(handler, "mute", SocketSession::audioMute, this);
	return true;
}
bool SocketSession::_unsubscribeOther(const QJsonObject &json) const
{
	UNUSED_PARAMETER(json);
	return true;
}

bool SocketSession::find(const NoSourceKey &key) const
{
	bool isSubscribed = false;
	QString strKey = key.toKey();
	std::unique_lock lock(_mutex);
	isSubscribed = _subscription.find(strKey) != _subscription.end();
	lock.unlock();
	return isSubscribed;
}

void SocketSession::startHeartBeat()
{
	stopHeartBeat();
	RC_LOG_INFO("Start heart beat");
	_heartBeatTimer = pls_new<QTimer>();

	connect(_heartBeatTimer, &QTimer::timeout, this, [this]() {
		pls_async_call_mt(this, [this]() {
			QJsonObject json;
			json["commandType"] = CommandTypeStrings[(int)CommandType::GetDeviceInfo];
			json["command"] = QJsonObject();
			SendRequest(QJsonDocument(json).toJson(QJsonDocument::Compact));
		});
	});
	_heartBeatTimer->setSingleShot(false);
	_heartBeatTimer->start(_heartBeatTimeOutMiliseconds);
}

void SocketSession::stopHeartBeat()
{
	RC_LOG_INFO("Stop heart beat");

	if (!_heartBeatTimer)
		return;

	_heartBeatTimer->stop();
	pls_delete(_heartBeatTimer, nullptr);
}

void SocketSession::currentSceneDidChange()
{
	std::map<QString, int> subscriptions;
	std::unique_lock lock(_mutex);
	subscriptions = _subscription;
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); ++iter) {
		Action action{};
		if (parseDynamicAction(iter->first) == DynamicActionType::scene) {
			auto key = SceneKey::parse(parseKeyByString(iter->first));
			auto scene = key.getScene();
			action = get_scene_action(scene);
		}
		if (action.source.has_value())
			sendUpdated(action.source.value());
	}
}

void SocketSession::currentCollectionDidChange()
{
	std::map<QString, int> subscriptions;
	std::unique_lock lock(_mutex);
	subscriptions = _subscription;
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); ++iter) {
		switch (parseDynamicAction(iter->first)) {
		case rc::DynamicActionType::scene:
		case rc::DynamicActionType::mute:
		case rc::DynamicActionType::rnnoise:
		case rc::DynamicActionType::volumeUp:
		case rc::DynamicActionType::volumeDown:
			sendDeleted(iter->first);
			RemoveSubscribeKey(iter->first);
			break;
		default:
			break;
		}
	}
}

void SocketSession::studioModeDidChange(bool enabled)
{
	if (!find({DynamicActionType::switchStudioMode}))
		return;

	QJsonObject json;
	QJsonObject param;
	auto a = get_studio_mode_switch_action();
	if (a.source.has_value()) {
		a.source.value().isOn = enabled;
		sendUpdated(a.source.value());
	}
}

void SocketSession::muteAllDidChange(bool isOn)
{
	auto set = isOn ? AllMuteSet::notAllMute : AllMuteSet::allMute;

	if (_isAllMute == set)
		return;

	_isAllMute = set;
	if (!find({DynamicActionType::muteAll}))
		return;

	auto a = get_mute_all_action(!isOn);
	if (a.source.has_value()) {
		sendUpdated(a.source.value());
	}
}

void SocketSession::alertWindowDidChange(bool isOn, int count)
{
	if (!find({DynamicActionType::alertVisible}))
		return;

	auto a = get_show_alert_action();
	if (count >= 0 && a.source.has_value())
		a.source.value().numberOfChildren = count;

	if (a.source.has_value()) {
		a.source.value().isOn = isOn;
		sendUpdated(a.source.value());
	}
}

void SocketSession::rnnoiseDidChange(long long sourceAddr)
{
	std::unique_lock lock(_mutex);
	auto subscriptions = _subscription;
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
		Action action{};
		if (parseDynamicAction(iter->first) == DynamicActionType::rnnoise) {
			auto key = SourceKey::parse(parseKeyByString(iter->first));
			auto source = key.getOBSSource();
			if ((long long)source.Get() == sourceAddr)
				action = get_rnnnoise_action(source);
		}

		if (action.source.has_value())
			sendUpdated(action.source.value());
	}
}

void SocketSession::obs_frontend_event_received(obs_frontend_event event, void *context)
{
	auto session = (SocketSession *)context;
	if (!session)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
		session->studioModeDidChange(true);
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		session->studioModeDidChange(false);
		break;
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		session->currentSceneDidChange();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		session->currentCollectionDidChange();
		break;
	default:
		break;
	}
}

void SocketSession::pls_frontend_event_received(pls_frontend_event event, const QVariantList &params, void *context)
{
	auto session = (SocketSession *)context;
	bool ok = false;

	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_ALL_MUTE:
		if (params.size() > 0)
			session->muteAllDidChange(!params.at(0).toBool());
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED:
		if (params.size() > 1 && params.at(0).toInt(&ok) == 4 && ok)
			session->alertWindowDidChange(params.at(1).toBool());
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_REMOTE_CONTROL_RNNOISE_CHANGED:
		if (params.size() >= 1 && params.at(0).toLongLong(&ok) && ok)
			session->rnnoiseDidChange(params.at(0).toLongLong());
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE:
		if (params.size() > 0) {
			int messageCount = params.at(0).toInt();
			session->alertWindowDidChange(pls_alert_message_visible(), messageCount);
		}
		break;
	default:
		break;
	}
}

void SocketSession::obs_source_added(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(calldata);
}

void SocketSession::obs_source_deleted(void *data, calldata_t *calldata)
{
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	auto session = (SocketSession *)data;
	if (!source || !session)
		return;

	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
		SceneKey key{};
		key.sceneName = obs_source_get_name(source);
		pls_async_call(session, [key, session]() { session->sendDeleted(key.toKey()); });
	}
}

void SocketSession::obs_source_renamed(void *data, calldata_t *calldata)
{
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	auto prevNamePtr = calldata_string(calldata, "prev_name");
	QString prevName = prevNamePtr ? QString(prevNamePtr) : "";
	auto session = (SocketSession *)data;
	if (!source || !session)
		return;

	pls_async_call(session, [source, prevName, session]() { session->obsSourceDidRename(source, prevName); });
}

void SocketSession::obs_source_active_changed(void *data, calldata_t *calldata)
{
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	auto session = (SocketSession *)data;
	uint32_t flags = obs_source_get_output_flags(source);
	if ((flags & OBS_SOURCE_AUDIO) && source && session) {
		pls_async_call(session, [source, session]() { session->obsSourceActiveDidChange(source); });
	}
}

Action SocketSession::getActionByJson(const QJsonObject &json, DynamicActionType a) const
{
	switch (a) {
	case DynamicActionType::sceneItem:
		return get_scene_item_action(pls_get_sceneitem_by_pointer_address(SceneItemKey::parse(json).getItem()));

	case DynamicActionType::groupItem:
		return get_group_item_action(pls_get_sceneitem_by_pointer_address(GroupItemKey::parse(json).getItem()));

	case DynamicActionType::scene:
		return get_scene_action(SceneKey::parse(json).getScene());

	case DynamicActionType::mute:
		return get_mute_action(SourceKey::parse(json).getOBSSource());

	case DynamicActionType::rnnoise:
		return get_rnnnoise_action(SourceKey::parse(json).getOBSSource());

	case DynamicActionType::volumeUp:
		return get_volume_up_action(SourceKey::parse(json).getOBSSource());

	case DynamicActionType::volumeDown:
		return get_volume_down_action(SourceKey::parse(json).getOBSSource());

	default:
		return Action();
	}
}

void SocketSession::obsSourceActiveDidChange(obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	std::map<QString, int> subscriptions;
	std::unique_lock lock(_mutex);
	subscriptions = _subscription;
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
		if (parseDynamicAction(iter->first) != DynamicActionType::scene) {
			continue;
		}
		auto subscribeSceneKey = SceneKey::parse(parseKeyByString(iter->first));
		auto action = get_scene_action(subscribeSceneKey.getScene());

		if (action.source.has_value())
			sendUpdated(action.source.value());
	}
}

void SocketSession::obsSourceDidRename(obs_source_t *source, const QString &prevName)
{
	UNUSED_PARAMETER(source);
	std::map<QString, int> subscriptions;
	std::unique_lock lock(_mutex);
	subscriptions = _subscription;
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
		auto a = parseDynamicAction(iter->first);
		auto json = parseKeyByString(iter->first);

		if (a == DynamicActionType::scene) {
			auto subscribeSceneKey = SceneKey::parse(json);
			if (subscribeSceneKey.sceneName == prevName && obs_scene_from_source(source)) {
				sendDeleted(iter->first);
				RemoveSubscribeKey(iter->first);
			}
			continue;
		}

		auto key = SourceKey::parse(json);
		if (!key.isValid() || key.getOBSSource().Get() != source)
			continue;

		Action action = getActionByJson(json, a);
		if (action.source.has_value())
			sendUpdated(action.source.value());
	}
}

void SocketSession::audioMute(void *data, calldata_t *cd)
{
	auto session = (SocketSession *)data;
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	bool muted = calldata_ptr(cd, "muted");
	UNUSED_PARAMETER(muted);
	if (!source || !session)
		return;

	auto action = get_mute_action(source);

	pls_async_call(session, [session, action, source]() {
		std::map<QString, int> subscriptions;
		std::unique_lock lock(session->_mutex);
		subscriptions = session->_subscription;
		lock.unlock();

		for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
			auto a = parseDynamicAction(iter->first);
			if (a != DynamicActionType::mute)
				continue;
			auto key = SourceKey::parse(parseKeyByString(iter->first));
			if (!key.isValid())
				continue;
			if (source != key.getOBSSource().Get())
				continue;

			if (action.source.has_value())
				session->sendUpdated(action.source.value());
		}
	});
}

void SocketSession::itemVisible(void *data, calldata_t *cd)
{
	auto session = (SocketSession *)data;
	auto parentscene = (obs_scene_t *)calldata_ptr(cd, "scene");
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (!parentscene || !item || !session)
		return;

	pls_async_call(session, [session, item]() {
		std::map<QString, int> subscriptions;
		std::unique_lock lock(session->_mutex);
		subscriptions = session->_subscription;
		lock.unlock();
		for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
			auto a = parseDynamicAction(iter->first);
			auto json = parseKeyByString(iter->first);
			Action action{};
			if (a == DynamicActionType::sceneItem && SceneItemKey::parse(json).item == item) {
				action = get_scene_item_action(item);
			} else if (a == DynamicActionType::groupItem && GroupItemKey::parse(json).item == item) {
				action = get_group_item_action(item);
			}
			if (action.source.has_value())
				session->sendUpdated(action.source.value());
		}
	});
}
void SocketSession::itemAdd(void *data, calldata_t *cd)
{
	auto session = (SocketSession *)data;
	auto scene = (obs_scene_t *)calldata_ptr(cd, "scene");
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	pls_async_call(session, [session, scene, item]() { SocketSession::_refreshByItemChange(session, scene, item, ItemOption::add); });
}
void SocketSession::itemRemove(void *data, calldata_t *cd)
{
	auto session = (SocketSession *)data;
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	auto scene = (obs_scene_t *)calldata_ptr(cd, "scene");
	pls_async_call(session, [session, scene, item]() { SocketSession::_refreshByItemChange(session, scene, item, ItemOption::remove); });
}
void SocketSession::reorder(void *data, calldata_t *cd)
{
	auto session = (SocketSession *)data;
	auto scene = (obs_scene_t *)calldata_ptr(cd, "scene");
	pls_async_call(session, [session, scene]() { SocketSession::_refreshByItemChange(session, scene, nullptr, ItemOption::other); });
}
void SocketSession::_handleItemTopic(SocketSession *session, const QJsonObject &json, const obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option)
{
	UNUSED_PARAMETER(scene);
	if (option != ItemOption::remove)
		return;
	auto key = SceneItemKey::parse(json);
	if (key.item != item)
		return;
	session->sendDeleted(key.toKey());
}

void SocketSession::_handelGroupTopic(SocketSession *session, const QJsonObject &json, const obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option)
{
	auto key = GroupItemKey::parse(json);
	if (key.item == item && option == ItemOption::remove) { // group item removed
		session->sendDeleted(key.toKey());
	} else if (key.group_scene == scene) { // children changed
		auto groupItem = pls_get_sceneitem_by_pointer_address(key.item);
		auto action = get_group_item_action(groupItem);
		if (!action.source.has_value())
			return;
		session->sendUpdated(action.source.value());
	}
}
void SocketSession::_handleSceneTopic(SocketSession *session, const QJsonObject &json, obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option)
{
	UNUSED_PARAMETER(item);
	UNUSED_PARAMETER(option);
	auto key = SceneKey::parse(json);
	Action action{};

	if (key.getScene().Get() == scene)
		action = get_scene_action(scene);
	if (!action.source.has_value())
		return;

	session->sendUpdated(action.source.value());
}

void SocketSession::_refreshByItemChange(SocketSession *session, obs_scene_t *scene, const obs_sceneitem_t *item, ItemOption option)
{
	std::map<QString, int> subscriptions;

	if (!scene || !session)
		return;

	std::unique_lock lock(session->_mutex);
	subscriptions = session->_subscription;
	lock.unlock();

	for (auto iter = subscriptions.begin(); iter != subscriptions.end(); iter++) {
		auto json = parseKeyByString(iter->first);

		switch (parseDynamicAction(iter->first)) {
		case DynamicActionType::sceneItem:
			_handleItemTopic(session, json, scene, item, option);
			break;

		case DynamicActionType::groupItem:
			_handelGroupTopic(session, json, scene, item, option);
			break;

		case DynamicActionType::scene:
			_handleSceneTopic(session, json, scene, item, option);
			break;

		default:
			break;
		}
	}
}

void SocketSession::sendUpdated(const Source &s)
{
	QJsonObject json;
	QJsonObject source;
	QJsonObject param;
	get_json_by_source(param, s);
	source["source"] = param;
	json["commandType"] = "updatedSource";
	json["command"] = source;

	pls_async_call(this, [this, json]() { SendRequest(QJsonDocument(json).toJson(QJsonDocument::Compact)); });
}

void SocketSession::sendDeleted(const QString &id)
{
	QJsonObject json;
	QJsonObject param;
	QJsonArray ids;
	ids.append(id);
	param["sourceIds"] = ids;
	json["commandType"] = "deletedSource";
	json["command"] = param;

	RC_LOG_INFO("Deleted source id is: %s", id.toStdString().c_str());
	pls_async_call(this, [this, json]() { SendRequest(QJsonDocument(json).toJson(QJsonDocument::Compact)); });
}

bool SocketSession::UnsubscribeKey(const QString &key)
{
	auto a = parseDynamicAction(key);
	auto json = parseKeyByString(key);
	bool ret = true;
	if ((int)a < 0 || json.isEmpty())
		return false;

	std::unique_lock lock(_mutex);
	auto iter = _subscription.find(key);
	if (iter == _subscription.end()) {
		ret = false;
	} else {
		iter->second -= 1;
		if (iter->second <= 0) {
			doUnsubscribe(key, a, json);
			ret = true;
			_subscription.erase(iter);
		}
	}
	lock.unlock();
	return ret;
}

bool SocketSession::RemoveSubscribeKey(const QString &key)
{
	auto a = parseDynamicAction(key);
	auto json = parseKeyByString(key);
	bool ret = true;
	if ((int)a < 0 || json.isEmpty())
		return false;

	std::unique_lock lock(_mutex);
	auto iter = _subscription.find(key);
	if (iter == _subscription.end()) {
		ret = false;
	} else {
		doUnsubscribe(key, a, json);
		ret = true;
		_subscription.erase(iter);
	}
	lock.unlock();
	return ret;
}

void SocketSession::SendReply(const CommandHeader &header, const QByteArray &body)
{
	QByteArray bytes;
	QDataStream stream(&bytes, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_6_0);
	stream.setByteOrder(QDataStream::LittleEndian);
	stream << (uint32_t)(body.length() + CommandHeaderLength);
	stream << header.version;
	stream << header.payloadType;
	stream << header.identifier;
	stream << header.tag;

	if (body.size() > 0) {
		stream.writeRawData(body, (int)body.size()); // use `writeRawData` instead of `<<` to avoid write length to stream
		LogHelper::printMessageIO(this, __FUNCTION__, header.length, header.tag, body.data());
	} else {
		LogHelper::printMessageIO(this, __FUNCTION__, header.length, header.tag, nullptr);
	}

	m_pSocket->write(bytes);
	m_pSocket->flush();
}

void SocketSession::SendRequest(const QByteArray &body)
{
	CommandHeader header{(uint32_t)body.length() + CommandHeaderLength, 1, (uint32_t)ProtocolType::Json, (uint32_t)RoleType::Server, (uint32_t)os_atomic_inc_long(&gRequestID)};

	QByteArray bytes;
	QDataStream stream(&bytes, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_6_0);
	stream.setByteOrder(QDataStream::LittleEndian);
	stream << header.length;
	stream << header.version;
	stream << header.payloadType;
	stream << header.identifier;
	stream << header.tag;

	if (body.size() > 0) {
		stream.writeRawData(body, (int)body.size()); // use `writeRawData` instead of `<<` to avoid write length to stream
		LogHelper::printMessageIO(this, __FUNCTION__, header.length, header.tag, body.data());
	} else {
		LogHelper::printMessageIO(this, __FUNCTION__, header.length, header.tag, nullptr);
	}

	m_pSocket->write(bytes);
	m_pSocket->flush();
}

QString SocketSession::GetName() const
{
	auto name = _peerDeviceInfo["name"].toString();
	return name;
}

void SocketSession::SetDisconnectReason(DisconnectReason r)
{
	auto textReason = getTextForType((int)r, DisconnectReasonStrs).toStdString();
	PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-DISCONNECT-REASON", textReason.c_str()}}, "Session statistics: session disconnect reason: %d", r);
	_disconnectReason = r;
}
