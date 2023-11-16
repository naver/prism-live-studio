#include <obs.h>
#include <obs.hpp>
#include <obs-module.h>
#include <media-io/audio-resampler.h>

#include <pls/pls-properties.h>
#include <pls/pls-source.h>

#include <array>
#include <mutex>

#include "sound-touch-pipe.h"

const auto ST_PLUGIN = "filter.soundtouch";
const auto ST_MODEL_MAN = "filter.soundtouch.man";
const auto ST_MODEL_WOMAN = "filter.soundtouch.woman";
const auto ST_MODEL_FUNNY = "filter.soundtouch.funny";
const auto ST_MODEL_MAGIC = "filter.soundtouch.magic";
const auto ST_TEMPO = "filter.soundtouch.tempo";
const auto ST_PITCH = "filter.soundtouch.pitch";
const auto ST_RATE = "filter.soundtouch.rate";

#define ST_AUDIO_FORMAT AUDIO_FORMAT_FLOAT
#define OUTPUT_AUDIO_FORMAT AUDIO_FORMAT_FLOAT_PLANAR

#define SAFE_DESTROY_RESAMPLER(s)           \
	if (s) {                            \
		audio_resampler_destroy(s); \
		s = nullptr;                \
	}

//------------------------------------------------------------------
struct sound_touch_filter : public sound_touch_cb {
	obs_source_t *source = nullptr;

	struct obs_audio_info audio_info_to = {};
	struct obs_audio_data audio_out = {};
	audio_resampler_t *resample_to = nullptr;
	audio_resampler_t *resample_out = nullptr;

	uint64_t last_timestamp = 0;

	std::recursive_mutex st_info_lock; // update_st_info is updated in UI thread and accessed in capture thread
	st_effect_info update_st_info = {};
	SOUNDTOUCH_WRAPPER sound_touch;

	//------------------------------------------------------------------
	sound_touch_filter(obs_data_t *settings, obs_source_t *s);
	~sound_touch_filter() override;

	sound_touch_filter(const sound_touch_filter &) = delete;
	sound_touch_filter &operator=(const sound_touch_filter &) = delete;
	sound_touch_filter(sound_touch_filter &&) = delete;
	sound_touch_filter &operator=(sound_touch_filter &&) = delete;

	void clear();
	bool check_reset();
	uint32_t get_audio_channels(enum speaker_layout speakers) const;

	void update(void *data, obs_data_t *settings);

	void get_info(st_effect_info &ei);

	struct obs_audio_data *process_audio(struct obs_audio_data *audio);

	void on_filter_audio(float *data, unsigned framesPerChn, uint64_t timestamp) override;
};

sound_touch_filter::sound_touch_filter(obs_data_t *settings, obs_source_t *s) : source(s)
{
	memset(&audio_info_to, 0, sizeof(audio_info_to));
	memset(&update_st_info, 0, sizeof(update_st_info));

	update(this, settings);
}

sound_touch_filter::~sound_touch_filter()
{
	clear();
}

void sound_touch_filter::clear()
{
	SAFE_DESTROY_RESAMPLER(resample_to)
	SAFE_DESTROY_RESAMPLER(resample_out)
	sound_touch.reset();
}

uint32_t sound_touch_filter::get_audio_channels(enum speaker_layout speakers) const
{
	switch (speakers) {
	case SPEAKERS_MONO:
		return 1;
	case SPEAKERS_STEREO:
		return 2;
	case SPEAKERS_2POINT1:
		return 3;
	case SPEAKERS_4POINT0:
		return 4;
	case SPEAKERS_4POINT1:
		return 5;
	case SPEAKERS_5POINT1:
		return 6;
	case SPEAKERS_7POINT1:
		return 8;

	default:
		assert(false);
		return 0;
	}
}

