#include "monitor-duplicator-pool.h"
#include "obs.h"
#include <liblog.h>

PLSMonitorDuplicatorPool *PLSMonitorDuplicatorPool::get_instance()
{
	static PLSMonitorDuplicatorPool pls_pool;
	return &pls_pool;
}

void PLSMonitorDuplicatorPool::clear()
{
	std::lock_guard<std::recursive_mutex> auto_lock(mtx);

	for (auto item : duplicator_vector) {
		item->set_removed();
	}

	duplicator_vector.clear();
}

DUPLICATOR_PTR PLSMonitorDuplicatorPool::get_duplicator(int adapter_index, int output_index, int display_id, bool enable_dif_adapter)
{
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi)) {
		PLS_ERROR("monitor capture", "Failed to get obs video info for creating duplicator");
		return DUPLICATOR_PTR();
	}

	DuplicatorType type;
	if (adapter_index == ovi.adapter) {
		type = DuplicatorType::DUPLICATOR_SHARED_HANDLE;
	} else {
		type = DuplicatorType::DUPLICATOR_MEMORY_COPY;
		if (!enable_dif_adapter) {
			return DUPLICATOR_PTR();
		}
	}

	//--------------------------------------------------------------------
	std::lock_guard<std::recursive_mutex> auto_lock(mtx);

	for (auto item : duplicator_vector) {
		if (item->get_adapter_id() == adapter_index && item->get_monitor_dev_id() == output_index) {
			if (!display_id || item->get_hardware_id() == display_id) {
				return item;
			} else {
				clear();
				break;
			}
		}
	}

	auto duplicator = DUPLICATOR_PTR(std::make_shared<PLSDuplicatorInstance>(type, adapter_index, output_index, display_id));
	duplicator_vector.push_back(duplicator);

	return duplicator;
}
