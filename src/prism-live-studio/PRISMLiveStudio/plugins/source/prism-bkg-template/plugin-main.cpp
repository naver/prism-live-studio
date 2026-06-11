#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-background-template-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "PRISM background template source";
}

extern void register_background_template_source();

bool obs_module_load(void)
{
	register_background_template_source();
	return true;
}

const char *obs_module_name(void)
{
	return obs_module_description();
}
