#include <obs-module.h>
#include <liblog.h>
#include "gdi-capture.h"
#include <libutils-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <pls/pls-obs-api.h>
#include "monitor-info.h"
#include <util/platform.h>
#include <winrt-capture.h>

using namespace std;

#define do_warn(format, ...) PLS_WARN("monitor capture", "[monitor(region) '%s' %p] " format, obs_source_get_name(source), source, ##__VA_ARGS__)
#define do_info(format, ...) PLS_INFO("monitor capture", "[monitor(region) '%s' %p] " format, obs_source_get_name(source), source, ##__VA_ARGS__)

#define warn(format, ...) do_warn(format, ##__VA_ARGS__)
#define info(format, ...) do_info(format, ##__VA_ARGS__)

const auto REGION_MIN_CX = 0;
const auto REGION_MIN_CY = 0;
const auto REGION_DEFAULT_CX = REGION_MIN_CX;
const auto REGION_DEFAULT_CY = REGION_MIN_CY;

// keys of language strings
const auto TEXT_REGION_NAME = "RegionSourceName";
const auto TEXT_REGION_CURSOR = "CaptureCursor";

// keys of obs_data_t
const auto REGION_KEY_REGION_SELECT = "region_select";
const auto REGION_KEY_CAPTURE_CURSOR = "capture_cursor";
const auto REGION_KEY_LEFT = "left";
const auto REGION_KEY_TOP = "top";
const auto REGION_KEY_WIDTH = "width";
const auto REGION_KEY_HEIGHT = "height";

extern bool graphics_uses_d3d11;

typedef BOOL (*PFN_winrt_capture_supported)();
typedef BOOL (*PFN_winrt_capture_cursor_toggle_supported)();
typedef struct winrt_capture *(*PFN_winrt_capture_init_window)(BOOL cursor, HWND window, BOOL client_area, BOOL force_sdr);
typedef struct winrt_capture *(*PFN_winrt_capture_init_desktop)(BOOL cursor, HMONITOR monitor, BOOL force_sdr, RECT tex_region);
typedef void (*PFN_winrt_capture_free)(struct winrt_capture *capture);

typedef BOOL (*PFN_winrt_capture_active)(const struct winrt_capture *capture);
typedef BOOL (*PFN_winrt_capture_show_cursor)(struct winrt_capture *capture, BOOL visible);
typedef enum gs_color_space (*PFN_winrt_capture_get_color_space)(const struct winrt_capture *capture);
typedef void (*PFN_winrt_capture_render)(struct winrt_capture *capture);
typedef uint32_t (*PFN_winrt_capture_width)(const struct winrt_capture *capture);
typedef uint32_t (*PFN_winrt_capture_height)(const struct winrt_capture *capture);

struct winrt_exports {
	PFN_winrt_capture_supported winrt_capture_supported;
	PFN_winrt_capture_cursor_toggle_supported winrt_capture_cursor_toggle_supported;
	PFN_winrt_capture_init_window winrt_capture_init_window;
	PFN_winrt_capture_init_desktop winrt_capture_init_desktop;
	PFN_winrt_capture_free winrt_capture_free;
	PFN_winrt_capture_active winrt_capture_active;
	PFN_winrt_capture_show_cursor winrt_capture_show_cursor;
	PFN_winrt_capture_get_color_space winrt_capture_get_color_space;
	PFN_winrt_capture_render winrt_capture_render;
	PFN_winrt_capture_width winrt_capture_width;
	PFN_winrt_capture_height winrt_capture_height;
};

class PLSAutoLockRender {
public:
	PLSAutoLockRender() { obs_enter_graphics(); }
	virtual ~PLSAutoLockRender() { obs_leave_graphics(); }

	PLSAutoLockRender(const PLSAutoLockRender &) = delete;
	PLSAutoLockRender &operator=(const PLSAutoLockRender &) = delete;
	PLSAutoLockRender(PLSAutoLockRender &&) = delete;
	PLSAutoLockRender &operator=(PLSAutoLockRender &&) = delete;
};

