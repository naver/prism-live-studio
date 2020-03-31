#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-monitor-capture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Prism monitor capture";
}

extern void register_prism_monitor_source();
extern void release_prism_monitor_data();

bool obs_module_load(void)
{
	register_prism_monitor_source();
	return true;
}

void obs_module_unload(void)
{
	release_prism_monitor_data();
}

const char *obs_module_name(void)
{
	return obs_module_description();
}
