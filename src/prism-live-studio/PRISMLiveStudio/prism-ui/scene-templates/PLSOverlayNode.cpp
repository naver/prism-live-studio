#include "PLSOverlayNode.h"
#include "PLSNodeManager.h"
#include "libutils-api.h"
#include "liblog.h"
#include "frontend-api.h"
#include "window-basic-main.hpp"
#include "PLSSceneDataMgr.h"
#include "PLSSceneitemMapManager.h"
#include "pls/pls-dual-output.h"

#include "pls-common-define.hpp"
using namespace common;

static const char *overlayNodeParserModuleName = "PLSOverlayNode";

PLSBaseNode::PLSBaseNode(const QString &sourceId_) : sourceId(sourceId_) {}

NodeErrorType PLSBaseNode::load(const QJsonObject &content)
{
	outputObject["width"] = content["width"];
	outputObject["height"] = content["height"];
	outputObject["settings"] = content["settings"];
	outputObject["private_settings"] = content["private_settings"];
	return NodeErrorType::Ok;
}

bool PLSBaseNode::checkSourceRegistered()
{
	if (sourceId.isEmpty()) {
		return true; // do not need check
	}
	return (nullptr != obs_source_get_display_name(sourceId.toStdString().c_str()));
}

bool PLSBaseNode::checkHasUpdate()
{
	if (sourceId.isEmpty()) {
		return false;
	}
	return PLSNodeManagerPtr->checkSourceHasUpgrade(sourceId);
}

void PLSBaseNode::setForceUpdateSource(bool update)
{
	forceUpdateSource = update;
}

void PLSBaseNode::save(QJsonObject &output)
{
	if (forceUpdateSource && checkHasUpdate()) {
		outputObject["id"] = PLSNodeManagerPtr->getSourceUpgradeId(sourceId);
		outputObject["updated"] = true;
		outputObject["oldId"] = sourceId;
	} else {
		outputObject["id"] = sourceId;
	}

	output = outputObject;
}

void PLSBaseNode::clear()
{
	forceUpdateSource = false;
	outputObject = QJsonObject();
}

bool PLSBaseNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	auto settingsContent = obs_data_get_json(settings);
	if (!pls_is_empty(settingsContent)) {
		QByteArray array(settingsContent);
		QJsonDocument doc = QJsonDocument::fromJson(array);
		output["settings"] = doc.object();
	}
	auto priSettingsContent = obs_data_get_json(priSettings);
	if (!pls_is_empty(priSettingsContent)) {
		QByteArray array(priSettingsContent);
		QJsonDocument doc = QJsonDocument::fromJson(array);
		output["private_settings"] = doc.object();
	}
	return true;
}

QString PLSBaseNode::getSourceId()
{
	return sourceId;
}

NodeErrorType PLSGameCaptureNode::load(const QJsonObject &content)
{
	PLSBaseNode::load(content);

	placeholderFile = content["placeholderFile"].toString();
	if (!placeholderFile.isEmpty()) {
		placeholderFile = PLSNodeManagerPtr->getTemplatesPath().append(placeholderFile);
	}
	width = content["width"].toInt();
	height = content["height"].toInt();

	return NodeErrorType::Ok;
}

void PLSGameCaptureNode::save(QJsonObject &output)
{
	PLSBaseNode::save(output);

	outputObject["auto_placeholder_image"] = placeholderFile;
	outputObject["width"] = width;
	outputObject["height"] = height;
	output = outputObject;
}

bool PLSGameCaptureNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);

	QJsonObject settingsObj = output["settings"].toObject();
	settingsObj["width"] = width;
	settingsObj["height"] = height;
	settingsObj["auto_placeholder_image"] = placeholderFile;
	output["settings"] = settingsObj;
	return true;
}

NodeErrorType PLSMediaNode::load(const QJsonObject &content)
{
	outputObject = content["settings"].toObject();
	outputObject["private_settings"] = content["private_settings"];

	QString filename = content["filename"].toString();
	if (!filename.isEmpty()) {
		localFile = PLSNodeManagerPtr->getTemplatesPath().append(filename);
	} else {
		localFile.clear();
	}

	return NodeErrorType::Ok;
}

void PLSMediaNode::save(QJsonObject &output)
{
	PLSBaseNode::save(output);

	QJsonObject outObject;
	outputObject["local_file"] = localFile;
	outObject["settings"] = outputObject;
	outObject["id"] = outputObject["id"];
	outObject["loop"] = true;
	output = outObject;
}

bool PLSMediaNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);

	QString localFile = obs_data_get_string(settings, "local_file");
	auto name = pls_get_path_file_name(localFile);
	if (!localFile.isEmpty()) {
		QString copyToPath = PLSNodeManagerPtr->getExportPath();
		PLSNodeManagerPtr->doCopy(localFile, copyToPath.append(name));
	}

	output["filename"] = name;

	return true;
}

NodeErrorType PLSScenesNode::load(const QJsonObject &scenesObject)
{
	schemaVersion = scenesObject["schemaVersion"].toString();

	QJsonArray scenesItemArray = scenesObject["items"].toArray();
	int count = scenesItemArray.count();
	for (int i = 0; i < scenesItemArray.count(); i++) {

		QJsonObject itemObject = scenesItemArray[i].toObject();

		SceneInfo sceneInfo;
		sceneInfo.name = itemObject["name"].toString();
		sceneInfo.sceneId = itemObject["sceneId"].toString();

		QJsonObject slotObject = itemObject["slots"].toObject();
		QString nodeType = slotObject["nodeType"].toString();

		QJsonObject outputObject;
		if (auto res = PLSNodeManagerPtr->loadNodeInfo(nodeType, slotObject, outputObject); res != NodeErrorType::Ok) {
			clear();
			return res;
		}

		sceneInfo.scenesInfo = outputObject;
		sceneInfoVec.push_back(sceneInfo);
	}
	return NodeErrorType::Ok;
}