struct prism_region_settings {
	int left = 0;
	int top = 0;
	int width = REGION_DEFAULT_CX;
	int height = REGION_DEFAULT_CY;
	bool capture_cursor = true;

	//---------------------------------------
	void keep_valid();
	bool region_empty() const;
};

struct prism_region_source {
	obs_source_t *source = nullptr;

	prism_region_settings region_settings;

	bool bRectangleValid = false;
	bool previously_failed = false;
	void *winrt_module = nullptr;
	struct winrt_exports exports = {0};
	struct winrt_capture *capture_winrt = nullptr;

	PLSGdiCapture gdi_capture;

	//------------------------------------------------------------------------
	prism_region_source(obs_data_t *settings, obs_source_t *source_);
	virtual ~prism_region_source();

	prism_region_source(const prism_region_source &) = delete;
	prism_region_source &operator=(const prism_region_source &) = delete;
	prism_region_source(prism_region_source &&) = delete;
	prism_region_source &operator=(prism_region_source &&) = delete;

	void tick();
	void render();
	void update(obs_data_t *settings);
	tuple<bool, HMONITOR, int, int> check_region();

private:
	bool wgc_render_region();
	void gdi_render_region();

	tuple<bool, HMONITOR, int, int> lastCheck;
};

//------------------------------------------------------------------------
void prism_region_settings::keep_valid()
{
	uint64_t max_size = pls_texture_get_max_size();
	if (max_size > 0) {
		if (width > max_size) {
			width = (int)max_size;
		}

		if (height > max_size) {
			height = (int)max_size;
		}
	}

	if (width < REGION_MIN_CX) {
		width = REGION_MIN_CX;
	}

	if (height < REGION_MIN_CY) {
		height = REGION_MIN_CY;
	}
}

bool prism_region_settings::region_empty() const
{
	if (width > 0 && height > 0) {
		return false;
	} else {
		return true;
	}
}

