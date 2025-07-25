#include "PLSSceneitemMapManager.h"
#include "window-basic-main.hpp"
#include "libutils-api.h"
#include "PLSBasic.h"
#include "pls/pls-dual-output.h"
#include "liblog.h"
#include "pls/pls-source.h"

PLSSceneitemMapManager *PLSSceneitemMapManager::Instance()
{
	static PLSSceneitemMapManager mgr;
	return &mgr;
}

static bool enumItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	QVector<OBSSceneItem> &items = *static_cast<QVector<OBSSceneItem> *>(ptr);

	obs_source_t *src = obs_sceneitem_get_source(item);
	if (obs_source_removed(src)) {
		return true;
	}

	if (!pls_is_alive(item)) {
		return true;
	}
	items.insert(0, item);
	return true;
}

static inline OBSScene GetCurrentScene()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	return main->GetCurrentScene();
}

static void handlePrismEvent(pls_frontend_event event, const QVariantList &params, void *context)
{
	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_ON: {
		PLSSceneitemMapMgrInstance->setDualOutputOpened(true);
		if (!PLSBasic::instance()->getSourceLoaded()) {
			return;
		}
		OBSBasic::Get()->undo_s.clear();
		OBSBasic::Get()->SaveProjectDeferred();
		OBSBasic::Get()->resetSourcesDockPressedEvent();
		PLSSceneitemMapMgrInstance->switchToDualOutputModeForAllScenes();
		OBSBasic::Get()->resetAllGroupTransforms();
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_OFF:
		PLSSceneitemMapMgrInstance->clearConfig();
		break;
	default:
		break;
	}
}

PLSSceneitemMapManager::~PLSSceneitemMapManager()
{
	pls_frontend_remove_event_callback(handlePrismEvent, this);
}

static bool EnumItemForScene(obs_scene_t *scene, obs_sceneitem_t *item, void *ptr)
{
	pls_unused(scene);
	QMap<QString, OBSData> &items = *static_cast<QMap<QString, OBSData> *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		pls_sceneitem_group_enum_items_all(item, EnumItemForScene, ptr);
	}

	if (!pls_is_vertical_sceneitem(item)) {
		return true;
	}

	OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(item);
	auto uuid = obs_data_get_string(settings, SCENE_ITEM_MAP_UUID);
	if (!pls_is_empty(uuid)) {
		OBSData obj;
		bool existed = false;
		auto iter = items.find(uuid);
		if (iter != items.end()) {
			existed = true;
			obj = iter.value();
		} else {
			obj = obs_data_create();
		}

		if (obs_sceneitem_is_group(item)) {
			obs_data_set_bool(obj, "visible", obs_sceneitem_visible(item));
			PLSSceneitemMapMgrInstance->saveHotkey(obj.Get(), item);
		} else {
			pls_scene_save_item(obj, item);
			obs_data_set_int(obj, "id", 0);
			PLSSceneitemMapMgrInstance->saveHotkey(obj.Get(), item);
		}

		items.insert(uuid, obj.Get());

		if (!existed) {
			obs_data_release(obj);
		}
	}

	return true;
}

OBSDataArray PLSSceneitemMapManager::saveVerticalSceneitemInfo()
{
	QMap<QString, OBSData> tempVerticalSceneitemInfos;
	auto findSceneitemCb = [](void *ptr, obs_source_t *src) {
		obs_scene_t *scene = obs_scene_from_source(src);
		if (!scene) {
			return true;
		}
		pls_scene_enum_items_all(scene, EnumItemForScene, ptr);
		return true;
	};
	pls_enum_all_scenes(findSceneitemCb, &tempVerticalSceneitemInfos);

	OBSDataArray arrays = obs_data_array_create();
	auto saveFunc = [&arrays](QMap<QString, OBSData> &infos) {
		for (auto key : infos.keys()) {
			OBSDataAutoRelease obj = obs_data_create();
			obs_data_set_string(obj, "uuid", key.toStdString().c_str());
			obs_data_set_obj(obj, "info", infos.value(key.toStdString().c_str()));
			obs_data_array_push_back(arrays, obj);
		}
	};

	if (!tempVerticalSceneitemInfos.isEmpty()) {
		verticalSceneitemInfos = tempVerticalSceneitemInfos;
	}
	saveFunc(verticalSceneitemInfos);
	return arrays;
}

void PLSSceneitemMapManager::loadVerticalSceneitemInfo(obs_data_array_t *arrays)
{
	auto count = obs_data_array_count(arrays);
	for (auto i = 0; i < count; i++) {
		OBSDataAutoRelease item_data = obs_data_array_item(arrays, i);
		auto uuid = obs_data_get_string(item_data, "uuid");
		auto obj = obs_data_get_obj(item_data, "info");
		verticalSceneitemInfos.insert(uuid, obj);
	}
}

void PLSSceneitemMapManager::switchToDualOutputMode()
{
	if (!pls_is_dual_output_on()) {
		return;
	}

	OBSScene curScene = GetCurrentScene();
	if (pls_is_vertical_scene(curScene)) {
		return;
	}

	// using when ungroup
	QMap<QString, DisplayId> ungroupItems;
	switchToDualOutputMode(curScene, ungroupItems);
}

void PLSSceneitemMapManager::switchToDualOutputModeForAllScenes()
{
	if (!pls_is_dual_output_on()) {
		return;
	}
	auto findSceneCb = [](void *, obs_source_t *src) {
		if (src && obs_source_removed(src)) {
			return true;
		}
		obs_scene_t *scene = obs_scene_from_source(src);
		if (!scene) {
			return true;
		}
		QMap<QString, DisplayId> items;
		PLSSceneitemMapMgrInstance->switchToDualOutputMode(scene, items);
		return true;
	};
	obs_enum_scenes(findSceneCb, nullptr);
}

