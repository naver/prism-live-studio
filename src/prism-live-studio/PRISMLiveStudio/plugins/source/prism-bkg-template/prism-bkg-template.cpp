#include <obs.h>
#include <stdio.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <sys/stat.h>
#include "prism-bkg-template.hpp"
#include <filesystem>
#include <string>
#include <codecvt>
#include <util/threading.h>
#include <log.h>
#include <utils-api.h>
#include <frontend-api.h>
#include "pls/pls-source.h"
#include "pls/pls-properties.h"
#include "pls/pls-obs-api.h"

#define debug(format, ...) PLS_PLUGIN_DEBUG(format, ##__VA_ARGS__)
#define info(format, ...) PLS_PLUGIN_INFO(format, ##__VA_ARGS__)
#define warn(format, ...) PLS_PLUGIN_WARN(format, ##__VA_ARGS__)
#define error(format, ...) PLS_PLUGIN_ERROR(format, ##__VA_ARGS__)

enum class image_type_t { UNKOWN, MOTION, STATIC };

enum class resource_type_t { RT_Image, RT_Video, RT_Unkonwn };

struct background_template_source {
	obs_source_t *source = nullptr;

	resource_type_t resource_type = resource_type_t::RT_Unkonwn;

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

	std::string current_item_id;
	pthread_mutex_t src_mutex;
	uint32_t src_width = 0;
	uint32_t src_height = 0;

	void set_image_active(bool active) { set_source_active(image, this->image_active, active); }

	void set_video_active(bool active) { set_source_active(video, this->video_active, active); }

	void set_source_active(obs_source_t *source_, bool &source_active, bool active) const
	{
		if (source_active == active) {
			return;
		}

		source_active = active;

		if (active) {
			obs_source_inc_active(source_);
		} else {
			obs_source_dec_active(source_);
		}
	}

	bool resource_is_image() const
	{
		switch (resource_type) {
		case resource_type_t::RT_Image:
			return true;
		case resource_type_t::RT_Video:
			return !motion_enabled;
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
		case resource_type_t::RT_Image:
			return (is_motion_image && (!motion_enabled)) ? image_file_path : file_path;
		case resource_type_t::RT_Video:
			return image_file_path;
		default:
			return nullptr;
		}
	}

