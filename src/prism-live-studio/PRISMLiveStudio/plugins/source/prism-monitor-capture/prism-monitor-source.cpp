#include <objbase.h>

#include <obs-module.h>
#include <util/dstr.h>
#include "../../libobs/util/platform.h"
#include "../../libobs/util/util.hpp"
#include <string>
#include <vector>

#include "monitor-info.h"
#include "monitor-duplicator-pool.h"
#include "window-version.h"
#include "gdi-capture.h"
#include "cursor-capture.h"
#include <liblog.h>
#include <libutils-api.h>
#include <pls/pls-properties.h>

using namespace std;

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")

#define do_warn(format, ...) PLS_WARN("monitor capture", "[monitor(full) '%s' %p] " format, obs_source_get_name(mc->source), mc->source, ##__VA_ARGS__)
#define do_info(format, ...) PLS_INFO("monitor capture", "[monitor(full) '%s' %p] " format, obs_source_get_name(mc->source), mc->source, ##__VA_ARGS__)

#define warn(format, ...) do_warn(format, ##__VA_ARGS__)
#define info(format, ...) do_info(format, ##__VA_ARGS__)

#define TEXT_MONITOR_CAPTURE obs_module_text("MonitorCapture")
#define TEXT_CAPTURE_CURSOR obs_module_text("CaptureCursor")
#define TEXT_MONITOR obs_module_text("Monitor")
#define TEXT_PRIMARY_MONITOR obs_module_text("PrimaryMonitor")

#define TEXT_METHOD obs_module_text("MonitorCapture.Method")
#define TEXT_METHOD_D3D obs_module_text("MonitorCapture.Method.D3D11")
#define TEXT_METHOD_WGC obs_module_text("MonitorCapture.Method.WGC")
#define TEXT_METHOD_GDI obs_module_text("MonitorCapture.Method.BitBlt")
#define TEXT_TIPS_D3D obs_module_text("MonitorCapture.Tips.D3D11")
#define TEXT_TIPS_WGC obs_module_text("MonitorCapture.Tips.WGC")
#define TEXT_TIPS_GDI obs_module_text("MonitorCapture.Tips.BitBlt")

const auto RESET_INTERVAL_MILLISEC = 3000; // millisecond

enum class monitor_capture_method {
	METHOD_NONE = 0,
	METHOD_D3D,
	METHOD_WGC,
	METHOD_BITBLT,
};

#define DEFAULT_METHOD monitor_capture_method::METHOD_D3D

using PFN_winrt_capture_supported = BOOL (*)();
using PFN_winrt_capture_init_monitor = struct winrt_capture *(*)(BOOL cursor, HMONITOR monitor);
using PFN_winrt_capture_free = void (*)(struct winrt_capture *capture);
using PFN_winrt_capture_active = BOOL (*)(const struct winrt_capture *capture);
using PFN_winrt_capture_success = BOOL (*)(const struct winrt_capture *capture);
using PFN_winrt_capture_show_cursor = BOOL (*)(const struct winrt_capture *capture, BOOL visible);
using PFN_winrt_capture_render = void (*)(struct winrt_capture *capture, gs_effect_t *effect);
using PFN_winrt_capture_width = uint32_t (*)(const struct winrt_capture *capture);
using PFN_winrt_capture_height = uint32_t (*)(const struct winrt_capture *capture);

struct winrt_exports {
	PFN_winrt_capture_supported winrt_capture_supported;
	PFN_winrt_capture_init_monitor winrt_capture_init_monitor;
	PFN_winrt_capture_free winrt_capture_free;
	PFN_winrt_capture_active winrt_capture_active;
	PFN_winrt_capture_success winrt_capture_success;
	PFN_winrt_capture_show_cursor winrt_capture_show_cursor;
	PFN_winrt_capture_render winrt_capture_render;
	PFN_winrt_capture_width winrt_capture_width;
	PFN_winrt_capture_height winrt_capture_height;
};