OBSDataArray PLSSceneitemMapManager::saveConfig()
{
	if (!pls_is_dual_output_on()) {
		return {};
	}

	OBSDataArray array = obs_data_array_create();
	for (auto key : sceneitemMap.keys()) {
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_string(data, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY, key.toStdString().c_str());
		SceneitemIdMap idMap = sceneitemMap.value(key);
		OBSDataArrayAutoRelease dataArray = obs_data_array_create();
		for (auto key : idMap.keys()) {
			OBSDataAutoRelease tmpData = obs_data_create();
			obs_data_set_string(tmpData, key.toStdString().c_str(), QString::number(idMap.value(key)).toStdString().c_str());
			obs_data_array_push_back(dataArray, tmpData);
		}
		obs_data_set_array(data, SCENE_ITEM_MAPS_KEY, dataArray);
		obs_data_array_push_back(array, data);
	}

	return array;
}

void PLSSceneitemMapManager::loadConfig(OBSData data)
{
	OBSDataArrayAutoRelease sceneitemMaps = obs_data_get_array(data, SCENE_ITEM_MAP_SAVE_KEY);
	auto count = obs_data_array_count(sceneitemMaps);
	if (0 == count) {
		return;
	}
	for (int i = 0; i < count; i++) {
		OBSDataAutoRelease dataObj = obs_data_array_item(sceneitemMaps, i);
		auto sceneName = obs_data_get_string(dataObj, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY);
		if (pls_is_empty(sceneName)) {
			continue;
		}

		OBSDataArrayAutoRelease maps = obs_data_get_array(dataObj, SCENE_ITEM_MAPS_KEY);
		for (int i = 0; i < obs_data_array_count(maps); i++) {
			OBSDataAutoRelease dataObj = obs_data_array_item(maps, i);
			obs_data_item_t *item = obs_data_first(dataObj);
			for (; item != nullptr; obs_data_item_next(&item)) {
				QString name = obs_data_item_get_name(item);
				if (pls_is_empty(name)) {
					continue;
				}
				const char *value = obs_data_item_get_string(item);
				if (pls_is_empty(value)) {
					continue;
				}
				auto data = getIdAndUuid(name);
				addConfig(sceneName, data.second.toStdString().c_str(), data.first, atoi(value));
			}
		}
	}
}

static bool renameFunc(obs_scene_t * /* scene */, obs_sceneitem_t *item, void *param)
{
	if (!pls_is_vertical_sceneitem(item)) {
		return true;
	}
	std::pair<const char *, const char *> &name = *reinterpret_cast<std::pair<const char *, const char *> *>(param);
	OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(item);
	auto saveName = obs_data_get_string(settings, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY);
	if (pls_is_equal(saveName, name.first)) {
		obs_data_set_string(settings, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY, name.second);
	}
	auto saveGroupName = obs_data_get_string(settings, SCENE_ITEM_REFERENCE_GROUP_NAME);
	if (pls_is_equal(saveGroupName, name.first)) {
		obs_data_set_string(settings, SCENE_ITEM_REFERENCE_GROUP_NAME, name.second);
	}
	return true;
}

void PLSSceneitemMapManager::renameConfig(OBSSource source, const char *srcName, const char *destName)
{
	if (!source || pls_is_empty(srcName) || pls_is_empty(destName)) {
		return;
	}
	auto iter = sceneitemMap.find(srcName);
	if (iter == sceneitemMap.end()) {
		return;
	}
	auto value = iter.value();
	sceneitemMap.insert(destName, value);
	sceneitemMap.erase(iter);

	auto scene = obs_scene_from_source(source);
	if (obs_source_is_group(source)) {
		scene = GetCurrentScene();
	}

	std::pair<const char *, const char *> name = {srcName, destName};
	pls_scene_enum_items_all(scene, renameFunc, &name);

	for (auto iter = referenceSceneSourcesSet.begin(); iter != referenceSceneSourcesSet.end(); ++iter) {
		pls_scene_enum_items_all(iter.value(), renameFunc, &name);
	}
}

void PLSSceneitemMapManager::clearConfig()
{
	sceneitemMap.clear();
}

PLSSceneitemMapManager::PLSSceneitemMapManager()
{
	pls_frontend_add_event_callback(handlePrismEvent, this);
}