bool sound_touch_filter::check_reset()
{
	st_effect_info ei;
	get_info(ei);

	struct obs_audio_info oai;
	obs_get_audio_info(&oai);

	uint32_t channels = get_audio_channels(oai.speakers);

	//----------------------------- check resampler --------------------------------
	if ((resample_to || resample_out) && (audio_info_to.samples_per_sec != oai.samples_per_sec || audio_info_to.speakers != oai.speakers)) {
		SAFE_DESTROY_RESAMPLER(resample_to)
		SAFE_DESTROY_RESAMPLER(resample_out)
	}
	if (!resample_to) {
		audio_info_to = oai;

		struct resample_info res_to;
		res_to.samples_per_sec = oai.samples_per_sec;
		res_to.speakers = oai.speakers;
		res_to.format = ST_AUDIO_FORMAT;

		struct resample_info res_back;
		res_back.samples_per_sec = oai.samples_per_sec;
		res_back.speakers = oai.speakers;
		res_back.format = OUTPUT_AUDIO_FORMAT;

		resample_to = audio_resampler_create(&res_to, &res_back);
		resample_out = audio_resampler_create(&res_back, &res_to);
		assert(resample_to && resample_out);
	}

	//------------------------------- check soundtouch ------------------------------
	st_audio_info stream;

	if (sound_touch) {
		st_effect_info effect;
		sound_touch->get_setup_info(stream, effect);

		if ((stream.sample_rate != oai.samples_per_sec) || (stream.channels != channels) || (effect.tempo != ei.tempo) || (effect.pitch != ei.pitch) || (effect.rate != ei.rate)) {
			sound_touch.reset();
		}
	}

	if (!sound_touch) {
		stream.sample_rate = oai.samples_per_sec;
		stream.channels = channels;

		sound_touch = SOUNDTOUCH_WRAPPER(std::make_shared<sound_touch_wrapper>(this));
		if (!sound_touch->setup_soundtouch(stream, ei)) {
			sound_touch.reset();
		}
	}

	return !!sound_touch;
}

void sound_touch_filter::update(void *data, obs_data_t *settings)
{
	auto filter = (sound_touch_filter *)data;
	st_effect_info temp;
	temp.tempo = obs_data_get_int(settings, ST_TEMPO);
	temp.pitch = obs_data_get_int(settings, ST_PITCH);
	temp.rate = obs_data_get_int(settings, ST_RATE);

	{
		std::lock_guard<std::recursive_mutex> auto_lock(st_info_lock);
		update_st_info = temp; // applied in process_audio function
	}
}

void sound_touch_filter::get_info(st_effect_info &ei)
{
	std::lock_guard<std::recursive_mutex> auto_lock(st_info_lock);
	ei = update_st_info;
}

struct obs_audio_data *sound_touch_filter::process_audio(struct obs_audio_data *audio)
{
	/* -----------------------------------------------
	 * if timestamp has dramatically changed, consider it a new stream of
	 * audio data.  clear all circular buffers to prevent old audio data
	 * from being processed as part of the new data. */
	if (last_timestamp > 0) {
		int64_t diff = llabs((int64_t)last_timestamp - (int64_t)audio->timestamp);
		if (diff > 1000000000LL) {
			info("Reset soundtouch because timestamp is dramatically changed. last_timestamp:%llu new_timestamp:%llu", last_timestamp, audio->timestamp);
			clear();
		}
	}
	last_timestamp = audio->timestamp;

	if (!check_reset()) {
		/* skip this filter */
		return audio;
	}

	std::array<uint8_t *, MAX_AV_PLANES> resample_data = {};
	uint32_t out_frames = 0;
	uint64_t ts_offset = 0;
	bool suc = audio_resampler_resample(resample_to, resample_data.data(), &out_frames, &ts_offset, audio->data, audio->frames);
	if (suc && out_frames > 0) {
		sound_touch->push_audio((float *)resample_data[0], out_frames, audio->timestamp);
	}

	memset(&audio_out, 0, sizeof(audio_out));
	sound_touch->pop_audio();

	if (audio_out.frames > 0) {
		return &audio_out;
	} else {
		return nullptr;
	}
}

void sound_touch_filter::on_filter_audio(float *data, unsigned framesPerChn, uint64_t timestamp)
{
	std::array<uint8_t *, MAX_AV_PLANES> input = {};
	input[0] = (uint8_t *)data;

	std::array<uint8_t *, MAX_AV_PLANES> resample_data = {};
	uint32_t out_frames = 0;
	uint64_t ts_offset = 0;
	bool suc = audio_resampler_resample(resample_out, resample_data.data(), &out_frames, &ts_offset, input.data(), framesPerChn);
	if (suc && out_frames > 0) {
		audio_out.timestamp = timestamp;
		audio_out.frames = out_frames;
		for (size_t i = 0; i < MAX_AV_PLANES; i++) {
			audio_out.data[i] = resample_data[i];
		}
	}
}

//--------------------------------------------------------------------------------------------
static void sound_touch_defaults(obs_data_t *s)
{
	obs_data_set_default_int(s, ST_TEMPO, 0);
	obs_data_set_default_int(s, ST_PITCH, 0);
	obs_data_set_default_int(s, ST_RATE, 0);
}