#define WINRT_IMPORT(func)                                                              \
	do {                                                                            \
		exports->func = (PFN_##func)os_dlsym(winrt_module, #func);              \
		if (!exports->func) {                                                   \
			success = false;                                                \
			PLS_WARN("monitor capture",                                     \
				 "[display capture] Could not load function '%s' from " \
				 "module '%s'",                                         \
				 #func, module_name);                                   \
		}                                                                       \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *winrt_module, const char *module_name)
{
	bool success = true;

	WINRT_IMPORT(winrt_capture_supported);
	WINRT_IMPORT(winrt_capture_init_monitor);
	WINRT_IMPORT(winrt_capture_free);
	WINRT_IMPORT(winrt_capture_active);
	WINRT_IMPORT(winrt_capture_success);
	WINRT_IMPORT(winrt_capture_show_cursor);
	WINRT_IMPORT(winrt_capture_render);
	WINRT_IMPORT(winrt_capture_width);
	WINRT_IMPORT(winrt_capture_height);

	return success;
}

static std::string get_method(monitor_capture_method m);

struct prism_monitor_capture {
	obs_source_t *source = nullptr;

	monitor_info mi;

	int display_id = 0;
	int monitor_index = -1; // index of PLSMonitorManager::monitor_info_array

	enum monitor_capture_method method_temp = DEFAULT_METHOD;
	enum monitor_capture_method method = monitor_capture_method::METHOD_NONE;
	enum monitor_capture_method real_method = monitor_capture_method::METHOD_NONE;

	bool is_capture_cursor = true;
	bool monitor_lost = false;

	uint32_t pre_try_d3d = 0; // millisecond
	DUPLICATOR_PTR duplicator = DUPLICATOR_PTR();
	std::shared_ptr<PLSGdiCapture> gdi_capture;
	std::shared_ptr<PLSCursorCapture> cursor_capture;

	bool method_invalid_log = false;
	bool init_wgc_failed = false;
	bool wgc_supported = false;
	void *winrt_module = nullptr;
	struct winrt_exports exports = {};
	struct winrt_capture *capture_winrt = nullptr;
	HMONITOR winrt_handle = nullptr;
};

static void free_duplicator_data(struct prism_monitor_capture *capture)
{
	capture->init_wgc_failed = false;

	if (capture->duplicator) {
		PLSAutoLockRender alr;
		capture->duplicator = DUPLICATOR_PTR();
	}
}

static inline void update_settings_properity(struct prism_monitor_capture *mc, obs_data_t *settings)
{
	auto new_monitor = (int)obs_data_get_int(settings, "monitor");

	mc->display_id = new_monitor;
	mc->monitor_index = new_monitor;
	mc->is_capture_cursor = obs_data_get_bool(settings, "capture_cursor");
	mc->method_temp = (monitor_capture_method)obs_data_get_int(settings, "method");
	mc->pre_try_d3d = 0;
	mc->init_wgc_failed = false;
	mc->mi = monitor_info();
	mc->method_invalid_log = false;

	info("monitor updated. displayID:%d monitor_index:%d capture_cursor:%d method:%s", mc->display_id, mc->monitor_index, mc->is_capture_cursor, get_method(mc->method_temp).c_str());
}

static const char *prism_monitor_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("MonitorCapture");
}

static void prism_monitor_update(void *data, obs_data_t *settings)
{
	auto mc = static_cast<prism_monitor_capture *>(data);
	auto new_monitor = (int)obs_data_get_int(settings, "monitor");
	if (mc->display_id != new_monitor || mc->monitor_index != new_monitor) {
		monitor_info info;
		bool found = PLSMonitorManager::get_instance()->find_monitor(new_monitor, new_monitor, info);
		if (found) {
			std::array<char, 40> res;
			sprintf_s(res.data(), res.size(), "%dx%d", info.width, info.height);
			PLS_LOGEX(PLS_LOG_INFO, "monitor_capture", {{"plugin", "prism_monitor_capture"}, {"editEvent", res.data()}}, "Nelo action log. <prism_monitor_capture> [editEvent : %s]", res.data());
		}
	}

	update_settings_properity(static_cast<prism_monitor_capture *>(data), settings);
}

static uint32_t prism_monitor_width(void *data)
{
	auto capture = static_cast<prism_monitor_capture *>(data);
	if (capture->duplicator)
		return capture->mi.rotation % 180 == 0 ? capture->mi.width : capture->mi.height;
	else if (capture->capture_winrt && capture->exports.winrt_capture_success(capture->capture_winrt))
		return capture->exports.winrt_capture_width(capture->capture_winrt);
	else
		return capture->mi.width;
}

static uint32_t prism_monitor_height(void *data)
{
	auto capture = static_cast<prism_monitor_capture *>(data);
	if (capture->duplicator)
		return capture->mi.rotation % 180 == 0 ? capture->mi.height : capture->mi.width;
	else if (capture->capture_winrt && capture->exports.winrt_capture_success(capture->capture_winrt))
		return capture->exports.winrt_capture_height(capture->capture_winrt);
	else
		return capture->mi.height;
}

static void prism_monitor_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "capture_cursor", true);
	obs_data_set_default_int(settings, "method", (long long)DEFAULT_METHOD);
}