void PLSSceneitemMapManager::switchToDualOutputMode(OBSScene scene, QMap<QString, DisplayId> &ungroupItems)
{
	QVector<OBSSceneItem> items;
	pls_scene_enum_items_all(scene, enumItem, &items);
	if (items.isEmpty()) {
		return;
	}

	OBSSource sceneSource = obs_scene_get_source(scene);
	auto sceneName = obs_source_get_name(sceneSource);
	for (auto item : items) {
		auto id = obs_sceneitem_get_id(item);
		auto display = getItemDisplayId(item);
		auto uuid = getItemDisplayUuid(item);
		if (pls_is_vertical_sceneitem(item)) {
			auto horId = findVerticalItem(sceneName, id, uuid);
			if (SCENE_ITEM_NOT_FOUND != horId) {
				// check hor item existed
				auto scene = obs_sceneitem_get_scene(item);
				auto item_ = pls_scene_find_sceneitem_by_id_uuid(scene, horId, uuid);
				if (item_) {
					continue;
				}
			}

			addUuid(ungroupItems, sceneName, scene, uuid, display, id, nullptr, item);
			continue;
		}

		auto findResult = findItemByHorizontalId(sceneName, sceneSource, id, uuid);
		if (SCENE_ITEM_NOT_FOUND == findResult.first || SCENE_ITEM_NOT_EXISTED == findResult.first) {
			if (pls_is_empty(uuid) || SCENE_ITEM_NOT_EXISTED == findResult.first) {
				bool isGroup = obs_scene_is_group(scene);
				auto verItem = duplicateSceneItem(scene, item);
				if (isGroup) {
					auto group = obs_sceneitem_get_group(scene, item);
					bool vis = obs_sceneitem_visible(group);
					if (!vis) {
						obs_sceneitem_set_visible(item, true);
					}
					obs_sceneitem_group_add_item(group, verItem.second);
					pls_obs_sceneitem_move_hotkeys(scene, verItem.second);
					if (!vis) {
						obs_sceneitem_set_visible(item, false);
					}
				}

				addConfig(sceneName, getItemDisplayUuid(item), id, verItem.first);
				emit duplicateItemSuccess(item, verItem.second);
			} else {
				addUuid(ungroupItems, sceneName, scene, uuid, display, id, item, nullptr);
			}
		} else {
			if (obs_sceneitem_is_group(item)) {
				switchToDualOutputMode(obs_group_from_source(obs_sceneitem_get_source(item)), ungroupItems);
			}
		}
	}
	addUuidConfig(ungroupItems);
}

OBSSceneItem PLSSceneitemMapManager::getCurVerticalSceneitem(OBSSceneItem horItem)
{
	if (!horItem) {
		return nullptr;
	}

	auto id = obs_sceneitem_get_id(horItem);
	auto sceneName = getItemSourceName(horItem);

	auto uuid = getItemDisplayUuid(horItem);
	auto verId = findVerticalItem(sceneName.first, id, uuid);
	if (SCENE_ITEM_NOT_FOUND == verId) {
		return nullptr;
	}
	auto scene = obs_sceneitem_get_scene(horItem);
	return pls_scene_find_sceneitem_by_id_uuid(scene, verId, uuid);
}

QString PLSSceneitemMapManager::generateKey(int64_t horizontalId, const char *uuid)
{
	if (0 == horizontalId || pls_is_empty(uuid)) {
		return QString();
	}
	return QString::number(horizontalId) + "-" + QString(uuid);
}

std::pair<int64_t, QString> PLSSceneitemMapManager::getIdAndUuid(const QString &key)
{
	if (key.isEmpty()) {
		return {};
	}
	auto index = key.indexOf('-');
	auto id = atoi(key.mid(0, index).toStdString().c_str());
	auto uuid = key.mid(index + 1);
	return {id, uuid};
}

void PLSSceneitemMapManager::recoverHotkey(OBSData sceneitemInfo, OBSSceneItem verSceneItem)
{
	PLS_SCENEITEM_HOTKEY_IDS newIds;
	pls_get_sceneitem_hotkey_ids(verSceneItem, &newIds);

	if (OBS_INVALID_HOTKEY_PAIR_ID != newIds.hotkey_id_0) {
		OBSDataArrayAutoRelease hotkeyArray = obs_data_get_array(sceneitemInfo, "hotkey_id_0_array");
		obs_hotkey_load(newIds.hotkey_id_0, hotkeyArray);
	}
	if (OBS_INVALID_HOTKEY_PAIR_ID != newIds.hotkey_id_1) {
		OBSDataArrayAutoRelease hotkeyArray = obs_data_get_array(sceneitemInfo, "hotkey_id_1_array");
		obs_hotkey_load(newIds.hotkey_id_1, hotkeyArray);
	}
}

void PLSSceneitemMapManager::saveHotkey(OBSData sceneitemInfo, OBSSceneItem verSceneItem)
{
	PLS_SCENEITEM_HOTKEY_IDS newIds;
	pls_get_sceneitem_hotkey_ids(verSceneItem, &newIds);

	if (OBS_INVALID_HOTKEY_PAIR_ID != newIds.hotkey_id_0) {
		obs_data_set_int(sceneitemInfo, "hotkey_id_0", newIds.hotkey_id_0);
		OBSDataArrayAutoRelease hotkeyArray = obs_hotkey_save(newIds.hotkey_id_0);
		obs_data_set_array(sceneitemInfo, "hotkey_id_0_array", hotkeyArray);
	}
	if (OBS_INVALID_HOTKEY_PAIR_ID != newIds.hotkey_id_1) {
		obs_data_set_int(sceneitemInfo, "hotkey_id_1", newIds.hotkey_id_1);
		OBSDataArrayAutoRelease hotkeyArray = obs_hotkey_save(newIds.hotkey_id_1);
		obs_data_set_array(sceneitemInfo, "hotkey_id_1_array", hotkeyArray);
	}
}

void PLSSceneitemMapManager::addConfig(SceneitemMap &itemIdMap, const char *sceneName, const char *uuid, int64_t horizontalId, int64_t verticalId)
{
	auto key = generateKey(horizontalId, uuid);
	if (key.isEmpty()) {
		return;
	}

	auto iter = itemIdMap.find(sceneName);
	if (iter == itemIdMap.end()) {
		SceneitemIdMap uuidMap;
		uuidMap.insert(key, verticalId);
		itemIdMap.insert(sceneName, uuidMap);
		return;
	}
	auto &sceneitemIdMap = iter.value();
	sceneitemIdMap[key] = verticalId;
}

void PLSSceneitemMapManager::addConfig(const char *sceneName, const char *uuid, int64_t originalId, int64_t id)
{
	addConfig(sceneitemMap, sceneName, uuid, originalId, id);
}

