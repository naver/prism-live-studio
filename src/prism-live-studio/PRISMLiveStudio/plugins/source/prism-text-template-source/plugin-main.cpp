#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-text-template-source", "en-US")
const char *obs_module_name(void)
{
	return "PRISM text template source";
}
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_name();
}

void register_prism_text_template_source();

bool obs_module_load(void)
{
	register_prism_text_template_source();
	return true;
}
