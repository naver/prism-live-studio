#include "PLSSceneitemMapManager.h"
#include "window-basic-main.hpp"
#include "libutils-api.h"
#include "PLSBasic.h"
#include "pls/pls-dual-output.h"
#include "liblog.h"

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
		PLSSceneitemMapMgrInstance->switchToDualOutputMode();
		OBSBasic::Get()->resetAllGroupTransforms();
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_OFF:
		PLSBasic::instance()->SaveProject();
		OBSBasic::Get()->undo_s.clear();
		break;
	default:
		break;
	}
}

PLSSceneitemMapManager::~PLSSceneitemMapManager()
{
	pls_frontend_remove_event_callback(handlePrismEvent, this);
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

OBSDataArray PLSSceneitemMapManager::saveConfig()
{
	OBSDataArray array = obs_data_array_create();

	for (auto key : sceneitemMap.keys()) {
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_string(data, SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY, key.toStdString().c_str());
		SceneitemIdMap idMap = sceneitemMap.value(key);
		OBSDataArrayAutoRelease dataArray = obs_data_array_create();
		for (auto id : idMap.keys()) {
			OBSDataAutoRelease tmpData = obs_data_create();
			obs_data_set_string(tmpData, QString::number(id).toStdString().c_str(), QString::number(idMap.value(id)).toStdString().c_str());
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
				const char *name = obs_data_item_get_name(item);
				if (pls_is_empty(name)) {
					continue;
				}
				const char *value = obs_data_item_get_string(item);
				if (pls_is_empty(value)) {
					continue;
				}
				addConfig(sceneName, atoi(name), atoi(value));
			}
		}
	}
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

	auto sceneName = obs_source_get_name(obs_scene_get_source(scene));
	for (auto item : items) {
		auto id = obs_sceneitem_get_id(item);
		auto display = getItemDisplayId(item);
		if (pls_is_vertical_sceneitem(item)) {
			auto horId = findVerticalItem(sceneName, id);
			if (SCENE_ITEM_NOT_FOUND != horId) {
				// check hor item existed
				auto scene = obs_sceneitem_get_scene(item);
				auto item_ = obs_scene_find_sceneitem_by_id(scene, horId);
				if (item_) {
					continue;
				}
			}

			auto uuid = getItemDisplayUuid(item);
			addUuid(ungroupItems, sceneName, uuid, display, id, nullptr, item);
			continue;
		}

		auto findResult = findItemByHorizontalId(sceneName, id);
		if (SCENE_ITEM_NOT_FOUND == findResult.first || SCENE_ITEM_NOT_EXISTED == findResult.first) {
			auto uuid = getItemDisplayUuid(item);
			if (pls_is_empty(uuid) || SCENE_ITEM_NOT_EXISTED == findResult.first) {
				bool isGroup = obs_scene_is_group(scene);
				if (isGroup) {
					scene = GetCurrentScene();
				}
				auto verItem = duplicateSceneItem(scene, item);
				if (isGroup) {
					obs_sceneitem_group_add_item(obs_sceneitem_get_group(scene, item), verItem.second);
				}

				addConfig(sceneName, id, verItem.first);
				emit duplicateItemSuccess(item, verItem.second);
			} else {
				addUuid(ungroupItems, sceneName, uuid, display, id, item, nullptr);
			}
		} else {
			if (obs_sceneitem_is_group(item)) {
				switchToDualOutputMode(obs_sceneitem_group_get_scene(item), ungroupItems);
			}
		}
	}
	addUuidConfig(ungroupItems);
}

void PLSSceneitemMapManager::addConfig(SceneitemMap &itemIdMap, const char *sceneName, int64_t horizontalId, int64_t verticalId)
{
	auto iter = itemIdMap.find(sceneName);
	if (iter == itemIdMap.end()) {
		SceneitemIdMap uuidMap;
		uuidMap.insert(horizontalId, verticalId);
		itemIdMap.insert(sceneName, uuidMap);
		return;
	}
	auto &sceneitemIdMap = iter.value();
	sceneitemIdMap[horizontalId] = verticalId;
}

void PLSSceneitemMapManager::addConfig(const char *sceneName, int64_t originalId, int64_t id)
{
	addConfig(sceneitemMap, sceneName, originalId, id);
}

void PLSSceneitemMapManager::addConfig(const SceneitemMap &idMap)
{
	for (auto iter = idMap.begin(); iter != idMap.end(); ++iter) {
		auto data = iter.value();
		for (auto iter1 = data.begin(); iter1 != data.end(); ++iter1) {
			addConfig(iter.key().toStdString().c_str(), iter1.key(), iter1.value());
		}
	}
}

std::pair<int64_t, OBSSceneItem> PLSSceneitemMapManager::findItemByHorizontalId(const char *sceneName, int64_t id)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}

	auto sceneitemIdMap = iter.value();
	auto tmpIter = sceneitemIdMap.find(id);
	if (tmpIter == sceneitemIdMap.end()) {
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sceneName);
	if (!source) {
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}

	auto scene = obs_group_or_scene_from_source(source);
	auto findId = tmpIter.value();
	auto item = obs_scene_find_sceneitem_by_id(scene, findId);
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