	bool resource_is_video() const
	{
		switch (resource_type) {
		case resource_type_t::RT_Image:
			return false;
		case resource_type_t::RT_Video:
			return motion_enabled;
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
		case resource_type_t::RT_Image:
			return nullptr;
		case resource_type_t::RT_Video:
			return motion_enabled ? file_path : nullptr;
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

	bool is_changed(const char *file_path_, const char *new_file_path) const
	{
		if (file_path_ == new_file_path) {
			return false;
		} else if (!file_path_ || !new_file_path) {
			return true;
		} else if (strcmp(file_path_, new_file_path)) {
			return true;
		}
		return false;
	}

	bool is_resource_valid() const
	{
		if (is_file_exists(thumbnail_file_path)) {
			return true;
		}
		return false;
	}

	bool is_file_exists(const char *file_path_) const
	{
		if (!file_path_ || !file_path_[0]) {
			return false;
		}

		QFile file(file_path_);
		return file.exists();
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

#if 0
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
#endif

static void background_template_source_init(background_template_source *context, obs_data_t *settings)
{
	auto image_type = static_cast<image_type_t>(obs_data_get_int(settings, "item_type"));
	context->is_motion_image = image_type == image_type_t::MOTION;
	context->motion_enabled = obs_data_get_bool(settings, "motion_enabled") || !obs_data_get_bool(settings, "prism_resource");

	switch (image_type) {
	case image_type_t::MOTION:
		context->resource_type = resource_type_t::RT_Video;
		break;
	case image_type_t::STATIC:
		context->resource_type = resource_type_t::RT_Image;
		break;
	default:
		context->resource_type = resource_type_t::RT_Unkonwn;
		break;
	}

	std::string file_path;
	if (const char *file_path_origin = obs_data_get_string(settings, "file_path"); !pls_is_empty(file_path_origin))
		file_path = pls_get_absolute_config_path(QString::fromUtf8(file_path_origin)).toStdString();

	if (context->is_changed(context->file_path, file_path.c_str())) {
		if (context->file_path) {
			pls_free(context->file_path, nullptr);
		}

		if (!file_path.empty()) {
#if defined(Q_OS_WIN)
			context->file_path = _strdup(file_path.c_str());
#elif defined(Q_OS_MACOS)
			context->file_path = strdup(file_path.c_str());
#endif

		} else {
			context->file_path = nullptr;
		}

#if 0
		const char *suffix = get_file_path_suffix(file_path);
		if (is_empty(suffix)) {
			context->resource_type = RT_Unkonwn;
		} else if (!stricmp(suffix, ".bmp") || !stricmp(suffix, ".tga") || !stricmp(suffix, ".png") || !stricmp(suffix, ".jpeg") || !stricmp(suffix, ".jpg") || !stricmp(suffix, ".gif") ||
			   !stricmp(suffix, ".psd")) {
			context->resource_type = RT_Image; // .bmp  .tga .png .jpg .gif  .psd
		} else if (!strcmp(suffix, ".mp4") || !strcmp(suffix, ".ts") || !strcmp(suffix, ".mov") || !strcmp(suffix, ".flv") || !strcmp(suffix, ".mkv") || !strcmp(suffix, ".avi") ||
			   !strcmp(suffix, ".webm")) {
			context->resource_type = RT_Video; // .mp4  .ts  .mov  .flv .mkv  .avi .webm
		} else {
			context->resource_type = RT_Unkonwn;
		}
#endif
	}

	std::string image_file_path;
	if (const char *image_file_path_origin = obs_data_get_string(settings, "image_file_path"); !pls_is_empty(image_file_path_origin))
		image_file_path = pls_get_absolute_config_path(QString::fromUtf8(image_file_path_origin)).toStdString();

	if (context->is_changed(context->image_file_path, image_file_path.c_str())) {
		if (context->image_file_path) {
			pls_free(context->image_file_path, nullptr);
		}

		if (is_not_empty(image_file_path.c_str())) {
#if defined(Q_OS_WIN)
			context->image_file_path = _strdup(image_file_path.c_str());
#elif defined(Q_OS_MACOS)
			context->image_file_path = strdup(image_file_path.c_str());
#endif
		} else {
			context->image_file_path = nullptr;
		}
	}

	std::string thumbnail_file_path;
	if (const char *thumbnail_file_path_origin = obs_data_get_string(settings, "thumbnail_file_path"); !pls_is_empty(thumbnail_file_path_origin))
		thumbnail_file_path = pls_get_absolute_config_path(QString::fromUtf8(thumbnail_file_path_origin)).toStdString();

	if (context->is_changed(context->thumbnail_file_path, thumbnail_file_path.c_str())) {
		if (context->thumbnail_file_path) {
			pls_free(context->thumbnail_file_path, nullptr);
		}

		if (is_not_empty(thumbnail_file_path.c_str())) {
#if defined(Q_OS_WIN)
			context->thumbnail_file_path = _strdup(thumbnail_file_path.c_str());
#elif defined(Q_OS_MACOS)
			context->thumbnail_file_path = strdup(thumbnail_file_path.c_str());
#endif
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

	pls_used(calldata);
	auto context = (background_template_source *)data;
	auto media_source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (media_source == context->video && !is_empty(context->get_video_file_path())) {
		obs_media_state state = obs_source_media_get_state(media_source);
		if (state == OBS_MEDIA_STATE_ERROR) {
			info("background template video state error: %d", state);
			pls_source_send_notify(context->source, OBS_SOURCE_EXCEPTION_BG_FILE_ERROR, 0);
		}
	}
}

static const char *background_template_source_get_name(void *param)
{
	pls_unused(param);
	return obs_module_text("VirtualBackground.SourceName");
}

#if 0
static bool is_resource_property_modified(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	const char *item_id = obs_data_get_string(settings, "item_id");
	return (!item_id || !item_id[0]) ? true : false;
}
#endif

static obs_properties_t *background_template_source_properties(void *data)
{
	pls_unused(data);
	obs_properties_t *props = obs_properties_create();

#if 0
	auto p = obs_properties_add_virtual_background_resource(props, "item_id", "");
	obs_property_set_modified_callback(p, &is_resource_property_modified);
#else
	pls_properties_virtual_background_add_resource(props, "item_id", "");
#endif
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
	obs_data_set_bool(video_settings, "restart_on_activate", false);
	context->video = obs_source_create_private("ffmpeg_source", "prism_virtual_background_video_source", video_settings);
	//obs_source_set_audio_output_flag(context->video, false);
	signal_handler_connect_ref(obs_source_get_signal_handler(context->video), "media_state_changed", media_state_changed_callback, context);
	obs_data_release(video_settings);
}

static void background_template_source_update(void *data, obs_data_t *settings)
{
	auto context = (background_template_source *)(data);

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
	pls_used(calldata);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || !data)
		return;

	auto context = static_cast<struct background_template_source *>(data);
	if (source == (uint64_t)context->source) {
		init_image_source(context);
		init_video_source(context);
	}
}

static void background_template_source_destroy(void *data);

static void *background_template_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto context = pls_new_nothrow<background_template_source>();
	if (!context) {
		error("virtual background create failed, because out of memory.");
		return nullptr;
	}
	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_notified, context);

	context->source = source;
	background_template_source_init(context, settings);

	pthread_mutex_init_value(&context->src_mutex);
	if (pthread_mutex_init(&context->src_mutex, nullptr) != 0) {
		error("Init source mutex failed.");
		background_template_source_destroy(context);
		return nullptr;
	}
	return context;
}

static void background_template_source_destroy(void *data)
{
	auto context = (background_template_source *)(data);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_notified, context);

	if (context->file_path) {
		pls_free(context->file_path);
	}

	if (context->image_file_path) {
		pls_free(context->image_file_path);
	}

	if (context->thumbnail_file_path) {
		pls_free(context->thumbnail_file_path);
	}

	if (context->image) {
		context->set_image_active(false);
		obs_source_release(context->image);
	}

	if (context->video) {
		context->set_video_active(false);
		signal_handler_disconnect(obs_source_get_signal_handler(context->video), "media_state_changed", media_state_changed_callback, context);
		obs_source_release(context->video);
	}

	if (context->source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->source_texture);
		obs_leave_graphics();
	}

	pthread_mutex_destroy(&context->src_mutex);

	pls_delete(context);
}

static uint32_t background_template_source_get_width(void *data)
{
	auto context = (background_template_source *)(data);

	pthread_mutex_lock(&context->src_mutex);
	uint32_t width = context->src_width;
	pthread_mutex_unlock(&context->src_mutex);

	return width;
}

static uint32_t background_template_source_get_height(void *data)
{
	auto context = (background_template_source *)(data);

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
	auto context = (background_template_source *)(data);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->source_texture);
	gs_draw_sprite(context->source_texture, 0, 0, 0);
	pls_used(effect);
}