void PLSSceneitemMapManager::addConfig(const SceneitemMap &idMap)
{
	for (auto iter = idMap.begin(); iter != idMap.end(); ++iter) {
		auto data = iter.value();
		for (auto iter1 = data.begin(); iter1 != data.end(); ++iter1) {
			auto key = getIdAndUuid(iter1.key());
			addConfig(iter.key().toStdString().c_str(), key.second.toStdString().c_str(), key.first, iter1.value());
		}
	}
}

std::pair<int64_t, OBSSceneItem> PLSSceneitemMapManager::findItemByHorizontalId(const char *sceneName, OBSSource sceneSource, int64_t id, const char *uuid)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}

	auto sceneitemIdMap = iter.value();
	auto tmpIter = sceneitemIdMap.find(generateKey(id, uuid));
	if (tmpIter == sceneitemIdMap.end()) {
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}

	auto scene = obs_group_or_scene_from_source(sceneSource);
	auto findId = tmpIter.value();
	auto item = pls_scene_find_sceneitem_by_id_uuid(scene, findId, uuid);
	if (item) {
		return {findId, item};
	}
	return {SCENE_ITEM_NOT_EXISTED, nullptr};
}

int64_t PLSSceneitemMapManager::getItemDisplayId(OBSSceneItem item)
{
	if (pls_is_vertical_sceneitem(item)) {
		return SceneItemDisplayMode::SCENE_ITEM_DISPLAY_VERTICAL;
	} else {
		return SceneItemDisplayMode::SCENE_ITEM_DISPLAY_HORIZONTAL;
	}
}

const char *PLSSceneitemMapManager::getItemDisplayUuid(OBSSceneItem item)
{
	OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(item);
	return obs_data_get_string(settings, SCENE_ITEM_MAP_UUID);
}

std::pair<const char *, OBSSource> PLSSceneitemMapManager::getItemSourceName(OBSSceneItem item)
{
	OBSScene curScene = GetCurrentScene();
	auto itemScene = obs_sceneitem_get_scene(item);
	if (obs_scene_is_group(itemScene)) {
		curScene = itemScene;
	}

	OBSSource sceneSource = obs_scene_get_source(curScene);
	return {obs_source_get_name(sceneSource), sceneSource};
}

int64_t PLSSceneitemMapManager::findVerticalItem(const char *sceneName, int64_t id, const char *uuid)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return SCENE_ITEM_NOT_FOUND;
	}
	auto sceneitemIdMap = iter.value();
	auto iter2 = sceneitemIdMap.find(generateKey(id, uuid));
	if (iter2 != sceneitemIdMap.end()) {
		return iter2.value();
	}

	for (auto iter = sceneitemIdMap.begin(); iter != sceneitemIdMap.end(); ++iter) {
		if (id == iter.value()) {
			return getIdAndUuid(iter.key()).first;
		}
	}
	return SCENE_ITEM_NOT_FOUND;
}

void PLSSceneitemMapManager::createRefrenceScene(OBSSceneItem item, QString &groupName, QString &referenceSceneName, OBSScene &verSceneObj)
{
	auto source = obs_sceneitem_get_source(item);
	groupName = obs_source_get_name(source);
	referenceSceneName = QString(groupName).append("-").append(pls_gen_uuid());

	verSceneObj = pls_create_vertical_scene(referenceSceneName.toStdString().c_str());
	referenceSceneSourcesSet.insert(referenceSceneName, verSceneObj);
}

void PLSSceneitemMapManager::createRefrenceSceneGroup(OBSSceneItem item, OBSScene scene)
{
	// must set group visible to avoid hide group ref count error
	bool vis = obs_sceneitem_visible(item);
	if (!vis) {
		obs_sceneitem_set_visible(item, true);
	}

	// group item
	QVector<OBSSceneItem> items;
	obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item);
	pls_scene_enum_items_all(groupScene, enumItem, &items);

	QMap<QString, DisplayId> groupItems;

	for (auto item_ : items) {
		auto originalId = obs_sceneitem_get_id(item_);
		auto source = obs_sceneitem_get_source(item);
		if (!source) {
			continue;
		}
		auto groupName = obs_source_get_name(source);
		auto uuid = getItemDisplayUuid(item_);
		if (SCENE_ITEM_NOT_FOUND != findVerticalItem(groupName, originalId, uuid)) {
			continue;
		}

		auto isVerticalItem = pls_is_vertical_sceneitem(item_);
		if (!pls_is_empty(uuid)) {
			auto display = SceneItemDisplayMode::SCENE_ITEM_DISPLAY_HORIZONTAL;
			if (isVerticalItem) {
				display = SceneItemDisplayMode::SCENE_ITEM_DISPLAY_VERTICAL;
				addUuid(groupItems, groupName, scene, uuid, display, originalId, nullptr, item_);
			} else {
				addUuid(groupItems, groupName, scene, uuid, display, originalId, item_, nullptr);
			}
			continue;
		}

		if (isVerticalItem) {
			continue;
		}

		auto newItem = duplicateSceneItem(scene, item_);
		if (newItem.first == SCENE_ITEM_NOT_FOUND) {
			continue;
		}
		pls_obs_sceneitem_group_add_item(item, newItem.second, item_);
		pls_obs_sceneitem_move_hotkeys(obs_group_from_source(obs_sceneitem_get_source(item)), newItem.second);
		addConfig(groupName, getItemDisplayUuid(newItem.second), originalId, newItem.first);
	}
	addUuidConfig(groupItems);

	if (!vis) {
		obs_sceneitem_set_visible(item, false);
	}
}

