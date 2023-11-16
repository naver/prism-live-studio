#pragma once

#include <QImage>
#include <qobject.h>
#include <screenshot-obj.hpp>
#include <QPointer>
#include "obs.hpp"

namespace takephoto {
#if defined(_WIN32)
constexpr const char *CSTR_VIDEO_DEVICE_ID = "video_device_id";
#elif defined(__APPLE__)
constexpr const char *CSTR_VIDEO_DEVICE_ID = "device";
#endif
constexpr const char *CSTR_CAPTURE_STATE = "capture_state";
constexpr const char *CSTR_SOURCE_IMAGE_STATUS = "source_image_status";

struct FindSourceResult {
	const char *camera;
	obs_source_t *source;
};

obs_source_t *createSource(const char *camera, const char *name);
}
