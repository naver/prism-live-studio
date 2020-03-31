#include "monitor-duplicator.h"
#include "obs.h"
#include <util/windows/ComPtr.hpp>
#include <dxgi.h>
#include <assert.h>

PLSMonitorDuplicator::PLSMonitorDuplicator() : adapter_id(0), monitor_dev_id(0), user_reference(1), monitor_texture(NULL), texture_width(0), texture_height(0) {}

PLSMonitorDuplicator::PLSMonitorDuplicator(int adapter, int dev_id) : adapter_id(adapter), monitor_dev_id(dev_id), user_reference(1), monitor_texture(NULL), texture_width(0), texture_height(0) {}

PLSMonitorDuplicator::~PLSMonitorDuplicator()
{
	adapter_id = -1;
	monitor_dev_id = -1;
}

bool PLSMonitorDuplicator::release_duplicator()
{
	sub_ref();
	if (user_reference > 0)
		return false;
	return true;
}

bool PLSMonitorDuplicator::update_frame()
{
	return true;
}

bool PLSMonitorDuplicator::create_duplicator()
{
	return true;
}

gs_texture *PLSMonitorDuplicator::get_texture()
{
	return monitor_texture;
}

bool PLSMonitorDuplicator::is_obs_gs_duplicator()
{
	return true;
}

void PLSMonitorDuplicator::add_ref()
{
	++user_reference;
}
void PLSMonitorDuplicator::sub_ref()
{
	--user_reference;
}

bool PLSMonitorDuplicator::init_device()
{
	return true;
}

bool PLSMonitorDuplicator::capture_thread_run()
{
	return true;
}

bool PLSMonitorDuplicator::get_monitor_info(monitor_info &info)
{
	return true;
}
