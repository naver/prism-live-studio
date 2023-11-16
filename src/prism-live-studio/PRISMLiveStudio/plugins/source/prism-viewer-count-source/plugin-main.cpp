#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-viewer-count-source", "en-US")
const char *obs_module_name(void)
{
	return "PRISM viewer count source";
}
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_name();
}

void register_prism_viewer_count_source();

bool obs_module_load(void)
{
	register_prism_viewer_count_source();
	return true;
}