static void background_templete_source_update_size(void *data, uint32_t width, uint32_t height)
{
	auto bg_source = (background_template_source *)(data);
	pthread_mutex_lock(&bg_source->src_mutex);
	bg_source->src_width = width;
	bg_source->src_height = height;
	pthread_mutex_unlock(&bg_source->src_mutex);
}

static void clear_thumbnail(void *data)
{
	auto context = (background_template_source *)(data);
	if (context->resource_is_image()) {
		obs_data_t *image_settings = obs_data_create();
		obs_data_set_string(image_settings, "file", "");
		obs_data_set_string(image_settings, "thumbnail_file", "");
		obs_source_update(context->image, image_settings);
		obs_data_release(image_settings);

		context->resource_type = resource_type_t::RT_Unkonwn;
	}
}

static void set_current_item_id(void *data, obs_data_t *private_data)
{
	auto context = (background_template_source *)(data);
	context->current_item_id = obs_data_get_string(private_data, "itemId");
}

static void get_current_item_id(void *data, obs_data_t *private_data)
{
	auto context = (background_template_source *)(data);

	obs_data_set_string(private_data, "itemId", context->current_item_id.c_str());
}

static void background_template_source_render(void *data, obs_source_t *source)
{
	auto bg_source = (background_template_source *)(data);
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

	background_templete_source_update_size(data, source_width, source_height);

	background_template_source_clear_texture(bg_source->source_texture);

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

static void background_template_source_video_tick(void *data, float /*seconds*/)
{
	auto context = (background_template_source *)(data);

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
	auto context = (background_template_source *)(data);

	if (context->resource_is_image()) {
		context->set_image_active(true);
	} else if (context->resource_is_video()) {
		context->set_video_active(true);
	}
}

static void background_template_source_deactivate(void *data)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_image()) {
		context->set_image_active(false);
	} else if (context->resource_is_video()) {
		context->set_video_active(false);
	}
}

static void background_template_source_play_pause(void *data, bool pause)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_play_pause(context->video, pause);
	}
}

static void background_template_source_restart(void *data)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_restart(context->video);
	}
}

static void background_template_source_stop(void *data)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_stop(context->video);
	}
}

static int64_t background_template_source_get_duration(void *data)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_get_duration(context->video);
	} else {
		return -1;
	}
}

static int64_t background_template_source_get_time(void *data)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_get_time(context->video);
	} else {
		return 0;
	}
}

static void background_template_source_set_time(void *data, int64_t ms)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		obs_source_media_set_time(context->video, ms);
	}
}

static enum obs_media_state background_template_source_get_state(void *data)
{
	auto context = (background_template_source *)(data);

	if (context->resource_is_video()) {
		return obs_source_media_get_state(context->video);
	} else {
		return OBS_MEDIA_STATE_NONE;
	}
}

static void background_template_source_set_private_data(void *data, obs_data_t *private_data)
{
	std::string method = obs_data_get_string(private_data, "method");
	if (method == "clear_image_texture") {
		clear_thumbnail(data);
	} else if (method == "set_current_clicked_id") {
		set_current_item_id(data, private_data);
	}
}

static void background_template_source_get_private_data(void *data, obs_data_t *private_data)
{
	std::string method = obs_data_get_string(private_data, "method");
	if (method == "get_current_clicked_id") {
		get_current_item_id(data, private_data);
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
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_VIRTUAL_BACKGROUND);

	pls_source_info prism_info = {};
	prism_info.set_private_data = background_template_source_set_private_data;
	prism_info.get_private_data = background_template_source_get_private_data;
	register_pls_source_info(&info, &prism_info);

	obs_register_source(&info);
}