void PLSScenesNode::save(QJsonObject &output)
{
	QJsonObject outputObject;

	QJsonArray sources;
	QJsonArray scenesOrder;
	QJsonArray groups;
	QString sceneName;
	QMap<QString, QString> sourceNames;
	for (int i = 0; i < sceneInfoVec.count(); i++) {
		SceneInfo sceneInfo = sceneInfoVec[i];

		QJsonObject sceneObject;
		sceneObject["id"] = SCENE_SOURCE_ID;
		sceneObject["name"] = sceneInfo.name;

		if (sceneName.isEmpty()) {
			sceneName = sceneInfo.name;
		}

		QJsonObject order;
		order["name"] = sceneInfo.name;
		scenesOrder.push_back(order);

		QJsonObject sceneItemObject = sceneInfo.scenesInfo;
		sceneItemObject.remove("sources");
		sceneItemObject.remove("groups");

		// check in group
		QJsonArray items = sceneItemObject["items"].toArray();
		for (int i = 0; i < items.count(); i++) {
			QJsonObject itemObject = items[i].toObject();
			QString sceneitemId = itemObject["sceneItemId"].toString();
			itemObject["group_item_backup"] = PLSNodeManagerPtr->getSceneItemInGroup(sceneitemId);
			items[i] = itemObject;
		}
		sceneItemObject["items"] = items;
		sceneObject["settings"] = sceneItemObject;
		sources.push_back(sceneObject);

		QJsonArray sourceArray = sceneInfo.scenesInfo["sources"].toArray();
		for (int i = 0; i < sourceArray.count(); i++) {
			QJsonObject sourceObject = sourceArray[i].toObject();
			if (sourceObject.isEmpty()) {
				continue;
			}

			QJsonObject object = sourceObject["sources"].toObject();
			if (object.isEmpty()) {
				continue;
			}

			if (object["id"].toString() == SCENE_SOURCE_ID) {
				continue;
			}
			QString name = sourceObject["name"].toString();
			auto iter = sourceNames.find(name);
			if (iter != sourceNames.end()) {
				continue;
			}

			sourceNames.insert(name, name);
			object["name"] = name;
			sources.push_back(object);
		}

		//group array
		QJsonArray groupArray = sceneInfo.scenesInfo["groups"].toArray();
		int coutn = groupArray.count();
		for (int i = 0; i < groupArray.count(); i++) {
			QJsonObject groupObject = groupArray[i].toObject()["groups"].toObject();
			if (groupObject.isEmpty()) {
				continue;
			}

			QJsonArray childrenArray = groupObject["childrenIds"].toArray();
			groupObject.remove("childrenIds");
			QJsonArray items;
			QMap<QString, QString> pushedObjs;
			for (int i = 0; i < childrenArray.count(); i++) {
				QString sceneItemId = childrenArray[i].toString();

				QJsonObject object = PLSNodeManagerPtr->getSceneItemsInfo(sceneItemId);
				if (object.isEmpty()) {
					continue;
				}
				auto iter = pushedObjs.find(sceneItemId);
				if (iter != pushedObjs.end()) {
					continue;
				}
				items.push_front(object);
				pushedObjs.insert(sceneItemId, sceneItemId);
			}
			QJsonObject settings;
			settings["items"] = items;
			settings["cx"] = groupObject["cx"];
			settings["cy"] = groupObject["cy"];
			settings["id_counter"] = groupObject["id_counter"];
			settings["custom_size"] = groupObject["custom_size"];
			groupObject["settings"] = settings;

			groups.push_back(groupObject);
		}
	}

	outputObject["sources"] = sources;
	outputObject["scene_order"] = scenesOrder;
	outputObject["current_scene"] = sceneName;
	outputObject["current_program_scene"] = sceneName;
	outputObject["groups"] = groups;

	output = outputObject;
	sceneInfoVec.clear();
}

static void writeFilter(obs_source_t *, obs_source_t *filter, void *param)
{

	QJsonArray &itemArray = *static_cast<QJsonArray *>(param);

	QJsonObject itemObject;
	const char *name = obs_source_get_name(filter);
	itemObject["name"] = name;

	const char *id = obs_source_get_id(filter);
	itemObject["type"] = id;

	OBSData settings = pls_get_source_setting(filter);

	QByteArray array(obs_data_get_json(settings));
	QJsonDocument doc = QJsonDocument::fromJson(array);
	itemObject["settings"] = doc.object();

	itemArray.push_back(itemObject);
}

static bool writeSceneItemInGroup(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	QJsonArray &itemArray = *static_cast<QJsonArray *>(param);

	QString uuid = PLSNodeManagerPtr->getSceneItemUuidInfo((int64_t)item);
	itemArray.push_front(uuid);

	return true;
}

