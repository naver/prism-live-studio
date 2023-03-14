#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-timer-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "PRISM timer source which can play GIF animations";
}

extern void RegisterPRISMSTimerSource();

bool obs_module_load(void)
{
	RegisterPRISMSTimerSource();
	return true;
}