OBSSceneItem PLSSceneitemMapManager::createRefrenceGroupItem(const QString &groupName, const QString &referenceSceneName, const QString &oriSceneName, OBSScene oriScene, OBSScene verSceneObj,
							     OBSSceneItem horSceneitem, OBSSceneItem verSceneitem)
{
	// refrence scene group item
	QString uuid = getItemDisplayUuid(horSceneitem);
	if (pls_is_empty(uuid)) {
		uuid = pls_gen_uuid();
	}

	OBSDataAutoRelease horSettings = obs_sceneitem_get_private_settings(horSceneitem);
	obs_data_set_string(horSettings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());

	OBSDataAutoRelease settings1 = obs_data_create();
	obs_data_set_string(settings1, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY, oriSceneName.toStdString().c_str());
	obs_data_set_string(settings1, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());
	obs_data_set_string(settings1, SCENE_ITEM_REFERENCE_SCENE_NAME, referenceSceneName.toStdString().c_str());
	auto itemGroup = pls_vertical_scene_add(verSceneObj, obs_sceneitem_get_source(horSceneitem), nullptr, settings1);
	if (!itemGroup) {
		PLS_WARN("PLSSceneitemMapManager", "Duplicate mapped group sceneitem failed H: %p", horSceneitem.Get());
		return nullptr;
	}

	if (oriScene) {
		pls_bind_vertical_scene(oriScene, verSceneObj);
		pls_obs_sceneitem_move_hotkeys(oriScene, itemGroup);
	}

	auto iter = verticalSceneitemInfos.find(uuid);
	if (iter != verticalSceneitemInfos.end()) {
		OBSData data = iter.value();
		obs_sceneitem_set_visible(itemGroup, obs_data_get_bool(data, "visible"));
		recoverHotkey(data, itemGroup);
	} else {
		obs_sceneitem_set_visible(itemGroup, obs_sceneitem_visible(horSceneitem));
	}

	auto verSceneSource = obs_scene_get_source(verSceneObj);
	obs_scene_release(verSceneObj);

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, SCENE_ITEM_REFERENCE_SCENE_NAME, referenceSceneName.toStdString().c_str());
	obs_data_set_string(settings, SCENE_ITEM_REFERENCE_GROUP_NAME, groupName.toStdString().c_str());
	obs_data_set_string(settings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());
	if (!verSceneitem) {
		return pls_vertical_scene_add(oriScene, verSceneSource, horSceneitem, settings);
	} else {
		OBSDataAutoRelease settings1 = obs_data_create();
		auto priSettings = obs_sceneitem_get_private_settings(verSceneitem);
		obs_data_apply(settings1, priSettings);

		obs_data_set_string(settings1, SCENE_ITEM_REFERENCE_SCENE_NAME, referenceSceneName.toStdString().c_str());
		obs_data_set_string(settings1, SCENE_ITEM_REFERENCE_GROUP_NAME, groupName.toStdString().c_str());
		obs_data_set_string(settings1, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());
		obs_sceneitem_remove(verSceneitem);
		return pls_vertical_scene_add(oriScene, verSceneSource, horSceneitem, settings1);
	}
}

void PLSSceneitemMapManager::removeConfig(const char *sceneName, int64_t id, const char *uuid)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return;
	}

	auto &sceneitemIdMap = iter.value();
	auto key = generateKey(id, uuid);
	auto tmpIter = sceneitemIdMap.find(key);
	if (tmpIter == sceneitemIdMap.end()) {
		return;
	}
	sceneitemIdMap.remove(key);
}

void PLSSceneitemMapManager::removeConfig(const char *sceneName)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return;
	}
	sceneitemMap.erase(iter);
}

static bool remove_items(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_remove(item);
	return true;
};

void deleteSceneInternalItem(OBSSource source)
{
	auto scene = obs_scene_from_source(source);
	pls_scene_enum_items_all(scene, remove_items, nullptr);
}

void PLSSceneitemMapManager::removeItem(OBSSceneItem horItem, bool unGroup)
{
	auto s = GetCurrentScene();
	auto curSceneName = obs_source_get_name(obs_scene_get_source(s));

	QMap<const char *, QString> removeIds;

	bool isGroup = obs_sceneitem_is_group(horItem);
	QVector<OBSSceneItem> items;
	if (isGroup) {
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(horItem);
		obs_data_set_bool(settings, "groupSelectedWithDualOutput", false);

		obs_sceneitem_group_enum_items(horItem, enumItem, &items);
		auto groupSource = obs_sceneitem_get_source(horItem);
		auto groupName = obs_source_get_name(groupSource);
		for (auto item_ : items) {
			removeIds.insert(groupName, generateKey(obs_sceneitem_get_id(item_), getItemDisplayUuid(horItem)));
		}
		auto groupId = obs_sceneitem_get_id(horItem);
		removeIds.insert(curSceneName, generateKey(groupId, getItemDisplayUuid(horItem)));
		removeConfig(groupName);
	}
	auto item = getVerticalSceneitem(horItem);
	if (nullptr == item) {
		return;
	}

	// do not remove the scene item when ungroup
	if (!unGroup) {
		obs_sceneitem_remove(item);
	}
	if (isGroup) {
		obs_sceneitem_remove(getCurVerticalSceneitem(horItem));
	}

	auto sceneName = getMappedVerticalSceneName(item);
	if (!pls_is_empty(sceneName)) {
		removeReferenceSceneSource(sceneName);
	}

	removeConfig(curSceneName, obs_sceneitem_get_id(horItem), getItemDisplayUuid(horItem));

	for (auto key : removeIds.keys()) {
		auto data = getIdAndUuid(key);
		removeConfig(key, data.first, data.second.toStdString().c_str());
	}
}