static bool writeSceneItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (pls_is_vertical_sceneitem(item)) {
		return true;
	}

	QJsonArray &itemArray = *static_cast<QJsonArray *>(param);

	QJsonObject itemObject;
	obs_source_t *source = obs_sceneitem_get_source(item);
	OBSData settings = pls_get_source_setting(source);
	OBSData priSettings = pls_get_source_private_setting(source);
	PLSNodeManagerPtr->exportLoadInfo(settings, priSettings, SNodeType::SceneItemNode, itemObject, item);

	bool group = obs_sceneitem_is_group(item);
	if (group) {
		itemObject["sceneNodeType"] = PLSNodeManagerPtr->sceneNodeTypeValueToKey(PLSNodeManager::SceneNodeType::Folder).toLower();

		QJsonArray groupArray;
		if (obs_sceneitem_is_group(item)) {
			obs_sceneitem_group_enum_items(item, writeSceneItem, &itemArray);
			obs_sceneitem_group_enum_items(item, writeSceneItemInGroup, &groupArray);

			OBSData settings = pls_get_source_setting(source);
			itemObject["custom_size"] = obs_data_get_bool(settings, "custom_size");
			itemObject["id_counter"] = obs_data_get_int(settings, "id_counter");

			if (obs_data_get_bool(settings, "custom_size")) {
				itemObject["cx"] = obs_data_get_int(settings, "cx");
				itemObject["cy"] = obs_data_get_int(settings, "cy");
			}
		}
		itemObject["childrenIds"] = groupArray;
		itemArray.push_back(itemObject);
		return true;
	} else {
		itemObject["sceneNodeType"] = PLSNodeManagerPtr->sceneNodeTypeValueToKey(PLSNodeManager::SceneNodeType::Item).toLower();
	}

	QJsonObject contentObject;
	const char *id = obs_source_get_unversioned_id(source);
	SNodeType nodeType = PLSNodeManagerPtr->getNodeTypeById(id);
	if (nodeType == SNodeType::UndefinedNode) {
		id = obs_source_get_id(source);
		nodeType = PLSNodeManagerPtr->getNodeTypeById(id);
	}

	contentObject["nodeType"] = PLSNodeManagerPtr->nodeTypeValueToKey(nodeType);
	if (nodeType == SNodeType::SceneSourceNode) {
		contentObject["sceneId"] = PLSNodeManagerPtr->getSceneUuidInfo(obs_source_get_name(source));
	}

	PLSNodeManagerPtr->exportLoadInfo(settings, priSettings, nodeType, contentObject);
	auto width = obs_source_get_width(source);
	auto height = obs_source_get_height(source);
	contentObject["width"] = QJsonValue::fromVariant(width);
	contentObject["height"] = QJsonValue::fromVariant(height);

	QJsonArray filterArray;
	obs_source_enum_filters(source, writeFilter, &filterArray);
	itemObject["filters"] = filterArray;
	itemObject["content"] = contentObject;
	itemArray.push_back(itemObject);

	return true;
}

bool PLSScenesNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	QJsonArray sceneItemsArray;
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		const PLSSceneItemView *item = iter->second;
		if (!item) {
			continue;
		}

		QJsonObject slotObject;
		slotObject["nodeType"] = PLSNodeManagerPtr->nodeTypeValueToKey(SNodeType::SlotsNode);

		OBSScene scene = item->GetData();
		obs_source_t *source = obs_scene_get_source(scene);
		const char *name = obs_source_get_name(source);

		QJsonObject sceneObject;
		sceneObject["name"] = name;
		sceneObject["sceneId"] = PLSNodeManagerPtr->getSceneUuidInfo(name);

		QJsonObject slotInputObject;
		PLSNodeManagerPtr->exportLoadInfo(settings, priSettings, SNodeType::SlotsNode, slotInputObject, scene.Get());
		slotObject["items"] = slotInputObject["array"].toArray();
		sceneObject["slots"] = slotObject;
		sceneItemsArray.push_back(sceneObject);
	}
	output["items"] = sceneItemsArray;
	return true;
}

void PLSScenesNode::clear()
{
	PLSBaseNode::clear();
	sceneInfoVec.clear();
}

NodeErrorType PLSRootNode::load(const QJsonObject &rootObject)
{
	schemaVersion = rootObject["schemaVersion"].toString();
	nodeType = rootObject["nodeType"].toString();

	auto res = loadScenes(rootObject["scenes"].toObject());
	if (res != NodeErrorType::Ok) {
		return res;
	}
	res = loadTransitions(rootObject["transition"].toObject());
	if (res != NodeErrorType::Ok) {
		return res;
	}
	chatTemplateOutputObject = rootObject["chatTemplate"].toObject();

	return NodeErrorType::Ok;
}

void PLSRootNode::save(QJsonObject &output)
{
	QJsonObject outputOjbect = scenesOutputObject;
	QJsonObject transitionObject = transitionsOutputObject;
	transitionObject.remove("transition_duration");
	transitionObject.remove("current_transition");

	QJsonArray transitionsArray;
	transitionsArray.push_back(transitionObject);

	outputOjbect["transitions"] = transitionsArray;
	outputOjbect["transition_duration"] = transitionsOutputObject["transition_duration"];
	outputOjbect["current_transition"] = transitionsOutputObject["current_transition"];
	outputOjbect["chatTemplate"] = chatTemplateOutputObject;
	outputOjbect[FROM_SCENE_TEMPLATE] = true;

	output = outputOjbect;
}

bool PLSRootNode::doExport(obs_data_t *rootSettings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	const char *curName = config_get_string(App()->GetUserConfig(), "Basic", "SceneCollection");

	QJsonObject rootObject;
	rootObject["nodeType"] = PLSNodeManagerPtr->nodeTypeValueToKey(SNodeType::RootNode);

	QJsonObject sceneObj;
	sceneObj["nodeType"] = PLSNodeManagerPtr->nodeTypeValueToKey(SNodeType::ScenesNode);
	PLSNodeManagerPtr->exportLoadInfo(rootSettings, priSettings, SNodeType::ScenesNode, sceneObj);
	rootObject["scenes"] = sceneObj;

	OBSSourceAutoRelease transtions = obs_frontend_get_current_transition();

	QJsonObject transitionObject;
	transitionObject["name"] = obs_source_get_name(transtions);
	transitionObject["nodeType"] = PLSNodeManagerPtr->nodeTypeValueToKey(SNodeType::TransitionNode);
	transitionObject["type"] = obs_source_get_id(transtions);

	OBSDataAutoRelease settings = pls_get_source_setting(transtions);
	OBSDataAutoRelease privateSettings = pls_get_source_setting(transtions);
	QJsonObject outputObject;
	PLSNodeManagerPtr->exportLoadInfo(settings, privateSettings, SNodeType::TransitionNode, outputObject);
	transitionObject["settings"] = outputObject["settings"];
	rootObject["transition"] = transitionObject;

	QJsonObject chatTemplateObj;
	chatTemplateObj.insert("chatTemplate", pls_get_chat_template_helper_instance()->getSaveTemplate());
	rootObject["chatTemplate"] = chatTemplateObj;

	return pls_write_json(PLSNodeManagerPtr->getExportPath() + "/config.json", rootObject);
}

