#include "PLSAudioMixer.h"
#include "pls/pls-properties.h"
#include "liblog.h"

static const char *module_name()
{
	return "PLSMixerOrderHelper";
}

PLSMixerOrderHelper::PLSMixerOrderHelper()
{
	signalHandlers.reserve(signalHandlers.size() + 4);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_destroy", PLSMixerOrderHelper::SourceDestroy, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_rename", PLSMixerOrderHelper::SourceRenamed, this);

	signalHandlers.emplace_back(obs_get_signal_handler(), "source_create", PLSMixerOrderHelper::SourceCreated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_remove", PLSMixerOrderHelper::SourceRemoved, this);
}

PLSMixerOrderHelper::~PLSMixerOrderHelper() {}

void PLSMixerOrderHelper::Load(obs_data_t *data)
{
	if (!data)
		return;

	source_uuids.clear();
	m_orderList.clear();
	if (obs_data_has_user_value(data, "mixer_order")) {
		OBSDataArrayAutoRelease array = obs_data_get_array(data, "mixer_order");
		size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			OBSDataAutoRelease item = obs_data_array_item(array, i);
			const char *uuid = obs_data_get_string(item, "scene_uuid");
			if (uuid && *uuid) {
				OBSDataArrayAutoRelease order_array = obs_data_get_array(item, "order");
				LoadSourceOrder(uuid, order_array);
			}
		}
	}
}

static void generateObj(obs_data_t *obj, const std::pair<std::string, MixerList> &item)
{
	OBSDataArrayAutoRelease order = obs_data_array_create();
	std::set<std::string> scene_source_uuids;
	for (size_t i = 0, count = item.second.size(); i < count; i++) {
		if (scene_source_uuids.find(item.second[i].uuid) != scene_source_uuids.end()) {
			assert(false);
			PLS_INFO(module_name(), "Trying to push a repeated source: '%s', uuid: '%s' into list.", //
				 item.second[i].name.c_str(), item.second[i].uuid.c_str());
			continue;
		}
		scene_source_uuids.insert(item.second[i].uuid);
		OBSDataAutoRelease orderObj = obs_data_create();
		obs_data_set_string(orderObj, "source_name", item.second[i].name.c_str());
		obs_data_set_string(orderObj, "source_uuid", item.second[i].uuid.c_str());
		obs_data_array_push_back(order, orderObj);
	}

	obs_data_set_string(obj, "scene_uuid", item.first.c_str());
	obs_data_set_array(obj, "order", order);
}

void PLSMixerOrderHelper::Save(obs_data_t *root_obj)
{
	if (!root_obj)
		return;

	std::lock_guard locker(mutex);
	OBSDataArrayAutoRelease mixer_order = obs_data_array_create();
	for (const auto &item : m_orderList) {
		OBSDataAutoRelease obj = obs_data_create();
		generateObj(obj, item);
		obs_data_array_push_back(mixer_order, obj);
	}

	obs_data_set_array(root_obj, "mixer_order", mixer_order);
}

void PLSMixerOrderHelper::AddScene(const std::string &scene_uuid)
{
	std::lock_guard locker(mutex);
	if (m_orderList.find(scene_uuid) == m_orderList.end()) {
		m_orderList.insert({scene_uuid, {}});
		auto &list = m_orderList[scene_uuid];
		/* insert global audio sources into the scene. */
		if (!globalAudio.empty()) {
			for (const auto &[_, obj] : globalAudio) {
				QString name = QString::fromStdString(obj.name);
				auto finder = [name](const SourceObject &obj) { return name.localeAwareCompare(QString::fromStdString(obj.name)) < 0; };
				auto found_at = std::find_if(list.begin(), list.end(), finder);

				list.insert(found_at, {obj.name, obj.uuid});
			}
		}
	}
}

void PLSMixerOrderHelper::RemoveScene(const std::string &scene_uuid)
{
	std::lock_guard locker(mutex);
	if (auto iter = m_orderList.find(scene_uuid); iter != m_orderList.end()) {
		m_orderList.erase(iter);
	}
}

