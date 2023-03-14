#pragma once

#define HARDWARE_STR_LENGTH 200

struct obs_hardware_info {
	char cpu_name[HARDWARE_STR_LENGTH];
	char windows_version[HARDWARE_STR_LENGTH];
	unsigned cpu_speed_mhz; // in MHZ
	unsigned physical_cores;
	unsigned logical_cores;
	unsigned total_physical_memory_mb; // in MB
	unsigned free_physical_memory_mb;  // in MB
};
