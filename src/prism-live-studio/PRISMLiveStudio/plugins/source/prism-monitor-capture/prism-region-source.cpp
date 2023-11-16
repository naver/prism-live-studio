#include <obs-module.h>
#include <liblog.h>
#include "monitor-info.h"
#include "monitor-duplicator-pool.h"
#include "window-version.h"
#include "gdi-capture.h"
#include "cursor-capture.h"
#include <libutils-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <pls/pls-obs-api.h>

#define do_warn(format, ...) PLS_WARN("monitor capture", "[monitor(region) '%s' %p] " format, obs_source_get_name(source), source, ##__VA_ARGS__)
#define do_info(format, ...) PLS_INFO("monitor capture", "[monitor(region) '%s' %p] " format, obs_source_get_name(source), source, ##__VA_ARGS__)

#define warn(format, ...) do_warn(format, ##__VA_ARGS__)
#define info(format, ...) do_info(format, ##__VA_ARGS__)

const auto REGION_MIN_CX = 0;
const auto REGION_MIN_CY = 0;
const auto REGION_DEFAULT_CX = REGION_MIN_CX;
const auto REGION_DEFAULT_CY = REGION_MIN_CY;

const auto RETRY_DUPLICATOR_INTERVAL = 2000; // millisecond

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

#define RELEASE_TEXTURE(tex)             \
	if (tex) {                       \
		gs_texture_destroy(tex); \
		tex = nullptr;           \
	}

#define CHECK_TEXTURE(tex, cx, cy)                                                         \
	if (tex) {                                                                         \
		if (gs_texture_get_width(tex) != cx || gs_texture_get_height(tex) != cy) { \
			gs_texture_destroy(tex);                                           \
			tex = nullptr;                                                     \
		}                                                                          \
	}

enum class RegionCaptureType {
	RegionTypeNone = 0,
	RegionTypeGDI,
	RegionTypeGPU,
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

struct hit_info {
	int display_id;
	int rotation;
	int adapter_index;
	int adapter_output_index;

	// rect in monitor texture without rotation, its left-top is in duplicator texture without rotation.
	// if there exist rotation with monitor, before using these variables, we should firstly rotate duplicator texture.
	int src_left;
	int src_top;
	int src_right;
	int src_bottom;

	// rect in canvas texture
	int canvas_left;
	int canvas_top;
	int canvas_right;
	int canvas_bottom;
};

struct prism_region_gpu {
	obs_source_t *source = nullptr;

	DWORD pre_fail_time = 0;
	gs_texture_t *output_texture = nullptr;
	gs_texture_t *rotate_target = nullptr;

	//---------------------------------------
	prism_region_gpu() = default;
	virtual ~prism_region_gpu();

	prism_region_gpu(const prism_region_gpu &) = delete;
	prism_region_gpu &operator=(const prism_region_gpu &) = delete;
	prism_region_gpu(prism_region_gpu &&) = delete;
	prism_region_gpu &operator=(prism_region_gpu &&) = delete;

	void clear_texture();
	bool update_gpu_texture(const prism_region_settings &region_settings, const std::vector<hit_info> &hit_list);
	gs_texture_t *get_texture();

private:
	bool check_output_texture(int cx, int cy);
	bool render_output_texture(const std::vector<hit_info> &hit_list);
	gs_texture_t *render_rotate_texture(gs_texture_t *monitor_texture, int rotation);
};

struct prism_region_source {
	obs_source_t *source = nullptr;

	prism_region_settings region_settings;

	std::atomic_bool source_valid;

	PLSGdiCapture gdi_capture;
	prism_region_gpu gpu_capture;
	PLSCursorCapture cursor_capture; // used for GPU capture
	RegionCaptureType capture_type = RegionCaptureType::RegionTypeNone;

	//---------------------------------------
	prism_region_source(obs_data_t *settings, obs_source_t *source_);
	virtual ~prism_region_source();

	prism_region_source(const prism_region_source &) = delete;
	prism_region_source &operator=(const prism_region_source &) = delete;
	prism_region_source(prism_region_source &&) = delete;
	prism_region_source &operator=(prism_region_source &&) = delete;

