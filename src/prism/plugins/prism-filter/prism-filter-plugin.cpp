#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-filter", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "prism-filter-module";
}

extern void register_filter_sound_touch();

bool obs_module_load(void)
{
	register_filter_sound_touch();
	return true;
}

const char *obs_module_name(void)
{
	return obs_module_description();
}