void PLSRootNode::clear()
{
	PLSBaseNode::clear();
	scenesOutputObject = QJsonObject();
	transitionsOutputObject = QJsonObject();
	chatTemplateOutputObject = QJsonObject();
}

NodeErrorType PLSRootNode::loadScenes(const QJsonObject &scenesObject)
{
	QString scenesNode = scenesObject["nodeType"].toString();

	auto node = PLSNodeManagerPtr->nodeTypeKeyToValue(scenesNode);
	if (node != SNodeType::ScenesNode) {
		PLS_WARN(overlayNodeParserModuleName, "get overlay scenes node config failed.");
		return NodeErrorType::NodeNotMatch;
	}
	return PLSNodeManagerPtr->loadNodeInfo(scenesNode, scenesObject, scenesOutputObject);
}

NodeErrorType PLSRootNode::loadTransitions(const QJsonObject &transitionsObject)
{
	QString transitionsNode = transitionsObject["nodeType"].toString();
	auto node = PLSNodeManagerPtr->nodeTypeKeyToValue(transitionsNode);
	if (node != SNodeType::TransitionNode) {
		PLS_WARN(overlayNodeParserModuleName, "get overlay transitions node config failed.");
		return NodeErrorType::NodeNotMatch;
	}
	return PLSNodeManagerPtr->loadNodeInfo(transitionsNode, transitionsObject, transitionsOutputObject);
}

NodeErrorType PLSSlotsNode::load(const QJsonObject &slotsObject)
{
	schemaVersion = slotsObject["schemaVersion"].toString();
	nodeType = slotsObject["nodeType"].toString();
	QVector<QJsonObject> outputObj;
	QJsonArray itemsArray = slotsObject["items"].toArray();
	for (int i = 0; i < itemsArray.count(); i++) {
		QJsonObject itemInfo = itemsArray[i].toObject();

		// distinguish whether it is a group
		QString sceneNodeType = itemInfo["sceneNodeType"].toString();
		QJsonObject outputObject;
		if (sceneNodeType == PLSNodeManagerPtr->sceneNodeTypeValueToKey(PLSNodeManager::SceneNodeType::Folder).toLower()) {
			if (auto res = PLSNodeManagerPtr->loadNodeInfo("GroupNode", itemInfo, outputObject); res != NodeErrorType::Ok) {
				clear();
				return res;
			}
		} else if (sceneNodeType == PLSNodeManagerPtr->sceneNodeTypeValueToKey(PLSNodeManager::SceneNodeType::Item).toLower()) {
			if (auto res = PLSNodeManagerPtr->loadNodeInfo("SceneItemNode", itemInfo, outputObject); res != NodeErrorType::Ok) {
				clear();
				return res;
			}
		}
		PLSNodeManagerPtr->addSceneItemsInfo(outputObject["sceneItemId"].toString(), outputObject);
		outputObj.push_back(outputObject);
	}
	outputObjects = outputObj;
	return NodeErrorType::Ok;
}

void PLSSlotsNode::save(QJsonObject &output)
{
	QJsonArray items;
	QJsonArray sources;
	QJsonArray groups;
	for (int i = 0; i < outputObjects.count(); i++) {
		QJsonObject object = outputObjects[i];
		if (object.isEmpty()) {
			continue;
		}
		QJsonObject settingObject;
		settingObject["name"] = object["name"].toString();
		settingObject["sources"] = object["sources"].toObject();

		object.remove("sources");

		if (object.contains("groups")) {
			QJsonObject groupObject;
			groupObject["groups"] = object["groups"].toObject();
			object.remove("groups");
			groups.push_back(groupObject);
		}
		sources.push_back(settingObject);
		items.push_back(object);
	}
	output["items"] = items;
	output["sources"] = sources;
	output["groups"] = groups;
}

bool PLSSlotsNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	auto scene = static_cast<obs_scene_t *>(param);
	if (!scene) {
		return false;
	}

	QJsonArray slotsArray;
	obs_scene_enum_items(scene, writeSceneItem, &slotsArray);
	output["array"] = slotsArray;
	return true;
}

void PLSSlotsNode::clear()
{
	PLSBaseNode::clear();
	outputObjects.clear();
}

NodeErrorType PLSSceneItemNode::load(const QJsonObject &itemInfo)
{
	id = itemInfo["id"].toString();
	sceneNodeType = itemInfo["sceneNodeType"].toString();
	name = itemInfo["name"].toString();
	display = itemInfo["display"].toString();
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
	uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
	x = itemInfo["x"].toDouble() * cx;
	y = itemInfo["y"].toDouble() * cy;
	scaleX = itemInfo["scaleX"].toDouble() * cx;
	scaleY = itemInfo["scaleY"].toDouble() * cy;

	rotation = itemInfo["rotation"].toDouble();
	mixerHidden = itemInfo["mixerHidden"].toBool();
	if (itemInfo.contains("visible")) {
		visible = itemInfo["visible"].toBool();
	}
	if (itemInfo.contains("align")) {
		align = itemInfo["align"].toInt();
	}
	if (itemInfo.contains("bounds_type")) {
		boundsType = itemInfo["bounds_type"].toInt();
	}
	if (itemInfo.contains("bounds_align")) {
		boundsAlign = itemInfo["bounds_align"].toInt();
	}
	if (itemInfo.contains("bounds")) {
		boundsX = itemInfo["bounds"].toObject().value("x").toDouble();
		boundsY = itemInfo["bounds"].toObject().value("y").toDouble();
	}

	QJsonObject cropObject = itemInfo["crop"].toObject();
	struct CropInfo cropInfo;
	cropInfo.top = cropObject["top"].toInt();
	cropInfo.bottom = cropObject["bottom"].toInt();
	cropInfo.left = cropObject["left"].toInt();
	cropInfo.right = cropObject["right"].toInt();
	crop = cropInfo;

	filters = itemInfo["filters"].toArray();

	QJsonObject contentObject = itemInfo["content"].toObject();
	struct ContentInfo contentInfo;

	contentInfo.schemaVersion = contentObject["schemaVersion"].toString();
	contentInfo.nodeType = contentObject["nodeType"].toString();
	content = contentInfo;

	return PLSNodeManagerPtr->loadNodeInfo(content.nodeType, contentObject, outputSettings);
}