static bool monitor_capture_method_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	update_settings_properity(static_cast<prism_monitor_capture *>(obs_properties_get_param(props)), settings);
	return true;
}

static obs_properties_t *prism_monitor_properties(void *data)
{
	PLSMonitorManager::get_instance()->reload_monitor(false);

	obs_properties_t *props = obs_properties_create();

	auto mc = (prism_monitor_capture *)data;
	obs_properties_set_param(props, mc, nullptr);

	obs_property_t *monitors = obs_properties_add_list(props, "monitor", TEXT_MONITOR, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	vector<monitor_info> monitor_array = PLSMonitorManager::get_instance()->get_monitor();
	vector<monitor_info>::iterator iter = monitor_array.begin();
	std::array<char, 40> header_title;
	std::array<char, 512> monitor_desc;
	for (int index = 0; iter != monitor_array.end(); iter++) {
		if (iter->friendly_name.empty()) {
			snprintf(header_title.data(), header_title.size(), "%s %d", TEXT_MONITOR, index);
		} else {
			snprintf(header_title.data(), header_title.size(), "%s", iter->friendly_name.c_str());
		}

		if (iter->is_primary) {
			snprintf(monitor_desc.data(), monitor_desc.size(), "%s: %dx%d %s", header_title.data(), iter->width, iter->height, TEXT_PRIMARY_MONITOR);
		} else {
			snprintf(monitor_desc.data(), monitor_desc.size(), "%s: %dx%d", header_title.data(), iter->width, iter->height);
		}

		if (iter->display_id > 0) {
			obs_property_list_add_int(monitors, monitor_desc.data(), iter->display_id);
		} else {
			obs_property_list_add_int(monitors, monitor_desc.data(), index);
		}

		index++;
	}

	obs_property_t *p = obs_properties_add_list(props, "method", TEXT_METHOD, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	size_t idx;

	idx = obs_property_list_add_int(p, TEXT_METHOD_D3D, (long long)monitor_capture_method::METHOD_D3D);

	idx = obs_property_list_add_int(p, TEXT_METHOD_WGC, (long long)monitor_capture_method::METHOD_WGC);

	idx = obs_property_list_add_int(p, TEXT_METHOD_GDI, (long long)monitor_capture_method::METHOD_BITBLT);

	obs_property_list_item_disable(p, 0, !PLSWindowVersion::is_support_monitor_duplicate());
	if (mc) {
		obs_property_list_item_disable(p, 1, !mc->wgc_supported);
		obs_property_set_modified_callback(p, monitor_capture_method_changed);
	}

	obs_properties_add_bool(props, "capture_cursor", TEXT_CAPTURE_CURSOR);
	return props;
}

static void *prism_monitor_create(obs_data_t *settings, obs_source_t *source)
{
	auto mc = pls_new<prism_monitor_capture>();

	obs_enter_graphics();
	const bool uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	if (uses_d3d11) {
		static const char *const module_name = "libobs-winrt";
		mc->winrt_module = os_dlopen(module_name);
		if (mc->winrt_module && load_winrt_imports(&mc->exports, mc->winrt_module, module_name) && mc->exports.winrt_capture_supported()) {
			mc->wgc_supported = true;
		} else {
			mc->wgc_supported = false;
		}

		//PRISM/WangShaohui/20200525/#2928/save log for WGC
		static bool wgc_log_saved = false;
		if (!wgc_log_saved) {
			wgc_log_saved = true;
			info("[display capture] support WGC : %s", mc->wgc_supported ? "YES" : "NO");
		}
	}

	mc->source = source;
	mc->cursor_capture = std::make_shared<PLSCursorCapture>();
	mc->gdi_capture = std::make_shared<PLSGdiCapture>();
	mc->duplicator = DUPLICATOR_PTR();
	mc->is_capture_cursor = false;
	mc->display_id = 0;
	mc->monitor_index = -1;
	mc->pre_try_d3d = 0;
	mc->monitor_lost = false;
	update_settings_properity(mc, settings);

	return mc;
}

struct winrt_destroy_info {
	void *module_ptr = nullptr;
	struct winrt_exports apis = {};
	struct winrt_capture *winrt_obj = nullptr;
};

static void prism_destroy_wgc_real(void *data)
{
	auto info = (winrt_destroy_info *)data;

	if (info->winrt_obj) {
		info->apis.winrt_capture_free(info->winrt_obj);
	}

	if (info->module_ptr) {
		os_dlclose(info->module_ptr);
	}

	pls_delete(info);
}

static void post_destroy_wgc(prism_monitor_capture *capture)
{
	auto info = pls_new<winrt_destroy_info>();
	info->module_ptr = capture->winrt_module;
	info->apis = capture->exports;
	info->winrt_obj = capture->capture_winrt;

	capture->winrt_module = nullptr;
	capture->capture_winrt = nullptr;
	capture->winrt_handle = nullptr;

	obs_queue_task(OBS_TASK_GRAPHICS, prism_destroy_wgc_real, info, false);
}

static void prism_monitor_destroy(void *data)
{
	auto capture = static_cast<prism_monitor_capture *>(data);

	post_destroy_wgc(capture);
	free_duplicator_data(capture);

	capture->gdi_capture.reset();
	capture->cursor_capture.reset();

	pls_delete(capture);
}

static void draw_cursor(const struct prism_monitor_capture *capture)
{
	capture->cursor_capture->cursor_render(-capture->mi.offset_x, -capture->mi.offset_y, 1.0f, 1.0f, capture->mi.rotation % 180 == 0 ? capture->mi.width : capture->mi.height,
					       capture->mi.rotation % 180 == 0 ? capture->mi.height : capture->mi.width);
}

static void prism_monitor_handle_rotation(const prism_monitor_capture *capture, int rot)
{
	if (capture->duplicator) {
		float x = 0.0f;
		float y = 0.0f;
		switch (rot) {
		case 90:
			x = (float)capture->mi.height;
			break;
		case 180:
			x = (float)capture->mi.width;
			y = (float)capture->mi.height;
			break;
		case 270:
			y = (float)capture->mi.width;
			break;
		default:
			break;
		}
		gs_matrix_translate3f(x, y, 0.0f);
		gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD((float)rot));
	}
}

static void prism_monitor_render_texture(const prism_monitor_capture *capture, gs_texture_t *texture, gs_effect_t *effect)
{
	PLSAutoLockRender alr;

	effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);

	int rot = capture->mi.rotation;

	while (gs_effect_loop(effect, "Draw")) {
		if (rot != 0) {
			gs_matrix_push();
			prism_monitor_handle_rotation(capture, rot);
		}

		obs_source_draw(texture, 0, 0, 0, 0, false);

		if (rot != 0)
			gs_matrix_pop();
	}
}

