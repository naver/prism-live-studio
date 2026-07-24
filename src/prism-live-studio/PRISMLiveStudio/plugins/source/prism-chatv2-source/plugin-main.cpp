#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-chatv2-source", "en-US")
const char *obs_module_name(void)
{
	return "prism-chatv2-source";
}
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_name();
}

void register_prism_template_chat_source();

bool obs_module_load(void)
{
	register_prism_template_chat_source();
	return true;
}
