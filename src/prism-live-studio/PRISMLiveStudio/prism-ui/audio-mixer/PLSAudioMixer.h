#pragma once

#include <map>
#include <string>
#include <list>
#include <set>
#include <qstring.h>
#include "obs.h"
#include "volume-control.hpp"

struct SourceObject {
	std::string name;
	std::string uuid;
};

using MixerList = std::vector<SourceObject>;                 // source uuid
using SceneMixerOrder = std::map<std::string, MixerList>; // key: scene uuid

class PLSMixerOrderHelper {

public:
	PLSMixerOrderHelper();
	~PLSMixerOrderHelper();

	void Load(obs_data_t *data);
	void Save(obs_data_t *data);

	void AddScene(const std::string &scene_uuid);
	void RemoveScene(const std::string &scene_uuid);
	/* If insert a global audio source, let scene_uuid empty */
	void Insert(const std::string &scene_uuid, const std::string &source_uuid, const std::string &sourceName);
	void Remove(const std::string &scene_uuid, const std::string &source_uuid, bool globalAudio = false);
	void UpdateName(const std::string &scene_uuid, const std::string &source_uuid, const std::string &newName, bool globalAudio = false);
	void UpdateOrder(const std::string &scene_uuid, MixerList &&newOrder);

	void Reorder(const std::string &scene_uuid, std::vector<VolControl *> &volumes);
	void RemoveFromSourceSet(const std::string &source_uuid);

private:
	static void SourceDestroy(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void SourceCreated(void *data, calldata_t *params);
	static void SourceRemoved(void *data, calldata_t *params);

	void LoadSourceOrder(const char *key, obs_data_array_t *array);
	SceneMixerOrder m_orderList;
	std::set<std::string> source_uuids;
	std::vector<OBSSignal> signalHandlers;
	/* For sources are destroyed in tiny_task_thread
	*  mutex is required to protect m_orderList
	*/
	std::recursive_mutex mutex;
	std::map<std::string, SourceObject> globalAudio;
};