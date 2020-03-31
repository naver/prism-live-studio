#include "monitor-duplicator-pool.h"
#include "graphics-duplicator.h"
#include "d3d11-duplicator.h"
#include "obs.h"
#include <log.h>

PLSMonitorDuplicatorPool::PLSMonitorDuplicatorPool() {}

PLSMonitorDuplicatorPool::~PLSMonitorDuplicatorPool() {}

void PLSMonitorDuplicatorPool::clear()
{
	std::unique_lock<std::mutex> lock(mtx);

	vector<PLSMonitorDuplicator *>::iterator iter = duplicator_vector.begin();
	for (; iter != duplicator_vector.end(); iter++) {
		PLSMonitorDuplicator *duplicator = reinterpret_cast<PLSMonitorDuplicator *>(*iter);
		delete duplicator;
		duplicator = NULL;
	}
	duplicator_vector.clear();
}

PLSMonitorDuplicatorPool *PLSMonitorDuplicatorPool::get_instance()
{
	static PLSMonitorDuplicatorPool pls_pool;
	return &pls_pool;
}

PLSMonitorDuplicator *PLSMonitorDuplicatorPool::get_duplicator(int adapter_id, int monitor_dev_id)
{
	std::unique_lock<std::mutex> lock(mtx);

	vector<PLSMonitorDuplicator *>::iterator iter = duplicator_vector.begin();
	for (; iter != duplicator_vector.end(); iter++) {
		PLSMonitorDuplicator *duplicator = reinterpret_cast<PLSMonitorDuplicator *>(*iter);
		if (duplicator->get_adapter_id() == adapter_id && duplicator->get_monitor_dev_id() == monitor_dev_id) {
			duplicator->add_ref();
			return duplicator;
		}
	}

	PLSMonitorDuplicator *duplicator = NULL;
	obs_video_info ovi;
	bool res = obs_get_video_info(&ovi);
	if (res) {
		PLS_PLUGIN_INFO("create new duplicator, adapter:%d, monitor:%d", adapter_id, monitor_dev_id);
		if (adapter_id == ovi.adapter) {
			duplicator = new (std::nothrow) PLSGraphicsDuplicator(adapter_id, monitor_dev_id);
		} else {
			duplicator = new (std::nothrow) PLSD3D11Duplicator(adapter_id, monitor_dev_id);
			res = duplicator->init_device();
			if (res)
				duplicator->capture_thread_run();
		}
		duplicator_vector.push_back(duplicator);

	} else {
		PLS_PLUGIN_ERROR("get obs video info failed");
	}
	return duplicator;
}

bool PLSMonitorDuplicatorPool::release_duplicator(int adapter_id, int monitor_dev_id)
{
	std::unique_lock<std::mutex> lock(mtx);
	vector<PLSMonitorDuplicator *>::iterator iter = duplicator_vector.begin();
	for (; iter != duplicator_vector.end(); iter++) {
		PLSMonitorDuplicator *duplicator = reinterpret_cast<PLSMonitorDuplicator *>(*iter);
		if (duplicator->get_adapter_id() == adapter_id && duplicator->get_monitor_dev_id() == monitor_dev_id) {
			bool res = duplicator->release_duplicator();
			if (res) {
				delete duplicator;
				duplicator = NULL;
				duplicator_vector.erase(iter);
				return true;
			}
		}
	}
	return false;
}