void PLSSceneitemMapManager::groupItems(const char *sceneName, const char *groupName, QVector<obs_sceneitem_t *> itemOrder)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return;
	}

	auto &sceneitemIdMap = iter.value();
	SceneitemIdMap groupItem;
	for (auto item : itemOrder) {
		auto id = obs_sceneitem_get_id(item);
		auto key = generateKey(id, getItemDisplayUuid(item));
		auto iter1 = sceneitemIdMap.find(key);
		if (iter1 == sceneitemIdMap.end()) {
			continue;
		}
		groupItem.insert(key, iter1.value());
		sceneitemIdMap.remove(key);
	}

	auto iterGroupMap = sceneitemMap.find(groupName);
	if (iterGroupMap != sceneitemMap.end()) {
		if (iterGroupMap.value().count() > 0) {
			sceneitemMap[groupName] = groupItem;
			return;
		}
	}

	sceneitemMap.insert(groupName, groupItem);
}

QVector<struct obs_sceneitem_order_info> PLSSceneitemMapManager::reorderItems(OBSScene scene, QVector<struct obs_sceneitem_order_info> orderList)
{
	QVector<struct obs_sceneitem_order_info> newOrderList;
	auto getVerItem = [this](obs_sceneitem_t *item) {
		auto id = obs_sceneitem_get_id(item);
		auto name = getItemSourceName(item);
		auto uuid = getItemDisplayUuid(item);
		return findItemByHorizontalId(name.first, name.second, id, uuid);
	};

	auto sceneName = obs_source_get_name(obs_scene_get_source(scene));

	QStringList oldSceneNames;
	SceneitemMap itemMap;
	for (auto order : orderList) {
		auto sceneName = obs_source_get_name(obs_scene_get_source(scene));

		obs_sceneitem_order_info newOrder;
		newOrder.group = order.group;
		OBSSource source;
		if (newOrder.group) {
			source = obs_sceneitem_get_source(newOrder.group);
			sceneName = obs_source_get_name(source);
		}

		if (order.item) {
			auto verId = getVerItem(order.item);
			if (verId.first == SCENE_ITEM_NOT_FOUND || verId.first == SCENE_ITEM_NOT_EXISTED) {
				newOrderList.push_back(order);
				continue;
			}

			addConfig(itemMap, sceneName, getItemDisplayUuid(order.item), obs_sceneitem_get_id(order.item), verId.first);

			newOrder.item = verId.second;
			newOrderList.push_back(newOrder);
			newOrderList.push_back(order);

			if (newOrder.item == order.item) {
				assert(false && "wrong reorder items. must check");
			}
		}
		oldSceneNames << sceneName;
	}

	for (auto name : oldSceneNames) {
		removeConfig(name.toStdString().c_str());
	}
	addConfig(itemMap);
	return newOrderList;
}

OBSSceneItem PLSSceneitemMapManager::getVerticalSceneitem(OBSSceneItem horItem)
{
	auto item = getCurVerticalSceneitem(horItem);
	if (!item) {
		return nullptr;
	}

	auto mappedScene = getMappedVerticalSceneName(item);
	if (pls_is_empty(mappedScene)) {
		return item;
	}

	auto iter = referenceSceneSourcesSet.find(mappedScene);
	if (iter != referenceSceneSourcesSet.end()) {
		auto scene = referenceSceneSourcesSet.value(iter.key());
		QVector<OBSSceneItem> items;
		pls_scene_enum_items_all(scene, enumItem, &items);
		if (items.count() == 1) {
			return items[0];
		}
	}

	return nullptr;
}

OBSSceneItem PLSSceneitemMapManager::getVerticalSelectedSceneitem(OBSSceneItem horItem)
{
	if (!horItem) {
		return nullptr;
	}

	auto verItem = getVerticalSceneitem(horItem);
	if (verItem && obs_sceneitem_selected(verItem)) {
		return verItem;
	}
	return nullptr;
}

OBSSceneItem PLSSceneitemMapManager::getHorizontalSceneitem(OBSSceneItem verItem)
{
	auto sceneName = getItemSourceName(verItem);
	auto iter = sceneitemMap.find(sceneName.first);
	if (iter == sceneitemMap.end()) {
		return nullptr;
	}
	auto sceneitemIdMap = iter.value();
	auto horId = SCENE_ITEM_NOT_FOUND;
	auto verItemId = obs_sceneitem_get_id(verItem);
	for (auto iter = sceneitemIdMap.begin(); iter != sceneitemIdMap.end(); ++iter) {
		if (verItemId == iter.value()) {
			horId = getIdAndUuid(iter.key()).first;
			break;
		}
	}
	if (horId != SCENE_ITEM_NOT_FOUND) {
		auto scene = obs_sceneitem_get_scene(verItem);
		return pls_scene_find_sceneitem_by_id_uuid(scene, horId, getItemDisplayUuid(verItem));
	}
	return nullptr;
}

bool PLSSceneitemMapManager::isMappedVerticalSceneItem(OBSSceneItem verItem)
{
	auto sceneName = getMappedVerticalSceneName(verItem);
	return !pls_is_empty(sceneName);
}

const char *PLSSceneitemMapManager::getMappedVerticalSceneName(OBSSceneItem verItem)
{
	if (!verItem) {
		return nullptr;
	}
	obs_source_t *source = obs_sceneitem_get_source(verItem);
	auto id = obs_source_get_id(source);
	if (pls_is_vertical_sceneitem(verItem) && (pls_is_equal(id, "scene") || pls_is_equal(id, "group"))) {
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(verItem);
		return obs_data_get_string(settings, SCENE_ITEM_REFERENCE_SCENE_NAME);
	}
	return nullptr;
}