#define WINRT_IMPORT(func)                                             \
	do {                                                           \
		exports->func = (PFN_##func)os_dlsym(szModule, #func); \
		if (!exports->func) {                                  \
			success = false;                               \
			blog(LOG_ERROR,                                \
			     "Could not load function '%s' from "      \
			     "module '%s'",                            \
			     #func, module_name);                      \
		}                                                      \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *szModule, const char *module_name)
{
	bool success = true;

	WINRT_IMPORT(winrt_capture_supported);
	WINRT_IMPORT(winrt_capture_cursor_toggle_supported);
	WINRT_IMPORT(winrt_capture_init_window);
	WINRT_IMPORT(winrt_capture_init_desktop);
	WINRT_IMPORT(winrt_capture_free);
	WINRT_IMPORT(winrt_capture_active);
	WINRT_IMPORT(winrt_capture_show_cursor);
	WINRT_IMPORT(winrt_capture_get_color_space);
	WINRT_IMPORT(winrt_capture_render);
	WINRT_IMPORT(winrt_capture_width);
	WINRT_IMPORT(winrt_capture_height);

	return success;
}

prism_region_source::prism_region_source(obs_data_t *settings, obs_source_t *source_) : source(source_)
{
	if (graphics_uses_d3d11) {
		static const char *const szModule = "libobs-winrt";
		winrt_module = os_dlopen(szModule);
		if (nullptr != winrt_module) {
			load_winrt_imports(&exports, winrt_module, szModule);
		} else {
			static auto bLog = true;
			if (bLog) {
				bLog = false;
				warn("load %s failed", szModule);
			}
		}
	} else {
		static auto bLog = true;
		if (bLog) {
			bLog = false;
			info("d3d11 renderer is not being used, fallback to gdi capture.");
		}
	}

	update(settings);
}

prism_region_source ::~prism_region_source()
{
	gdi_capture.uninit();
}

void handle_hooked_message();

tuple<bool, HMONITOR, int, int> prism_region_source::check_region()
{
	auto iMonitors = 0;
	HMONITOR hMonitor = nullptr;
	auto iLeft = 0;
	auto iTop = 0;
	auto iMinLeft = 0;
	auto iMinTop = 0;

	std::vector<monitor_info> monitors = PLSMonitorManager::get_instance()->get_monitor();
	for (const auto &item : monitors) {
		if (region_settings.left <= item.offset_x + item.width && region_settings.top <= item.offset_y + item.height && region_settings.left + region_settings.width >= item.offset_x &&
		    region_settings.top + region_settings.height >= item.offset_y) {
			++iMonitors;
			hMonitor = item.handle;
			iLeft = item.offset_x;
			iTop = item.offset_y;
		}

		if (item.offset_x < iMinLeft)
			iMinLeft = item.offset_x;
		if (item.offset_y < iMinTop)
			iMinTop = item.offset_y;
	}

	return make_tuple(iMonitors > 0, 1 == iMonitors ? hMonitor : nullptr, 1 == iMonitors ? iLeft : iMinLeft, 1 == iMonitors ? iTop : iMinTop);
}

void prism_region_source::tick()
{
	if (region_settings.region_empty())
		return;

	handle_hooked_message();

	auto checkResult = check_region();
	auto [bValid, hMonitor, iLeft, iTop] = checkResult;

	bRectangleValid = bValid;
	if (!bRectangleValid) {
		return;
	}

	PLSAutoLockRender alr;

	if (lastCheck != checkResult) {
		lastCheck = checkResult;
		if (nullptr != capture_winrt) {
			exports.winrt_capture_free(capture_winrt);
			capture_winrt = nullptr;
		}
	}

	if (nullptr == capture_winrt && !previously_failed && nullptr != exports.winrt_capture_init_desktop) {

		RECT rect = {region_settings.left - iLeft, region_settings.top - iTop, region_settings.left - iLeft + region_settings.width, region_settings.top - iTop + region_settings.height};
		if (rect.left < 0)
			rect.left = 0;
		if (rect.top < 0)
			rect.top = 0;
		capture_winrt = exports.winrt_capture_init_desktop(region_settings.capture_cursor, hMonitor, FALSE, rect);

		if (nullptr != capture_winrt) {
			info("winrt_capture_init_desktop: %d,%d  %dx%d", rect.left, rect.top, rect.right, rect.bottom);
			return;
		} else {
			previously_failed = true;
		}
	}

	if (nullptr != capture_winrt) {
		return;
	}

	if (gdi_capture.check_init(region_settings.width, region_settings.height)) {
		gdi_capture.capture(region_settings.left, region_settings.top, region_settings.capture_cursor, region_settings.left, region_settings.top, 1.0);
	}
}

void prism_region_source::render()
{
	if (!bRectangleValid) {
		return;
	}

	PLSAutoLockRender alr;

	if (region_settings.region_empty())
		return;

	if (!wgc_render_region())
		gdi_render_region();
}

bool prism_region_source::wgc_render_region()
{
	PLSAutoLockRender alr;

	if (nullptr != capture_winrt) {
		if (exports.winrt_capture_active(capture_winrt)) {
			exports.winrt_capture_render(capture_winrt);

			return true;
		} else {
			exports.winrt_capture_free(capture_winrt);
			capture_winrt = NULL;
		}
	}

	return false;
}

void prism_region_source::gdi_render_region()
{
	PLSAutoLockRender alr;

	auto tex = gdi_capture.get_texture();
	if (tex) {
		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);
		while (gs_effect_loop(effect, "Draw")) {
			obs_source_draw(tex, 0, 0, 0, 0, false);
		}
	}
}

void prism_region_source::update(obs_data_t *settings)
{
	prism_region_settings temp;

	obs_data_t *region_obj = obs_data_get_obj(settings, REGION_KEY_REGION_SELECT);
	temp.left = (int)obs_data_get_int(region_obj, REGION_KEY_LEFT);
	temp.top = (int)obs_data_get_int(region_obj, REGION_KEY_TOP);
	temp.width = (int)obs_data_get_int(region_obj, REGION_KEY_WIDTH);
	temp.height = (int)obs_data_get_int(region_obj, REGION_KEY_HEIGHT);
	temp.capture_cursor = obs_data_get_bool(settings, REGION_KEY_CAPTURE_CURSOR);
	obs_data_release(region_obj);

	bool bChanged = temp.capture_cursor != region_settings.capture_cursor || temp.left != region_settings.left || temp.top != region_settings.top || temp.width != region_settings.width ||
			temp.height != region_settings.height;

	temp.keep_valid();
	region_settings = temp;

	if (previously_failed) {
		previously_failed = false;
	}
	if (bChanged && nullptr != capture_winrt) {
		exports.winrt_capture_free(capture_winrt);
		capture_winrt = nullptr;
	}

	info("region updated. offset:%d,%d  size:%dx%d  cursor:%d", temp.left, temp.top, temp.width, temp.height, temp.capture_cursor);
}

static obs_properties_t *region_source_get_properties(void *)
{
	obs_properties_t *props = obs_properties_create();

	pls_properties_add_region_select(props, REGION_KEY_REGION_SELECT, "");
	obs_properties_add_bool(props, REGION_KEY_CAPTURE_CURSOR, obs_module_text(TEXT_REGION_CURSOR));

	return props;
}

static void region_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "noLabelHeader", true);

	obs_data_t *region_obj = obs_data_create();
	obs_data_set_default_int(region_obj, REGION_KEY_LEFT, 0);
	obs_data_set_default_int(region_obj, REGION_KEY_TOP, 0);
	obs_data_set_default_int(region_obj, REGION_KEY_WIDTH, REGION_DEFAULT_CX);
	obs_data_set_default_int(region_obj, REGION_KEY_HEIGHT, REGION_DEFAULT_CY);

	obs_data_set_default_obj(settings, REGION_KEY_REGION_SELECT, region_obj);
	obs_data_set_default_bool(settings, REGION_KEY_CAPTURE_CURSOR, true);

	obs_data_release(region_obj);
}

