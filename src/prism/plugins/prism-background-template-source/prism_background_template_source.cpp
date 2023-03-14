#include <Windows.h>
#include <obs.h>
#include <stdio.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <sys/stat.h>
#include "prism_background_template_source.hpp"
#include <filesystem>
#include <string>
#include <codecvt>
#include <util/threading.h>
#include <log.h>

#define debug(format, ...) PLS_PLUGIN_DEBUG(format, ##__VA_ARGS__)
#define info(format, ...) PLS_PLUGIN_INFO(format, ##__VA_ARGS__)
#define warn(format, ...) PLS_PLUGIN_WARN(format, ##__VA_ARGS__)
#define error(format, ...) PLS_PLUGIN_ERROR(format, ##__VA_ARGS__)

enum class image_type_t { UNKOWN, MOTION, STATIC };

static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

enum resource_type_t { RT_Image, RT_Video, RT_Unkonwn };

struct background_template_source {
	obs_source_t *source = nullptr;

	resource_type_t resource_type = RT_Unkonwn;

	char *file_path = nullptr;
	char *image_file_path = nullptr;
	char *thumbnail_file_path = nullptr;

	bool is_motion_image = false;
	bool motion_enabled = true;

	bool image_active = false;
	bool video_active = false;

	obs_source_t *image = nullptr;
	obs_source_t *video = nullptr;

	gs_texture_t *source_texture = nullptr;

	bool source_texture_clear = false;

	pthread_mutex_t src_mutex;
	uint32_t src_width = 0;
	uint32_t src_height = 0;

	void set_image_active(bool active) { set_source_active(image, this->image_active, active); }

	void set_video_active(bool active) { set_source_active(video, this->video_active, active); }

	void set_source_active(obs_source_t *source, bool &source_active, bool active)
	{
		if (source_active == active) {
			return;
		}

		source_active = active;

		if (active) {
			obs_source_inc_active(source);
		} else {
			obs_source_dec_active(source);
		}
	}

	bool resource_is_image() const
	{
		switch (resource_type) {
		case RT_Image:
			return true;
		case RT_Video:
			return !motion_enabled;
		case RT_Unkonwn:
		default:
			return false;
		}
	}

	const char *get_image_file_path() const
	{
		if (!is_resource_valid()) {
			return nullptr;
		}

		switch (resource_type) {
		case RT_Image:
			return (is_motion_image && (!motion_enabled)) ? image_file_path : file_path;
		case RT_Video:
			return image_file_path;
		case RT_Unkonwn:
		default:
			return nullptr;
		}
	}

	bool resource_is_video() const
	{
		switch (resource_type) {
		case RT_Image:
			return false;
		case RT_Video:
			return motion_enabled;
		case RT_Unkonwn:
		default:
			return false;
		}
	}

	const char *get_video_file_path() const
	{
		if (!is_resource_valid()) {
			return nullptr;
		}

		switch (resource_type) {
		case RT_Image:
			return nullptr;
		case RT_Video:
			return motion_enabled ? file_path : nullptr;
		case RT_Unkonwn:
		default:
			return nullptr;
		}
	}

	void update_source_file_path(bool force_update_image, bool force_update_video)
	{
		if (!force_update_image) {
			obs_data_t *old_image_settings = obs_source_get_settings(image);
			bool image_settings_changed = is_changed(obs_data_get_string(old_image_settings, "file"), get_image_file_path()) ||
						      is_changed(obs_data_get_string(old_image_settings, "thumbnail_file"), thumbnail_file_path);
			obs_data_release(old_image_settings);

			if (image_settings_changed) {
				update_image_source_file_path();
			}

		} else {
			update_image_source_file_path();
		}

		if (!force_update_video) {
			obs_data_t *old_video_settings = obs_source_get_settings(video);
			bool video_settings_changed = is_changed(obs_data_get_string(old_video_settings, "local_file"), get_video_file_path()) ||
						      is_changed(obs_data_get_string(old_video_settings, "thumbnail_file"), thumbnail_file_path);
			obs_data_release(old_video_settings);

			if (video_settings_changed) {
				update_video_source_file_path();
			}
		} else {
			update_video_source_file_path();
		}
	}

	void update_image_source_file_path()
	{
		obs_data_t *image_settings = obs_data_create();
		obs_data_set_string(image_settings, "file", get_image_file_path());
		obs_data_set_string(image_settings, "thumbnail_file", thumbnail_file_path);
		obs_source_update(image, image_settings);
		obs_data_release(image_settings);
	}