void PLSSceneItemNode::save(QJsonObject &outObject)
{
	// check source update
	bool hasUpdated = outputSettings["updated"].toBool();
	if (hasUpdated) {
		auto width = outputSettings["width"].toInt();
		auto height = outputSettings["height"].toInt();
		auto oldId = outputSettings["oldId"].toString();
		if (0 == width || 0 == height) {
			auto info = PLSNodeManagerPtr->getSourceUpgradeDefaultInfo(oldId);
			width = info.width;
			height = info.height;
		}
		auto newId = PLSNodeManagerPtr->getSourceUpgradeId(oldId);
		auto info = PLSNodeManagerPtr->getSourceUpgradeDefaultInfo(newId);
		float scale = (float)width / (float)info.width;
		scaleX = (float)scaleX * scale;
		scaleY = (float)scaleY * scale;
	}

	QJsonObject outputItemObjects;

	outputItemObjects["name"] = name;
	outputItemObjects["sceneItemId"] = id;

	QJsonObject posObject = {{"x", x}, {"y", y}};
	outputItemObjects["pos"] = posObject;

	QJsonObject scaleObject = {{"x", scaleX}, {"y", scaleY}};
	outputItemObjects["scale"] = scaleObject;
	outputItemObjects["mixerHidden"] = mixerHidden;
	outputItemObjects["visible"] = visible;

	outputItemObjects["crop_left"] = crop.left;
	outputItemObjects["crop_right"] = crop.right;
	outputItemObjects["crop_top"] = crop.top;
	outputItemObjects["crop_bottom"] = crop.bottom;

	outputItemObjects["rot"] = rotation;
	outputItemObjects["group"] = false;

	if (align != -1) {
		outputItemObjects["align"] = align;
	}
	if (boundsType != -1) {
		outputItemObjects["bounds_type"] = boundsType;
	}
	if (boundsAlign != -1) {
		outputItemObjects["bounds_align"] = boundsAlign;
	}

	QJsonObject bounds;
	bounds["x"] = boundsX;
	bounds["y"] = boundsY;
	outputItemObjects["bounds"] = bounds;

	outputItemObjects["private_settings"] = privateSettings;

	outObject[FROM_SCENE_TEMPLATE] = true;
	outObject = outputItemObjects;

	QJsonArray outputFilters;
	for (int i = 0; i < filters.count(); i++) {
		QJsonObject obj = filters[i].toObject();
		obj["id"] = obj["type"].toString();
		obj.remove("type");
		outputFilters.push_back(obj);
	}

	if (!outputFilters.isEmpty()) {
		outputSettings["filters"] = outputFilters;
	}
	outObject["sources"] = outputSettings;
}

bool PLSSceneItemNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &itemObject, void *param)
{
	auto item = static_cast<obs_sceneitem_t *>(param);
	if (!item) {
		return false;
	}

	OBSDataAutoRelease sceneitemSettings = obs_sceneitem_get_private_settings(item);
	auto sceneitemSettingsContent = obs_data_get_json(sceneitemSettings);
	if (!pls_is_empty(sceneitemSettingsContent)) {
		QByteArray array(sceneitemSettingsContent);
		QJsonDocument doc = QJsonDocument::fromJson(array);
		itemObject["private_settings"] = doc.object();
	}

	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	itemObject["name"] = name;
	QString uuid = PLSNodeManagerPtr->getSceneItemUuidInfo((int64_t)item);
	itemObject["id"] = uuid;

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
	uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");

	vec2 pos, scale;
	obs_sceneitem_get_pos(item, &pos);
	itemObject["x"] = pos.x / cx;
	itemObject["y"] = pos.y / cy;

	obs_sceneitem_get_scale(item, &scale);
	itemObject["scaleX"] = scale.x / cx;
	itemObject["scaleY"] = scale.y / cy;

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);
	QJsonObject cropObject;
	cropObject["top"] = crop.top;
	cropObject["bottom"] = crop.bottom;
	cropObject["left"] = crop.left;
	cropObject["right"] = crop.right;
	itemObject["crop"] = cropObject;

	float rot = obs_sceneitem_get_rot(item);
	itemObject["rotation"] = rot;
	itemObject["align"] = (int)obs_sceneitem_get_alignment(item);
	itemObject["bounds_type"] = (int)obs_sceneitem_get_bounds_type(item);
	itemObject["bounds_align"] = (int)obs_sceneitem_get_bounds_alignment(item);

	itemObject["visible"] = obs_sceneitem_visible(item);

	struct vec2 vec;
	obs_sceneitem_get_bounds(item, &vec);
	QJsonObject bounds;
	bounds["x"] = vec.x;
	bounds["y"] = vec.y;
	itemObject["bounds"] = bounds;

	itemObject[FROM_SCENE_TEMPLATE] = true;
	return true;
}

void PLSSceneItemNode::clear()
{
	outputSettings = QJsonObject();
	content = ContentInfo();
	filters = QJsonArray();
}

