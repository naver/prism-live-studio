#include "sound-touch-pipe.h"
#include <obs.h>
#include <assert.h>
#include <liblog.h>

const auto MAX_READ_SAMPLES = 48000; // per channel

sound_touch_wrapper::sound_touch_wrapper(sound_touch_cb *cb) : callback(cb)
{
	memset(&save_buffer, 0, sizeof(save_buffer));
	info_queue.reserve(20);
}

sound_touch_wrapper::~sound_touch_wrapper()
{
	circlebuf_free(&save_buffer);
}

bool sound_touch_wrapper::setup_soundtouch(const st_audio_info &stream, const st_effect_info &effect)
{
	//------------------ clear old state --------------------
	clear();

	//------------------- init new state -------------------
	stream_settings = stream;
	effect_settings = effect;

	try {
		sound_touch.setSampleRate(stream.sample_rate);
		sound_touch.setChannels(stream.channels);

		sound_touch.setTempoChange((double)effect.tempo);
		sound_touch.setPitchSemiTones((int)effect.pitch);
		sound_touch.setRateChange((double)effect.rate);

		// from soundstretch, incread speed, reduce quality
		sound_touch.setSetting(SETTING_SEQUENCE_MS, 40);
		sound_touch.setSetting(SETTING_SEEKWINDOW_MS, 15);
		sound_touch.setSetting(SETTING_OVERLAP_MS, 8);
		sound_touch.setSetting(SETTING_USE_QUICKSEEK, 1);
		sound_touch.setSetting(SETTING_USE_AA_FILTER, 0);
	} catch (...) {
		warn("Exception happened");
		return false;
	}

	circlebuf_reserve(&save_buffer, stream.sample_rate * sizeof(float) * stream.channels);

	output_ratio = sound_touch.getInputOutputSampleRatio();
	first_timestamp = -1;

	unsigned sz = sizeof(float) * MAX_READ_SAMPLES * stream.channels;
	read_buffer = std::shared_ptr<float>(new float[sz], std::default_delete<float[]>());

	return true;
}

void sound_touch_wrapper::get_setup_info(st_audio_info &stream, st_effect_info &effect) const
{
	stream = stream_settings;
	effect = effect_settings;
}

void sound_touch_wrapper::push_audio(const float *data, unsigned framesPerChn, uint64_t timestamp)
{
	assert(framesPerChn < stream_settings.sample_rate);
	if (!data)
		return;

	try {
		sound_touch.putSamples(data, framesPerChn);
	} catch (...) {
		warn("Exception happened");
		return;
	}

	if (first_timestamp < 0) {
		first_timestamp = timestamp;
	}

	audio_packet_info info;
	info.insert_time = os_gettime_ns() / 1000000;
	info.ts = timestamp - first_timestamp;
	info.framesPerChn_input = framesPerChn;
	info.framesPerChn_output = unsigned(output_ratio * (double)framesPerChn);

	info_queue.push_back(info);
}

void sound_touch_wrapper::pop_audio()
{
	while (true) {
		unsigned frames = sound_touch.receiveSamples(read_buffer.get(), MAX_READ_SAMPLES);
		if (frames <= 0)
			break;

		circlebuf_push_back(&save_buffer, read_buffer.get(), sizeof(float) * frames * stream_settings.channels);
		saved_frames += frames;
	}

	if (saved_frames >= stream_settings.sample_rate) {
		warn("Buffer of audio is too large. saved_frames_per_chn:%lld samplerate:%u", saved_frames, stream_settings.sample_rate);
	}

	push_result();
}

void sound_touch_wrapper::push_result()
{
	if (info_queue.empty() || saved_frames <= 0)
		return;

	int64_t output_frames = 0;
	std::vector<audio_packet_info>::iterator itr = info_queue.begin();

	while (itr != info_queue.end()) {
		if (saved_frames < (output_frames + itr->framesPerChn_output)) {
			break;
		}
		output_frames += itr->framesPerChn_output;
		++itr;

#ifdef DEBUG_SOUND_TOUCH
		uint64_t ts_second = itr->ts / 1000000000;
		uint64_t delay = (os_gettime_ns() / 1000000) - itr->insert_time;
		if (ts_second == current_second) {
			delay_time_list.push_back(delay);
		} else {
			std::string str = "";
			char temp[20];
			for (size_t i = 0; i < delay_time_list.size(); i++) {
				sprintf(temp, "%lldms ", delay_time_list[i]);
				str += temp;
			}
			PLS_PLUGIN_DEBUG("ts:%llds [frameCount:%u] %s", current_second, delay_time_list.size(), str.c_str());
			current_second = ts_second;
			delay_time_list.clear();
		}
#endif // DEBUG_SOUND_TOUCH
	}

	if (output_frames > 0) {
		audio_packet_info info = info_queue[0];
		uint64_t timestamp = first_timestamp + uint64_t(output_ratio * double(info.ts));
		size_t push_size = sizeof(float) * output_frames * stream_settings.channels;

		if (push_buffer_size < push_size) {
			push_buffer_size = push_size;
			push_buffer = std::shared_ptr<float>(new float[push_size], std::default_delete<float[]>());
		}

		info_queue.erase(info_queue.begin(), itr);
		circlebuf_pop_front(&save_buffer, push_buffer.get(), push_size);
		saved_frames -= output_frames;

		callback->on_filter_audio(push_buffer.get(), (unsigned)output_frames, timestamp);
	}
}

void sound_touch_wrapper::clear()
{
	first_timestamp = -1;

	try {
		sound_touch.clear();
	} catch (...) {
		warn("Exception happened");
	}

	saved_frames = 0;
	circlebuf_pop_front(&save_buffer, nullptr, save_buffer.size);
	info_queue.clear();
}
