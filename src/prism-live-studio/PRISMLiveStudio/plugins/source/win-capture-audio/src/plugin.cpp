#include <obs-module.h>

#include "common.hpp"
#include "audio-capture-helper.hpp"
#include "session-monitor.hpp"
//PRISM/WangShaohui/20220310/none/integrate plugin
//#include "plugin-macros.generated.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-capture-audio", "en-US")

extern struct obs_source_info audio_capture_info;

bool obs_module_load(void)
{
	//PRISM/WangShaohui/20220310/none/integrate plugin
	//blog(LOG_INFO, "[win-capture-audio] Version %s (%s)", PLUGIN_VERSION, GIT_HASH);
	SessionMonitor::Create();

	obs_register_source(&audio_capture_info);
	return true;
}

void obs_module_unload()
{
	SessionMonitor::Destroy();
}