static void prism_monitor_render_d3d_gdi(const struct prism_monitor_capture *capture, gs_texture_t *texture, gs_effect_t *effect)
{
	prism_monitor_render_texture(capture, texture, effect);

	if (capture->is_capture_cursor) {
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		while (gs_effect_loop(effect, "Draw")) {
			draw_cursor(capture);
		}
	}
}

void update_real_method(struct prism_monitor_capture *mc, enum monitor_capture_method m)
{
	if (mc->real_method != m) {
		mc->real_method = m;
		info("[display capture] Really using method: %s, selected: %s", get_method(m).c_str(), get_method(mc->method).c_str());
	}
}

static void prism_monitor_render(void *data, gs_effect_t *effect)
{
	auto mc = static_cast<prism_monitor_capture *>(data);

	if (mc->monitor_lost || !obs_source_showing(mc->source))
		return;

	PLSAutoLockRender alr;

	if (mc->method == monitor_capture_method::METHOD_D3D && mc->duplicator) {
		gs_texture_t *texture = mc->duplicator->get_texture();
		if (texture) {
			prism_monitor_render_d3d_gdi(mc, texture, effect);
			update_real_method(mc, monitor_capture_method::METHOD_D3D);
			return;
		}
	}

	if (mc->method == monitor_capture_method::METHOD_WGC && mc->capture_winrt) {
		gs_effect_t *const opaque = obs_get_base_effect(OBS_EFFECT_OPAQUE);
		if (mc->exports.winrt_capture_active(mc->capture_winrt)) {
			if (mc->exports.winrt_capture_success(mc->capture_winrt)) {
				mc->exports.winrt_capture_render(mc->capture_winrt, opaque);
				update_real_method(mc, monitor_capture_method::METHOD_WGC);
				return;
			}
		} else {
			mc->exports.winrt_capture_free(mc->capture_winrt);
			mc->capture_winrt = nullptr;
		}
	}

	update_real_method(mc, monitor_capture_method::METHOD_BITBLT);

	if (mc->method != monitor_capture_method::METHOD_BITBLT && !mc->method_invalid_log) {
		mc->method_invalid_log = true;
		warn("[display capture] %s method is invalid and choose GDI", get_method(mc->method).c_str());
	}

	if (mc->gdi_capture->check_init(mc->mi.width, mc->mi.height)) {
		mc->gdi_capture->capture(mc->mi.offset_x, mc->mi.offset_y, false, mc->mi.offset_x, mc->mi.offset_y, 1.0);
		gs_texture_t *texture = mc->gdi_capture->get_texture();
		if (texture) {
			prism_monitor_render_d3d_gdi(mc, texture, effect);
		}
	}
}

