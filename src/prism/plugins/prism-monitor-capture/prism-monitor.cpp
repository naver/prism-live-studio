#include <objbase.h>

#include <obs-module.h>
#include <util/dstr.h>
#include <string>
#include <vector>

#include "monitor-info.h"
#include "monitor-duplicator.h"
#include "monitor-duplicator-pool.h"
#include "window-version.h"
#include "gdi-capture.h"
#include "cursor-capture.h"
#include <log.h>
using namespace std;

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")

#define TEXT_MONITOR_CAPTURE obs_module_text("MonitorCapture")
#define TEXT_CAPTURE_CURSOR obs_module_text("CaptureCursor")
#define TEXT_COMPATIBILITY obs_module_text("Compatibility")
#define TEXT_MONITOR obs_module_text("Monitor")
#define TEXT_PRIMARY_MONITOR obs_module_text("PrimaryMonitor")

#define RESET_INTERVAL_MILLISEC 3000 // millisecond

HHOOK size_change_hook = NULL;
static bool pls_monitor_size_changed = false;

struct prism_monitor_capture {
	obs_source_t *source;
	int monitor;
	int monitor_pre;
	bool is_capture_cursor;
	bool showing;

	long offset_x;
	long offset_y;
	int rot;
	uint32_t width;
	uint32_t height;
	uint32_t preInitTime; // millisecond

	PLSMonitorDuplicator *duplicator;
	PLSGdiCapture *gdi_capture;
	PLSCursorCapture *cursor_capture;
};

static void free_capture_data(struct prism_monitor_capture *capture)
{
	if (capture->duplicator) {
		PLSMonitorDuplicatorPool::get_instance()->release_duplicator(capture->duplicator->get_adapter_id(), capture->duplicator->get_monitor_dev_id());
	}
	capture->duplicator = NULL;
	capture->width = 0;
	capture->height = 0;
	capture->offset_x = 0;
	capture->offset_y = 0;
	capture->rot = 0;
	if (capture->gdi_capture) {
		delete capture->gdi_capture;
		capture->gdi_capture = NULL;
	}
	if (capture->cursor_capture) {
		delete capture->cursor_capture;
		capture->cursor_capture = NULL;
	}
	capture = NULL;
}

static void reset_capture_data(struct prism_monitor_capture *capture)
{
	int adpter_id = -1;
	int monitor_dev_id = -1;
	bool res = PLSMonitorManager::get_instance()->get_adapter_monitor_dev_id(adpter_id, monitor_dev_id, capture->monitor);
	if (!res)
		return;

	int monitor_width, monitor_height, monitor_offset_x, monitor_offset_y, monitor_rotation;
	res = PLSMonitorManager::get_instance()->get_monitor_detail(monitor_width, monitor_height, monitor_offset_x, monitor_offset_y, monitor_rotation, capture->monitor);

	if (!res)
		return;

	bool support_duplicator = PLSWindowVersion::is_support_monitor_duplicate();
	if (support_duplicator && capture->duplicator) {
		struct monitor_info info = {};
		res = capture->duplicator->get_monitor_info(info);
		if (res) {
			capture->width = info.width;
			capture->height = info.height;
			capture->offset_x = info.offset_x;
			capture->offset_y = info.offset_y;
			capture->rot = info.rotation;
		} else {
			PLS_PLUGIN_ERROR("adaptor %d, monitor %d, get duplicator info error", adpter_id, monitor_dev_id);
		}
	} else {
		capture->width = monitor_width;
		capture->height = monitor_height;
		capture->offset_x = monitor_offset_x;
		capture->offset_y = monitor_offset_y;
		capture->rot = monitor_rotation;
	}

	if (!capture->cursor_capture) {
		capture->cursor_capture = new PLSCursorCapture;
	}
	if (!capture->gdi_capture && !support_duplicator) {
		capture->gdi_capture = new PLSGdiCapture;
	}
	return;
}

static inline void update_settings_properity(struct prism_monitor_capture *capture, obs_data_t *settings)
{
	capture->monitor = (int)obs_data_get_int(settings, "monitor");
	capture->is_capture_cursor = obs_data_get_bool(settings, "capture_cursor");

	if (capture->duplicator && capture->monitor_pre != capture->monitor) {
		PLSMonitorDuplicatorPool::get_instance()->release_duplicator(capture->duplicator->get_adapter_id(), capture->duplicator->get_monitor_dev_id());
		capture->duplicator = NULL;
	}

	capture->monitor_pre = capture->monitor;
	capture->width = 0;
	capture->height = 0;
	capture->offset_x = 0;
	capture->offset_y = 0;
	capture->rot = 0;
	capture->preInitTime = 0;
}

static const char *prism_monitor_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("MonitorCapture");
}

static void prism_monitor_update(void *data, obs_data_t *settings)
{
	struct prism_monitor_capture *mc = reinterpret_cast<prism_monitor_capture *>(data);
	update_settings_properity(mc, settings);
}

