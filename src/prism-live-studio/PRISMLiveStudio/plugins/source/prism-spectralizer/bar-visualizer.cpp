﻿/*************************************************************************
 * This file is part of spectralizer
 * github.con/univrsal/spectralizer
 * Copyright 2020 univrsal <universailp@web.de>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "bar-visualizer.hpp"
#include "prism-visualizer-source.hpp"

#include <util/platform.h>

static const int RADIUS = 2.0f;
static const float BLUR = 6.0;
static const float EXTEND_RADIUS = 1.0;

bar_visualizer::bar_visualizer(config *cfg) : spectrum_visualizer(cfg), m_cfg(cfg), render_texture(nullptr)
{
	char *mask_path = obs_module_file("color_mask.effect");
	char *gauss_path = obs_module_file("blur_filter.effect");
	struct gs_sampler_info sampler_info = {GS_FILTER_LINEAR, GS_ADDRESS_WRAP, GS_ADDRESS_WRAP};

	obs_enter_graphics();
	mask_effect = gs_effect_create_from_file(mask_path, nullptr);
	mask_tech = gs_effect_get_technique(mask_effect, "Draw");
	sampler = gs_samplerstate_create(&sampler_info);

	const char *outter_blur_path = obs_module_file("outter_blur.effect");
	outter_blur_effect = gs_effect_create_from_file(outter_blur_path, nullptr);
	outter_blur_tech = gs_effect_get_technique(outter_blur_effect, "Draw");

	const char *region_extend_path = obs_module_file("color_region_extend.effect");
	region_extend_effect = gs_effect_create_from_file(region_extend_path, nullptr);
	region_extend_tech = gs_effect_get_technique(region_extend_effect, "Draw");

	obs_leave_graphics();

	bfree(mask_path);
	bfree(gauss_path);
}

bar_visualizer::~bar_visualizer()
{
	obs_enter_graphics();

	gs_effect_destroy(mask_effect);
	gs_effect_destroy(outter_blur_effect);
	outter_blur_effect = nullptr;
	gs_effect_destroy(region_extend_effect);
	region_extend_effect = nullptr;
	gs_samplerstate_destroy(sampler);
	release_frame();

	obs_leave_graphics();
}

void bar_visualizer::render()
{
	obs_enter_graphics();

	update_frame();

	render_begin(source_texture);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 colorVal;
	vec4_from_rgba(&colorVal, m_cfg->vm_params[m_cfg->visual].color);
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	draw_bars();

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	render_end();

	render_bars();

	old_bar_mode = m_cfg->vm_params[m_cfg->visual].bar_mode;
	old_gradient_mode = m_cfg->vm_params[m_cfg->visual].gradient_mode;

	obs_leave_graphics();
}

bool bar_visualizer::update_frame()
{
	visual_params params = m_cfg->vm_params[m_cfg->visual];
	bool size_changed = false;
	if (source_texture) {
		uint32_t cx = gs_texture_get_width(source_texture);
		uint32_t cy = gs_texture_get_height(source_texture);
		size_changed = (cx != m_cfg->cx || cy != m_cfg->cy);
	}

	if (old_bar_mode != params.bar_mode || size_changed) {
		release_frame();
	}

	if (!source_texture) {
		source_texture = gs_texture_create(m_cfg->cx, m_cfg->cy, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
	}

	enum gs_color_format format = gs_texture_get_color_format(source_texture);
	if (!render_texture) {
		render_texture = gs_texture_create(m_cfg->cx, m_cfg->cy, format, 1, nullptr, GS_RENDER_TARGET);
	}

	if (!alpha_blur_texture) {
		alpha_blur_texture = gs_texture_create(m_cfg->cx, m_cfg->cy, format, 1, nullptr, GS_RENDER_TARGET);
	}

	if (!solid_texture) {
		solid_texture = gs_texture_create(m_cfg->cx, m_cfg->cy, format, 1, nullptr, GS_RENDER_TARGET);
	}

	if (!extend_texture) {
		extend_texture = gs_texture_create(m_cfg->cx, m_cfg->cy, format, 1, nullptr, GS_RENDER_TARGET);
	}
	return true;
}

gs_texture *bar_visualizer::get_texture()
{
	return render_texture;
}

void bar_visualizer::draw_rectangle_bars()
{
	size_t i = 0;
	size_t pos_x = 0;
	uint32_t height;
	visual_params params = m_cfg->vm_params[m_cfg->visual];
	for (; i < get_bars_left().size() - DEAD_BAR_OFFSET; i++) { /* Leave the four dead bars the end */
		auto val = get_bars_left()[i] > 1.0 ? get_bars_left()[i] : 1.0;
		height = UTIL_MAX(static_cast<uint32_t>(round(val)), 1);
		height = UTIL_MIN(height, params.bar_height);

		pos_x = m_cfg->margin + i * (params.bar_width + params.bar_space);
		gs_matrix_push();
		gs_matrix_translate3f((float)pos_x, (float)(params.bar_height + m_cfg->margin - height), 0);
		gs_draw_sprite(nullptr, 0, params.bar_width, height);
		gs_matrix_pop();
	}
}

