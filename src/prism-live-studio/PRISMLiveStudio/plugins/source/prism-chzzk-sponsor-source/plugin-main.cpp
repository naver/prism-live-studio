#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-chzzk-sponsor-source", "en-US")
const char *obs_module_name(void)
{
	return "PRISM chzzk sponsor source";
}
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_name();
}

void register_prism_chzzk_sponsor_source();

bool obs_module_load(void)
{
	register_prism_chzzk_sponsor_source();
	return true;
}