const char *PLSSceneitemMapManager::getMappedOriginalSceneName(OBSSceneItem verItem)
{
	if (!verItem) {
		return nullptr;
	}
	if (pls_is_vertical_sceneitem(verItem)) {
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(verItem);
		return obs_data_get_string(settings, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY);
	}
	return nullptr;
}

void PLSSceneitemMapManager::deleteSelectedScene(OBSScene scene)
{
	pls_scene_enum_items_all(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			PLSSceneitemMapManager *mgr = static_cast<PLSSceneitemMapManager *>(param);
			auto sceneName = PLSSceneitemMapMgrInstance->getMappedVerticalSceneName(item);
			if (!pls_is_empty(sceneName)) {
				mgr->removeReferenceSceneSource(sceneName);
			}
			return true;
		},
		this);
}

std::pair<int64_t, OBSSceneItem> PLSSceneitemMapManager::duplicateSceneItem(OBSScene scene, OBSSceneItem item)
{
	OBSSceneItem newVerticalItem = nullptr;
	bool isGroup = obs_sceneitem_is_group(item);
	if (isGroup) {
		OBSScene verSceneObj;
		QString referenceSceneName;
		QString groupName;
		createRefrenceScene(item, groupName, referenceSceneName, verSceneObj);
		createRefrenceSceneGroup(item, scene);
		newVerticalItem = createRefrenceGroupItem(groupName, referenceSceneName, obs_source_get_name(obs_scene_get_source(scene)), scene, verSceneObj, item, nullptr);
		obs_sceneitem_set_visible(newVerticalItem, true);
		PLS_INFO("PLSSceneitemMapManager", "Group sceneitem relationship H: %p, V: %p, mapped scene : %p", item.Get(), newVerticalItem.Get(), verSceneObj.Get());
	} else {
		auto source = obs_sceneitem_get_source(item);
		if (pls_is_equal("scene", obs_source_get_id(source))) {
			QMap<QString, DisplayId> groupItems;
			auto scene = obs_scene_from_source(source);
			switchToDualOutputMode(scene, groupItems);
		}

		auto iter = verticalSceneitemInfos.find(getItemDisplayUuid(item));
		if (iter != verticalSceneitemInfos.end()) {
			newVerticalItem = pls_scene_load_item(scene, iter.value(), source, item);
			recoverHotkey(iter.value(), newVerticalItem);
		} else {
			auto uuid = pls_gen_uuid();
			OBSDataAutoRelease privateSettings = obs_data_create();
			obs_data_set_string(privateSettings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());
			newVerticalItem = pls_vertical_scene_add(scene, source, item, privateSettings);
			obs_sceneitem_set_visible(newVerticalItem, obs_sceneitem_visible(item));

			OBSDataAutoRelease horItemSettings = obs_sceneitem_get_private_settings(item);
			obs_data_set_string(horItemSettings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());
		}
	}

	if (!newVerticalItem) {
		PLS_INFO("PLSSceneitemMapManager", "Duplicate sceneitem failed H: %p", item.Get());
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}
	PLS_INFO("PLSSceneitemMapManager", "Sceneitem relationship H: %p, V: %p", item.Get(), newVerticalItem.Get());
	return {obs_sceneitem_get_id(newVerticalItem), newVerticalItem};
}

void PLSSceneitemMapManager::removeReferenceSceneSource(const char *sceneName)
{
	OBSScene scene = nullptr;
	auto iter = referenceSceneSourcesSet.find(sceneName);
	if (iter != referenceSceneSourcesSet.end()) {
		scene = iter.value();
		referenceSceneSourcesSet.remove(sceneName);
	}

	if (!scene) {
		return;
	}

	auto source = obs_scene_get_source(scene);
	deleteSceneInternalItem(source);
	obs_source_remove(source);
}

void PLSSceneitemMapManager::addReferenceScene(const char *sceneName, OBSScene scene)
{
	referenceSceneSourcesSet.insert(sceneName, scene);
}

void PLSSceneitemMapManager::clearSceneitemReferenceSceneName(OBSScene scene)
{
	pls_scene_enum_items_all(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *ptr) {
			if (!pls_is_vertical_sceneitem(item)) {
				return true;
			}

			auto source = obs_sceneitem_get_source(item);
			auto name = obs_source_get_name(source);
			if (!pls_is_equal("scene", obs_source_get_id(source))) {
				return true;
			}

			OBSDataAutoRelease priSettings = obs_sceneitem_get_private_settings(item);
			obs_data_set_string(priSettings, SCENE_ITEM_REFERENCE_SCENE_NAME, "");

			return true;
		},
		nullptr);
}

void PLSSceneitemMapManager::bindMappedVerticalItemHotkeys()
{
	for (auto sceneName : referenceSceneSourcesSet.keys()) {
		auto scene = referenceSceneSourcesSet.value(sceneName);
		if (!scene) {
			continue;
		}
		bindMappedVerticalItemHotkeys(scene);
	}
}

void PLSSceneitemMapManager::bindMappedVerticalItemHotkeys(OBSScene scene)
{
	QVector<OBSSceneItem> items;
	pls_scene_enum_items_all(scene, enumItem, &items);

	for (auto item : items) {
		auto originalName = PLSSceneitemMapMgrInstance->getMappedOriginalSceneName(item);
		if (!pls_is_empty(originalName)) {
			if (OBSSceneAutoRelease originalScene = obs_get_scene_by_name(originalName); originalScene) {
				pls_bind_vertical_scene(originalScene, scene);
				pls_obs_sceneitem_move_hotkeys(originalScene, item);
			}
		}
	}
}

