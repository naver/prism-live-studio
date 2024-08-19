#include "PLSWinrtNotify.h"
#include "log.h"
#include <obs.h>
#include <util/platform.h>

PLSWinRTNotify::PLSWinRTNotify(QObject *parent) : QObject(parent)
{
	static const char *const module = "libobs-winrt";
	winrt_module = os_dlopen(module);
	if (nullptr != winrt_module) {
		func_on_display_changed = (PFN_winrt_capture_on_display_changed)os_dlsym(winrt_module, "winrt_capture_on_display_changed");
		if (!func_on_display_changed) {
			PLS_ERROR("PLSWinRTNotify", "Failed to load winrt_capture_on_display_changed");
		}
	} else {
		static auto bLog = true;
		if (bLog) {
			bLog = false;
			PLS_WARN("PLSWinRTNotify", "load %s failed", module);
		}
	}
}

PLSWinRTNotify::~PLSWinRTNotify()
{
	if (winrt_module)
		os_dlclose(winrt_module);
}

void PLSWinRTNotify::onDisplayChanged()
{
	if (pls_is_app_exiting())
		return;

	obs_queue_task(
		OBS_TASK_GRAPHICS,
		[](void *param) {
			if (auto context = (PLSWinRTNotify *)param; context) {
				if (pls_is_app_exiting())
					return;

				if (context->func_on_display_changed)
					context->func_on_display_changed();
			}
		},
		this, false);
}
