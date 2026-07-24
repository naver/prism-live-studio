#include "PLSTakePhotoHelper.h"
#include "pls-common-define.hpp"
//PRISM/FanZirong/20251201/PRISM_PC-4571/use valid source
#include "pls/pls-obs-api.h"

using namespace common;

bool findAllSource(void *context, obs_source_t *source)
{
	auto result = (takephoto::FindSourceResult *)context;
	//if (obs_source_error error; !obs_source_get_capture_valid(source, &error)) {
	//	return true;
	//}

	const char *plugin = obs_source_get_id(source);
	if (!plugin) {
		return true;
	}

	if (0 != strcmp(plugin, OBS_DSHOW_SOURCE_ID)) {
		return true;
	}

	obs_data_t *settings = obs_source_get_settings(source);
	if (!settings) {
		return true;
	}

	if (bool active = obs_data_get_bool(settings, "active"); !active) {
		//PRISM/FanZirong/20251201/PRISM_PC-4571/use valid source
		obs_data_release(settings);
		return true;
	}

	if (const char *camera = obs_data_get_string(settings, takephoto::CSTR_VIDEO_DEVICE_ID); camera && !strcmp(camera, result->camera)) {
		//PRISM/FanZirong/20251201/PRISM_PC-4571/use valid source
		result->matchedSources.push_back(source);
		obs_data_release(settings);
		if (pls_source_get_failed_status_sub_code(source) == OBS_SOURCE_STATUS_SUCCESS) {
			return false;
		}
		return true;
	}

	obs_data_release(settings);
	return true;
}

obs_source_t *takephoto::createSource(const char *camera, const char *name)
{
	FindSourceResult result = {camera, {}};
	obs_enum_sources(&findAllSource, &result);

	obs_source_t *source = nullptr;
	//PRISM/FanZirong/20251201/PRISM_PC-4571/use valid source
	if (result.matchedSources.empty()) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, CSTR_VIDEO_DEVICE_ID, camera);
#if defined(_WIN32)
		obs_data_set_int(settings, RES_TYPE, 0);
#endif

		source = obs_source_create_private(OBS_DSHOW_SOURCE_ID, name, settings);
		obs_data_release(settings);
	} else {
		//PRISM/FanZirong/20251201/PRISM_PC-4571/use valid source
		for (obs_source_t *matchedSource : result.matchedSources) {
			if (pls_source_get_failed_status_sub_code(matchedSource) == OBS_SOURCE_STATUS_SUCCESS) {
				source = obs_source_get_ref(matchedSource);
				break;
			}
		}
		if (!source)
			source = obs_source_get_ref(result.matchedSources[0]);
	}
	return source;
}
