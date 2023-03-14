#pragma once
#include "obs.h"
#ifdef _WIN32
enum win_power_state {
	WPS_POWER_NORMAL = 0,
	WPS_POWER_SUSPEND = 0x1,
	WPS_POWER_RESUME_SUSPEND = WPS_POWER_SUSPEND << 1
};

struct power_monitor;
typedef struct power_monitor power_monitor_t;

EXPORT power_monitor_t *power_monitor_start();
EXPORT void power_monitor_stop(power_monitor_t **pm);
EXPORT bool power_monitor_state(power_monitor_t *pm, enum win_power_state state,
				bool reset);
#endif