NodeErrorType PLSTransitionNode::load(const QJsonObject &object)
{
	QJsonObject settingsObject = object["settings"].toObject();
	QString path = settingsObject["path"].toString();
	if (!path.isEmpty()) {
		QString localFile = PLSNodeManagerPtr->getTemplatesPath().append(path);
		settingsObject["path"] = localFile;
	}

	QString name = object["name"].toString();
	if (name.isEmpty()) {
		QString type = object["type"].toString();
		outputObject["name"] = type;
		outputObject["current_transition"] = type;
	} else {
		outputObject["name"] = name;
		outputObject["current_transition"] = name;
	}

	outputObject["settings"] = settingsObject;
	outputObject["id"] = object["type"].toString();
	outputObject["transition_duration"] = object["duration"];
	return NodeErrorType::Ok;
}

void PLSTransitionNode::save(QJsonObject &output)
{
	output = outputObject;
}

bool PLSTransitionNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);

	QString file = obs_data_get_string(settings, "path");
	auto name = pls_get_path_file_name(file);
	if (!name.isEmpty()) {
		QString copyToPath = PLSNodeManagerPtr->getExportPath();
		PLSNodeManagerPtr->doCopy(file, copyToPath.append(name));
	}

	QJsonObject settingsObj = output["settings"].toObject();
	settingsObj["path"] = name;
	output["settings"] = settingsObj;
	return true;
}

NodeErrorType PLSImageNode::load(const QJsonObject &content)
{
	QString path = content["filename"].toString();
	if (!path.isEmpty()) {
		fileName = PLSNodeManagerPtr->getTemplatesPath().append(path);
	} else {
		fileName.clear();
	}
	return NodeErrorType::Ok;
}

void PLSImageNode::save(QJsonObject &output)
{
	PLSBaseNode::save(output);
	outputObject["file"] = fileName;

	QJsonObject settings;
	settings["settings"] = outputObject;
	settings["id"] = outputObject["id"];

	output = settings;
}

bool PLSImageNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	QString file = obs_data_get_string(settings, "file");
	auto name = pls_get_path_file_name(file);
	if (!name.isEmpty()) {
		QString copyToPath = PLSNodeManagerPtr->getExportPath();
		PLSNodeManagerPtr->doCopy(file, copyToPath.append(name));
	}

	output["filename"] = name;
	return true;
}

NodeErrorType PLSGroupNode::load(const QJsonObject &slotsObject)
{
	outputObjects = slotsObject;
	outputObjects["sceneItemId"] = slotsObject["id"];

	QJsonArray childrenArray = slotsObject["childrenIds"].toArray();
	for (int i = 0; i < childrenArray.count(); i++) {
		QString sceneItemId = childrenArray[i].toString();
		PLSNodeManagerPtr->setSceneItemInGroup(sceneItemId, true);
		PLS_DEBUG("PLSGroupNode", "PLSGroupNode childrenIds : %s", sceneItemId.toStdString().c_str());
	}

	return NodeErrorType::Ok;
}

void PLSGroupNode::save(QJsonObject &output)
{
	//group scene default item info
	QJsonObject outputObj;
	if (outputObjects.contains("visible")) {
		outputObj["visible"] = outputObjects["visible"];
	} else {
		outputObj["visible"] = true;
	}

	auto sceneItemId = outputObjects["sceneItemId"].toString();
	auto childrenIds = outputObjects["childrenIds"].toArray();
	if (!outputObjects.contains("x") || !outputObjects.contains("y")) {
		auto minX = 9999;
		auto maxY = 0;
		for (int i = 0; i < childrenIds.count(); i++) {
			auto data = PLSNodeManagerPtr->getSceneItemsInfo(sceneItemId);
			auto posX = data["pos"].toObject()["x"].toDouble();
			auto posY = data["pos"].toObject()["y"].toDouble();
			if (posX < minX) {
				minX = posX;
			}
			if (posY > maxY) {
				maxY = posY;
			}
		}
		QJsonObject posObject = {{"x", minX}, {"y", maxY}};
		outputObj["pos"] = posObject;
	} else {
		outputObj["mixerHidden"] = outputObjects["mixerHidden"];
		OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
		uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
		auto x = outputObjects["x"].toDouble() * cx;
		auto y = outputObjects["y"].toDouble() * cy;
		auto scaleX = outputObjects["scaleX"].toDouble() * cx;
		auto scaleY = outputObjects["scaleY"].toDouble() * cy;

		QJsonObject posObject = {{"x", x}, {"y", y}};
		outputObj["pos"] = posObject;

		QJsonObject scaleObject = {{"x", scaleX}, {"y", scaleY}};
		outputObj["scale"] = scaleObject;

		outputObj["crop_left"] = outputObjects["left"].toInt();
		outputObj["crop_right"] = outputObjects["right"].toInt();
		outputObj["crop_top"] = outputObjects["top"].toInt();
		outputObj["crop_bottom"] = outputObjects["bottom"].toInt();

		outputObj["rot"] = outputObjects["rotation"];
	}
	outputObj["group"] = true;

	QJsonObject groupOjbect;
	groupOjbect["id"] = GROUP_SOURCE_ID;
	groupOjbect["sceneItemId"] = sceneItemId;
	groupOjbect["name"] = outputObjects["name"];
	groupOjbect["childrenIds"] = outputObjects["childrenIds"];
	if (outputObjects.contains("cx") || outputObjects.contains("cy")) {
		groupOjbect["cx"] = outputObjects["cx"];
		groupOjbect["cy"] = outputObjects["cy"];
	}
	groupOjbect["id_counter"] = outputObjects["id_counter"];
	groupOjbect["custom_size"] = outputObjects["custom_size"];

	outputObj["groups"] = groupOjbect;
	outputObj["sceneItemId"] = sceneItemId;

	outputObj["name"] = outputObjects["name"];
	output = outputObj;
}

void PLSGroupNode::clear()
{
	PLSBaseNode::clear();
	outputObjects = QJsonObject();
}

