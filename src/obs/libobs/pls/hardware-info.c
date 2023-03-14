#include "hardware-info.h"
#include "util/windows/win-registry.h"
#include "util/windows/win-version.h"
#include "util/platform.h"
#include "util/dstr.h"
#include "obs.h"
#include "obs-internal.h"

#include <windows.h>
#include <wscapi.h>
#include <iwscapi.h>

void fill_processor_info(struct obs_hardware_info *output_info);
void fill_memory_info(struct obs_hardware_info *output_info);
void fill_windows_version(struct obs_hardware_info *output_info);

void obs_get_current_hardware_info(struct obs_hardware_info *output_info)
{
	if (!output_info) {
		return;
	}

	memset(output_info, 0, sizeof(struct obs_hardware_info));

	output_info->physical_cores = os_get_physical_cores();
	output_info->logical_cores = os_get_logical_cores();
	fill_processor_info(output_info);
	fill_memory_info(output_info);
	fill_windows_version(output_info);
}

void fill_processor_info(struct obs_hardware_info *output_info)
{
	HKEY key;
	wchar_t data[1024];
	char *str = NULL;
	DWORD size, speed;
	LSTATUS status;

	memset(data, 0, sizeof(data));

	status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", &key);
	if (status != ERROR_SUCCESS)
		return;

	size = sizeof(data);
	status = RegQueryValueExW(key, L"ProcessorNameString", NULL, NULL, (LPBYTE)data, &size);
	if (status == ERROR_SUCCESS) {
		os_wcs_to_utf8_ptr(data, 0, &str);
		snprintf(output_info->cpu_name, sizeof(output_info->cpu_name), "%s", str);
		bfree(str);
	}

	size = sizeof(speed);
	status = RegQueryValueExW(key, L"~MHz", NULL, NULL, (LPBYTE)&speed, &size);
	if (status == ERROR_SUCCESS) {
		output_info->cpu_speed_mhz = speed;
	}

	RegCloseKey(key);
}

void fill_memory_info(struct obs_hardware_info *output_info)
{
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);

	GlobalMemoryStatusEx(&ms);

	output_info->total_physical_memory_mb = (ms.ullTotalPhys / 1024 / 1024);
	output_info->free_physical_memory_mb = (ms.ullAvailPhys / 1024 / 1024);
}

void fill_windows_version(struct obs_hardware_info *output_info)
{
	struct win_version_info ver;
	get_win_ver(&ver);

	bool b64 = is_64_bit_windows();
	const char *windows_bitness = b64 ? "64" : "32";

	snprintf(output_info->windows_version, sizeof(output_info->windows_version), "Windows Version: %d.%d Build %d (revision: %d; %s-bit)", ver.major, ver.minor, ver.build, ver.revis,
		 windows_bitness);
}
