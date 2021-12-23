#include "prism-cam-ground.h"
#include <obs-module.h>

static const int GAUSS_BLUR_TIMES = 3;
static const float BLUR_WEIGHT = 0.06;

PLSCamGround::PLSCamGround(obs_source_t *dshow_source, void *cam_effect,
			   ground_exception cb, GROUND_TYPE type)
	: parent_source(dshow_source),
	  ground_texture(nullptr),
	  gauss_effect(nullptr),
	  gauss_tech(nullptr),
	  background_clear(false),
	  pls_cam_effect(cam_effect),
	  ground_type(type)
{
	exception_callback = cb;
}

PLSCamGround::~PLSCamGround()
{
	for (auto item : vecGroundItems) {
		delete item;
		item = nullptr;
	}
	vecGroundItems.clear();

	obs_enter_graphics();
	if (ground_texture) {
		gs_texture_destroy(ground_texture);
		ground_texture = nullptr;
	}
	if (gauss_effect) {
		gs_effect_destroy(gauss_effect);
		gauss_effect = nullptr;
	}
	obs_leave_graphics();
}

bool PLSCamGround::RenderGround(gs_texture_t *rt_tex, bool flip)
{
	if (!rt_tex)
		return false;

	CAutoLockCS autoLock(param_lock);

	if (ground_data.ground_seg_type != GROUND_SEG_TYPE_ADD_BG) {
		return false;
	}

	//if do not add background image/video, do not render ground
	if (vecGroundItems.empty()) {
		ClearBGTexture();
		return false;
	}

	//do not clear background texture every frame to avoid flick
	if (background_clear || ground_type == GROUND_TYPE_FG) {
		ClearBGTexture();
		background_clear = false;
	}

	if (!ground_data.enable) {
		return false;
	}

	uint32_t width = gs_texture_get_width(rt_tex);
	uint32_t height = gs_texture_get_height(rt_tex);
	gs_color_format fmt = gs_texture_get_color_format(rt_tex);
	UpdateGroundTexture(width, height, fmt);

	for (auto item : vecGroundItems) {
		item->RenderItemToRT(ground_texture, flip);
	}

	//blur
	if (ground_type == GROUND_TYPE_BG) {
		if (ground_data.blur < 1 || !ground_data.blur_enable) {
			gs_copy_texture(rt_tex, ground_texture);
		} else {
			BlurGroundTexture(rt_tex);
		}
	} else {
		BlendGroundTexture(rt_tex);
	}

	return true;
}

void PLSCamGround::BlurGroundTexture(gs_texture_t *rt_tex)
{
	if (!rt_tex || !ground_texture) {
		return;
	}

	if (!gauss_effect) {
		InitGaussEffect();
	}

	if (!gauss_effect) {
		plog(LOG_WARNING, "Gauss filter invalid");
		gs_copy_texture(rt_tex, ground_texture);
		return;
	}

	gs_texture_t *pre_target = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_set_render_target(rt_tex, nullptr);
	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

	uint32_t rt_width = gs_texture_get_width(rt_tex);
	uint32_t rt_height = gs_texture_get_height(rt_tex);

	gs_ortho(0.0f, rt_width, 0.0f, rt_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, rt_width, rt_height);

	BlurGroundTexthreHV(rt_tex, gauss_tech);

	gs_viewport_pop();
	gs_projection_pop();
	gs_matrix_pop();
	gs_set_render_target(pre_target, nullptr);
}

void PLSCamGround::BlurGroundTexthreHV(gs_texture_t *rt_tex,
				       gs_technique_t *tech)
{
	uint32_t tex_width = gs_texture_get_width(ground_texture);
	uint32_t tex_height = gs_texture_get_height(ground_texture);
	if (!tech || tex_width <= 0 || tex_height <= 0) {
		return;
	}
	gs_technique_begin(tech);

	float input_blur = ground_data.blur * BLUR_WEIGHT;

	for (int index = 0; index < GAUSS_BLUR_TIMES; index++) {
		float iteraionOffs = index * 1.0f;

		for (int i = 0; i < 2; i++) {
			gs_eparam_t *param_image = gs_effect_get_param_by_name(
				gauss_effect, "image");
			gs_effect_set_texture(param_image, ground_texture);
			gs_eparam_t *param_inv_size =
				gs_effect_get_param_by_name(gauss_effect,
							    "inv_size");
			struct vec2 inv_size = {1.0 / (float)tex_width,
						1.0 / (float)tex_height};
			gs_effect_set_vec2(param_inv_size, &inv_size);
			gs_eparam_t *param_blur = gs_effect_get_param_by_name(
				gauss_effect, "blur_size");
			gs_effect_set_float(param_blur,
					    input_blur + iteraionOffs);
			gs_technique_begin_pass(tech, i);
			gs_draw_sprite(ground_texture, 0, 0, 0);
			gs_technique_end_pass(tech);
			gs_copy_texture(ground_texture, rt_tex);
		}
	}
	gs_technique_end(tech);
}

bool PLSCamGround::InitGaussEffect()
{
	if (gauss_effect) {
		return true;
	}

	char *effect_path = obs_module_file("blur_filter.effect");
	gauss_effect = gs_effect_create_from_file(effect_path, NULL);
	bfree(effect_path);
	gauss_tech = gs_effect_get_technique(gauss_effect, "Draw");
	return gauss_effect && gauss_tech;
}

