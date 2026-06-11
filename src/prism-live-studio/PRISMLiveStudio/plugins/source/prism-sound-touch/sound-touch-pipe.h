#pragma once
#include <SoundTouch.h>
#include <util/circlebuf.h>
#include <util/platform.h>
#include <memory>
#include <vector>
#include <liblog.h>

//#define DEBUG_SOUND_TOUCH

#define warn(format, ...) pls_warn(false, "[sound-touch filter] ", __FILE__, __LINE__, format, ##__VA_ARGS__)
#define info(format, ...) pls_info(false, "[sound-touch filter] ", __FILE__, __LINE__, format, ##__VA_ARGS__)

struct st_audio_info {
	unsigned sample_rate;
	unsigned channels;
};

struct st_effect_info {
	int64_t tempo; // [-50, 100]
	int64_t pitch; // [-12, 12]
	int64_t rate;  // [-50, 100]
};

class sound_touch_cb {
public:
	virtual ~sound_touch_cb() = default;
	virtual void on_filter_audio(float *data, unsigned framesPerChn, uint64_t timestamp) = 0;
};

// It should be accessed in one thread because there is no mutex inner.
class sound_touch_wrapper {
public:
	explicit sound_touch_wrapper(sound_touch_cb *cb);
	virtual ~sound_touch_wrapper();

	sound_touch_wrapper(const sound_touch_wrapper &) = delete;
	sound_touch_wrapper &operator=(const sound_touch_wrapper &) = delete;
	sound_touch_wrapper(sound_touch_wrapper &&) = delete;
	sound_touch_wrapper &operator=(sound_touch_wrapper &&) = delete;

	bool setup_soundtouch(const st_audio_info &stream, const st_effect_info &effect);
	void get_setup_info(st_audio_info &stream, st_effect_info &effect) const;

	void push_audio(const float *data, unsigned framesPerChn, uint64_t timestamp);
	void pop_audio();

private:
	void push_result();
	void clear();

	struct audio_packet_info {
		uint64_t insert_time; // in ms
		int64_t ts;
		int64_t framesPerChn_input;
		int64_t framesPerChn_output;
	};
	std::vector<audio_packet_info> info_queue;

	sound_touch_cb *callback = nullptr;
	int64_t first_timestamp = -1;
	double output_ratio = 1.0;

	soundtouch::SoundTouch sound_touch;
	st_audio_info stream_settings;
	st_effect_info effect_settings;

	std::shared_ptr<float> read_buffer;

	struct circlebuf save_buffer;
	int64_t saved_frames = 0;

	std::shared_ptr<float> push_buffer;
	size_t push_buffer_size = 0;

	std::vector<uint64_t> delay_time_list;
};

using SOUNDTOUCH_WRAPPER = std::shared_ptr<sound_touch_wrapper>;
