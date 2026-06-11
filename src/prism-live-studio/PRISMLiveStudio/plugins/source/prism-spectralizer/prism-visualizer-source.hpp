/**
 * This file is part of spectralizer
 * which is licensed under the GPL v2.0
 * See LICENSE or http://www.gnu.org/licenses
 * github.com/univrsal/spectralizer
 */
#pragma once

#include "util.hpp"
#include <cstdint>
#include <map>
#include <mutex>
#include <fftw3.h>

class audio_visualizer;

using fftw_plan_dft_r2c_1d_func = fftw_plan (*)(int n, double *in, fftw_complex *out, unsigned int flags);
using fftw_destroy_plan_func = void (*)(fftw_plan p);
using fftw_execute_func = void (*)(fftw_plan p);

struct visual_params {
	smooting_mode smoothing = smooting_mode::SM_NONE;
	uint32_t color = defaults::basic_color;
	uint16_t detail = defaults::basic_detail;

	/* smoothing */
	uint32_t sgs_points = defaults::sgs_points;
	uint32_t sgs_passes = defaults::sgs_passes;

	/* scaling */
	bool use_auto_scale = defaults::use_auto_scale;
	double scale_boost = defaults::scale_boost;
	double scale_size = defaults::scale_size;

	double mcat_smoothing_factor = defaults::mcat_smooth;

	/* Bar visualizer settings */
	enum bar_mode bar_mode = bar_mode::BM_BASIC;
	uint16_t bar_space = defaults::basic_space;
	uint16_t bar_width = defaults::basic_width;
	uint16_t bar_height = defaults::basic_height;
	uint16_t bar_min_height = defaults::basic_min_height;
	float radius = 0.0;

	bool rounded_corners = false;
	float corner_radius = 0.5f;
	uint16_t corner_points = 10;

	/* General spectrum settings */
	bool stereo = defaults::stereo;
	uint16_t stereo_space = 0;
	double falloff_weight = defaults::falloff_weight;
	double gravity = defaults::gravity;

	/* Color settings */
	enum solid_color solid_mode = solid_color::SOLID_COLOR_1;
	enum gradient_color gradient_mode = gradient_color::GRADIENT_COLOR_0;
};

struct config {
	std::mutex value_mutex;

	/* obs source stuff */
	obs_source_t *source{};
	obs_data_t *settings{};

	std::string capture_name{};
	obs_weak_source_t *capture_source{};

	gs_texture_t *render_texture{};

	std::string audio_source_name = "";
	uint16_t fps = defaults::fps;

	/* Audio settings */
	uint32_t sample_rate = defaults::sample_rate;
	uint32_t sample_size = defaults::sample_size;

	double low_cutoff_freq = defaults::lfreq_cut;
	double high_cutoff_freq = defaults::hfreq_cut;

	uint16_t cx = defaults::cx;
	uint16_t cy = defaults::cy;
	uint16_t draw_cx = defaults::cx;
	uint16_t draw_cy = defaults::cy;
	uint16_t margin = defaults::margin;

	/* Misc */
	bool auto_clear = false;
	bool read_data = false; /* Audio source will return false if reading failed */
	pcm_stereo_sample *buffer = nullptr;

	/* Appearance settings */
	visual_mode visual = visual_mode::VM_BASIC_BARS;

	std::map<visual_mode, visual_params> vm_params{};
};

enum class source_event_type { SET_CREATE = 1, SET_DESTROY, SET_RENAME };

struct changed_source_info {
	source_event_type type;
	std::string prev_name;
	std::string cur_name;
};

class PLSVisualizerResource {
protected:
	PLSVisualizerResource();
	virtual ~PLSVisualizerResource();

public:
	static PLSVisualizerResource *Instance()
	{
		static PLSVisualizerResource resource;
		return &resource;
	};

	gs_texture_t *getGradientTexture(gradient_color mode) { return gradient_texture[mode]; }
	long long getSolidColor(solid_color mode) { return solid_rgb[mode]; }
	std::vector<std::string> getAudioSources() const { return audio_sources; }
	void setAudioSources(std::vector<std::string> vec)
	{
		audio_sources.clear();
		audio_sources.swap(vec);
	}

	void freeVisualizerResource();

private:
	static void source_changed(void *data, calldata_t *calldata);

	std::map<solid_color, long long> solid_rgb{};
	std::map<gradient_color, gs_texture_t *> gradient_texture{};
	std::vector<std::string> audio_sources{};

	source_event_type create_type = source_event_type::SET_CREATE;
	source_event_type destroy_type = source_event_type::SET_DESTROY;
	source_event_type rename_type = source_event_type::SET_RENAME;
};

class visualizer_source {
	config config;
	std::unique_ptr<audio_visualizer> visualizer{nullptr};

public:
	visualizer_source(obs_source_t *source, obs_data_t *settings);
	~visualizer_source() = default;

	void destory();
	void init_params();
	void update(obs_data_t *settings);
	void tick(float seconds) const;
	void render(gs_effect_t *effect);
	bool set_private_data(obs_data_t *private_data);
	obs_properties_t *get_properties_for_visualiser();
	uint32_t get_width() const { return config.cx; }
	uint32_t get_height() const { return config.cy; }
	visual_params get_current_visual_params(visual_mode vm);
	visual_mode get_old_visual_mode() const;
	obs_data_t *getPropsParams();
};
