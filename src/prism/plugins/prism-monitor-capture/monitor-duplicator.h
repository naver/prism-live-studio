#pragma once
#include "monitor-info.h"
#include "graphics/graphics.h"
#include <vector>
using namespace std;

class PLSMonitorDuplicator {
public:
	PLSMonitorDuplicator();
	PLSMonitorDuplicator(int adapter, int dev_id);
	virtual ~PLSMonitorDuplicator();

	int get_adapter_id() { return adapter_id; };
	int get_monitor_dev_id() { return monitor_dev_id; };
	void add_ref();
	void sub_ref();

public:
	virtual bool release_duplicator();
	virtual bool update_frame();
	virtual bool create_duplicator();
	virtual gs_texture *get_texture();
	virtual bool is_obs_gs_duplicator();
	virtual bool init_device();
	virtual bool capture_thread_run();
	virtual bool get_monitor_info(monitor_info &info);

public:
	int system_adapter_id;
	int adapter_id;
	int monitor_dev_id;

	int texture_width;
	int texture_height;
	gs_texture *monitor_texture; //texture on system graphics

	int user_reference;
};