	void update_video_source_file_path()
	{
		obs_data_t *video_settings = obs_data_create();
		obs_data_set_string(video_settings, "local_file", get_video_file_path());
		obs_data_set_string(video_settings, "thumbnail_file", thumbnail_file_path);
		obs_source_update(video, video_settings);
		obs_data_release(video_settings);
	}

	bool is_changed(const char *file_path, const char *new_file_path)
	{
		if (file_path == new_file_path) {
			return false;
		} else if (!file_path || !new_file_path) {
			return true;
		} else if (strcmp(file_path, new_file_path)) {
			return true;
		}
		return false;
	}

	bool is_resource_valid() const
	{
		if (/*is_file_exists(file_path) && is_file_exists(image_file_path) && */ is_file_exists(thumbnail_file_path)) {
			return true;
		}
		return false;
	}

	bool is_file_exists(const char *file_path) const
	{
		if (!file_path || !file_path[0]) {
			return false;
		}

		int wpath_length = MultiByteToWideChar(CP_UTF8, 0, file_path, -1, nullptr, 0);
		if (wpath_length > 0) {
			wchar_t *wpath = (wchar_t *)malloc((size_t(wpath_length) + 1) * sizeof(wchar_t));
			if (!wpath) {
				return false;
			}

			memset(wpath, 0, (size_t(wpath_length) + 1) * sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, file_path, -1, wpath, wpath_length + 1);

			struct _stat64 s;
			int result = _wstat64(wpath, &s);
			free(wpath);

			if (result) {
				return false;
			} else if (s.st_mode & S_IFREG) {
				return true;
			}
		}
		return false;
	}
};

static inline bool is_empty(const char *str)
{
	return !str || !str[0];
}

static inline bool is_not_empty(const char *str)
{
	return !is_empty(str);
}

static const char *get_file_path_suffix(const char *file_path)
{
	if (is_empty(file_path)) {
		return nullptr;
	}

	for (const char *tmp = file_path + strlen(file_path) - 1; tmp >= file_path; --tmp) {
		if (*tmp == '.') {
			return tmp;
		}
	}
	return nullptr;
}

static void background_template_source_init(background_template_source *context, obs_data_t *settings)
{
	image_type_t image_type = static_cast<image_type_t>(obs_data_get_int(settings, "item_type"));
	context->is_motion_image = image_type == image_type_t::MOTION;
	context->motion_enabled = obs_data_get_bool(settings, "motion_enabled") || !obs_data_get_bool(settings, "prism_resource");

	switch (image_type) {
	case image_type_t::MOTION:
		context->resource_type = RT_Video;
		break;
	case image_type_t::STATIC:
		context->resource_type = RT_Image;
		break;
	default:
		context->resource_type = RT_Unkonwn;
		break;
	}

	const char *file_path = obs_data_get_string(settings, "file_path");
	if (context->is_changed(context->file_path, file_path)) {
		if (context->file_path) {
			free(context->file_path);
		}

		if (is_not_empty(file_path)) {
			context->file_path = _strdup(file_path);
		} else {
			context->file_path = nullptr;
		}

		//const char *suffix = get_file_path_suffix(file_path);
		//if (is_empty(suffix)) {
		//	context->resource_type = RT_Unkonwn;
		//} else if (!stricmp(suffix, ".bmp") || !stricmp(suffix, ".tga") || !stricmp(suffix, ".png") || !stricmp(suffix, ".jpeg") || !stricmp(suffix, ".jpg") || !stricmp(suffix, ".gif") ||
		//	   !stricmp(suffix, ".psd")) {
		//	context->resource_type = RT_Image; // .bmp  .tga .png .jpg .gif  .psd
		//} else if (!strcmp(suffix, ".mp4") || !strcmp(suffix, ".ts") || !strcmp(suffix, ".mov") || !strcmp(suffix, ".flv") || !strcmp(suffix, ".mkv") || !strcmp(suffix, ".avi") ||
		//	   !strcmp(suffix, ".webm")) {
		//	context->resource_type = RT_Video; // .mp4  .ts  .mov  .flv .mkv  .avi .webm
		//} else {
		//	context->resource_type = RT_Unkonwn;
		//}
	}

	const char *image_file_path = obs_data_get_string(settings, "image_file_path");
	if (context->is_changed(context->image_file_path, image_file_path)) {
		if (context->image_file_path) {
			free(context->image_file_path);
		}

		if (is_not_empty(image_file_path)) {
			context->image_file_path = _strdup(image_file_path);
		} else {
			context->image_file_path = nullptr;
		}
	}

	const char *thumbnail_file_path = obs_data_get_string(settings, "thumbnail_file_path");
	if (context->is_changed(context->thumbnail_file_path, thumbnail_file_path)) {
		if (context->thumbnail_file_path) {
			free(context->thumbnail_file_path);
		}

		if (is_not_empty(thumbnail_file_path)) {
			context->thumbnail_file_path = _strdup(thumbnail_file_path);
		} else {
			context->thumbnail_file_path = nullptr;
		}
	}
}