static std::string get_method(monitor_capture_method m)
{
	switch (m) {
	case monitor_capture_method::METHOD_BITBLT:
		return "GDI";
	case monitor_capture_method::METHOD_D3D:
		return "D3D11";
	case monitor_capture_method::METHOD_WGC:
		return "WGC";
	default:
		return "unknown";
	}
}

void update_method(struct prism_monitor_capture *mc)
{
	if (mc->method != mc->method_temp) {
		mc->method = mc->method_temp;
	}

	if (mc->duplicator && mc->method != monitor_capture_method::METHOD_D3D) {
		free_duplicator_data(mc);
	}

	if (mc->capture_winrt && mc->method != monitor_capture_method::METHOD_WGC) {
		mc->exports.winrt_capture_free(mc->capture_winrt);
		mc->capture_winrt = nullptr;
	}
}

void tick_check(struct prism_monitor_capture *mc, const monitor_info &info)
{
	if (mc->capture_winrt && mc->winrt_handle != info.handle) {
		mc->exports.winrt_capture_free(mc->capture_winrt);
		mc->capture_winrt = nullptr;
		mc->winrt_handle = nullptr;
		mc->init_wgc_failed = false;
	}

	if (mc->duplicator && (info.display_id != mc->mi.display_id || info.adapter_index != mc->mi.adapter_index || info.adapter_output_index != mc->mi.adapter_output_index)) {
		free_duplicator_data(mc);
		PLSMonitorDuplicatorPool::get_instance()->clear();
		info("[display capture] Display source should be reset. oldAdapter:%d oldOutput:%d newAdapter:%d newOutput:%d", mc->mi.adapter_index, mc->mi.adapter_output_index, info.adapter_index,
		     info.adapter_output_index);
	}

	if (mc->duplicator && !mc->duplicator->is_adapter_valid()) {
		free_duplicator_data(mc);
		PLSMonitorDuplicatorPool::get_instance()->clear();
		info("[display capture] Display source should be reset because adapter is changed.");
	}

	if (mc->duplicator && mc->duplicator->is_removed()) {
		free_duplicator_data(mc);
	}
}

