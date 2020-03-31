#pragma once
#include "monitor-duplicator.h"

class PLSGraphicsDuplicator : public PLSMonitorDuplicator {
public:
	PLSGraphicsDuplicator();
	PLSGraphicsDuplicator(int adapter, int dev_id);
	~PLSGraphicsDuplicator();

	virtual bool update_frame();
	virtual bool create_duplicator();
	virtual gs_texture *get_texture();
	virtual bool is_obs_gs_duplicator();
	virtual bool get_monitor_info(monitor_info &info);

protected:
private:
	void release();

private:
	gs_duplicator *duplicator;
};