static uint32_t prism_monitor_width(void *data)
{
	struct prism_monitor_capture *capture = reinterpret_cast<prism_monitor_capture *>(data);
	if (capture->gdi_capture)
		return capture->width;
	return capture->rot % 180 == 0 ? capture->width : capture->height;
}

static uint32_t prism_monitor_height(void *data)
{
	struct prism_monitor_capture *capture = reinterpret_cast<prism_monitor_capture *>(data);
	if (capture->gdi_capture)
		return capture->height;
	return capture->rot % 180 == 0 ? capture->height : capture->width;
}

static void prism_monitor_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "monitor", 0);
	obs_data_set_default_bool(settings, "capture_cursor", false);
}

static obs_properties_t *prism_monitor_properties(void *data)
{
	int monitor_idx = 0;

	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	obs_property_t *monitors = obs_properties_add_list(props, "monitor", TEXT_MONITOR, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_properties_add_bool(props, "capture_cursor", TEXT_CAPTURE_CURSOR);

	vector<monitor_info *> monitor_array = PLSMonitorManager::get_instance()->get_monitor_info_array();
	vector<monitor_info *>::iterator iter = monitor_array.begin();
	int index = 0;
	for (; iter != monitor_array.end(); iter++) {
		monitor_info *info = reinterpret_cast<monitor_info *>(*iter);
		struct dstr monitor_desc = {0};
		if (info->is_primary) {
			dstr_catf(&monitor_desc, "%s %d: %ldx%ld %s", TEXT_MONITOR, index, info->width, info->height, TEXT_PRIMARY_MONITOR);
		} else {
			dstr_catf(&monitor_desc, "%s %d: %ldx%ld", TEXT_MONITOR, index, info->width, info->height);
		}
		info->monitor_id = index;
		obs_property_list_add_int(monitors, monitor_desc.array, index);
		index++;
		dstr_free(&monitor_desc);
	}
	return props;
}

static void *prism_monitor_create(obs_data_t *settings, obs_source_t *source)
{
	struct prism_monitor_capture *capture;

	capture = reinterpret_cast<prism_monitor_capture *>(bzalloc(sizeof(struct prism_monitor_capture)));
	capture->source = source;

	capture->duplicator = NULL;
	capture->gdi_capture = NULL;
	capture->is_capture_cursor = false;
	capture->monitor = -1;
	capture->monitor_pre = -1;
	capture->preInitTime = 0;
	PLSMonitorManager::get_instance()->load_monitors();

	update_settings_properity(capture, settings);

	if (!PLSWindowVersion::is_support_monitor_duplicate())
		capture->gdi_capture = new PLSGdiCapture;

	capture->cursor_capture = new PLSCursorCapture;

	PLS_PLUGIN_INFO("create monitor capture source");

	return capture;
}

static void prism_monitor_destroy(void *data)
{
	struct prism_monitor_capture *capture = reinterpret_cast<prism_monitor_capture *>(data);

	free_capture_data(capture);

	capture = NULL;
}

static void draw_cursor(struct prism_monitor_capture *capture)
{
	if (!capture->cursor_capture)
		return;
	capture->cursor_capture->cursor_render(-capture->offset_x, -capture->offset_y, 1.0f, 1.0f, capture->rot % 180 == 0 ? capture->width : capture->height,
					       capture->rot % 180 == 0 ? capture->height : capture->width);
}

static void prism_monitor_render_texture(prism_monitor_capture *capture, gs_texture_t *texture, gs_effect_t *effect)
{
	if (!texture)
		return;

	obs_enter_graphics();
	effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);

	int rot = capture->rot;

	while (gs_effect_loop(effect, "Draw")) {
		if (rot != 0) {
			float x = 0.0f;
			float y = 0.0f;
			gs_matrix_push();
			if (!capture->gdi_capture) {
				switch (rot) {
				case 90:
					x = (float)capture->height;
					break;
				case 180:
					x = (float)capture->width;
					y = (float)capture->height;
					break;
				case 270:
					y = (float)capture->width;
					break;
				}
				gs_matrix_translate3f(x, y, 0.0f);
				gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD((float)rot));
			}
		}

		obs_source_draw(texture, 0, 0, 0, 0, false);

		if (rot != 0)
			gs_matrix_pop();
	}
	obs_leave_graphics();
}

static void prism_monitor_render(void *data, gs_effect_t *effect)
{
	struct prism_monitor_capture *capture = reinterpret_cast<prism_monitor_capture *>(data);

	int adpter_id = -1;
	int monitor_dev_id = -1;
	bool res = PLSMonitorManager::get_instance()->get_adapter_monitor_dev_id(adpter_id, monitor_dev_id, capture->monitor);
	if (!res)
		return;

	gs_texture_t *texture = NULL;
	if (PLSWindowVersion::is_support_monitor_duplicate()) {
		if (capture->duplicator) {
			texture = capture->duplicator->get_texture();
		}
	} else {
		if (capture->gdi_capture) {
			if (capture->gdi_capture->check_init(capture->width, capture->height)) {
				capture->gdi_capture->capture(capture->offset_x, capture->offset_y, capture->is_capture_cursor, capture->offset_x, capture->offset_y, 1.0);
				texture = capture->gdi_capture->get_texture();
			}
		}
	}

	if (texture)
		prism_monitor_render_texture(capture, texture, effect);

	if (capture->is_capture_cursor) {
		obs_enter_graphics();
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

		while (gs_effect_loop(effect, "Draw")) {
			draw_cursor(capture);
		}
		obs_leave_graphics();
	}

	UNUSED_PARAMETER(effect);
}

