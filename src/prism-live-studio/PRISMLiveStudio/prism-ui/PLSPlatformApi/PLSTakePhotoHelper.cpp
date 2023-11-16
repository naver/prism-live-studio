#include "PLSTakePhotoHelper.h"
#include "pls-common-define.hpp"

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
		return true;
	}

	if (const char *camera = obs_data_get_string(settings, takephoto::CSTR_VIDEO_DEVICE_ID); camera && !strcmp(camera, result->camera)) {
		result->source = source;
		obs_data_release(settings);
		return false;
	}

	obs_data_release(settings);
	return true;
}

obs_source_t *takephoto::createSource(const char *camera, const char *name)
{
	FindSourceResult result = {camera, nullptr};
	obs_enum_sources(&findAllSource, &result);

	obs_source_t *source = nullptr;
	if (!result.source) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, CSTR_VIDEO_DEVICE_ID, camera);
		source = obs_source_create_private(OBS_DSHOW_SOURCE_ID, name, settings);
		obs_data_release(settings);
	} else {
		source = obs_source_get_ref(result.source);
	}
	return source;
}
