#pragma once

#include <QImage>
#include <qobject.h>
#include <screenshot-obj.hpp>
#include <QPointer>
#include "obs.hpp"

namespace takephoto {
#if defined(_WIN32)
constexpr const char *CSTR_VIDEO_DEVICE_ID = "video_device_id";
constexpr const char *RES_TYPE = "res_type";

#elif defined(__APPLE__)
constexpr const char *CSTR_VIDEO_DEVICE_ID = "device";
#endif
constexpr const char *CSTR_CAPTURE_STATE = "capture_state";
constexpr const char *CSTR_SOURCE_IMAGE_STATUS = "source_image_status";

struct FindSourceResult {
	const char *camera;
	//PRISM/FanZirong/20251201/PRISM_PC-4571/use valid source
	std::vector<obs_source_t *> matchedSources;
};

obs_source_t *createSource(const char *camera, const char *name);
}