void PLSCamGround::SetBackgroundInfo(const ground_private_data gd_data,
				     item_private_data &item_data)
{
	CAutoLockCS autoLock(param_lock);
	if (ground_data.ground_seg_type != gd_data.ground_seg_type) {
		plog(LOG_INFO, "Ground type switch from %d to %d",
		     ground_data.ground_seg_type, gd_data.ground_seg_type);
	}
	ground_data.enable = gd_data.enable;
	ground_data.blur_enable = gd_data.blur_enable;
	ground_data.blur = gd_data.blur;
	ground_data.temp_origin = gd_data.temp_origin;
	if (ground_data.ground_seg_type == GROUND_SEG_TYPE_ADD_BG &&
	    gd_data.ground_seg_type == GROUND_SEG_TYPE_ORIGINAL &&
	    gd_data.temp_origin == 1) {
	} else {
		NotifyBackgroundAction(gd_data);
		ground_data.ground_seg_type = gd_data.ground_seg_type;
	}

	if (vecGroundItems.empty() &&
	    ground_data.ground_seg_type == GROUND_SEG_TYPE_ADD_BG) {
		PLSCamGroundItem *item = new PLSCamGroundItem(
			parent_source, pls_cam_effect, exception_callback);
		if (ground_type == GROUND_TYPE_FG) {
			item->SetAutoFit(true);
		}
		vecGroundItems.push_back(item);
	}

	if (!vecGroundItems.empty()) {
		PLSCamGroundItem *item = vecGroundItems.at(0);
		if (item) {
			if (ground_data.ground_seg_type ==
				    GROUND_SEG_TYPE_ADD_BG &&
			    !item_data.path.empty() &&
			    !item_data.stop_moiton_path.empty()) {
				//video, image background, update info
				item_data.enable = true;
			} else {
				//set video state
				item_data.enable = false;
			}
			item->SetPrivateData(item_data);
		}
	}
}

void PLSCamGround::Tick()
{
	CAutoLockCS autoLock(param_lock);
	//item tick
	bool res = false;
	for (auto item : vecGroundItems) {
		res = item->Tick();
		if (res) {
			background_clear = true;
		}
	}
}

void PLSCamGround::ClearBGTexture()
{
	if (!ground_texture)
		return;

	obs_enter_graphics();
	gs_texture_t *pre_target = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();

	gs_set_render_target(ground_texture, nullptr);
	struct vec4 clear_color;
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

	gs_set_render_target(pre_target, nullptr);
	gs_projection_pop();
	gs_viewport_pop();
	obs_leave_graphics();
}

void PLSCamGround::SetClearAllTextureToken()
{
	CAutoLockCS autoLock(param_lock);
	background_clear = true;
	for (auto &item : vecGroundItems) {
		item->SetClearItemTexture(true);
	}
}

void PLSCamGround::ClearResWhenEffectOff()
{
	CAutoLockCS autoLock(param_lock);
	for (auto &item : vecGroundItems) {
		item->ClearResWhenEffectOff();
	}
}

void PLSCamGround::BlendGroundTexture(gs_texture_t *rt_tex)
{
	if (!rt_tex || ground_data.temp_origin == 1) {
		return;
	}
	obs_enter_graphics();

	gs_texture_t *pre_target = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	gs_blend_state_push();
	gs_reset_blend_state();

	gs_matrix_push();
	gs_matrix_identity();
	gs_set_render_target(rt_tex, nullptr);
	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	uint32_t rt_width = gs_texture_get_width(rt_tex);
	uint32_t rt_height = gs_texture_get_height(rt_tex);

	gs_ortho(0.0f, rt_width, 0.0f, rt_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, rt_width, rt_height);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      ground_texture);
	gs_draw_sprite(ground_texture, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_viewport_pop();
	gs_projection_pop();
	gs_blend_state_pop();
	gs_matrix_pop();
	gs_set_render_target(pre_target, nullptr);
	obs_leave_graphics();
}

void PLSCamGround::NotifyBackgroundAction(const ground_private_data gd_data)
{
	if (!exception_callback || !parent_source ||
	    ground_type == GROUND_TYPE_FG) {
		return;
	}
	if (gd_data.ground_seg_type != ground_data.ground_seg_type) {
		if (gd_data.ground_seg_type == GROUND_SEG_TYPE_DEL_BG) {
			obs_source_action_event_notify(
				parent_source,
				OBS_SOURCE_VIRTUAL_BACKGROUND_STATUS,
				VB_ACTIONLOG_MODULE, VB_ACTIONLOG_REMOVE, "",
				"");
		}
	}
}

void PLSCamGround::UpdateGroundTexture(uint32_t width, uint32_t height,
				       gs_color_format fmt)
{
	if (ground_texture) {
		uint32_t tex_w = gs_texture_get_width(ground_texture);
		uint32_t tex_h = gs_texture_get_height(ground_texture);
		gs_color_format tex_fmt =
			gs_texture_get_color_format(ground_texture);
		if (tex_w != width || tex_h != height || tex_fmt != fmt) {
			gs_texture_destroy(ground_texture);
			ground_texture = nullptr;
		}
	}
	if (!ground_texture) {
		ground_texture = gs_texture_create(width, height, fmt, 1, NULL,
						   GS_RENDER_TARGET);
	}
}