NodeErrorType PLSCameraNode::load(const QJsonObject &content)
{
	QJsonObject object;
	object["width"] = content["width"];
	object["height"] = content["height"];
	object["settings"] = content["settings"];

	outputObject = object;
	return NodeErrorType::Ok;
}

NodeErrorType PLSBrowserNode::load(const QJsonObject &slotsObject)
{
	outputObject["settings"] = slotsObject["settings"];
	outputObject["private_settings"] = slotsObject["private_settings"];
	outputObject["type"] = slotsObject["type"];
	return NodeErrorType::Ok;
}

void PLSTextNode::save(QJsonObject &output)
{
	PLSBaseNode::save(output);

	outputObject["versioned_id"] = GDIP_TEXT_SOURCE_ID_V2;
	QJsonObject settings = outputObject["settings"].toObject();
	settings["custom_font"] = true;
	outputObject["settings"] = settings;

	output = outputObject;
}

bool PLSTextNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);

	QJsonObject settingsJson = output["settings"].toObject();
	settingsJson["custom_font"] = true;
	output["settings"] = settingsJson;

	return true;
}

NodeErrorType PLSSceneSourceNode::load(const QJsonObject &scenesObject)
{
	outputObject["sceneId"] = scenesObject["sceneId"];
	outputObject["width"] = scenesObject["width"];
	outputObject["height"] = scenesObject["height"];
	return NodeErrorType::Ok;
}

NodeErrorType PLSMusicPlaylistNode::load(const QJsonObject &content)
{
	QJsonObject privateSettings = content["private_settings"].toObject();
	QJsonArray playList = privateSettings["play list"].toArray();

	QJsonArray newPlayList;
	for (auto i = 0; i < playList.count(); i++) {
		auto data = playList[i].toObject();
		bool isLocalFile = data["is_local_file"].toBool();
		if (!isLocalFile) {
			newPlayList.push_back(data);
			continue;
		}
		QString url = data["music"].toString();
		if (!url.isEmpty()) {
			auto name = pls_get_path_file_name(url);
			data["music"] = PLSNodeManagerPtr->getTemplatesPath().append(name);
			newPlayList.push_back(data);
		}
	}

	privateSettings["play list"] = newPlayList;
	outputObject["private_settings"] = privateSettings;
	outputObject["settings"] = content["settings"];

	return NodeErrorType::Ok;
}

bool PLSMusicPlaylistNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);

	OBSDataArrayAutoRelease playList = obs_data_get_array(priSettings, "play list");
	auto count = obs_data_array_count(playList);
	if (count == 0) {
		return true;
	}

	bool copy = false;
	for (auto i = 0; i < count; i++) {
		auto data = obs_data_array_item(playList, i);
		bool isLocalFile = obs_data_get_bool(data, "is_local_file");
		if (!isLocalFile) {
			continue;
		}
		QString url = obs_data_get_string(data, "music");
		if (!url.isEmpty()) {
			QString copyToPath = PLSNodeManagerPtr->getExportPath();
			auto name = pls_get_path_file_name(url);
			PLSNodeManagerPtr->doCopy(url, copyToPath.append(name));
			copy = true;
		}
	}
	if (copy) {
		auto priSettingsContent = obs_data_get_json(priSettings);
		QByteArray array(priSettingsContent);
		QJsonDocument doc = QJsonDocument::fromJson(array);
		output["private_settings"] = doc.object();
	}
	return true;
}

NodeErrorType PLSPrismStickerNode::load(const QJsonObject &content)
{
	PLSBaseNode::load(content);

	QJsonObject privateSettings = outputObject["private_settings"].toObject();
	QString landscapeVideo = privateSettings["landscapeVideo"].toString();
	if (!landscapeVideo.isEmpty()) {
		landscapeVideo = PLSNodeManagerPtr->getTemplatesPath().append(landscapeVideo);
		privateSettings["landscapeVideo"] = landscapeVideo;
	}
	QString landscapeImage = privateSettings["landscapeImage"].toString();
	if (!landscapeImage.isEmpty()) {
		landscapeImage = PLSNodeManagerPtr->getTemplatesPath().append(landscapeImage);
		privateSettings["landscapeImage"] = landscapeImage;
	}
	QString portraitVideo = privateSettings["portraitVideo"].toString();
	if (!portraitVideo.isEmpty()) {
		portraitVideo = PLSNodeManagerPtr->getTemplatesPath().append(portraitVideo);
		privateSettings["portraitVideo"] = portraitVideo;
	}
	QString portraitImage = privateSettings["portraitImage"].toString();
	if (!portraitImage.isEmpty()) {
		portraitImage = PLSNodeManagerPtr->getTemplatesPath().append(portraitImage);
		privateSettings["portraitImage"] = portraitImage;
	}
	outputObject["private_settings"] = privateSettings;
	return NodeErrorType::Ok;
}

bool PLSPrismStickerNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);
	QString landscapeVideo = obs_data_get_string(priSettings, "landscapeVideo");
	QString splitKey = "prism_sticker/";
	QString path = landscapeVideo.section(splitKey, 1, 1);
	QString userPath = landscapeVideo.section(splitKey, 0, 0);
	QString dirName = path.mid(0, path.indexOf("/"));
	QString copyToPath = PLSNodeManagerPtr->getExportPath();
	PLSNodeManagerPtr->doCopy(userPath + splitKey + dirName, copyToPath + dirName);

	QString video = landscapeVideo.section(splitKey, 1, 1);
	if (!video.isEmpty()) {
		PLSNodeManagerPtr->doCopy(landscapeVideo, copyToPath + video);
	}

	QString landscapeImage = obs_data_get_string(priSettings, "landscapeImage");
	QString image = landscapeImage.section(splitKey, 1, 1);
	if (!image.isEmpty()) {
		PLSNodeManagerPtr->doCopy(landscapeImage, copyToPath + image);
	}

	QString portraitVideo = obs_data_get_string(priSettings, "portraitVideo");
	QString pVideo = portraitVideo.section(splitKey, 1, 1);
	if (!pVideo.isEmpty()) {
		PLSNodeManagerPtr->doCopy(portraitVideo, copyToPath + pVideo);
	}

	QString portraitImage = obs_data_get_string(priSettings, "portraitImage");
	QString pImage = portraitImage.section(splitKey, 1, 1);
	if (!pImage.isEmpty()) {
		PLSNodeManagerPtr->doCopy(portraitImage, copyToPath + pImage);
	}

	QJsonObject settingsObj = output["private_settings"].toObject();
	settingsObj["landscapeVideo"] = video;
	settingsObj["landscapeImage"] = image;
	settingsObj["portraitVideo"] = pVideo;
	settingsObj["portraitImage"] = pImage;
	output["private_settings"] = settingsObj;

	return true;
}