void bar_visualizer::draw_stereo_rectangle_bars()
{
	size_t i = 0;
	size_t pos_x = 0;
	uint32_t height_l;
	uint32_t height_r;
	visual_params params = m_cfg->vm_params[m_cfg->visual];

	bool odd = params.stereo_space % 2;
	uint16_t offset = params.stereo_space / 2;
	uint16_t offset_l = odd ? offset + 1 : offset;
	uint16_t offset_r = offset;
	uint16_t center = m_cfg->cy / 2;
	odd = m_cfg->cy % 2;
	center = odd ? center + 1 : center;

	for (; i < get_bars_left().size() - DEAD_BAR_OFFSET; i++) { /* Leave the four dead bars the end */
		double bar_left = (get_bars_left()[i] > 1.0 ? get_bars_left()[i] : 1.0);
		double bar_right = (get_bars_right()[i] > 1.0 ? get_bars_right()[i] : 1.0);

		height_l = UTIL_MAX(static_cast<uint32_t>(round(bar_left)), 1);
		height_l = UTIL_MIN(height_l, (params.bar_height / 2));
		height_r = UTIL_MAX(static_cast<uint32_t>(round(bar_right)), 1);
		height_r = UTIL_MIN(height_r, (params.bar_height / 2));

		pos_x = m_cfg->margin + i * (params.bar_width + params.bar_space);

		/* Top */
		gs_matrix_push();
		gs_matrix_translate3f((float)pos_x, (float)(center - height_l) - offset_l, 0);
		gs_draw_sprite(nullptr, 0, params.bar_width, height_l);
		gs_matrix_pop();

		/* Bottom */
		gs_matrix_push();
		gs_matrix_translate3f((float)pos_x, center + offset_r, 0);
		gs_draw_sprite(nullptr, 0, params.bar_width, height_r);
		gs_matrix_pop();
	}
}

void bar_visualizer::draw_rounded_bars()
{
	size_t i = 0;
	size_t pos_x = 0;
	uint32_t height;
	visual_params params = m_cfg->vm_params[m_cfg->visual];
	uint32_t vert_count = params.corner_points * 4;

	for (; i < get_bars_left().size() - DEAD_BAR_OFFSET; i++) { /* Leave the four dead bars the end */
		vert_count = 0;
		auto val = get_bars_left()[i] > 1.0 ? get_bars_left()[i] : 1.0;

		// The bar needs to be at least a square so the circle fits
		height = UTIL_MAX(static_cast<uint32_t>(round(val)), params.bar_width);
		height = UTIL_MIN(height, params.bar_height);

		pos_x = m_cfg->margin + i * (params.bar_width + params.bar_space);
		auto verts = make_rounded_rectangle((float)height, height == params.bar_width);
		gs_matrix_push();
		gs_load_vertexbuffer(verts);
		gs_matrix_translate3f((float)pos_x, (float)(params.bar_height + m_cfg->margin - height), 0);

		gs_draw(GS_TRISTRIP, 0, 0);

		gs_matrix_pop();
		gs_load_vertexbuffer(nullptr);
		gs_vertexbuffer_destroy(verts);
		verts = nullptr;
	}
}