static void media_state_changed_callback(void *data, calldata_t *calldata)
{
	if (!data) {
		return;
	}

	background_template_source *context = (background_template_source *)data;
	obs_source_t *media_source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (media_source == context->video && !is_empty(context->get_video_file_path())) {
		obs_media_state state = obs_source_media_get_state(media_source);
		if (state == OBS_MEDIA_STATE_ERROR) {
			// item->exception_callback(item->pls_cam_effect, OBS_SOURCE_EXCEPTION_BG_FILE_ERROR, 0);
			info("background template video state error: %d", state);
			obs_source_send_notify(context->source, OBS_SOURCE_EXCEPTION_BG_FILE_ERROR, 0);
		}
	}
}

static void media_network_changed_callback(void *data, calldata_t *calldata)
{
	if (!data) {
		return;
	}

	background_template_source *context = (background_template_source *)data;
	obs_source_t *media_source = (obs_source_t *)calldata_ptr(calldata, "source");
	bool network_off = calldata_bool(calldata, "network_off");
	if (media_source == context->video && network_off && !is_empty(context->get_video_file_path())) {
		if (os_is_file_exist(context->file_path)) {
			info("background template file error when network off!");
			obs_source_send_notify(context->source, OBS_SOURCE_EXCEPTION_BG_FILE_NETWORK_ERROR, 0);
		}
	}
}

static const char *background_template_source_get_name(void *)
{
	return obs_module_text("VirtualBackground.SourceName");
}

static bool is_resource_property_modified(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	const char *item_id = obs_data_get_string(settings, "item_id");
	return (!item_id || !item_id[0]) ? true : false;
}

static obs_properties_t *background_template_source_properties(void *data)
{
	background_template_source *context = (background_template_source *)data;

	obs_properties_t *props = obs_properties_create();
	auto p = obs_properties_add_virtual_background_resource(props, "item_id", "");
	//obs_property_set_modified_callback(p, &is_resource_property_modified);
	return props;
}

static void init_image_source(struct background_template_source *context)
{
	if (!context || context->image) {
		return;
	}

	// init image source
	obs_data_t *image_settings = obs_data_create();
	obs_data_set_string(image_settings, "file", context->get_image_file_path());
	context->image = obs_source_create_private("image_source", "prism_virtual_background_image_source", image_settings);
	obs_data_release(image_settings);
}

static void init_video_source(struct background_template_source *context)
{
	if (!context || context->video) {
		return;
	}

	// init video source
	obs_data_t *video_settings = obs_data_create();
	obs_data_set_bool(video_settings, "is_local_file", true);
	obs_data_set_bool(video_settings, "bgm_source", false);
	obs_data_set_bool(video_settings, "looping", true);
	obs_data_set_bool(video_settings, "hw_decode", false);
	obs_data_set_bool(video_settings, "virtual_background_source", true);
	obs_data_set_string(video_settings, "local_file", context->get_video_file_path());
	context->video = obs_source_create_private("ffmpeg_source", "prism_virtual_background_video_source", video_settings);
	obs_source_set_audio_output_flag(context->video, false);
	signal_handler_connect_ref(obs_source_get_signal_handler(context->video), "media_state_changed", media_state_changed_callback, context);
	signal_handler_connect_ref(obs_source_get_signal_handler(context->video), "network_changed", media_network_changed_callback, context);
	obs_data_release(video_settings);
}

