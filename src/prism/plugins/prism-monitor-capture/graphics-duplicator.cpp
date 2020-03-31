#include "graphics-duplicator.h"
#include "obs.h"

PLSGraphicsDuplicator::PLSGraphicsDuplicator()
{
	duplicator = NULL;
}

PLSGraphicsDuplicator::PLSGraphicsDuplicator(int adapter, int dev_id) : PLSMonitorDuplicator(adapter, dev_id)
{
	obs_enter_graphics();
	duplicator = gs_duplicator_create(dev_id);
	obs_leave_graphics();
}

PLSGraphicsDuplicator::~PLSGraphicsDuplicator()
{
	release();
}

bool PLSGraphicsDuplicator::update_frame()
{
	if (!duplicator)
		return false;

	obs_enter_graphics();
	bool res = gs_duplicator_update_frame(duplicator);
	monitor_texture = gs_duplicator_get_texture(duplicator);
	obs_leave_graphics();
	return res;
}

bool PLSGraphicsDuplicator::create_duplicator()
{
	obs_enter_graphics();
	duplicator = gs_duplicator_create(monitor_dev_id);
	obs_leave_graphics();
	return true;
}

gs_texture *PLSGraphicsDuplicator::get_texture()
{
	return monitor_texture;
}

bool PLSGraphicsDuplicator::is_obs_gs_duplicator()
{
	return true;
}

void PLSGraphicsDuplicator::release()
{
	obs_enter_graphics();
	gs_duplicator_destroy(duplicator);
	duplicator = NULL;
	obs_leave_graphics();
}

bool PLSGraphicsDuplicator::get_monitor_info(monitor_info &info)
{
	struct gs_monitor_info gs_info = {0};
	obs_enter_graphics();
	gs_get_duplicator_monitor_info(monitor_dev_id, &gs_info);
	info.offset_x = gs_info.x;
	info.offset_y = gs_info.y;
	info.rotation = gs_info.rotation_degrees;
	info.width = gs_texture_get_width(monitor_texture);
	info.height = gs_texture_get_height(monitor_texture);
	obs_leave_graphics();
	return true;
}
