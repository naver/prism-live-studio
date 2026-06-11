#include "PLSDrawPenEffect.h"
#include "PLSDrawPenStroke.h"

PLSDrawPenGlowEffect::PLSDrawPenGlowEffect()
{
	const char *filename = obs_find_data_file("outer_glow.effect");
	obs_enter_graphics();
	outerGlowEffect = gs_effect_create_from_file(filename, nullptr);
	struct gs_sampler_info sampler = {};
	sampler.max_anisotropy = 1;
	pointSampler = gs_samplerstate_create(&sampler);
	obs_leave_graphics();
}

PLSDrawPenGlowEffect::~PLSDrawPenGlowEffect()
{
	obs_enter_graphics();
	gs_effect_destroy(outerGlowEffect);
	gs_samplerstate_destroy(pointSampler);
	obs_leave_graphics();
}

void PLSDrawPenGlowEffect::RenderEffect(gs_texture_t *srcTexture, gs_texture_t *dstTexture, uint32_t rgba, float range, bool clear)
{
	if (!dstTexture || !srcTexture)
		return;

	obs_enter_graphics();
	gs_texture_t *pre_target = gs_get_render_target();
	gs_set_render_target(dstTexture, nullptr);

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();

	gs_enable_blending(false);
	gs_set_cull_mode(GS_NEITHER);

	if (clear) {
		struct vec4 clear_color = {0};
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	}

	uint32_t rt_width = gs_texture_get_width(dstTexture);
	uint32_t rt_height = gs_texture_get_height(dstTexture);

	gs_ortho(0.0f, (float)rt_width, 0.0f, (float)rt_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, rt_width, rt_height);

	gs_technique_t *tech = gs_effect_get_technique(outerGlowEffect, "Draw");
	gs_eparam_t *param = gs_effect_get_param_by_name(outerGlowEffect, "image");

	gs_eparam_t *param_tex_size = gs_effect_get_param_by_name(outerGlowEffect, "tex_size");
	struct vec2 inv_size = {(float)rt_width, (float)rt_height};
	gs_effect_set_vec2(param_tex_size, &inv_size);

	gs_eparam_t *param_range = gs_effect_get_param_by_name(outerGlowEffect, "range");
	gs_effect_set_int(param_range, (int)range);

	gs_eparam_t *param_glow_color = gs_effect_get_param_by_name(outerGlowEffect, "glow_color");
	colorf_from_rgba(glowColor.x, glowColor.y, glowColor.z, glowColor.w, rgba);
	gs_effect_set_vec4(param_glow_color, &glowColor);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(param, srcTexture);
	gs_effect_set_next_sampler(param, pointSampler);
	gs_draw_sprite(srcTexture, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
	gs_viewport_pop();
	gs_projection_pop();
	gs_matrix_pop();
	gs_set_render_target(pre_target, nullptr);
	obs_leave_graphics();
}

void PLSDrawPenHighlighterEffect::RenderEffect(gs_texture_t *srcTexture, gs_texture_t *dstTexture, bool clear, float opacity) const
{
	if (!srcTexture || !dstTexture)
		return;

	obs_enter_graphics();

	uint32_t rt_width = gs_texture_get_width(dstTexture);
	uint32_t rt_height = gs_texture_get_height(dstTexture);

	gs_texture_t *pre_target = gs_get_render_target();
	gs_set_render_target(dstTexture, nullptr);

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();

	gs_enable_blending(false);
	gs_set_cull_mode(GS_NEITHER);

	if (clear) {
		struct vec4 clear_color = {0};
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	}

	gs_ortho(0.0f, (float)rt_width, 0.0f, (float)rt_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, rt_width, rt_height);

	gs_technique_t *tech = gs_effect_get_technique(opaqueEffect, "Draw");
	gs_eparam_t *param = gs_effect_get_param_by_name(opaqueEffect, "image");

	gs_eparam_t *param_opacity = gs_effect_get_param_by_name(opaqueEffect, "opacity");
	gs_effect_set_float(param_opacity, opacity);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(param, srcTexture);
	gs_draw_sprite(srcTexture, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
	gs_viewport_pop();
	gs_projection_pop();
	gs_matrix_pop();
	gs_set_render_target(pre_target, nullptr);
	obs_leave_graphics();
}

PLSDrawPenHighlighterEffect::PLSDrawPenHighlighterEffect()
{
	const char *filename = obs_find_data_file("opaque_pls.effect");
	obs_enter_graphics();
	opaqueEffect = gs_effect_create_from_file(filename, nullptr);
	obs_leave_graphics();
}

PLSDrawPenHighlighterEffect::~PLSDrawPenHighlighterEffect()
{
	obs_enter_graphics();
	gs_effect_destroy(opaqueEffect);
	obs_leave_graphics();
}

PLSDrawPenMixEffect::PLSDrawPenMixEffect()
{
	const char *filename = obs_find_data_file("mix_texture.effect");
	obs_enter_graphics();
	mixEffect = gs_effect_create_from_file(filename, nullptr);
	obs_leave_graphics();
}

PLSDrawPenMixEffect::~PLSDrawPenMixEffect()
{
	obs_enter_graphics();
	gs_effect_destroy(mixEffect);
	obs_leave_graphics();
}

void PLSDrawPenMixEffect::RenderEffect(gs_texture_t *srcTop, gs_texture_t *srcBottom, gs_texture_t *target) const
{
	if (!target)
		return;

	obs_enter_graphics();

	uint32_t rt_width = gs_texture_get_width(target);
	uint32_t rt_height = gs_texture_get_height(target);

	gs_texture_t *pre_target = gs_get_render_target();
	gs_set_render_target(target, nullptr);

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();

	gs_enable_blending(false);
	gs_set_cull_mode(GS_NEITHER);

	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_ortho(0.0f, (float)rt_width, 0.0f, (float)rt_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, rt_width, rt_height);

	gs_technique_t *tech = gs_effect_get_technique(mixEffect, "Draw");
	gs_eparam_t *bottom_param = gs_effect_get_param_by_name(mixEffect, "bottom_image");
	gs_eparam_t *top_param = gs_effect_get_param_by_name(mixEffect, "top_image");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_effect_set_texture(bottom_param, srcBottom);
	gs_effect_set_texture(top_param, srcTop);
	gs_draw_sprite(srcTop, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
	gs_viewport_pop();
	gs_projection_pop();
	gs_matrix_pop();
	gs_set_render_target(pre_target, nullptr);
	obs_leave_graphics();
}