const char *PLSSceneitemMapManager::getItemSourceName(OBSSceneItem item)
{
	OBSScene curScene = GetCurrentScene();
	auto itemScene = obs_sceneitem_get_scene(item);
	if (obs_scene_is_group(itemScene)) {
		curScene = itemScene;
	}
	return obs_source_get_name(obs_scene_get_source(curScene));
}

int64_t PLSSceneitemMapManager::findVerticalItem(const char *sceneName, int64_t id)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return SCENE_ITEM_NOT_FOUND;
	}
	auto sceneitemIdMap = iter.value();
	auto iter2 = sceneitemIdMap.find(id);
	if (iter2 != sceneitemIdMap.end()) {
		return iter2.value();
	}

	for (auto iter = sceneitemIdMap.begin(); iter != sceneitemIdMap.end(); ++iter) {
		if (id == iter.value()) {
			return iter.key();
		}
	}
	return SCENE_ITEM_NOT_FOUND;
}

void PLSSceneitemMapManager::removeConfig(const char *sceneName, int64_t id)
{
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return;
	}

	auto &sceneitemIdMap = iter.value();
	auto tmpIter = sceneitemIdMap.find(id);
	if (tmpIter == sceneitemIdMap.end()) {
		return;
	}
	sceneitemIdMap.remove(id);
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

	QMap<const char *, int64_t> removeIds;

	QVector<OBSSceneItem> items;
	if (obs_sceneitem_is_group(horItem)) {
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(horItem);
		obs_data_set_bool(settings, "groupSelectedWithDualOutput", false);

		obs_sceneitem_group_enum_items(horItem, enumItem, &items);
		auto groupSource = obs_sceneitem_get_source(horItem);
		auto groupName = obs_source_get_name(groupSource);
		for (auto item_ : items) {
			removeIds.insert(groupName, obs_sceneitem_get_id(item_));
		}
		auto groupId = obs_sceneitem_get_id(horItem);
		removeIds.insert(curSceneName, groupId);
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

	auto sceneName = getMappedVerticalSceneName(item);
	if (!pls_is_empty(sceneName)) {
		removeReferenceSceneSource(sceneName);
	}

	removeConfig(curSceneName, obs_sceneitem_get_id(horItem));

	for (auto key : removeIds.keys()) {
		removeConfig(key, removeIds.value(key));
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
		auto iter1 = sceneitemIdMap.find(id);
		if (iter1 == sceneitemIdMap.end()) {
			continue;
		}
		groupItem.insert(id, iter1.value());
		sceneitemIdMap.remove(id);
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
		return findItemByHorizontalId(name, id);
	};

	auto sceneName = obs_source_get_name(obs_scene_get_source(scene));

	QStringList oldSceneNames;
	SceneitemMap itemMap;
	for (auto order : orderList) {

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

			addConfig(itemMap, sceneName, obs_sceneitem_get_id(order.item), verId.first);

			newOrder.item = verId.second;
			newOrderList.push_back(newOrder);
			newOrderList.push_back(order);
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
	if (!horItem) {
		return nullptr;
	}

	auto id = obs_sceneitem_get_id(horItem);
	auto sceneName = getItemSourceName(horItem);

	auto verId = findVerticalItem(sceneName, id);
	if (SCENE_ITEM_NOT_FOUND == verId) {
		return nullptr;
	}
	auto scene = obs_sceneitem_get_scene(horItem);
	auto item = obs_scene_find_sceneitem_by_id(scene, verId);
	return item;
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
	auto iter = sceneitemMap.find(sceneName);
	if (iter == sceneitemMap.end()) {
		return nullptr;
	}
	auto sceneitemIdMap = iter.value();
	auto horId = SCENE_ITEM_NOT_FOUND;
	auto verItemId = obs_sceneitem_get_id(verItem);
	for (auto iter = sceneitemIdMap.begin(); iter != sceneitemIdMap.end(); ++iter) {
		if (verItemId == iter.value()) {
			horId = iter.key();
			break;
		}
	}
	if (horId != SCENE_ITEM_NOT_FOUND) {
		auto scene = obs_sceneitem_get_scene(verItem);
		return obs_scene_find_sceneitem_by_id(scene, horId);
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
	if (pls_is_equal(id, "scene") && pls_is_vertical_sceneitem(verItem)) {
		OBSDataAutoRelease settings = obs_sceneitem_get_private_settings(verItem);
		return obs_data_get_string(settings, SCENE_ITEM_REFERENCE_SCENE_NAME);
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
	auto uuid = pls_gen_uuid();

	// original sceneitem
	OBSDataAutoRelease horItemSettings = obs_sceneitem_get_private_settings(item);
	obs_data_set_string(horItemSettings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());

	obs_sceneitem_t *newVerticalItem = nullptr;

	auto source = obs_sceneitem_get_source(item);
	bool isGroup = obs_sceneitem_is_group(item);
	if (isGroup) {

		auto groupName = obs_source_get_name(source);
		auto referenceSceneName = QString(groupName).append("-").append(pls_gen_uuid());
		referenceSceneSourcesSet.insert(referenceSceneName);

		OBSSceneAutoRelease verSceneObj = pls_create_vertical_scene(referenceSceneName.toStdString().c_str());

		// group item
		QVector<OBSSceneItem> items;
		obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item);
		pls_scene_enum_items_all(groupScene, enumItem, &items);

		QMap<QString, DisplayId> groupItems;

		for (auto item_ : items) {
			auto originalId = obs_sceneitem_get_id(item_);
			if (SCENE_ITEM_NOT_FOUND != findVerticalItem(groupName, originalId)) {
				continue;
			}

			auto isVerticalItem = pls_is_vertical_sceneitem(item_);
			auto uuid = getItemDisplayUuid(item_);
			if (!pls_is_empty(uuid)) {
				auto display = SceneItemDisplayMode::SCENE_ITEM_DISPLAY_HORIZONTAL;
				if (isVerticalItem) {
					display = SceneItemDisplayMode::SCENE_ITEM_DISPLAY_VERTICAL;
					addUuid(groupItems, groupName, uuid, display, originalId, nullptr, item_);
				} else {
					addUuid(groupItems, groupName, uuid, display, originalId, item_, nullptr);
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
			obs_sceneitem_group_add_item(item, newItem.second);
			addConfig(groupName, originalId, newItem.first);
		}

		addUuidConfig(groupItems);

		// refrence scene group item
		pls_vertical_scene_add(verSceneObj, source, nullptr, OBSData());

		auto verSceneSource = obs_scene_get_source(verSceneObj);
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, SCENE_ITEM_REFERENCE_SCENE_NAME, referenceSceneName.toStdString().c_str());
		obs_data_set_string(settings, SCENE_ITEM_REFERENCE_GROUP_NAME, groupName);
		obs_data_set_string(settings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());
		newVerticalItem = pls_vertical_scene_add(scene, verSceneSource, item, settings);

	} else {
		OBSDataAutoRelease privateSettings = obs_data_create();
		obs_data_set_string(privateSettings, SCENE_ITEM_MAP_UUID, uuid.toStdString().c_str());

		newVerticalItem = pls_vertical_scene_add(scene, source, item, privateSettings);
	}

	if (!newVerticalItem) {
		return {SCENE_ITEM_NOT_FOUND, nullptr};
	}

	return {obs_sceneitem_get_id(newVerticalItem), newVerticalItem};
}

void PLSSceneitemMapManager::removeReferenceSceneSource(const char *sceneName)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(sceneName);
	deleteSceneInternalItem(source.Get());
	obs_source_remove(source);
}

void PLSSceneitemMapManager::addReferenceScene(const char *sceneName)
{
	referenceSceneSourcesSet.insert(sceneName);
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
	for (auto iter = referenceSceneSourcesSet.begin(); iter != referenceSceneSourcesSet.end(); ++iter) {
		auto value = *iter;
		removeReferenceSceneSource(value.toStdString().c_str());
	}
	referenceSceneSourcesSet.clear();
}

bool PLSSceneitemMapManager::getDualOutputOpened()
{
	return dualOutputOpened;
}

void PLSSceneitemMapManager::addUuid(QMap<QString, DisplayId> &newSceneData, const char *sceneName, const char *uuid, int display, int64_t id, OBSSceneItem horItem, OBSSceneItem verItem)
{
	auto iter = newSceneData.find(uuid);
	if (iter == newSceneData.end()) {
		DisplayId displayId;
		setDisplayId(displayId, sceneName, display, id, horItem, verItem);
		newSceneData.insert(uuid, displayId);
	} else {
		auto &displayId = iter.value();
		setDisplayId(displayId, sceneName, display, id, horItem, verItem);
	}
}

void PLSSceneitemMapManager::setDisplayId(DisplayId &displayId, const char *sceneName, int display, int64_t id, OBSSceneItem horItem, OBSSceneItem verItem)
{
	displayId.sceneName = sceneName;
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
			PLS_INFO("PLSSceneitemMapManager", "Delete the verItem where no horItem is found");
			obs_sceneitem_remove(displayId.verticalItem);
			continue;
		} else if (0 == displayId.verticalId && displayId.horizontalItem) {
			OBSSourceAutoRelease source = obs_get_source_by_name(displayId.sceneName);
			auto scene = obs_scene_from_source(source);
			bool isGroup = obs_source_is_group(source);
			if (isGroup) {
				scene = GetCurrentScene();
			}
			auto verItem = duplicateSceneItem(scene, displayId.horizontalItem);
			if (isGroup) {
				obs_sceneitem_group_add_item(obs_sceneitem_get_group(scene, displayId.horizontalItem), verItem.second);
			}
			addConfig(displayId.sceneName, displayId.horizontalId, verItem.first);
			continue;
		}
		addConfig(displayId.sceneName, displayId.horizontalId, displayId.verticalId);
	}
}

void PLSSceneitemMapManager::setDualOutputOpened(bool open)
{
	dualOutputOpened = open;
}