/* When add a UUID for the first time, put it in the list by dictionary order*/
void PLSMixerOrderHelper::Insert(const std::string &scene_uuid, const std::string &source_uuid, const std::string &sourceName)
{
	std::lock_guard locker(mutex);

	if (scene_uuid.empty()) {
		if (auto iter = globalAudio.find(source_uuid); iter == globalAudio.end()) {
			globalAudio.insert({source_uuid, {sourceName, source_uuid}});
		}
	}

	if (source_uuids.find(source_uuid) == source_uuids.end()) {
		if (scene_uuid.empty()) {
			/* insert global audio device for all scenes */
			for (auto &[_, list] : m_orderList) {
				QString name = QString::fromStdString(sourceName);
				auto finder = [name](const SourceObject &obj) { return name.localeAwareCompare(QString::fromStdString(obj.name)) < 0; };
				auto found_at = std::find_if(list.begin(), list.end(), finder);

				list.insert(found_at, {sourceName, source_uuid});
			}
			source_uuids.insert(source_uuid);
		} else {
			auto &list = m_orderList[scene_uuid];
			QString name = QString::fromStdString(sourceName);
			auto finder = [name](const SourceObject &obj) { return name.localeAwareCompare(QString::fromStdString(obj.name)) < 0; };
			auto found_at = std::find_if(list.begin(), list.end(), finder);

			list.insert(found_at, {sourceName, source_uuid});
			source_uuids.insert(source_uuid);
		}
	}
}

void PLSMixerOrderHelper::Remove(const std::string &scene_uuid, const std::string &source_uuid, bool isGlobalAudio)
{
	std::lock_guard locker(mutex);
	if (!scene_uuid.empty()) {
		auto iter = m_orderList.find(scene_uuid);
		if (iter != m_orderList.end()) {
			/* remove all the elements by scene_uuid*/
			if (source_uuid.empty())
				m_orderList.erase(iter);
			else {
				/* remove a specific elements by scene_uuid and source_uuid*/
				auto order_iter = iter->second.begin();
				for (; order_iter < iter->second.end(); order_iter++) {
					if (source_uuid == (*order_iter).uuid) {
						iter->second.erase(order_iter);
						break;
					}
				}
			}
		}
	} else if (!source_uuid.empty()) {

		/* remove global audio from globalAudio*/
		if (isGlobalAudio) {
			auto iter = globalAudio.find(source_uuid);
			if (iter != globalAudio.end()) {
				globalAudio.erase(iter);
			}
		}

		for (auto &list : m_orderList) {
			auto iter = list.second.begin();
			bool found = false;
			for (; iter < list.second.end(); iter++) {
				if ((*iter).uuid == source_uuid) {
					list.second.erase(iter);
					found = true;
					break;
				}
			}
			if (found && !isGlobalAudio) {
				RemoveFromSourceSet(source_uuid);
				return;
			}
			/* continue to remove global audio from other scenes inside of m_orderList*/
		}
	}

	RemoveFromSourceSet(source_uuid);
}

void PLSMixerOrderHelper::UpdateName(const std::string &scene_uuid, const std::string &source_uuid, const std::string &newName, bool isGlobalAudio)
{
	std::lock_guard locker(mutex);
	if (!scene_uuid.empty() && !source_uuid.empty()) {
		auto it = m_orderList.find(scene_uuid);
		if (it != m_orderList.end()) {
			auto &list = (*it).second;
			for (auto &item : list) {
				if (item.uuid == source_uuid) {
					item.name = newName;
					break;
				}
			}
		}
	} else if (!source_uuid.empty()) {
		/* rename global audio source's name in globalAudio*/
		if (isGlobalAudio) {
			auto iter = globalAudio.find(source_uuid);
			if (iter != globalAudio.end()) {
				(*iter).second.name = newName;
			}
		}

		for (auto &list : m_orderList) {
			bool found = false;
			auto iter = list.second.begin();
			for (; iter < list.second.end(); iter++) {
				if ((*iter).uuid == source_uuid) {
					(*iter).name = newName;
					found = true;
					break;
				}
			}

			if (found && !isGlobalAudio)
				return;

			/* continue to rename global source from other scenes inside of m_orderList*/
		}
	}
}

void PLSMixerOrderHelper::UpdateOrder(const std::string &scene_uuid, MixerList &&newOrder)
{
	std::lock_guard locker(mutex);
	auto it = m_orderList.find(scene_uuid);
	if (it != m_orderList.end()) {
		size_t count = (*it).second.size();
		if (newOrder.size() == count)
			(*it).second = std::move(newOrder);
		else if (newOrder.size() < count) {
			// do merge
			MixerList temp = newOrder;
			MixerList notFound;
			int tail = temp.size() - 1;
			auto find_func = [&temp, &tail, &notFound](const SourceObject &obj) {
				bool found = false;
				for (int i = 0; i <= tail; i++) {
					auto uuid = temp[i].uuid;
					if (uuid == obj.uuid) {
						// swap
						SourceObject tempObj = temp[i];
						temp[i] = temp[tail];
						temp[tail] = tempObj;
						tail--;
						found = true;
						break;
					}
				}

				if (!found) {
					notFound.emplace_back(obj);
				}
			};
			std::for_each((*it).second.begin(), (*it).second.end(), find_func);
			newOrder.insert(newOrder.end(), notFound.begin(), notFound.end());
			(*it).second = std::move(newOrder);
		} else
			assert(false);
	}
}

