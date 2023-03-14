#include <obs.h>
#include <obs-module.h>
#include <media-io/audio-resampler.h>
#include <mutex>
#include "sound-touch-pipe.h"
#include <log.h>

#define warn(format, ...) PLS_PLUGIN_WARN("[sound-touch filter: '%s'] " format, obs_source_get_name(source), ##__VA_ARGS__)
#define info(format, ...) PLS_PLUGIN_INFO("[sound-touch filter: '%s'] " format, obs_source_get_name(source), ##__VA_ARGS__)

#define ST_PLUGIN "filter.soundtouch"
#define ST_MODEL_MAN "filter.soundtouch.man"
#define ST_MODEL_WOMAN "filter.soundtouch.woman"
#define ST_MODEL_FUNNY "filter.soundtouch.funny"
#define ST_MODEL_MAGIC "filter.soundtouch.magic"
#define ST_TEMPO "filter.soundtouch.tempo"
#define ST_PITCH "filter.soundtouch.pitch"
#define ST_RATE "filter.soundtouch.rate"

#define ST_AUDIO_FORMAT AUDIO_FORMAT_FLOAT
#define OUTPUT_AUDIO_FORMAT AUDIO_FORMAT_FLOAT_PLANAR

#define SAFE_DELETE_OBJ(p) \
	if (p) {           \
		delete p;  \
		p = NULL;  \
	}

#define SAFE_DESTROY_RESAMPLER(s)           \
	if (s) {                            \
		audio_resampler_destroy(s); \
		s = NULL;                   \
	}

//------------------------------------------------------------------
struct sound_touch_filter : public sound_touch_cb {
	obs_source_t *source = NULL;

	struct obs_audio_info audio_info_to = {};
	struct obs_audio_data audio_out = {};
	audio_resampler_t *resample_to = NULL;
	audio_resampler_t *resample_out = NULL;

	uint64_t last_timestamp = 0;

	std::mutex st_info_lock; // update_st_info is updated in UI thread and accessed in capture thread
	st_effect_info update_st_info = {};
	sound_touch_wrapper *sound_touch = NULL;

	//------------------------------------------------------------------
	sound_touch_filter(obs_data_t *settings, obs_source_t *s);
	~sound_touch_filter();

	void clear();
	void check_reset();
	uint32_t get_audio_channels(enum speaker_layout speakers);

	void update(obs_data_t *settings);
	struct obs_audio_data *process_audio(struct obs_audio_data *audio);

	virtual void on_filter_audio(float *data, unsigned framesPerChn, uint64_t timestamp);
};

sound_touch_filter::sound_touch_filter(obs_data_t *settings, obs_source_t *s) : source(s)
{
	memset(&audio_info_to, 0, sizeof(audio_info_to));
	memset(&update_st_info, 0, sizeof(update_st_info));

	update(settings);
}

sound_touch_filter::~sound_touch_filter()
{
	clear();
}

void sound_touch_filter::clear()
{
	SAFE_DESTROY_RESAMPLER(resample_to);
	SAFE_DESTROY_RESAMPLER(resample_out);
	SAFE_DELETE_OBJ(sound_touch);
}

uint32_t sound_touch_filter::get_audio_channels(enum speaker_layout speakers)
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

	case SPEAKERS_UNKNOWN:
	default:
		assert(false);
		return 0;
	}
}

void sound_touch_filter::check_reset()
{
	st_effect_info ei;
	{
		std::lock_guard<std::mutex> auto_lock(st_info_lock);
		ei = update_st_info;
	}

	struct obs_audio_info oai;
	obs_get_audio_info(&oai);

	uint32_t channels = get_audio_channels(oai.speakers);

	//----------------------------- check resampler --------------------------------
	if (resample_to || resample_out) {
		if (audio_info_to.samples_per_sec != oai.samples_per_sec || audio_info_to.speakers != oai.speakers) {
			SAFE_DESTROY_RESAMPLER(resample_to);
			SAFE_DESTROY_RESAMPLER(resample_out);
		}
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
			SAFE_DELETE_OBJ(sound_touch);
		}
	}

	if (!sound_touch) {
		stream.sample_rate = oai.samples_per_sec;
		stream.channels = channels;

		sound_touch = new sound_touch_wrapper(this);
		sound_touch->setup_soundtouch(stream, ei);
	}
}