void register_prism_region_source()
{
	obs_source_info info = {};

	info.id = "prism_region_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.icon_type = (obs_icon_type)PLS_ICON_TYPE_REGION;
	info.get_name = [](void *) { return obs_module_text(TEXT_REGION_NAME); };

	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		PLSMonitorManager::get_instance()->reload_monitor(false);
		return pls_new<prism_region_source>(settings, source);
	};

	info.destroy = [](void *data) {
		obs_queue_task(
			OBS_TASK_GRAPHICS,
			[](void *data) {
				auto region = static_cast<prism_region_source *>(data);

				if (region->capture_winrt) {
					region->exports.winrt_capture_free(region->capture_winrt);
				}
				if (region->winrt_module)
					os_dlclose(region->winrt_module);

				pls_delete(region);
			},
			data, false);
	};

	info.get_width = [](void *data) {
		auto region = static_cast<prism_region_source *>(data);
		return (uint32_t)region->region_settings.width;
	};

	info.get_height = [](void *data) {
		auto region = static_cast<prism_region_source *>(data);
		return (uint32_t)region->region_settings.height;
	};

	info.video_tick = [](void *data, float) { static_cast<prism_region_source *>(data)->tick(); };
	info.video_render = [](void *data, gs_effect_t *) { static_cast<prism_region_source *>(data)->render(); };
	info.get_properties = region_source_get_properties;
	info.get_defaults = region_source_get_defaults;
	info.update = [](void *data, obs_data_t *settings) { static_cast<prism_region_source *>(data)->update(settings); };

	obs_register_source(&info);
}
