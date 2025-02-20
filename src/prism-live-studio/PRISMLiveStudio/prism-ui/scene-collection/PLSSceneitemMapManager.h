#ifndef PLSSCENITEMMAPMANAGER_H
#define PLSSCENITEMMAPMANAGER_H

#include <QSet>
#include <QMap>
#include <QVector>
#include <QString>
#include <QObject>
#include "obs.hpp"

using SceneitemIdMap = QMap<int64_t, int64_t>;      // horizontal sceneitem id : vertical sceneitem id
using SceneitemMap = QMap<QString, SceneitemIdMap>; // key:scene uuid

static int64_t SCENE_ITEM_NOT_FOUND = -1;
static int64_t SCENE_ITEM_NOT_EXISTED = -2;
static const char *SCENE_ITEM_MAP_UUID = "uuid";
static const char *SCENE_ITEM_MAP_SAVE_KEY = "sceneitemMaps";
static const char *SCENE_ITEM_MAPS_KEY = "maps";
static const char *SCENE_ITEM_MAP_SAVE_SCENE_NAME_KEY = "sceneName";
static const char *SCENE_ITEM_REFERENCE_SCENE_NAME = "referenceSceneName";
static const char *SCENE_ITEM_REFERENCE_GROUP_NAME = "referenceGroupName";
static const char *SCENE_ITEM_REFERENCE_SCENE_SOURCE = "referenceSceneSource";

enum SceneItemDisplayMode { SCENE_ITEM_DISPLAY_HORIZONTAL = 0, SCENE_ITEM_DISPLAY_VERTICAL };

struct DisplayId {
	int horizontalId = 0;
	int verticalId = 0;
	OBSSceneItem horizontalItem;
	OBSSceneItem verticalItem;
	const char *sceneName{nullptr};
};

class PLSSceneitemMapManager : public QObject {
	Q_OBJECT
public:
	static PLSSceneitemMapManager *Instance();
	~PLSSceneitemMapManager();

	void switchToDualOutputMode();
	OBSDataArray saveConfig();
	void loadConfig(OBSData data);

	void removeConfig(const char *sceneName);
	void removeItem(OBSSceneItem horItem, bool unGroup = false);
	void groupItems(const char *sceneName, const char *groupName, QVector<obs_sceneitem_t *> itemOrder);
	QVector<struct obs_sceneitem_order_info> reorderItems(OBSScene scene, QVector<struct obs_sceneitem_order_info> orderList);
	OBSSceneItem getVerticalSceneitem(OBSSceneItem horItem);
	OBSSceneItem getVerticalSelectedSceneitem(OBSSceneItem horItem);
	OBSSceneItem getHorizontalSceneitem(OBSSceneItem verItem);

	bool isMappedVerticalSceneItem(OBSSceneItem verItem);
	const char *getMappedVerticalSceneName(OBSSceneItem verItem);

	void deleteSelectedScene(OBSScene scene);
	void clearSource();

	bool getDualOutputOpened();
	void setDualOutputOpened(bool open);

	void removeReferenceSceneSource(const char *sceneName);
	void addReferenceScene(const char *sceneName);

private:
	explicit PLSSceneitemMapManager();
	void switchToDualOutputMode(OBSScene scene, QMap<QString, DisplayId> &items);

	void addConfig(SceneitemMap &itemIdMap, const char *sceneName, int64_t horizontalId, int64_t verticalId);
	void addConfig(const char *sceneName, int64_t horizontalId, int64_t verticalId);
	void addConfig(const SceneitemMap &idMap);
	void removeConfig(const char *sceneName, int64_t id);

	std::pair<int64_t, OBSSceneItem> findItemByHorizontalId(const char *sceneName, int64_t horId);
	int64_t getItemDisplayId(OBSSceneItem item);
	const char *getItemDisplayUuid(OBSSceneItem item);
	const char *getItemSourceName(OBSSceneItem item);
	int64_t findVerticalItem(const char *sceneName, int64_t id);

	std::pair<int64_t, OBSSceneItem> duplicateSceneItem(OBSScene scene, OBSSceneItem item);
	void removeGroupSelectedStatus();
	void addUuid(QMap<QString, DisplayId> &newSceneData, const char *sceneName, const char *uuid, int display, int64_t id, OBSSceneItem horItem, OBSSceneItem verItem);
	void setDisplayId(DisplayId &displayId, const char *sceneName, int display, int64_t id, OBSSceneItem horItem, OBSSceneItem verItem);
	void addUuidConfig(const QMap<QString, DisplayId> &ungroupItems);
signals:
	void duplicateItemSuccess(OBSSceneItem horItem, OBSSceneItem verItem);

private:
	SceneitemMap sceneitemMap;
	bool dualOutputOpened = false;

	QSet<QString> referenceSceneSourcesSet;
};
#define PLSSceneitemMapMgrInstance PLSSceneitemMapManager::Instance()

#endif // PLSSCENITEMMAPMANAGER_H