void tick_D3D(struct prism_monitor_capture *mc)
{
	if (!PLSWindowVersion::is_support_monitor_duplicate())
		return;

	if (!mc->duplicator) {
		DWORD currentTime = GetTickCount();
		if (mc->pre_try_d3d >= currentTime) // handle exception
			mc->pre_try_d3d = currentTime;

		if (currentTime - mc->pre_try_d3d >= RESET_INTERVAL_MILLISEC) {
			mc->duplicator = PLSMonitorDuplicatorPool::get_instance()->get_duplicator(mc->mi.adapter_index, mc->mi.adapter_output_index, mc->mi.display_id, true);
			mc->pre_try_d3d = currentTime;
		}
	}

	if (!mc->duplicator)
		return;

	if (mc->duplicator->update_frame()) {
		const gs_texture_t *texture = mc->duplicator->get_texture();
		if (texture) {
			mc->mi.width = gs_texture_get_width(texture);
			mc->mi.height = gs_texture_get_height(texture);
			return; // D3D capture is valid, skip WGC capture
		}
	} else {
		free_duplicator_data(mc);
	}
}

void tick_WGC(struct prism_monitor_capture *mc, const monitor_info &info)
{
	bool wgc_available = (mc->wgc_supported && !mc->init_wgc_failed);
	if (!wgc_available)
		return;

	if (!mc->capture_winrt) {
		mc->capture_winrt = mc->exports.winrt_capture_init_monitor(mc->is_capture_cursor, info.handle);
		mc->winrt_handle = info.handle;

		if (!mc->capture_winrt)
			mc->init_wgc_failed = true;
	}

	if (mc->capture_winrt)
		mc->exports.winrt_capture_show_cursor(mc->capture_winrt, mc->is_capture_cursor);
}

extern void handle_hooked_message();
static void prism_monitor_tick(void *data, float)
{
	handle_hooked_message();

	auto mc = static_cast<prism_monitor_capture *>(data);

	update_method(mc);

	monitor_info info;
	mc->monitor_lost = !PLSMonitorManager::get_instance()->find_monitor(mc->monitor_index, mc->display_id, info);
	if (mc->monitor_lost)
		return;

	tick_check(mc, info);

	if (!obs_source_showing(mc->source))
		return;

	mc->mi = info;

	PLSAutoLockRender alr;

	if (mc->method == monitor_capture_method::METHOD_D3D) {
		tick_D3D(mc);
		return;
	}

	if (mc->method == monitor_capture_method::METHOD_WGC) {

		tick_WGC(mc, info);
		return;
	}
}

//PRISM/ZengQin/20210604/#none/Get properties parameters
static obs_data_t *prism_monitor_props_params(void *data)
{
	if (!data)
		return nullptr;

	auto mc = static_cast<prism_monitor_capture *>(data);

	monitor_info info;
	PLSMonitorManager::get_instance()->find_monitor(mc->monitor_index, mc->display_id, info);

	obs_data_t *params = obs_data_create();
	obs_data_set_string(params, "monitor", info.friendly_name.c_str());
	obs_data_set_string(params, "method", get_method(mc->method_temp).c_str());
	return params;
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
}