void sound_touch_filter::update(obs_data_t *settings)
{
	st_effect_info temp;
	temp.tempo = (int64_t)obs_data_get_int(settings, ST_TEMPO);
	temp.pitch = (int64_t)obs_data_get_int(settings, ST_PITCH);
	temp.rate = (int64_t)obs_data_get_int(settings, ST_RATE);

	std::lock_guard<std::mutex> auto_lock(st_info_lock);
	update_st_info = temp; // applied in process_audio function
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

	check_reset();

	uint8_t *resample_data[MAX_AV_PLANES] = {};
	uint32_t out_frames = 0;
	uint64_t ts_offset = 0;
	bool suc = audio_resampler_resample(resample_to, resample_data, &out_frames, &ts_offset, audio->data, audio->frames);
	if (suc && out_frames > 0) {
		sound_touch->push_audio((float *)resample_data[0], out_frames, audio->timestamp);
	}

	memset(&audio_out, 0, sizeof(audio_out));
	sound_touch->pop_audio();

	if (audio_out.frames > 0) {
		return &audio_out;
	} else {
		return NULL;
	}
}

void sound_touch_filter::on_filter_audio(float *data, unsigned framesPerChn, uint64_t timestamp)
{
	uint8_t *input[MAX_AV_PLANES] = {};
	input[0] = (uint8_t *)data;

	uint8_t *resample_data[MAX_AV_PLANES] = {};
	uint32_t out_frames = 0;
	uint64_t ts_offset = 0;
	bool suc = audio_resampler_resample(resample_out, resample_data, &out_frames, &ts_offset, input, framesPerChn);
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
	struct sound_touch_filter *filter = (struct sound_touch_filter *)data;

	obs_data_t *settings = obs_source_get_settings(filter->source);

	obs_data_set_int(settings, ST_PITCH, pitch);
	obs_data_set_string(settings, "clicked_name", obs_property_name(p));
	obs_data_set_int(settings, "clicked_index", idx);
	filter->update(settings);

	obs_data_release(settings);

	obs_property_t *property = obs_properties_first(ppts);
	while (property) {
		obs_property_type type = obs_property_get_type(property);
		if (OBS_PROPERTY_BUTTON_GROUP == type) {
			int count = obs_property_button_group_item_count(property);
			for (int i = 0; i < count; i++) {
				obs_property_button_group_set_item_highlight(property, i, (property == p) && (idx == i));
			}
		}
		obs_property_next(&property);
	}
	return true;
}

static obs_properties_t *sound_touch_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();

	struct sound_touch_filter *filter = (struct sound_touch_filter *)data;
	obs_data_t *settings = obs_source_get_settings(filter->source);
	const char *clicked_name = obs_data_get_string(settings, "clicked_name");
	int clicked_index = obs_data_get_int(settings, "clicked_index");
	obs_data_release(settings);

	obs_property_t *p = obs_properties_add_button_group(ppts, "model1", "");
	obs_property_button_group_add_item(p, ST_MODEL_MAN, obs_module_text(ST_MODEL_MAN), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, -3, 0); });
	obs_property_button_group_set_item_highlight(p, 0, (0 == clicked_index) && (clicked_name && 0 == strcmp(clicked_name, "model1")));
	obs_property_button_group_add_item(p, ST_MODEL_WOMAN, obs_module_text(ST_MODEL_WOMAN), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, 3, 1); });
	obs_property_button_group_set_item_highlight(p, 1, (1 == clicked_index) && (clicked_name && 0 == strcmp(clicked_name, "model1")));

	p = obs_properties_add_button_group(ppts, "model2", "");
	obs_property_button_group_add_item(p, ST_MODEL_MAGIC, obs_module_text(ST_MODEL_MAGIC), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, -6, 0); });
	obs_property_button_group_set_item_highlight(p, 0, (0 == clicked_index) && (clicked_name && 0 == strcmp(clicked_name, "model2")));
	obs_property_button_group_add_item(p, ST_MODEL_FUNNY, obs_module_text(ST_MODEL_FUNNY), true,
					   [](obs_properties_t *pps, obs_property_t *prop, void *private_data) { return pitch_model_clicked(pps, prop, private_data, 6, 1); });
	obs_property_button_group_set_item_highlight(p, 1, (1 == clicked_index) && (clicked_name && 0 == strcmp(clicked_name, "model2")));

	obs_properties_add_int_slider(ppts, ST_PITCH, obs_module_text(ST_PITCH), -9, 9, 1);

	return ppts;
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
	info.update = [](void *data, obs_data_t *settings) { static_cast<sound_touch_filter *>(data)->update(settings); };
	info.filter_audio = [](void *data, struct obs_audio_data *audio) { return static_cast<sound_touch_filter *>(data)->process_audio(audio); };

	info.get_defaults = sound_touch_defaults;
	info.get_properties = sound_touch_properties;

	obs_register_source(&info);
}