void bar_visualizer::draw_stereo_rounded_bars()
{
	size_t i = 0;
	size_t pos_x = 0;
	uint32_t height_l;
	uint32_t height_r;
	visual_params params = m_cfg->vm_params[m_cfg->visual];

	bool odd = params.stereo_space % 2;
	uint16_t offset = params.stereo_space / 2;
	uint16_t offset_l = odd ? offset + 1 : offset;
	uint16_t offset_r = offset;
	uint16_t center = m_cfg->cy / 2;
	odd = m_cfg->cy % 2;
	center = odd ? center + 1 : center;

	for (; i < get_bars_left().size() - DEAD_BAR_OFFSET; i++) { /* Leave the four dead bars the end */
		double bar_left = (get_bars_left()[i] > 1.0 ? get_bars_left()[i] : 1.0);
		double bar_right = (get_bars_right()[i] > 1.0 ? get_bars_right()[i] : 1.0);

		// The bar needs to be at least a square so the circle fits
		height_l = UTIL_MAX(static_cast<uint32_t>(round(bar_left)), params.bar_width);
		height_l = UTIL_MIN(height_l, (params.bar_height / 2));
		height_r = UTIL_MAX(static_cast<uint32_t>(round(bar_right)), params.bar_width);
		height_r = UTIL_MIN(height_r, (params.bar_height / 2));

		pos_x = m_cfg->margin + i * (params.bar_width + params.bar_space);

		auto verts_left = make_rounded_rectangle((float)height_l, height_l == params.bar_width);
		auto verts_right = make_rounded_rectangle((float)height_r, height_r == params.bar_width);

		/* Top  */
		gs_matrix_push();
		gs_load_vertexbuffer(verts_left);
		auto pos_y = params.stereo_space ? (float)(center - height_l - offset_l) : (float)(center - height_l + params.radius + 1);
		gs_matrix_translate3f((float)pos_x, pos_y, 0);

		gs_draw(GS_TRISTRIP, 0, 0);
		gs_matrix_pop();
		gs_load_vertexbuffer(nullptr);
		gs_vertexbuffer_destroy(verts_left);

		/* Bottom */
		gs_matrix_push();
		gs_load_vertexbuffer(verts_right);
		pos_y = params.stereo_space ? center + offset_r : center - params.radius;
		gs_matrix_translate3f((float)pos_x, pos_y, 0);

		gs_draw(GS_TRISTRIP, 0, 0);
		gs_matrix_pop();
		gs_load_vertexbuffer(nullptr);
		gs_vertexbuffer_destroy(verts_right);
	}
}

void bar_visualizer::draw_bars()
{
	visual_params params = m_cfg->vm_params[m_cfg->visual];
	if (params.stereo) {
		if (params.rounded_corners) {
			draw_stereo_rounded_bars();
		} else {
			draw_stereo_rectangle_bars();
		}
	} else {
		if (params.rounded_corners) {
			draw_rounded_bars();
		} else {
			draw_rectangle_bars();
		}
	}
	return;
}

void bar_visualizer::render_begin(gs_texture_t *render_target)
{
	pre_target = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_set_render_target(render_target, nullptr);
	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	uint32_t rt_width = gs_texture_get_width(render_target);
	uint32_t rt_height = gs_texture_get_height(render_target);

	gs_ortho(0.0f, (float)rt_width, 0.0f, (float)rt_height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, rt_width, rt_height);
}

void bar_visualizer::render_end()
{
	gs_viewport_pop();
	gs_projection_pop();
	gs_matrix_pop();
	gs_set_render_target(pre_target, nullptr);
}

void bar_visualizer::fill_solid_color()
{
	render_begin(solid_texture);

	const gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *solid_tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 colorVal;
	vec4_from_rgba(&colorVal, (uint32_t)PLSVisualizerResource::Instance()->getSolidColor(m_cfg->vm_params[m_cfg->visual].solid_mode));
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(solid_tech);
	gs_technique_begin_pass(solid_tech, 0);

	gs_draw_sprite(nullptr, 0, gs_texture_get_width(solid_texture), gs_texture_get_height(solid_texture));

	gs_technique_end_pass(solid_tech);
	gs_technique_end(solid_tech);

	render_end();
}

void bar_visualizer::render_bars()
{
	visual_mode mode = m_cfg->visual;
	if (mode == visual_mode::VM_LINEAR_BARS) {
		region_extend_render(source_texture);
		extend_blur_render(extend_texture);
		fill_solid_color();
	}

	render_begin(render_texture);

	if (mode == visual_mode::VM_BASIC_BARS || mode == visual_mode::VM_FILLET_BARS) {

		const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);

		gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(param, source_texture);
		gs_draw_sprite(source_texture, 0, 0, 0);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);

	} else {
		if (mode == visual_mode::VM_LINEAR_BARS) {
			render_mix_sampling(solid_texture, alpha_blur_texture);
		} else if (mode == visual_mode::VM_GRADIENT_BARS) {
			render_mix_sampling(PLSVisualizerResource::Instance()->getGradientTexture(m_cfg->vm_params[mode].gradient_mode), source_texture);
		}
	}
	render_end();
}