static bool pitch_model_clicked(obs_properties_t *ppts, obs_property_t *p, void *data, int pitch, int idx)
{
	auto filter = (struct sound_touch_filter *)data;
	obs_data_t *settings = obs_source_get_settings(filter->source);

	obs_data_set_int(settings, ST_PITCH, pitch);
	obs_data_set_string(settings, "clicked_name", obs_property_name(p));
	obs_data_set_int(settings, "clicked_index", idx);
	filter->update(filter, settings);
	obs_data_release(settings);

	obs_property_t *property = obs_properties_first(ppts);
	while (property) {
		auto count = pls_property_button_group_item_count(property);
		for (int i = 0; i < count; i++) {
			pls_property_button_group_set_item_highlight(property, i, (property == p) && (idx == i));
		}
		obs_property_next(&property);
	}

	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(filter->source);
	obs_data_set_bool(privateSettings, "set_default_highlight", false);

	obs_source_update_properties(filter->source);
	return true;
}

static bool is_clicked(sound_touch_filter const *filter, char const *model, int index)
{
	obs_data_t *settings = obs_source_get_settings(filter->source);
	const char *clicked_name = obs_data_get_string(settings, "clicked_name");
	auto clicked_index = (int)obs_data_get_int(settings, "clicked_index");
	obs_data_release(settings);
	return (index == clicked_index) && (clicked_name && 0 == strcmp(clicked_name, model));
}

static obs_properties_t *sound_touch_properties(void *data)
{
	obs_properties_t *ppts = pls_properties_create();
	auto filter = (struct sound_touch_filter *)data;

	// MAN
	obs_property_t *p = pls_properties_add_button_group(ppts, "model1", "");
	pls_property_button_group_add_item(p, ST_MODEL_MAN, obs_module_text(ST_MODEL_MAN), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, -3, 0); });
	pls_property_button_group_set_item_highlight(p, 0, is_clicked(filter, "model1", 0));

	// WOMAN
	pls_property_button_group_add_item(p, ST_MODEL_WOMAN, obs_module_text(ST_MODEL_WOMAN), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, 3, 1); });
	pls_property_button_group_set_item_highlight(p, 1, is_clicked(filter, "model1", 1));

	// MAGIC
	p = pls_properties_add_button_group(ppts, "model2", "");
	pls_property_button_group_add_item(p, ST_MODEL_MAGIC, obs_module_text(ST_MODEL_MAGIC), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, -6, 0); });
	pls_property_button_group_set_item_highlight(p, 0, is_clicked(filter, "model2", 0));

	// FUNNY
	pls_property_button_group_add_item(p, ST_MODEL_FUNNY, obs_module_text(ST_MODEL_FUNNY), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, 6, 1); });
	pls_property_button_group_set_item_highlight(p, 1, is_clicked(filter, "model2", 1));

	// SLIDER
	p = obs_properties_add_int_slider(ppts, ST_PITCH, obs_module_text(ST_PITCH), -9, 9, 1);

	return ppts;
}

static void prism_filter_set_private_data(void *data, obs_data_t *private_data)
{
	auto filter = static_cast<sound_touch_filter *>(data);

	std::string method = obs_data_get_string(private_data, "method");
	if (method == "set_default") {
		OBSDataAutoRelease privateSettings = obs_source_get_private_settings(filter->source);
		obs_data_set_bool(privateSettings, "set_default_highlight", true);
	}
}

void register_filter_sound_touch()
{
	obs_source_info info = {};
	info.id = "sound_touch_filter";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_AUDIO;

	info.get_name = [](void *) { return obs_module_text(ST_PLUGIN); };
	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * { return new sound_touch_filter(settings, source); };
	info.destroy = [](void *data) { delete static_cast<sound_touch_filter *>(data); };
	info.update = [](void *data, obs_data_t *settings) { static_cast<sound_touch_filter *>(data)->update(data, settings); };
	info.filter_audio = [](void *data, struct obs_audio_data *audio) { return static_cast<sound_touch_filter *>(data)->process_audio(audio); };

	info.get_defaults = sound_touch_defaults;
	info.get_properties = sound_touch_properties;

	pls_source_info prism_info = {};
	prism_info.set_private_data = prism_filter_set_private_data;
	register_pls_source_info(&info, &prism_info);

	obs_register_source(&info);
}