static void background_template_source_update(void *data, obs_data_t *settings)
{
	background_template_source *context = (background_template_source *)(data);

	bool path_is_changed = false;
	bool force_auto_restore = obs_data_get_bool(settings, "force_auto_restore");
	if (!force_auto_restore) {
		path_is_changed = context->is_changed(context->file_path, obs_data_get_string(settings, "file_path")) ||
				  context->is_changed(context->image_file_path, obs_data_get_string(settings, "image_file_path")) ||
				  context->is_changed(context->thumbnail_file_path, obs_data_get_string(settings, "thumbnail_file_path"));
		if (!(path_is_changed || context->motion_enabled != obs_data_get_bool(settings, "motion_enabled"))) {
			return;
		}
	} else {
		obs_data_set_bool(settings, "force_auto_restore", false);
	}

	background_template_source_init(context, settings);

	context->update_source_file_path(force_auto_restore, force_auto_restore);

	if (context->resource_is_image()) {
		context->set_video_active(false);
		context->set_image_active(true);
	} else if (context->resource_is_video()) {
		context->set_image_active(false);
		context->set_video_active(true);
	} else {
		context->set_image_active(false);
		context->set_video_active(false);
		context->source_texture_clear = true;
	}
}

static void background_template_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "force_auto_restore", false);
	obs_data_set_default_string(settings, "item_id", "");
	obs_data_set_default_int(settings, "item_type", 0);
	obs_data_set_default_bool(settings, "motion_enabled", true);
	obs_data_set_default_string(settings, "file_path", "");
	obs_data_set_default_string(settings, "image_file_path", "");
	obs_data_set_default_string(settings, "thumbnail_file_path", "");
}

static void source_notified(void *data, calldata_t *calldata)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!source || !data)
		return;

	int type = (int)calldata_int(calldata, "message");
	if (type != OBS_SOURCE_CREATED_FINISHED) {
		return;
	}

	struct background_template_source *context = static_cast<struct background_template_source *>(data);
	if (source == context->source) {
		init_image_source(context);
		init_video_source(context);
	}
}

static void background_template_source_destroy(void *data);

static void *background_template_source_create(obs_data_t *settings, obs_source_t *source)
{
	obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	background_template_source *context = new (std::nothrow) background_template_source();
	if (!context) {
		error("virtual background create failed, because out of memory.");
		obs_source_set_capture_valid(source, false, OBS_SOURCE_ERROR_UNKNOWN);
		return nullptr;
	}
	signal_handler_connect_ref(obs_source_get_signal_handler(source), "source_notify", source_notified, context);

	context->source = source;
	background_template_source_init(context, settings);

	pthread_mutex_init_value(&context->src_mutex);
	if (pthread_mutex_init(&context->src_mutex, NULL) != 0) {
		error("Init source mutex failed.");
		background_template_source_destroy(context);
		return nullptr;
	}
	return context;
}

static void background_template_source_destroy(void *data)
{
	background_template_source *context = (background_template_source *)(data);
	signal_handler_disconnect(obs_source_get_signal_handler(context->source), "source_notify", source_notified, context);

	if (context->file_path) {
		free(context->file_path);
	}

	if (context->image_file_path) {
		free(context->image_file_path);
	}

	if (context->thumbnail_file_path) {
		free(context->thumbnail_file_path);
	}

	if (context->image) {
		context->set_image_active(false);
		obs_source_release(context->image);
	}

	if (context->video) {
		context->set_video_active(false);
		signal_handler_disconnect(obs_source_get_signal_handler(context->video), "media_state_changed", media_state_changed_callback, context);
		signal_handler_disconnect(obs_source_get_signal_handler(context->video), "network_changed", media_network_changed_callback, context);
		obs_source_release(context->video);
	}

	if (context->source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->source_texture);
		obs_leave_graphics();
	}

	pthread_mutex_destroy(&context->src_mutex);

	delete context;
}

static uint32_t background_template_source_get_width(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	pthread_mutex_lock(&context->src_mutex);
	uint32_t width = context->src_width;
	pthread_mutex_unlock(&context->src_mutex);

	return width;
}

static uint32_t background_template_source_get_height(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	pthread_mutex_lock(&context->src_mutex);
	uint32_t height = context->src_height;
	pthread_mutex_unlock(&context->src_mutex);
	return height;
}

static void background_template_source_clear_texture(gs_texture_t *tex)
{
	if (!tex) {
		return;
	}
	obs_enter_graphics();
	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_set_render_target(tex, nullptr);
	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	gs_set_render_target(pre_rt, nullptr);
	gs_projection_pop();
	obs_leave_graphics();
}

static void background_template_source_video_render(void *data, gs_effect_t *effect)
{
	background_template_source *context = (background_template_source *)(data);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->source_texture);
	gs_draw_sprite(context->source_texture, 0, 0, 0);
}