void bar_visualizer::render_mix_sampling(gs_texture_t *color_texture, gs_texture_t *src_texture)
{
	gs_technique_begin(mask_tech);
	gs_technique_begin_pass(mask_tech, 0);

	if (color_texture && src_texture) {
		gs_eparam_t *param_image = gs_effect_get_param_by_name(mask_effect, "color_image");
		gs_eparam_t *param_texture = gs_effect_get_param_by_name(mask_effect, "texture_image");
		gs_eparam_t *param_src = gs_effect_get_param_by_name(mask_effect, "src_image");

		gs_effect_set_next_sampler(param_image, sampler);
		gs_effect_set_texture(param_image, color_texture);
		gs_effect_set_texture(param_texture, src_texture);
		gs_effect_set_texture(param_src, source_texture);
		gs_draw_sprite(src_texture, 0, 0, 0);
	} else
		gs_copy_texture(render_texture, source_texture);

	gs_technique_end_pass(mask_tech);
	gs_technique_end(mask_tech);
}

void bar_visualizer::release_frame()
{
	if (render_texture) {
		gs_texture_destroy(render_texture);
		render_texture = nullptr;
	}

	if (source_texture) {
		gs_texture_destroy(source_texture);
		source_texture = nullptr;
	}

	if (alpha_blur_texture) {
		gs_texture_destroy(alpha_blur_texture);
		alpha_blur_texture = nullptr;
	}

	if (solid_texture) {
		gs_texture_destroy(solid_texture);
		solid_texture = nullptr;
	}

	if (extend_texture) {
		gs_texture_destroy(extend_texture);
		extend_texture = nullptr;
	}
}

void bar_visualizer::region_extend_render(gs_texture_t *src)
{
	if (!extend_texture || !src) {
		return;
	}
	render_begin(extend_texture);
	gs_technique_begin(region_extend_tech);

	uint32_t tex_width = gs_texture_get_width(src);
	uint32_t tex_height = gs_texture_get_height(src);
	gs_eparam_t *param_image = gs_effect_get_param_by_name(region_extend_effect, "image");
	gs_effect_set_texture(param_image, src);
	gs_eparam_t *param_inv_size = gs_effect_get_param_by_name(region_extend_effect, "inv_size");
	struct vec2 inv_size = {(float)1.0 / (float)tex_width, (float)1.0 / (float)tex_height};
	gs_effect_set_vec2(param_inv_size, &inv_size);
	gs_eparam_t *param_blur = gs_effect_get_param_by_name(region_extend_effect, "blur_size");
	visual_params params = m_cfg->vm_params[m_cfg->visual];
	if (params.bar_space < EXTEND_RADIUS * 2.0 + 1) {
		gs_effect_set_float(param_blur, 0);
	} else {
		gs_effect_set_float(param_blur, EXTEND_RADIUS);
	}
	gs_technique_begin_pass(region_extend_tech, 0);
	gs_draw_sprite(src, 0, 0, 0);
	gs_technique_end_pass(region_extend_tech);

	gs_technique_end(region_extend_tech);
	render_end();
}

void bar_visualizer::extend_blur_render(gs_texture_t *src)
{
	if (!alpha_blur_texture || !src) {
		return;
	}
	render_begin(alpha_blur_texture);
	gs_technique_begin(outter_blur_tech);

	uint32_t tex_width = gs_texture_get_width(src);
	uint32_t tex_height = gs_texture_get_height(src);
	gs_eparam_t *param_image = gs_effect_get_param_by_name(outter_blur_effect, "image");
	gs_effect_set_texture(param_image, src);
	gs_eparam_t *param_inv_size = gs_effect_get_param_by_name(outter_blur_effect, "inv_size");
	struct vec2 inv_size = {(float)1.0 / (float)tex_width, (float)1.0 / (float)tex_height};
	gs_effect_set_vec2(param_inv_size, &inv_size);
	gs_eparam_t *param_blur = gs_effect_get_param_by_name(outter_blur_effect, "blur_size");
	gs_effect_set_float(param_blur, BLUR);

	gs_technique_begin_pass(outter_blur_tech, 0);
	gs_draw_sprite(src, 0, 0, 0);
	gs_technique_end_pass(outter_blur_tech);

	gs_technique_end(outter_blur_tech);
	render_end();
}
