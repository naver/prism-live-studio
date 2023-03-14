#include "prism-cam-segmentation.h"
#include <util/platform.h>

PLSCamSegmen::PLSCamSegmen(obs_source_t *source)
	: fin_output_texture(nullptr), dshow_source(source)
{
}

PLSCamSegmen::~PLSCamSegmen()
{
	back_ground.reset();
	fore_ground.reset();
	ReleaseOutputTexture();
}

gs_texture *PLSCamSegmen::GetOutputVideo(gs_texture_t *texture)
{
	if (!texture) {
		return nullptr;
	}

	// Output video never change, use the openned texture directly.
	// render & mix ground
	obs_enter_graphics();
	UpdateFinOutputTexture(texture);
	RenderAllLayers(texture, false);
	obs_leave_graphics();
	return fin_output_texture;
}

void PLSCamSegmen::CamAsyncTick()
{
	EffectTick();
}

bool PLSCamSegmen::ParseSegmentationParam(obs_data_t *data)
{
	if (!data) {
		return false;
	}

	ground_private_data ground_data;
	ground_data.blur_enable = obs_data_get_bool(data, GROUND_BLUR_ENABLE);
	ground_data.blur = obs_data_get_double(data, GROUND_BLUR);
	ground_data.ground_seg_type = static_cast<GROUND_SEG_TYPE>(
		obs_data_get_int(data, GROUND_SELECT_TYPE));
	ground_data.temp_origin =
		obs_data_get_int(data, GROUND_IS_TEMP_ORIGINAL);

	if (ground_data.ground_seg_type == GROUND_SEG_TYPE_UNKNOWN) {
		plog(LOG_WARNING, "Ground type error: unknown");
		return false;
	}

	item_private_data item_data;
	std::string str_type = obs_data_get_string(data, GROUND_BG_ITEM_TYPE);
	if (ground_data.ground_seg_type == GROUND_SEG_TYPE_ADD_BG) {
		if (str_type.compare("image") == 0) {
			item_data.item_type = ITEM_TYPE_IMAGE;
		} else if (str_type.compare("video") == 0) {
			item_data.item_type = ITEM_TYPE_VIDEO;
		} else {
			plog(LOG_INFO, "Background type[%s] invalid", str_type);
			return false;
		}
	}

	item_data.path = obs_data_get_string(data, GROUND_PATH);
	item_data.stop_moiton_path =
		obs_data_get_string(data, GROUND_STOPMOTION_PATH);
	item_data.rt_source.x = obs_data_get_double(data, GROUND_SRC_RECT_X);
	item_data.rt_source.y = obs_data_get_double(data, GROUND_SRC_RECT_Y);
	item_data.rt_source.cx = obs_data_get_double(data, GROUND_SRC_RECT_CX);
	item_data.rt_source.cy = obs_data_get_double(data, GROUND_SRC_RECT_CY);

	item_data.rt_target.x = obs_data_get_double(data, GROUND_DST_RECT_X);
	item_data.rt_target.y = obs_data_get_double(data, GROUND_DST_RECT_Y);
	item_data.rt_target.cx = obs_data_get_double(data, GROUND_DST_RECT_CX);
	item_data.rt_target.cy = obs_data_get_double(data, GROUND_DST_RECT_CY);

	item_data.motion = obs_data_get_bool(data, GROUND_MOTION);
	item_data.h_flip = obs_data_get_bool(data, GROUND_H_FLIP);
	item_data.v_flip = obs_data_get_bool(data, GROUND_V_FLIP);
	item_data.ui_motion_id = obs_data_get_string(data, GROUND_UI_MOTION_ID);
	item_data.thumbnail_file_path =
		obs_data_get_string(data, GROUND_THUMBNAIL_FILE_PATH);
	item_data.is_prism_resource =
		obs_data_get_bool(data, GROUND_IS_PRISM_RESOURCE);

	bool ck_enable = obs_data_get_bool(data, CK_CAPTURE);

	if (!back_ground) {
		ground_exception callback =
			(ground_exception)&PLSCamSegmen::GroundExceptionHandler;
		back_ground = std::make_shared<PLSCamGround>(
			dshow_source, this, callback, GROUND_TYPE_BG);
	}
	back_ground->SetBackgroundInfo(ground_data, item_data);

	if (!fore_ground) {
		ground_exception callback =
			(ground_exception)&PLSCamSegmen::GroundExceptionHandler;
		fore_ground = std::make_shared<PLSCamGround>(
			dshow_source, this, callback, GROUND_TYPE_FG);
	}

	item_private_data it;
	it.path = obs_data_get_string(data, FORE_GROUND_PATH);
	it.stop_moiton_path =
		obs_data_get_string(data, FORE_GROUND_STATIC_PATH);
	it.item_type = item_data.item_type;
	it.ui_motion_id = item_data.ui_motion_id;
	it.motion = item_data.motion;
	it.is_prism_resource = item_data.is_prism_resource;
	if (it.path.empty() || it.stop_moiton_path.empty()) {
		ground_data.enable = false;
	}
	fore_ground->SetBackgroundInfo(ground_data, it);
	return true;
}

