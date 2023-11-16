#include <obs.h>
#include <stdio.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <thread>
#include <mutex>
#include <obs-module.h>
#include <pls/media-info.h>
#include <media-io/media-remux.h>
#include <pls/pls-source.h>

#define T_LOOP obs_module_text("StickerReactionLoop")

using namespace std;

struct sticker_reaction {
	obs_source_t *source{};
	obs_source_t *image{};
	obs_source_t *media{};

	gs_texture_t *source_texture{};
	string last_video_input{};
	string last_image_input{};
	string video_input{};
	string image_input{};
	string landscape_video{};
	string landscape_image{};
	string portrait_video{};
	string portrait_image{};
	uint32_t cx;
	uint32_t cy;
	int64_t duration;

	bool loop = false;

	uint32_t base_width;
	uint32_t base_height;
	bool landscape;
	bool firstInit = true;
};

static const char *sticker_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("PRISMStickerReaction");
}

static void update_image_source(sticker_reaction *sticker)
{
	if (sticker->image_input.empty())
		return;
	obs_data_t *settings = nullptr;
	if (!sticker->image) {
		settings = obs_data_create();
		obs_data_set_string(settings, "file", sticker->image_input.c_str());
		obs_data_set_bool(settings, "unload", false);
		sticker->image = obs_source_create_private("image_source", nullptr, settings);

	} else if (sticker->last_image_input != sticker->image_input) {
		settings = obs_source_get_settings(sticker->image);
		obs_data_set_string(settings, "file", sticker->image_input.c_str());
		obs_data_set_bool(settings, "unload", false);
		obs_source_update(sticker->image, settings);
	}

	obs_data_release(settings);
}

static void update_video_source(sticker_reaction *sticker, bool lastLoop)
{
	obs_data_t *media_settings = nullptr;
	if (!sticker->media) {
		media_settings = obs_data_create();
		obs_data_set_bool(media_settings, "looping", sticker->loop);
		obs_data_set_string(media_settings, "local_file", sticker->video_input.c_str());
		obs_data_set_bool(media_settings, "restart_on_activate", false);
		sticker->media = obs_source_create_private("ffmpeg_source", "ffmpeg_sticker", media_settings);

	} else {
		media_settings = obs_source_get_settings(sticker->media);
		int64_t seek_to = 0;
		std::string last_file = obs_data_get_string(media_settings, "local_file");
		bool file_changed = last_file != sticker->video_input;
		if (file_changed) {
			int64_t duration = obs_source_media_get_duration(sticker->media);
			int64_t time = obs_source_media_get_time(sticker->media);
			seek_to = duration ? time / duration * sticker->duration : 0;
		}
		obs_data_set_bool(media_settings, "looping", sticker->loop);
		obs_data_set_string(media_settings, "local_file", sticker->video_input.c_str());
		obs_source_update(sticker->media, media_settings);

		if (lastLoop && !sticker->loop && !sticker->video_input.empty()) {
			obs_source_media_play_pause(sticker->media, true);
		} else if (!lastLoop && sticker->loop && !sticker->video_input.empty() || file_changed) {
			obs_source_output_video(sticker->media, nullptr);
			obs_source_media_restart(sticker->media);
		}
	}
	obs_data_release(media_settings);
}

static void sticker_source_update(void *data, obs_data_t *settings)
{
	auto sticker = static_cast<sticker_reaction *>(data);
	bool lastLoop = sticker->loop;
	sticker->loop = obs_data_get_bool(settings, "loop");

	obs_data_t *priv_settings = obs_source_get_private_settings(sticker->source);
	sticker->landscape_video = obs_data_get_string(priv_settings, "landscapeVideo");
	sticker->landscape_image = obs_data_get_string(priv_settings, "landscapeImage");
	sticker->portrait_video = obs_data_get_string(priv_settings, "portraitVideo");
	sticker->portrait_image = obs_data_get_string(priv_settings, "portraitImage");
	obs_data_release(priv_settings);

	obs_video_info ovi;
	obs_get_video_info(&ovi);
	sticker->landscape = ovi.base_width >= ovi.base_height;

	sticker->last_image_input = sticker->image_input;
	sticker->last_video_input = sticker->video_input;
	sticker->video_input = sticker->landscape ? sticker->landscape_video : sticker->portrait_video;
	sticker->image_input = sticker->landscape ? sticker->landscape_image : sticker->portrait_image;

	bool videoChanged = sticker->last_video_input != sticker->video_input;
	if (videoChanged || !sticker->cx || !sticker->cy) {
		media_info_t mi;
		if (!mi_open(&mi, sticker->video_input.c_str(), MI_OPEN_DIRECTLY)) {
			sticker->cx = 0;
			sticker->cy = 0;
			sticker->duration = 0;
		} else {
			sticker->cx = (uint32_t)mi_get_int(&mi, "width");
			sticker->cy = (uint32_t)mi_get_int(&mi, "height");
			sticker->duration = mi_get_int(&mi, "duration");
			mi_free(&mi);
		}
	}

	update_image_source(sticker);
	update_video_source(sticker, lastLoop);
}

