#include <obs-module.h>
#include <Windows.h>
#include <obs-module.h>
#include <liblog.h>
#include "monitor-info.h"

static HHOOK size_change_hook = nullptr;
static bool pls_monitor_size_changed = false;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-monitor-capture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Prism monitor capture";
}

extern void register_prism_region_source();
LRESULT CALLBACK pls_sizechange_hook_pro(int nCode, WPARAM wParam, LPARAM lParam);

bool graphics_uses_d3d11 = false;

bool obs_module_load(void)
{
	obs_enter_graphics();
	graphics_uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	size_change_hook = ::SetWindowsHookEx(WH_CALLWNDPROC, pls_sizechange_hook_pro, nullptr, ::GetCurrentThreadId());
	if (!size_change_hook) {
		PLS_ERROR("monitor capture", "Failed to hook display size change. error:%u", GetLastError());
	}

	register_prism_region_source();
	return true;
}

void obs_module_unload(void) {}

const char *obs_module_name(void)
{
	return obs_module_description();
}

LRESULT CALLBACK pls_sizechange_hook_pro(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (!pls_monitor_size_changed) {
		auto cwp = (CWPSTRUCT *)(lParam);
		if (cwp->message == WM_DISPLAYCHANGE) {
			pls_monitor_size_changed = true;
		}
	}

	return CallNextHookEx(size_change_hook, nCode, wParam, lParam);
}

void handle_hooked_message()
{
	const auto CHECK_MONITOR_MSG_INTERVAL = 2000;

	if (pls_monitor_size_changed) {
		static unsigned pls_previous_check_time = 0;
		if ((GetTickCount() - pls_previous_check_time) >= CHECK_MONITOR_MSG_INTERVAL) {
			pls_monitor_size_changed = false;
			pls_previous_check_time = GetTickCount();
			PLSMonitorManager::get_instance()->reload_monitor(true);
		}
	}
}