	void tick();
	void render();
	void update(obs_data_t *settings);

private:
	gs_texture_t *get_region_texture();
	void set_capture_type(RegionCaptureType type);
	void hit_monitor_test(std::vector<hit_info> &hit_list) const;
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

prism_region_gpu::~prism_region_gpu()
{
	clear_texture();
}

gs_texture_t *prism_region_gpu::get_texture()
{
	return output_texture;
}

bool prism_region_gpu::update_gpu_texture(const prism_region_settings &region_settings, const std::vector<hit_info> &hit_list)
{
	// Sometimes, duplicator is not available, we should retry it with interval
	if (GetTickCount() - pre_fail_time < RETRY_DUPLICATOR_INTERVAL) {
		return false;
	}

	do {
		if (!check_output_texture(region_settings.width, region_settings.height)) {
			break;
		}

		if (!render_output_texture(hit_list)) {
			break;
		}

		pre_fail_time = 0;
		return true;

	} while (false);

	clear_texture();
	pre_fail_time = GetTickCount();

	return false;
}

void prism_region_gpu::clear_texture()
{
	PLSAutoLockRender alr;

	RELEASE_TEXTURE(output_texture)
	RELEASE_TEXTURE(rotate_target)
}

bool prism_region_gpu::check_output_texture(int cx, int cy)
{
	PLSAutoLockRender alr;

	CHECK_TEXTURE(output_texture, cx, cy)
	if (!output_texture) {
		output_texture = gs_texture_create(cx, cy, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
		if (!output_texture) {
			warn("Failed to create output texture for region source. %dx%d", cx, cy);
		}
	}

	return !!output_texture;
}

bool prism_region_gpu::render_output_texture(const std::vector<hit_info> &hit_list)
{
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	PLSAutoLockRender alr;

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();

	gs_texture_t *pre_target = gs_get_render_target();
	gs_set_render_target(output_texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	bool ret = true;
	for (const auto &item : hit_list) {
		DUPLICATOR_PTR duplicator = PLSMonitorDuplicatorPool::get_instance()->get_duplicator(item.adapter_index, item.adapter_output_index, item.display_id, true);
		if (!duplicator) {
			ret = false;
			break;
		}

		if (!duplicator->is_adapter_valid()) {
			PLSMonitorDuplicatorPool::get_instance()->clear();
			ret = false;
			break;
		}

		if (!duplicator->update_frame()) {
			ret = false;
			break;
		}

		gs_texture_t *rotated_texture = render_rotate_texture(duplicator->get_texture(), item.rotation);
		if (!rotated_texture) {
			ret = false;
			break;
		}

		const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
		gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");

		int dest_cx = (item.canvas_right - item.canvas_left);
		int dest_cy = (item.canvas_bottom - item.canvas_top);

		gs_effect_set_texture(param, rotated_texture);
		gs_matrix_identity();
		gs_ortho(0.0f, (float)dest_cx, 0.0f, (float)dest_cy, -100.0f, 100.0f);
		gs_set_viewport(item.canvas_left, item.canvas_top, dest_cx, dest_cy);

		size_t passes = gs_technique_begin(tech);
		for (size_t i = 0; i < passes; i++) {
			gs_technique_begin_pass(tech, i);
			gs_draw_sprite_subregion(rotated_texture, false, item.src_left, item.src_top, (item.src_right - item.src_left), (item.src_bottom - item.src_top));
			gs_technique_end_pass(tech);
		}
		gs_technique_end(tech);
	}

	gs_set_render_target(pre_target, nullptr);
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();

	return ret;
}

gs_texture_t *prism_region_gpu::render_rotate_texture(gs_texture_t *monitor_texture, int rotation)
{
	if (rotation == 0) {
		return monitor_texture;
	}

	int tex_cx = gs_texture_get_width(monitor_texture);
	int tex_cy = gs_texture_get_height(monitor_texture);

	int target_cx;
	int target_cy;
	if ((rotation % 180) == 0) {
		target_cx = tex_cx;
		target_cy = tex_cy;
	} else {
		target_cx = tex_cy;
		target_cy = tex_cx;
	}

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	PLSAutoLockRender alr;

	CHECK_TEXTURE(rotate_target, target_cx, target_cy)
	if (!rotate_target) {
		rotate_target = gs_texture_create(target_cx, target_cy, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
		if (!rotate_target) {
			warn("Failed to create rotate texture for region source. %dx%d", target_cx, target_cy);
			return nullptr;
		}
	}

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_texture_t *pre_target = gs_get_render_target();
	gs_zstencil_t *pre_ztl = gs_get_zstencil_target();

	gs_set_render_target(rotate_target, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");

	gs_effect_set_texture(param, monitor_texture);
	gs_matrix_identity();
	gs_ortho(0.0f, (float)target_cx, 0.0f, (float)target_cy, -100.0f, 100.0f);
	gs_set_viewport(0, 0, target_cx, target_cy);

	while (gs_effect_loop(effect, "Draw")) {
		if (rotation != 0) {
			float x = 0.0f;
			float y = 0.0f;
			switch (rotation) {
			case 90:
				x = (float)tex_cy;
				break;
			case 180:
				x = (float)tex_cx;
				y = (float)tex_cy;
				break;
			case 270:
				y = (float)tex_cx;
				break;
			default:
				break;
			}
			gs_matrix_push();
			gs_matrix_translate3f(x, y, 0.0f);
			gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD((float)rotation));
		}

		obs_source_draw(monitor_texture, 0, 0, 0, 0, false);

		if (rotation != 0) {
			gs_matrix_pop();
		}
	}

	gs_set_render_target(pre_target, pre_ztl);
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();

	return rotate_target;
}

prism_region_source::prism_region_source(obs_data_t *settings, obs_source_t *source_) : source(source_)
{
	gpu_capture.source = source_;
	update(settings);
}

prism_region_source ::~prism_region_source()
{
	gdi_capture.uninit();
	if (PLSWindowVersion::is_support_monitor_duplicate()) {
		gpu_capture.clear_texture();
	}
}

extern void handle_hooked_message();
void prism_region_source::tick()
{
	handle_hooked_message();

	if (region_settings.region_empty()) {
		source_valid = true;
	} else {
		std::vector<hit_info> hit_list;
		hit_monitor_test(hit_list);

		if (hit_list.empty()) {
			source_valid = false;
		} else {
			source_valid = true;
		}

		PLSAutoLockRender alr;

		if (PLSWindowVersion::is_support_monitor_duplicate() && gpu_capture.update_gpu_texture(region_settings, hit_list)) {
			return; // GPU works normally, we will skip GDI capture
		}

		// GPU is unavailable, we have to use GDI capture
		if (gdi_capture.check_init(region_settings.width, region_settings.height)) {
			gdi_capture.capture(region_settings.left, region_settings.top, region_settings.capture_cursor, region_settings.left, region_settings.top, 1.0);
		}
	}
}

void prism_region_source::render()
{
	PLSAutoLockRender alr;

	gs_texture_t *tex = get_region_texture();
	if (!tex) {
		return;
	}

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_OPAQUE);
	while (gs_effect_loop(effect, "Draw")) {
		obs_source_draw(tex, 0, 0, 0, 0, false);
	}

	if (region_settings.capture_cursor && capture_type == RegionCaptureType::RegionTypeGPU) {
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		while (gs_effect_loop(effect, "Draw")) {
			cursor_capture.cursor_render(-region_settings.left, -region_settings.top, 1.0f, 1.0f, region_settings.width, region_settings.height);
		}
	}
}

gs_texture_t *prism_region_source::get_region_texture()
{
	if (region_settings.region_empty()) {
		return nullptr;
	}

	if (!source_valid) {
		return nullptr;
	}

	if (PLSWindowVersion::is_support_monitor_duplicate()) {
		gs_texture_t *texture = gpu_capture.get_texture();
		if (texture) {
			set_capture_type(RegionCaptureType::RegionTypeGPU);
			return texture;
		}
	}

	gs_texture_t *texture = gdi_capture.get_texture();
	if (texture) {
		set_capture_type(RegionCaptureType::RegionTypeGDI);
		return texture;
	}

	return nullptr;
}

void prism_region_source::set_capture_type(RegionCaptureType type)
{
	if (capture_type == type) {
		return;
	}

	capture_type = type;
	if (type == RegionCaptureType::RegionTypeGPU) {
		info("Region capture type is GPU.");
	} else {
		info("Region capture type is GDI.");
	}
}

void prism_region_source::hit_monitor_test(std::vector<hit_info> &hit_list) const
{
	int dest_left = region_settings.left;
	int dest_top = region_settings.top;
	int dest_right = region_settings.left + region_settings.width;
	int dest_bottom = region_settings.top + region_settings.height;

	std::vector<monitor_info> monitors = PLSMonitorManager::get_instance()->get_monitor();
	for (const auto &item : monitors) {
		int monitor_right = item.offset_x + item.width;
		int monitor_bottom = item.offset_y + item.height;

		if (dest_right <= item.offset_x || dest_bottom <= item.offset_y || dest_left >= monitor_right || dest_top >= monitor_bottom) {
			continue; // never hit this monitor
		}

		hit_info info;
		info.display_id = item.display_id;
		info.rotation = item.rotation;
		info.adapter_index = item.adapter_index;
		info.adapter_output_index = item.adapter_output_index;

		if (dest_left < item.offset_x) {
			info.src_left = 0;
			info.canvas_left = item.offset_x - dest_left;
		} else {
			info.src_left = dest_left - item.offset_x;
			info.canvas_left = 0;
		}

		if (dest_top < item.offset_y) {
			info.src_top = 0;
			info.canvas_top = item.offset_y - dest_top;
		} else {
			info.src_top = dest_top - item.offset_y;
			info.canvas_top = 0;
		}

		if (dest_right < monitor_right) {
			info.src_right = dest_right - item.offset_x;
			info.canvas_right = region_settings.width;
		} else {
			info.src_right = item.width;
			info.canvas_right = monitor_right - dest_left;
		}

		if (dest_bottom < monitor_bottom) {
			info.src_bottom = dest_bottom - item.offset_y;
			info.canvas_bottom = region_settings.height;
		} else {
			info.src_bottom = item.height;
			info.canvas_bottom = monitor_bottom - dest_top;
		}

		hit_list.push_back(info);
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
	obs_data_release(region_obj);

	temp.capture_cursor = obs_data_get_bool(settings, REGION_KEY_CAPTURE_CURSOR);

	temp.keep_valid();

	//-------------------------------------------------
	region_settings = temp;
	info("region updated. offset:%d,%d  size:%dx%d  cursor:%d", temp.left, temp.top, temp.width, temp.height, temp.capture_cursor);

	if (PLSWindowVersion::is_support_monitor_duplicate()) {
		tick(); // Because GPU texture is updated in worker thread, so here we invoke tick() to active worker thread early.
	}
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

	info.destroy = [](void *data) { pls_delete(static_cast<prism_region_source *>(data)); };

	info.get_width = [](void *data) {
		auto region = static_cast<prism_region_source *>(data);
		if (region->source_valid) {
			return (uint32_t)region->region_settings.width;
		} else {
			return (uint32_t)0;
		}
	};

	info.get_height = [](void *data) {
		auto region = static_cast<prism_region_source *>(data);
		if (region->source_valid) {
			return (uint32_t)region->region_settings.height;
		} else {
			return (uint32_t)0;
		}
	};

	info.video_tick = [](void *data, float) { static_cast<prism_region_source *>(data)->tick(); };
	info.video_render = [](void *data, gs_effect_t *) { static_cast<prism_region_source *>(data)->render(); };
	info.get_properties = region_source_get_properties;
	info.get_defaults = region_source_get_defaults;
	info.update = [](void *data, obs_data_t *settings) { static_cast<prism_region_source *>(data)->update(settings); };

	obs_register_source(&info);
}