void PLSCamSegmen::ClearResWhenEffectOff()
{
	if (back_ground) {
		back_ground->ClearResWhenEffectOff();
	}

	if (fore_ground) {
		fore_ground->ClearResWhenEffectOff();
	}
}

void PLSCamSegmen::EffectTick()
{
	if (back_ground) {
		back_ground->Tick();
	}

	if (fore_ground) {
		fore_ground->Tick();
	}
}

void PLSCamSegmen::RenderAllLayers(gs_texture_t *texture, bool flip)
{
	if (!fin_output_texture) {
		return;
	}

	obs_enter_graphics();

	bool res = false;
	if (back_ground) {
		res = back_ground->RenderGround(fin_output_texture, flip);
	}

	if (res) {
		//mix output_video_tex to fin_output_texture
		gs_texture_t *pre_rt_tex = gs_get_render_target();

		gs_viewport_push();
		gs_projection_push();
		gs_matrix_push();
		gs_matrix_identity();
		uint32_t width = gs_texture_get_width(fin_output_texture);
		uint32_t height = gs_texture_get_height(fin_output_texture);
		gs_set_render_target(fin_output_texture, nullptr);
		gs_set_viewport(0, 0, width, height);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f,
			 100.0f);
		gs_blend_state_push();
		gs_reset_blend_state();

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);

		gs_eparam_t *param_image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(param_image, texture);

		gs_draw_sprite(texture, 0, 0, 0);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		gs_blend_state_pop();
		gs_set_render_target(pre_rt_tex, nullptr);
		gs_matrix_pop();
		gs_projection_pop();
		gs_viewport_pop();
	} else {
		gs_copy_texture(fin_output_texture, texture);
	}

	if (fore_ground) {
		fore_ground->RenderGround(fin_output_texture, flip);
	}

	obs_leave_graphics();
}

void PLSCamSegmen::GroundExceptionHandler(void *data, int type, int subcode)
{
	PLSCamSegmen *cam_seg = (PLSCamSegmen *)data;
	if (!cam_seg) {
		return;
	}
	obs_source_send_notify(cam_seg->dshow_source,
			       static_cast<obs_source_event_type>(type),
			       subcode);
}

void PLSCamSegmen::UpdateFinOutputTexture(const gs_texture *tex)
{
	if (!tex)
		return;
	uint32_t width = gs_texture_get_width(tex);
	uint32_t height = gs_texture_get_height(tex);
	gs_color_format fmt = gs_texture_get_color_format(tex);
	if (fin_output_texture &&
	    (gs_texture_get_width(fin_output_texture) != width ||
	     gs_texture_get_height(fin_output_texture) != height ||
	     fmt != gs_texture_get_color_format(fin_output_texture))) {
		gs_texture_destroy(fin_output_texture);
		fin_output_texture = nullptr;
	}

	if (!fin_output_texture) {
		fin_output_texture = gs_texture_create(
			width, height, fmt, 1, nullptr, GS_RENDER_TARGET);
	}
}

void PLSCamSegmen::ReleaseOutputTexture()
{
	obs_enter_graphics();
	if (fin_output_texture) {
		gs_texture_destroy(fin_output_texture);
		fin_output_texture = nullptr;
	}
	obs_leave_graphics();
}