NodeErrorType PLSPrismGiphyNode::load(const QJsonObject &content)
{
	PLSBaseNode::load(content);

	QJsonObject settings = outputObject["settings"].toObject();
	QString file = settings["file"].toString();
	if (!file.isEmpty()) {
		file = PLSNodeManagerPtr->getTemplatesPath().append(file);
		settings["file"] = file;
	}
	outputObject["settings"] = settings;
	return NodeErrorType::Ok;
}

bool PLSPrismGiphyNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);
	QString file = obs_data_get_string(settings, "file");
	QString splitKey = "sticker/";
	QString path = file.section(splitKey, 1, 1);

	if (!path.isEmpty()) {
		QString copyToPath = PLSNodeManagerPtr->getExportPath();
		PLSNodeManagerPtr->doCopy(file, copyToPath + path);
	}

	QJsonObject settingsObj = output["settings"].toObject();
	settingsObj["file"] = path;
	output["settings"] = settingsObj;

	return true;
}

NodeErrorType PLSImageSlideShowNode::load(const QJsonObject &content)
{
	PLSBaseNode::load(content);
	QJsonObject settings = outputObject["settings"].toObject();
	QJsonArray files = settings["files"].toArray();
	QJsonArray newFiles;
	for (int i = 0; i < files.count(); i++) {
		QJsonObject obj = files[i].toObject();
		auto value = obj["value"].toString();
		if (!value.isEmpty()) {
			obj["value"] = PLSNodeManagerPtr->getTemplatesPath().append(value);
		}
		newFiles.push_back(obj);
	}
	settings["files"] = newFiles;
	outputObject["settings"] = settings;
	return NodeErrorType::Ok;
}

bool PLSImageSlideShowNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);
	QJsonArray filesArray;
	OBSDataArrayAutoRelease files = obs_data_get_array(settings, "files");
	auto count = obs_data_array_count(files);
	for (int i = 0; i < count; i++) {

		OBSDataAutoRelease item = obs_data_array_item(files, i);
		QString value = obs_data_get_string(item, "value");
		if (value.isEmpty()) {
			QByteArray arrayData(obs_data_get_json(item));
			QJsonDocument doc = QJsonDocument::fromJson(arrayData);
			filesArray.push_back(doc.object());
			continue;
		}
		QString copyToPath = PLSNodeManagerPtr->getExportPath();
		auto name = pls_get_path_file_name(value);
		PLSNodeManagerPtr->doCopy(value, copyToPath.append(name));
		QByteArray arrayData(obs_data_get_json(item));
		QJsonObject oldData = QJsonDocument::fromJson(arrayData).object();
		oldData["value"] = name;
		filesArray.push_back(oldData);
	}

	QJsonObject settingsObj = output["settings"].toObject();
	settingsObj["files"] = filesArray;
	output["settings"] = settingsObj;
	return true;
}

NodeErrorType PLSVlcVideoNode::load(const QJsonObject &content)
{
	PLSBaseNode::load(content);
	QJsonObject settings = outputObject["settings"].toObject();
	QJsonArray files = settings["playlist"].toArray();
	QJsonArray newFiles;
	for (int i = 0; i < files.count(); i++) {
		QJsonObject obj = files[i].toObject();
		auto value = obj["value"].toString();
		bool notFile = obj["NotAFileOrFolder"].toBool();
		if (!value.isEmpty()) {
			if (notFile) {
				obj.remove("NotAFileOrFolder");
			} else {
				obj["value"] = PLSNodeManagerPtr->getTemplatesPath().append(value);
			}
		}
		newFiles.push_back(obj);
	}
	settings["playlist"] = newFiles;
	outputObject["settings"] = settings;
	return NodeErrorType::Ok;
}

bool PLSVlcVideoNode::doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param)
{
	PLSBaseNode::doExport(settings, priSettings, output, param);
	QJsonArray filesArray;
	OBSDataArrayAutoRelease files = obs_data_get_array(settings, "playlist");
	auto count = obs_data_array_count(files);
	for (int i = 0; i < count; i++) {

		OBSDataAutoRelease item = obs_data_array_item(files, i);
		QString value = obs_data_get_string(item, "value");
		QByteArray arrayData(obs_data_get_json(item));
		QJsonDocument doc = QJsonDocument::fromJson(arrayData);
		if (value.isEmpty()) {
			filesArray.push_back(doc.object());
			continue;
		}
		QJsonObject oriData = doc.object();
		QFileInfo info(value);
		bool isFile = info.isFile();
		bool isDir = info.isDir();
		if (!isFile && !isDir) {
			oriData["NotAFileOrFolder"] = true;
		} else {
			QString copyToPath = PLSNodeManagerPtr->getExportPath();
			auto name = pls_get_path_file_name(value);
			PLSNodeManagerPtr->doCopy(value, copyToPath.append(name));
			oriData["value"] = name;
		}
		filesArray.push_back(oriData);
	}

	QJsonObject settingsObj = output["settings"].toObject();
	settingsObj["playlist"] = filesArray;
	output["settings"] = settingsObj;
	return true;
}