static void prism_monitor_tick(void *data, float seconds)
{
	if (pls_monitor_size_changed) {
		PLSMonitorManager::get_instance()->load_monitors();
		pls_monitor_size_changed = false;
	}

	struct prism_monitor_capture *capture = reinterpret_cast<prism_monitor_capture *>(data);

	int adpter_id = -1;
	int monitor_dev_id = -1;
	bool res = PLSMonitorManager::get_instance()->get_adapter_monitor_dev_id(adpter_id, monitor_dev_id, capture->monitor);
	bool support_duplicator = PLSWindowVersion::is_support_monitor_duplicate();
	if (res && !support_duplicator) {
		int gdi_width, gdi_height;
		capture->gdi_capture->get_width_height(gdi_width, gdi_height);

		int monitor_width, monitor_height, monitor_offset_x, monitor_offset_y, monitor_rotation;
		bool res = PLSMonitorManager::get_instance()->get_monitor_detail(monitor_width, monitor_height, monitor_offset_x, monitor_offset_y, monitor_rotation, capture->monitor);

		if (!res) {
			PLS_PLUGIN_INFO("gdi capture, get monitor %d detail failed", capture->monitor);
			return;
		}

		if (gdi_width != monitor_width || gdi_height != monitor_height || gdi_width != capture->width || gdi_height != capture->height) {
			reset_capture_data(capture);
		}
		return;
	} else if (!res) {
		return;
	}

	if (!support_duplicator)
		return;

	res = obs_source_showing(capture->source);
	if (!res) {
		if (capture->showing) {
			free_capture_data(capture);
			if (capture->duplicator) {
				PLSMonitorDuplicatorPool::get_instance()->release_duplicator(capture->duplicator->get_adapter_id(), capture->duplicator->get_monitor_dev_id());
				capture->duplicator = NULL;
			}
			capture->showing = false;
		}
		return;
	} else if (!capture->showing) {
		capture->preInitTime = 0;
	}

	if (!capture->duplicator) {
		DWORD currentTime = GetTickCount();

		// handle exception
		if (capture->preInitTime >= currentTime)
			capture->preInitTime = currentTime;

		if (currentTime - capture->preInitTime >= RESET_INTERVAL_MILLISEC) {
			capture->duplicator = PLSMonitorDuplicatorPool::get_instance()->get_duplicator(adpter_id, monitor_dev_id);
			capture->preInitTime = currentTime;
		}
	}

	if (capture->is_capture_cursor) {
		if (!capture->cursor_capture) {
			capture->cursor_capture = new PLSCursorCapture;
		}
	}

	if (capture->duplicator) {
		if (!capture->duplicator->update_frame()) {
			free_capture_data(capture);
		} else if (capture->width == 0) {
			reset_capture_data(capture);
		}
	}

	if (!capture->showing)
		capture->showing = true;

	UNUSED_PARAMETER(seconds);
}

LRESULT CALLBACK pls_sizechange_hook_pro(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (!lParam)
		return 0;

	if (nCode == HC_ACTION) {
		CWPSTRUCT *cwp = reinterpret_cast<CWPSTRUCT *>(lParam);
		if (cwp->message == WM_DISPLAYCHANGE) {
			PLS_PLUGIN_INFO("display size changed");
			pls_monitor_size_changed = true;
		}
	}
	return CallNextHookEx(size_change_hook, nCode, wParam, lParam);
}

void register_prism_monitor_source()
{
	obs_source_info info = {};
	info.id = "prism_monitor_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = prism_monitor_get_name;
	info.create = prism_monitor_create;
	info.destroy = prism_monitor_destroy;
	info.video_render = prism_monitor_render;
	info.video_tick = prism_monitor_tick;
	info.update = prism_monitor_update;
	info.get_width = prism_monitor_width;
	info.get_height = prism_monitor_height;
	info.get_defaults = prism_monitor_defaults;
	info.get_properties = prism_monitor_properties;
	info.icon_type = OBS_ICON_TYPE_DESKTOP_CAPTURE;
	obs_register_source(&info);

	size_change_hook = ::SetWindowsHookEx(WH_CALLWNDPROC, pls_sizechange_hook_pro, NULL, ::GetCurrentThreadId());

	if (!size_change_hook)
		PLS_PLUGIN_INFO("hook display size change failed");
}

void release_prism_monitor_data()
{
	PLSMonitorManager::get_instance()->clear();
	PLSMonitorDuplicatorPool::get_instance()->clear();
	if (size_change_hook)
		UnhookWindowsHookEx(size_change_hook);
}