void PLSSceneitemMapManager::removeGroupSelectedStatus()
{
	auto other_scenes_cb = [](void *data_ptr, obs_source_t *scene) {
		if (!scene) {
			return true;
		}
		auto source = obs_scene_from_source(scene);
		if (!source) {
			return true;
		}
		QVector<OBSSceneItem> items;
		pls_scene_enum_items_all(source, enumItem, &items);

		for (auto item : items) {
			if (!obs_sceneitem_is_group(item)) {
				continue;
			}
			OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(item);
			obs_data_set_bool(settings, "groupSelectedWithDualOutput", false);
		}

		return true;
	};
	pls_enum_all_scenes(other_scenes_cb, nullptr);
}

void PLSSceneitemMapManager::clearSource()
{
	removeGroupSelectedStatus();
	sceneitemMap.clear();
	auto iter = referenceSceneSourcesSet.begin();
	while (iter != referenceSceneSourcesSet.end()) {
		auto value = iter.key();
		removeReferenceSceneSource(value.toStdString().c_str());
		iter = referenceSceneSourcesSet.begin();
	}
	referenceSceneSourcesSet.clear();
}

bool PLSSceneitemMapManager::getDualOutputOpened()
{
	return dualOutputOpened;
}

void PLSSceneitemMapManager::addUuid(QMap<QString, DisplayId> &newSceneData, const char *sceneName, OBSScene scene, const char *uuid, int display, int64_t id, OBSSceneItem horItem,
				     OBSSceneItem verItem)
{
	auto iter = newSceneData.find(uuid);
	if (iter == newSceneData.end()) {
		DisplayId displayId;
		setDisplayId(displayId, sceneName, scene, display, id, horItem, verItem);
		newSceneData.insert(uuid, displayId);
	} else {
		auto &displayId = iter.value();
		setDisplayId(displayId, sceneName, scene, display, id, horItem, verItem);
	}
}

void PLSSceneitemMapManager::setDisplayId(DisplayId &displayId, const char *sceneName, OBSScene scene, int display, int64_t id, OBSSceneItem horItem, OBSSceneItem verItem)
{
	displayId.sceneName = sceneName;
	displayId.scene = scene;
	if (display == SceneItemDisplayMode::SCENE_ITEM_DISPLAY_HORIZONTAL) {
		displayId.horizontalId = id;
		displayId.horizontalItem = horItem;
	} else if (display == SceneItemDisplayMode::SCENE_ITEM_DISPLAY_VERTICAL) {
		displayId.verticalId = id;
		displayId.verticalItem = verItem;
	}
}

void PLSSceneitemMapManager::addUuidConfig(const QMap<QString, DisplayId> &ungroupItems)
{
	for (auto displayId : ungroupItems.values()) {
		if (0 == displayId.horizontalId && displayId.verticalItem) {
			PLS_WARN("PLSSceneitemMapManager", "the verItem where no horItem is found : %p", displayId.verticalItem.Get());
			obs_sceneitem_remove(displayId.verticalItem);
			continue;
		} else if (0 == displayId.verticalId && displayId.horizontalItem) {
			OBSSourceAutoRelease source = obs_get_source_by_name(displayId.sceneName);
			auto scene = obs_group_or_scene_from_source(source);
			bool isGroup = obs_source_is_group(source);
			auto verItem = duplicateSceneItem(displayId.scene, displayId.horizontalItem);
			if (isGroup) {
				auto group = obs_sceneitem_get_group(displayId.scene, displayId.horizontalItem);
				bool vis = obs_sceneitem_visible(group);
				if (!vis) {
					obs_sceneitem_set_visible(group, true);
				}

				pls_obs_sceneitem_group_add_item(group, verItem.second, displayId.horizontalItem);
				pls_obs_sceneitem_move_hotkeys(scene, verItem.second);

				if (!vis) {
					obs_sceneitem_set_visible(group, false);
				}
			}
			addConfig(displayId.sceneName, getItemDisplayUuid(displayId.horizontalItem), displayId.horizontalId, verItem.first);
			continue;
		} else { // matched same uuid
			obs_source_t *source = obs_sceneitem_get_source(displayId.verticalItem);
			auto id = obs_source_get_id(source);
			if (pls_is_vertical_sceneitem(displayId.verticalItem) && (pls_is_equal(id, "scene"))) {
				OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(displayId.verticalItem);
				auto groupName = obs_data_get_string(settings, SCENE_ITEM_REFERENCE_GROUP_NAME);
				if (!pls_is_empty(groupName)) {
					if (auto mappedSceneName = obs_data_get_string(settings, SCENE_ITEM_REFERENCE_SCENE_NAME); pls_is_empty(mappedSceneName)) {
						OBSScene verSceneObj;
						QString referenceSceneName;
						QString groupName;
						createRefrenceScene(displayId.verticalItem, groupName, referenceSceneName, verSceneObj);
						auto newVerticalItem = createRefrenceGroupItem(groupName, referenceSceneName, displayId.sceneName, displayId.scene, verSceneObj,
											       displayId.horizontalItem, displayId.verticalItem);
						PLS_INFO("PLSSceneitemMapManager", "Group sceneitem relationship H: %p, V: %p, mapped scene : %p", displayId.horizontalItem.Get(),
							 newVerticalItem.Get(), verSceneObj.Get());
						addConfig(displayId.sceneName, getItemDisplayUuid(displayId.horizontalItem), displayId.horizontalId, obs_sceneitem_get_id(newVerticalItem));
						continue;
					}
				}
			}
		}

		addConfig(displayId.sceneName, getItemDisplayUuid(displayId.horizontalItem), displayId.horizontalId, displayId.verticalId);
	}
}

void PLSSceneitemMapManager::setDualOutputOpened(bool open)
{
	dualOutputOpened = open;
}