void PLSMixerOrderHelper::Reorder(const std::string &scene_uuid, std::vector<VolControl *> &volumes)
{
	/* Sort volumes by orders in m_orderList */
	auto find_in_volumes = [](const SourceObject &obj, std::vector<VolControl *> &list, size_t start_pos) {
		VolControl *temp = nullptr;

		for (auto it = list.begin() + start_pos; it < list.end(); it++) {
			VolControl *vol = *it;
			if (0 == strcmp(obs_source_get_uuid(vol->GetSource()), obj.uuid.c_str())) {
				list.erase(it);
				return vol;
			}
		}

		return temp;
	};

	std::lock_guard locker(mutex);
	if (!scene_uuid.empty() && m_orderList.find(scene_uuid) != m_orderList.end()) {
		const auto &order = m_orderList[scene_uuid];
		size_t start_pos = 0;
		for (const auto &item : order) {
			if (start_pos >= volumes.size())
				break;

			if (auto vol = find_in_volumes(item, volumes, start_pos); vol != nullptr) {
				volumes.insert(volumes.begin() + start_pos, vol);
				start_pos++;
			}
		}
	}
}

void PLSMixerOrderHelper::RemoveFromSourceSet(const std::string &source_uuid)
{
	auto iter = source_uuids.find(source_uuid);
	if (iter != source_uuids.end()) {
		source_uuids.erase(iter);
	}
}

void PLSMixerOrderHelper::SourceDestroy(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (uint32_t output_flags = obs_source_get_output_flags(source); output_flags & OBS_SOURCE_AUDIO) {

		auto context = static_cast<PLSMixerOrderHelper *>(data);
		if (!context)
			return;

		const char *uuid = obs_source_get_uuid(source);
		bool globalAudio = obs_source_get_flags(source) & DEFAULT_AUDIO_DEVICE_FLAG;
		if (uuid && *uuid) {
			context->Remove("", uuid, globalAudio);
		}
	}
}

void PLSMixerOrderHelper::SourceRenamed(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (uint32_t output_flags = obs_source_get_output_flags(source); output_flags & OBS_SOURCE_AUDIO) {

		auto context = static_cast<PLSMixerOrderHelper *>(data);
		if (!context)
			return;

		const char *uuid = obs_source_get_uuid(source);
		const char *name = obs_source_get_name(source);
		bool globalAudio = obs_source_get_flags(source) & DEFAULT_AUDIO_DEVICE_FLAG;
		if (uuid && *uuid && name && *name) {
			context->UpdateName("", uuid, name, globalAudio);
		}
	}
}

void PLSMixerOrderHelper::SourceCreated(void *data, calldata_t *params)
{
	auto context = static_cast<PLSMixerOrderHelper *>(data);
	if (!context)
		return;

	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_scene_from_source(source) != NULL) {
		const char *uuid = obs_source_get_uuid(source);
		if (uuid && *uuid) {
			context->AddScene(uuid);
		}
	}
}

void PLSMixerOrderHelper::SourceRemoved(void *data, calldata_t *params)
{
	auto context = static_cast<PLSMixerOrderHelper *>(data);
	if (!context)
		return;

	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_scene_from_source(source) != NULL) {
		const char *uuid = obs_source_get_uuid(source);
		if (uuid && *uuid) {
			context->RemoveScene(uuid);
		}
	}
}

void PLSMixerOrderHelper::LoadSourceOrder(const char *key, obs_data_array_t *array)
{
	size_t count = obs_data_array_count(array);
	if (count > 0) {
		m_orderList.insert(std::make_pair<std::string, MixerList>(key, {}));
		auto &list = m_orderList[key];
		std::set<std::string> scene_source_uuids;
		for (size_t i = 0; i < count; i++) {
			OBSDataAutoRelease obj = obs_data_array_item(array, i);
			const char *uuid = obs_data_get_string(obj, "source_uuid");
			const char *name = obs_data_get_string(obj, "source_name");
			if (uuid && *uuid && name && *name) {
				source_uuids.insert(uuid);
				if (scene_source_uuids.find(uuid) == scene_source_uuids.end()) {
					scene_source_uuids.insert(uuid);
					list.push_back({name, uuid});
				} else {
					assert(false && "Trying to push a repeated source into list.");
					PLS_INFO(module_name(), "Trying to push a repeated source: '%s', uuid: '%s' into list.", name, uuid);
				}
			}
		}
	}
}