static void sticker_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "noLabelHeader", true);
	obs_data_set_default_bool(settings, "loop", true);
}

static obs_properties_t *sticker_source_getproperties(void *data)
{
	if (!data)
		return nullptr;

	obs_properties_t *props = obs_properties_create();
	obs_properties_add_bool(props, "loop", T_LOOP);
	return props;
}

static void *sticker_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto sticker = static_cast<sticker_reaction *>(bzalloc(sizeof(struct sticker_reaction)));
	sticker->source = source;
	sticker->firstInit = true;
	return sticker;
}

static void sticker_source_destroy(void *data)
{
	auto sticker = static_cast<sticker_reaction *>(data);
	if (sticker->source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(sticker->source_texture);
		obs_leave_graphics();
	}

	if (sticker->image)
		obs_source_release(sticker->image);
	if (sticker->media)
		obs_source_release(sticker->media);
}

static uint32_t sticker_source_getwidth(void *data)
{
	auto sticker = static_cast<sticker_reaction *>(data);
	return sticker->cx;
}

static uint32_t sticker_source_getheight(void *data)
{
	auto sticker = static_cast<sticker_reaction *>(data);
	return sticker->cy;
}

static void sticker_source_render(void *data, gs_effect_t *effect)
{
	auto sticker = static_cast<sticker_reaction *>(data);
	if (!sticker || !sticker->source_texture)
		return;

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), sticker->source_texture);
	gs_draw_sprite(sticker->source_texture, 0, sticker->cx, sticker->cy);
}

static void sticker_source_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	auto sticker = static_cast<sticker_reaction *>(data);
	if (!sticker)
		return;

	obs_video_info ovi;
	obs_get_video_info(&ovi);
	bool l_screen = ovi.base_width >= ovi.base_height;
	bool screen_changed = sticker->landscape != l_screen;
	bool needUpdate = sticker->firstInit;
	if (needUpdate) {
		sticker->firstInit = false;
	}
	sticker->landscape = l_screen;
	if (needUpdate || (sticker->media && screen_changed)) {
		obs_data_t *settings = obs_source_get_settings(sticker->source);
		obs_source_update(sticker->source, settings);
		obs_data_release(settings);
	}

	if (!sticker->media)
		return;

	if (0 == sticker->cx || 0 == sticker->cy)
		return;

	obs_enter_graphics();
	if (!sticker->source_texture) {
		sticker->source_texture = gs_texture_create(sticker->cx, sticker->cy, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	} else {
		uint32_t texture_width = gs_texture_get_width(sticker->source_texture);
		uint32_t texture_height = gs_texture_get_height(sticker->source_texture);
		if (texture_width != sticker->cx || texture_height != sticker->cy) {
			gs_texture_destroy(sticker->source_texture);
			sticker->source_texture = gs_texture_create(sticker->cx, sticker->cy, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
		}
	}

	if (!sticker->source_texture) {
		obs_leave_graphics();
		return;
	}

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_viewport_push();
	gs_projection_push();

	gs_texture_t *pre_target = gs_get_render_target();
	gs_set_render_target(sticker->source_texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	gs_ortho(0.0f, (float)sticker->cx, 0.0f, (float)sticker->cy, -100.0f, 100.0f);
	gs_set_viewport(0, 0, sticker->cx, sticker->cy);

	if (sticker->image && !sticker->loop) {
		if (obs_source_removed(sticker->image)) {
			obs_source_release(sticker->image);
		} else {
			obs_source_video_render(sticker->image);
		}
	} else {
		if (obs_source_removed(sticker->media)) {
			obs_source_release(sticker->media);
		} else {
			obs_source_video_render(sticker->media);
		}
	}

	gs_set_render_target(pre_target, nullptr);
	gs_projection_pop();
	gs_viewport_pop();
	obs_leave_graphics();
}

static void sticker_private_update(void *data, obs_data_t *settings)
{
	if (!settings || !data)
		return;

	auto sticker = static_cast<sticker_reaction *>(data);
	obs_data_t *setting = obs_source_get_settings(sticker->source);
	sticker_source_update(data, setting);
	obs_data_release(setting);
}

void RegisterPRISMStickerSource()
{
	obs_source_info info = {};
	info.id = "prism_sticker_reaction";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = sticker_source_get_name;
	info.create = sticker_source_create;
	info.destroy = sticker_source_destroy;
	info.update = sticker_source_update;
	info.get_defaults = sticker_source_defaults;
	info.get_properties = sticker_source_getproperties;
	info.get_width = sticker_source_getwidth;
	info.get_height = sticker_source_getheight;
	info.video_render = sticker_source_render;
	info.video_tick = sticker_source_tick;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_PRISM_STICKER);

	pls_source_info pls_info = {};
	pls_info.set_private_data = sticker_private_update;
	register_pls_source_info(&info, &pls_info);

	obs_register_source(&info);
}