static void background_template_source_render(void *data, obs_source_t *source)
{
	background_template_source *bg_source = (background_template_source *)(data);
	uint32_t source_width = obs_source_get_width(source);
	uint32_t source_height = obs_source_get_height(source);
	if (source_width <= 0 || source_height <= 0) {
		background_template_source_clear_texture(bg_source->source_texture);
		return;
	}
	obs_enter_graphics();
	if (bg_source->source_texture) {
		uint32_t tex_width = gs_texture_get_width(bg_source->source_texture);
		uint32_t tex_height = gs_texture_get_height(bg_source->source_texture);
		if (tex_width != source_width || tex_height != source_height) {
			gs_texture_destroy(bg_source->source_texture);
			bg_source->source_texture = nullptr;
		}
	}

	if (!bg_source->source_texture) {
		bg_source->source_texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}

	{
		pthread_mutex_lock(&bg_source->src_mutex);
		bg_source->src_width = source_width;
		bg_source->src_height = source_height;
		pthread_mutex_unlock(&bg_source->src_mutex);
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_viewport_push();
	gs_blend_state_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	gs_set_render_target(bg_source->source_texture, nullptr);
	gs_set_viewport(0, 0, source_width, source_height);

	gs_ortho(0.0f, float(source_width), 0.0f, float(source_height), -100.0f, 100.0f);
	obs_source_video_render(source);

	gs_set_render_target(pre_rt, nullptr);
	gs_matrix_pop();
	gs_viewport_pop();
	gs_projection_pop();
	gs_blend_state_pop();

	obs_leave_graphics();
}

static void background_template_source_video_tick(void *data, float seconds)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_image()) {
		background_template_source_render(data, context->image);
	} else if (context->resource_is_video()) {
		background_template_source_render(data, context->video);
	} else if (context->source_texture_clear) {
		background_template_source_clear_texture(context->source_texture);
		context->source_texture_clear = false;
	}
}

static void background_template_source_activate(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_image()) {
		context->set_image_active(true);
	} else if (context->resource_is_video()) {
		context->set_video_active(true);
	}
}

static void background_template_source_deactivate(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_image()) {
		context->set_image_active(false);
	} else if (context->resource_is_video()) {
		context->set_video_active(false);
	}
}

static void background_template_source_play_pause(void *data, bool pause)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_play_pause(context->video, pause);
	}
}

static void background_template_source_restart(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_restart(context->video);
	}
}

static void background_template_source_stop(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_stop(context->video);
	}
}

static int64_t background_template_source_get_duration(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_get_duration(context->video);
	} else {
		return -1;
	}
}

static int64_t background_template_source_get_time(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_get_time(context->video);
	} else {
		return 0;
	}
}

static void background_template_source_set_time(void *data, int64_t ms)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_set_time(context->video, ms);
	}
}

static enum obs_media_state background_template_source_get_state(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_get_state(context->video);
	} else {
		return OBS_MEDIA_STATE_NONE;
	}
}

static bool background_template_source_is_update_done(void *data)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_is_update_done(context->video);
	} else {
		return false;
	}
}

static void background_template_source_network_state_changed(void *data, bool off)
{
	background_template_source *context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_network_state_changed(context->video, off);
	}
}

void register_background_template_source()
{
	obs_source_info info = {};
	info.id = "prism_background_template_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = background_template_source_get_name;
	info.create = background_template_source_create;
	info.destroy = background_template_source_destroy;
	info.get_defaults = background_template_source_defaults;
	info.get_properties = background_template_source_properties;
	info.activate = background_template_source_activate;
	info.deactivate = background_template_source_deactivate;
	info.video_render = background_template_source_video_render;
	info.video_tick = background_template_source_video_tick;
	info.get_width = background_template_source_get_width;
	info.get_height = background_template_source_get_height;
	info.update = background_template_source_update;
	info.media_play_pause = background_template_source_play_pause;
	info.media_restart = background_template_source_restart;
	info.media_stop = background_template_source_stop;
	info.media_get_duration = background_template_source_get_duration;
	info.media_get_time = background_template_source_get_time;
	info.media_set_time = background_template_source_set_time;
	info.media_get_state = background_template_source_get_state;
	info.is_update_done = background_template_source_is_update_done;
	info.network_state_changed = background_template_source_network_state_changed;

	info.icon_type = OBS_ICON_TYPE_VIRTUAL_BACKGROUND;
	obs_register_source(&info);
}
