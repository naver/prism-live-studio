#include <stdlib.h>

#include <obs-module.h>
#include <log.h>
#include "graphics/matrix4.h"

#include "pls/pls-source.h"
#include "pls/pls-properties.h"
#include "libutils-api.h"
#include "frontend-api.h"
#include "pls/pls-obs-api.h"

#include <string>
#include <sstream>
#include <list>
#include <iostream>
#include <ctime>
#include <vector>
#include <algorithm>
#include <random>
#include <set>
#include <map>
#include <math.h>
#include <mutex>
#include <array>
#include <QFontDatabase>

#include "util/platform.h"
#include "utils-api.h"
using namespace std;

/* clang-format off */
//
//	|_____________________610_______________________|
//	|		|				|
//	|		|				|
//	| 120x120	|_________490x68________________|
//	|		|                               |
//	|		|                               |
//	|_______________|_________490x52________________|
//
/* clang-format on */

static const int SOURCE_TOP_MARGIN = 22;
static const int SOURCE_BOTTOM_MARGIN = 22;
static const int SOURCE_LEFT_RIGHT_MARGIN = 22;
static const int INNER_RADIUS = 3;
static const double PI = acos(-1);

static const int BASE_WIDTH = 690 + SOURCE_LEFT_RIGHT_MARGIN * 2;
static const int BASE_HEIGHT = 198 + SOURCE_TOP_MARGIN + SOURCE_BOTTOM_MARGIN;

static const int COVER_LEFT_RIGHT_MARGIN = 40 + SOURCE_LEFT_RIGHT_MARGIN;
static const int COVER_TOP_BOTTOM_MARGIN = 34;
static const int NAME_PRODUCER_HEIGHT = 65;
static const int PRODUCER_MARGIN_TOP = 5;

static const int COVER_IMAGE_WIDTH = 130;
static const int COVER_IMAGE_HEIGHT = 130;

static const int TEXT_LEFT_MARGIN = 30;

static const int NAME_WIDTH = BASE_WIDTH - COVER_IMAGE_WIDTH - TEXT_LEFT_MARGIN - COVER_LEFT_RIGHT_MARGIN * 2 - SOURCE_LEFT_RIGHT_MARGIN * 2;
static const int NAME_HEIGHT = 58;

static const int PRODUCER_WIDTH = BASE_WIDTH - COVER_IMAGE_WIDTH - TEXT_LEFT_MARGIN - COVER_LEFT_RIGHT_MARGIN * 2 - SOURCE_LEFT_RIGHT_MARGIN * 2;
static const int PRODUCER_HEIGHT = 47;

static const float BG_COLOR_R = 21.f / 255.f;
static const float BG_COLOR_G = BG_COLOR_R;
static const float BG_COLOR_B = BG_COLOR_R;
static const float BG_COLOR_A = 0.8f;

static constexpr auto IS_LOOP = "is_loop";
static constexpr auto IS_SHOW = "is_show";
static constexpr auto SCENE_ENABLE = "scene enable";
static constexpr auto MUSIC = "music";
static constexpr auto RANDOM_PLAY = "random play";
static constexpr auto PLAY_IN_ORDER = "play in order";
static constexpr auto GROUP = "group";
static constexpr auto PLAY_LIST = "play list";
static constexpr auto TITLE = "title";
static constexpr auto PRODUCER = "producer";
static constexpr auto DURATION = "duration";
static constexpr auto DURATION_TYPE = "duration_type";

static constexpr auto NAME_FONT = "name_font";
static constexpr auto NAME_COLOR = "name_color";
static constexpr auto NAME_OPACITY = "name_opacity";
static constexpr auto IS_CURRENT = "is_current";
static constexpr auto IS_LOCAL_FILE = "is_local_file";
static constexpr auto IS_DISABLE = "is_disable";
static constexpr auto HAS_COVER = "has_cover";
static constexpr auto COVER_PATH = "cover_path";
static constexpr auto URLS = "urls";

static constexpr auto DEFAULT_COVER_IMAGE = "bgm-default.png";

#define TEXT_MUSIC obs_module_text("PlayList")
#define TEXT_SHOW obs_module_text("ShowMusicInfo")
#define TEXT_SCENE_ENABLE obs_module_text("PlayWhenActivate")
#define TEXT_SONG_COUNT obs_module_text("SongCount")
#define TEXT_TIPS obs_module_text("Tips")

#define TEXT_OPEN_BUTTON obs_module_text("Add.And.Edit")

const int VALID_FONT_SIZE = 6;
static const std::array<std::string, VALID_FONT_SIZE> font_array = {"Segoe UI", "MalgunGothic", "Malgun Gothic", "Dotum", "Gulin", "Arial Unicode MS"};

struct prism_source_wrapper {
	obs_source_t *default_source = nullptr;
	obs_source_t *source = nullptr;
	gs_texture_t *texture = nullptr;
	gs_texture_t *mask = nullptr;
	gs_rect targetRect = {0};
	uint32_t width = 0;
	uint32_t height = 0;
	bool is_cover = false;
};

struct bgm_url_info {
	std::string title;
	std::string duration_type;
	std::string producer;
	std::string url;
	std::string duration;
	std::string cover_path;
	std::string group;
	bool is_local_file{false};
	bool is_disable{false};
	bool is_current{false};
	bool has_cover{false};
};

struct latest_url {
	string url;
	string id;
	bool is_loaclfile{false};
	bool update{false};
};

struct prism_bgm_source {
	obs_source_t *media_source{};
	obs_source_t *source{};
	obs_source_t *name_scroll_source{};
	obs_source_t *producer_scroll_source{};

	prism_source_wrapper name_source{};
	prism_source_wrapper producer_source{};
	prism_source_wrapper cover_source{};

	gs_texture_t *output_texture{};
	uint32_t output_width{BASE_WIDTH};
	uint32_t output_height{BASE_HEIGHT};

	gs_vertbuffer_t *arc_vert{};
	gs_vertbuffer_t *rect_vert{};
	gs_effect_t *mask_mixer_effect{};

	string select_title{};
	string select_producer{};
	string select_music{};
	string select_group{};
	string select_id{};
	string remove_url{};
	string remove_id{};
	string disable_url{};
	string disable_id{};
	string playing_url{};
	string playing_id{};
	bool is_show;
	bool scene_enable;
	bool real_stop;
	bool restart{false};

	bool set_auto_pause{false};

	string valid_font_name{};
	std::set<string, std::less<>> played_urls;
	vector<bgm_url_info> urls;

	std::vector<latest_url> queue_urls;
	mutex mtx;
};

static vector<bgm_url_info> get_enable_url(const struct prism_bgm_source *source)
{
	vector<bgm_url_info> vecs;
	for (const auto &url : source->urls) {
		if (url.is_disable) {
			continue;
		}
		vecs.push_back(url);
	}

	return vecs;
}

static void set_disable_url(struct prism_bgm_source *source, const std::string &disable_url, const std::string &disable_id)
{
	for (auto &url : source->urls) {
		if (url.url == disable_url && url.duration_type == disable_id) {
			url.is_disable = true;
			break;
		}
	}
}

static void set_current_url(struct prism_bgm_source *source, const std::string &current_url, const string &id)
{
	for (auto &url : source->urls) {
		if (url.url == current_url && url.duration_type == id) {
			url.is_current = true;
			continue;
		}
		url.is_current = false;
	}
}

static void set_settings_from_urls_info(obs_data_t *private_settings, const struct bgm_url_info &info)
{
	if (!private_settings) {
		return;
	}
	obs_data_set_string(private_settings, MUSIC, info.url.c_str());
	obs_data_set_string(private_settings, TITLE, info.title.c_str());
	obs_data_set_string(private_settings, PRODUCER, info.producer.c_str());
	obs_data_set_string(private_settings, DURATION_TYPE, info.duration_type.c_str());
	obs_data_set_string(private_settings, DURATION, info.duration.c_str());
	obs_data_set_string(private_settings, COVER_PATH, info.cover_path.c_str());
	obs_data_set_string(private_settings, GROUP, info.group.c_str());
	obs_data_set_bool(private_settings, IS_LOCAL_FILE, info.is_local_file);
	obs_data_set_bool(private_settings, IS_DISABLE, info.is_disable);
	obs_data_set_bool(private_settings, IS_CURRENT, info.is_current);
	obs_data_set_bool(private_settings, HAS_COVER, info.has_cover);
}

static void set_playlist_data(const bgm_url_info &data, obs_data_array_t &dataArray, int index = -1)
{
	obs_data_t *playList = obs_data_create();
	set_settings_from_urls_info(playList, data);

	if (-1 == index) {
		obs_data_array_push_back(&dataArray, playList);
	} else {
		obs_data_array_insert(&dataArray, index, playList);
	}

	obs_data_release(playList);
}

static void update_source_settings_playlist_data(const struct prism_bgm_source *source)
{
	obs_data_t *settings = obs_source_get_private_settings(source->source);
	obs_data_array_t *playListArray = obs_data_get_array(settings, PLAY_LIST);
	if (playListArray) {
		size_t item_count = obs_data_array_count(playListArray);
		for (int i = 0; i < item_count; i++) {
			obs_data_array_erase(playListArray, 0);
		}
		obs_data_array_release(playListArray);
	}

	playListArray = obs_data_array_create();
	vector<bgm_url_info> urls = source->urls;
	for (const auto &iter : urls) {
		set_playlist_data(iter, *playListArray);
	}
	obs_data_set_array(settings, PLAY_LIST, playListArray);
	obs_data_array_release(playListArray);
	obs_data_release(settings);
}

static void clear_current_url_info(struct prism_bgm_source *source)
{
	set_current_url(source, "", "");
	source->select_music.clear();
	source->select_producer.clear();
	source->select_title.clear();
	source->select_id.clear();
	source->playing_url.clear();
	source->playing_id.clear();
	update_source_settings_playlist_data(source);
	obs_data_t *settings = obs_source_get_settings(source->source);
	obs_source_update(source->source, settings);
	obs_data_release(settings);
}

latest_url get_queue_first_url(struct prism_bgm_source *source)
{
	std::lock_guard<std::mutex> locker(source->mtx);
	if (!source->queue_urls.empty()) {
		return source->queue_urls[0];
	}
	return latest_url();
}

latest_url erase_queue_first_url(struct prism_bgm_source *source)
{
	latest_url url;
	std::lock_guard<std::mutex> locker(source->mtx);
	if (!source->queue_urls.empty()) {
		url = source->queue_urls[0];
		source->queue_urls.erase(source->queue_urls.begin());
	}
	return url;
}

bool need_update_queue_first_url(struct prism_bgm_source *source, latest_url &get_url)
{
	std::lock_guard<std::mutex> locker(source->mtx);
	if (source->queue_urls.empty()) {
		return false;
	}

	auto &url = source->queue_urls[0];
	if (!url.update) {
		url.update = true;
		get_url = url;
		return true;
	}

	return false;
}

void push_back_queue_url(struct prism_bgm_source *source, const latest_url &url)
{
	std::lock_guard<std::mutex> locker(source->mtx);
	source->queue_urls.push_back(url);
}

bgm_url_info get_current_url_info(const struct prism_bgm_source *source)
{
	for (const auto &url : source->urls) {
		if (url.url == source->playing_url && url.duration_type == source->playing_id) {
			return url;
		}
	}
	return bgm_url_info();
}

const char *get_state_string(obs_media_state state)
{
	switch (state) {
	case OBS_MEDIA_STATE_NONE:
		return "OBS_MEDIA_STATE_NONE";
	case OBS_MEDIA_STATE_PLAYING:
		return "OBS_MEDIA_STATE_PLAYING";
	case OBS_MEDIA_STATE_OPENING:
		return "OBS_MEDIA_STATE_OPENING";
	case OBS_MEDIA_STATE_BUFFERING:
		return "OBS_MEDIA_STATE_BUFFERING";
	case OBS_MEDIA_STATE_PAUSED:
		return "OBS_MEDIA_STATE_PAUSED";
	case OBS_MEDIA_STATE_STOPPED:
		return "OBS_MEDIA_STATE_STOPPED";
	case OBS_MEDIA_STATE_ENDED:
		return "OBS_MEDIA_STATE_ENDED";
	case OBS_MEDIA_STATE_ERROR:
		return "OBS_MEDIA_STATE_ERROR";
	default:
		return "default";
	}
}

static void prism_bgm_switch_to_next_song(void *data);

static void media_state_changed(obs_media_state state, void *data, calldata_t *calldata)
{
	auto source = static_cast<prism_bgm_source *>(data);
	auto media_source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (media_source != source->media_source)
		return;

	PLS_PLUGIN_INFO("bgm: receive media state changed from source: %p, state:[%d] %s .", source, state, get_state_string(state));

	if (state == OBS_MEDIA_STATE_STOPPED || state == OBS_MEDIA_STATE_ENDED) {
		if (!source->real_stop) {
			bgm_url_info current = get_current_url_info(source);
			bool network_off = !pls_get_network_state();
			if (network_off && !current.is_local_file) {
				pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_STATE_CHANGED, state);
				return;
			} else if (current.is_local_file) {
				pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_STATE_CHANGED, state);
				return;
			}
			prism_bgm_switch_to_next_song(source);
			return;
		} else {
			clear_current_url_info(source);
		}
	} else if (state == OBS_MEDIA_STATE_PLAYING && source->restart) {
		latest_url url = get_queue_first_url(source);
		source->playing_url = url.url;
		source->playing_id = url.id;

		if (source->urls.empty()) {
			source->playing_url.clear();
			source->playing_id.clear();
			obs_source_media_stop(source->source);
		}

		obs_data_t *settings = obs_source_get_settings(source->source);
		obs_source_update(source->source, settings);
		obs_data_release(settings);
	}
	pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_STATE_CHANGED, state);
}

static void media_load(void *data, calldata_t *calldata)
{
	auto source = static_cast<prism_bgm_source *>(data);

	auto media_source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (media_source == source->media_source) {
		bool load = calldata_bool(calldata, "load");
		//obs_source_media_load(source->source, load);
	}
}

static void bgm_erase_queue_first_url(struct prism_bgm_source *source)
{
	erase_queue_first_url(source);
}

static void media_skip(void *data, calldata_t *calldata)
{
	auto source = static_cast<prism_bgm_source *>(data);

	auto media_source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (media_source == source->media_source) {
		bgm_erase_queue_first_url(source);
	}
}

static void media_pause(void *data, calldata_t *calldata)
{
	media_state_changed(OBS_MEDIA_STATE_PAUSED, data, calldata);
}
static void media_restart(void *data, calldata_t *calldata)
{
	media_state_changed(OBS_MEDIA_STATE_PLAYING, data, calldata);
}
static void media_stopped(void *data, calldata_t *calldata)
{
	media_state_changed(OBS_MEDIA_STATE_STOPPED, data, calldata);
}
static void media_started(void *data, calldata_t *calldata)
{
	media_state_changed(OBS_MEDIA_STATE_PLAYING, data, calldata);
}
static void media_ended(void *data, calldata_t *calldata)
{
	media_state_changed(OBS_MEDIA_STATE_ENDED, data, calldata);
}

static void bgm_source_start(struct prism_bgm_source *s);

static unsigned int generate_random_number(const unsigned int &max)
{
	std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<> dis{0, (int)(max - 1)};
	return dis(rng);
}

static void playlist_row_changed(struct prism_bgm_source *source, const int &srcIndex, const int &destIndex)
{
	if (!source) {
		return;
	}

	long long count = source->urls.size();
	if (srcIndex >= count || srcIndex < 0 || destIndex < 0 || destIndex >= count || srcIndex == destIndex) {
		return;
	}

	vector<bgm_url_info> urls = source->urls;
	auto iters = urls.begin();
	iters += srcIndex;
	bgm_url_info data = *iters;
	urls.erase(iters);

	auto iter_ = urls.begin();
	urls.insert(iter_ + destIndex, data);
	source->urls = urls;
}

static bool is_existed_in_played_url(const struct prism_bgm_source *source, const string &url, const string &id)
{
	return std::any_of(source->played_urls.begin(), source->played_urls.end(), [url, id](const string &url_info) { return (url_info == url + id); });
}

std::vector<bgm_url_info> get_unplayed_url(const struct prism_bgm_source *source)
{
	std::vector<bgm_url_info> unplayed_url;
	std::vector<bgm_url_info> urls = source->urls;

	for (const auto &url : urls) {
		if (url.is_disable) {
			continue;
		}

		if (is_existed_in_played_url(source, url.url, url.duration_type)) {
			continue;
		}
		unplayed_url.push_back(url);
	}
	return unplayed_url;
}

static bgm_url_info get_next_available_song_in_playorder_mode_with_loop(const struct prism_bgm_source *source)
{
	vector<bgm_url_info> urls = source->urls;
	for (const auto &url : urls) {
		if (url.is_disable) {
			continue;
		}

		return url;
	}
	return bgm_url_info();
}

static bgm_url_info switch_to_next_song_in_playorder_mode(const struct prism_bgm_source *source, bool loop)
{
	if (!source) {
		return bgm_url_info();
	}

	vector<bgm_url_info> urls = source->urls;
	bool get_current = false;
	auto bgmIter = urls.begin();
	for (; bgmIter != urls.end(); ++bgmIter) {
		bgm_url_info info = *bgmIter;
		if (!info.is_current && !get_current) {
			continue;
		}

		get_current = true;
		if (info.is_disable || info.is_current) {
			continue;
		}

		return info;
	}

	if (bgmIter == urls.end()) {
		if (loop) {
			return get_next_available_song_in_playorder_mode_with_loop(source);
		}
		return bgm_url_info();
	}

	return bgm_url_info();
}

static vector<bgm_url_info> get_available_song_in_random_mode_with_loop(const struct prism_bgm_source *source)
{
	vector<bgm_url_info> available_urls;
	for (const auto &url : source->urls) {
		if (url.is_disable) {
			continue;
		}

		if (is_existed_in_played_url(source, url.url, url.duration_type)) {
			continue;
		}

		available_urls.push_back(url);
	}
	return available_urls;
}

static bgm_url_info switch_to_next_song_in_random_mode(struct prism_bgm_source *source, bool loop)
{
	if (!source) {
		return bgm_url_info();
	}

	std::vector<bgm_url_info> unplayed_urls = get_unplayed_url(source);
	if (unplayed_urls.empty()) {
		if (!loop) {
			return bgm_url_info();
		}

		source->played_urls.clear();
		vector<bgm_url_info> enable_url = get_enable_url(source);
		if (!source->select_music.empty() && enable_url.size() > 1) {
			source->played_urls.insert(source->select_music + source->select_id);
		}

		unplayed_urls = get_available_song_in_random_mode_with_loop(source);
		if (unplayed_urls.empty()) {
			return bgm_url_info();
		}
	}

	int index = generate_random_number((unsigned int)unplayed_urls.size());
	auto iter = unplayed_urls.begin();
	while (index) {
		iter++;
		index--;
	}

	return *iter;
}

static void without_loop(prism_bgm_source *source)
{
	obs_media_state state = obs_source_media_get_state(source->source);

	// there was only ong song and removing it.
	if ((source->select_music == source->remove_url && source->select_id == source->remove_id) && !source->remove_url.empty()) {
		source->remove_url.clear();
		source->remove_id.clear();
		source->played_urls.clear();
		if (state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_PAUSED || state == OBS_MEDIA_STATE_OPENING) {
			push_back_queue_url(source, latest_url());
			clear_current_url_info(source);
		}
	} else {
		if (state == OBS_MEDIA_STATE_STOPPED || state == OBS_MEDIA_STATE_ENDED) {
			clear_current_url_info(source);
			source->played_urls.clear();
			source->real_stop = true;
			push_back_queue_url(source, latest_url());
		} else if (state == OBS_MEDIA_STATE_ERROR) {
			clear_current_url_info(source);
			pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_STATE_CHANGED, state);
			source->played_urls.clear();
		} else if (source->disable_url == source->select_music && source->disable_id == source->select_id) {
			obs_source_media_stop(source->source);
			source->disable_url.clear();
			source->disable_id.clear();
			source->played_urls.clear();
		}
	}
}

static void bgm_bgm_restart(void *data);

static void prism_bgm_switch_to_next_song(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);
	std::vector<bgm_url_info> vecs = source->urls;
	if (vecs.empty()) {
		return;
	}

	bgm_url_info next_song_info{};
	obs_data_t *private_settings = obs_source_get_private_settings(source->source);
	bool loop = obs_data_get_bool(private_settings, IS_LOOP);
	bool play_in_order = obs_data_get_bool(private_settings, PLAY_IN_ORDER);
	if (play_in_order) {
		next_song_info = switch_to_next_song_in_playorder_mode(source, loop);
	} else {
		next_song_info = switch_to_next_song_in_random_mode(source, loop);
	}

	// next song was same with playing songs
	// 1: only one song with loop and removing it : stop playing music.
	// 2: only one song without out loop: ignore.
	if (next_song_info.url == source->select_music && next_song_info.duration_type == source->select_id) {
		if (source->select_music == source->remove_url && source->select_id == source->remove_id) { // remove current
			source->remove_url.clear();
			source->remove_id.clear();
			source->played_urls.clear();
			clear_current_url_info(source);

			push_back_queue_url(source, latest_url());
			obs_data_release(private_settings);
			return;
		}

		bool is_random_mode = obs_data_get_bool(private_settings, RANDOM_PLAY);
		if (is_random_mode) {
			source->played_urls.insert(source->select_music + source->select_id);
		}

		if (!loop) {
			obs_data_release(private_settings);
			return;
		} else {
			obs_media_state state = obs_source_media_get_state(source->source);
			if (state == OBS_MEDIA_STATE_PAUSED || state == OBS_MEDIA_STATE_PLAYING) {
				obs_data_release(private_settings);
				return;
			}
		}
	}

	// with out loop
	if (next_song_info.url.empty()) {
		without_loop(source);
		obs_data_release(private_settings);
		return;
	}

	// there are more than one songs.
	set_settings_from_urls_info(private_settings, next_song_info);
	obs_data_release(private_settings);
	bgm_bgm_restart(source);
}

static bgm_url_info get_previous_available_song_in_playorder_mode_with_loop(struct prism_bgm_source *source)
{
	for (long long i = source->urls.size() - 1; i >= 0; i--) {
		if (source->urls[i].is_disable) {
			continue;
		}
		return source->urls[i];
	}

	return bgm_url_info();
}

static bgm_url_info switch_to_previous_song_in_playorder_mode(struct prism_bgm_source *source, bool loop)
{
	if (!source) {
		return bgm_url_info();
	}

	vector<bgm_url_info> previous_urls;
	for (const auto &data : source->urls) {
		if (!data.is_current) {
			if (!data.is_disable) {
				previous_urls.insert(previous_urls.begin(), data);
			}
			continue;
		}
		break;
	}

	if (previous_urls.empty()) {
		if (!loop) {
			return bgm_url_info();
		}
		return get_previous_available_song_in_playorder_mode_with_loop(source);
	}

	return previous_urls[0];
}
static void prism_bgm_switch_to_previous_song(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);
	bgm_url_info previous_song_info{};
	obs_data_t *private_settings = obs_source_get_private_settings(source->source);
	bool loop = obs_data_get_bool(private_settings, IS_LOOP);
	bool play_in_order = obs_data_get_bool(private_settings, PLAY_IN_ORDER);
	if (play_in_order) {
		previous_song_info = switch_to_previous_song_in_playorder_mode(source, loop);
	} else {
		previous_song_info = switch_to_next_song_in_random_mode(source, loop);
	}

	if (previous_song_info.url == source->select_music && previous_song_info.duration_type == source->select_id) {
		bool is_random_mode = obs_data_get_bool(private_settings, RANDOM_PLAY);
		if (is_random_mode) {
			source->played_urls.insert(source->select_music + source->select_id);
		}
		obs_data_release(private_settings);
		return;
	}

	if (previous_song_info.url.empty()) {
		obs_data_release(private_settings);
		return;
	}

	set_settings_from_urls_info(private_settings, previous_song_info);

	obs_data_release(private_settings);
	bgm_bgm_restart(source);
}

static const char *prism_bgm_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("PrismBGMPlugins");
}

static void bgm_source_start(struct prism_bgm_source *s)
{
	obs_data_t *settings = obs_source_get_private_settings(s->source);

	string music_url = obs_data_get_string(settings, MUSIC);
	string url_id = obs_data_get_string(settings, DURATION_TYPE);

	bool id_changed = !(url_id == s->select_id);
	bool music_changed = !(music_url == s->select_music);
	bool changed = id_changed || music_changed;

	s->select_id = obs_data_get_string(settings, DURATION_TYPE);
	s->select_music = obs_data_get_string(settings, MUSIC);
	s->select_title = obs_data_get_string(settings, TITLE);
	s->select_producer = obs_data_get_string(settings, PRODUCER);

	if (s->select_music.empty() || s->select_title.empty() || s->select_producer.empty()) {
		obs_data_release(settings);
		return;
	}

	obs_media_state state = obs_source_media_get_state(s->media_source);
	if (!changed && state == OBS_MEDIA_STATE_PLAYING) {
		obs_source_media_play_pause(s->source, true);
		obs_data_release(settings);
		return;
	}

	if (!changed && state == OBS_MEDIA_STATE_PAUSED) {
		obs_source_media_play_pause(s->source, false);
		obs_data_release(settings);
		return;
	}

	set_current_url(s, s->select_music, s->select_id);
	s->real_stop = false;
	s->restart = true;
	bool is_random_mode = obs_data_get_bool(settings, RANDOM_PLAY);
	if (is_random_mode) {
		s->played_urls.insert(s->select_music + s->select_id);
	}

	latest_url urls;
	urls.is_loaclfile = obs_data_get_bool(settings, IS_LOCAL_FILE);
	urls.url = s->select_music;
	urls.id = s->select_id;
	push_back_queue_url(s, urls);

	obs_data_release(settings);
}

static void update_scroll_source(obs_source_t **source, obs_source_t **scroll_source, const uint32_t &width)
{
	uint32_t source_width = obs_source_get_width(*source);
	if (source_width > width) {
		if (!*scroll_source) {
			*scroll_source = obs_source_create_private("scroll_filter", "prism_bgm_scroll_source", nullptr);
			obs_source_filter_add(*source, *scroll_source);

			obs_data_t *scroll_settings = obs_source_get_settings(*scroll_source);
			if (0 == obs_data_get_double(scroll_settings, "speed_x")) {
				obs_data_set_double(scroll_settings, "speed_x", 50);
				obs_data_set_double(scroll_settings, "speed_y", 0);
				obs_data_set_bool(scroll_settings, "limit_cx", false);
				obs_source_update(*scroll_source, scroll_settings);
			}
			obs_data_release(scroll_settings);
		}

		obs_source_t *filter_source = obs_source_get_filter_by_name(*source, "prism_bgm_scroll_source");
		if (!filter_source) {
			obs_data_t *scroll_settings = obs_source_get_settings(*scroll_source);
			obs_data_set_bool(scroll_settings, "offset_reset", true);
			obs_source_update(*scroll_source, scroll_settings);
			obs_data_release(scroll_settings);

			obs_source_filter_add(*source, *scroll_source);
		} else {
			obs_source_release(filter_source);
		}

		return;
	}

	if (*scroll_source) {
		obs_source_filter_remove(*source, *scroll_source);
	}
}

static void update_name(prism_bgm_source *source, const obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
	if (!source->name_source.source) {
#ifdef Q_OS_WIN
		source->name_source.source = obs_source_create_private("text_gdiplus", "prism_bgm_name_source", nullptr);
#else
		source->name_source.source = obs_source_create_private("text_ft2_source", "prism_bgm_name_source", nullptr);
#endif
	}

	obs_data_t *name_settings = obs_source_get_settings(source->name_source.source);
	string title = source->select_title;
	if (!title.empty())
		title.append("   ");
	obs_data_set_string(name_settings, "text", title.c_str());
	obs_data_set_bool(name_settings, "outline", true);
	obs_data_set_int(name_settings, "outline_color", 0xff000000);
	obs_data_set_int(name_settings, "outline_size", 1);
	obs_data_set_int(name_settings, "outline_opacity", 70);

	OBSDataAutoRelease font_obj = obs_data_create();
	obs_data_set_string(font_obj, "face", source->valid_font_name.c_str());
	obs_data_set_int(font_obj, "size", 36);
	int64_t font_flags = obs_data_get_int(font_obj, "flags");
	font_flags |= OBS_FONT_BOLD;
	obs_data_set_int(font_obj, "flags", font_flags);

	obs_data_set_obj(name_settings, "font", font_obj);

	obs_data_set_int(name_settings, "color", 0xFFFFFF);
	obs_data_set_int(name_settings, "opacity", 100);
	obs_data_set_string(name_settings, "align", "left");
	obs_data_set_string(name_settings, "valign", "center");

	obs_source_update(source->name_source.source, name_settings);
	obs_data_release(name_settings);
}

static void update_producer(prism_bgm_source *source, const obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
	if (!source->producer_source.source) {
#ifdef Q_OS_WIN
		source->producer_source.source = obs_source_create_private("text_gdiplus", "prism_bgm_producer_source", nullptr);
#else
		source->producer_source.source = obs_source_create_private("text_ft2_source", "prism_bgm_producer_source", nullptr);
#endif // Q_OS_WIN
	}

	obs_data_t *producer_settings = obs_source_get_settings(source->producer_source.source);
	string producer_name = source->select_producer;
	if (!producer_name.empty())
		producer_name.append("   ");

	obs_data_set_string(producer_settings, "text", producer_name.c_str());

	obs_data_set_bool(producer_settings, "outline", true);
	obs_data_set_int(producer_settings, "outline_color", 0xff000000);
	obs_data_set_int(producer_settings, "outline_size", 1);
	obs_data_set_int(producer_settings, "outline_opacity", 70);

	OBSDataAutoRelease font_obj = obs_data_create();
	obs_data_set_string(font_obj, "face", source->valid_font_name.c_str());
	obs_data_set_int(font_obj, "size", 28);
	obs_data_set_string(font_obj, "style", "Regular");
	auto flag = static_cast<int>(obs_data_get_int(font_obj, "flag"));
	flag &= OBS_FONT_BOLD;
	obs_data_set_int(font_obj, "flag", flag);
	obs_data_set_obj(producer_settings, "font", font_obj);

	obs_data_set_int(producer_settings, "color", 0xFFFFFF);
	obs_data_set_int(producer_settings, "opacity", 80);
	obs_data_set_string(producer_settings, "align", "left");
	obs_data_set_string(producer_settings, "valign", "center");

	obs_source_update(source->producer_source.source, producer_settings);
	obs_data_release(producer_settings);
}

static void update_cover(prism_bgm_source *source, const obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
	string cover_path;
	bgm_url_info info = get_current_url_info(source);
	if (info.is_local_file) {
		if (info.has_cover) {
			source->cover_source.source = source->media_source;
			return;
		}

		cover_path = DEFAULT_COVER_IMAGE;

	} else {
		if (!info.cover_path.empty()) {
			cover_path = info.cover_path.substr(info.cover_path.find_last_of('/') + 1);
		} else {
			cover_path = DEFAULT_COVER_IMAGE;
		}
	}

	if (!source->cover_source.default_source) {
		source->cover_source.default_source = obs_source_create_private("image_source", "prism_bgm_cover_source", nullptr);
	}
	source->cover_source.source = source->cover_source.default_source;

	cover_path = string("images/").append(cover_path);
	char *path = obs_module_file(cover_path.c_str());
	if (!path || std::string(path).empty()) {
		path = obs_module_file(string("images/").append(DEFAULT_COVER_IMAGE).c_str());
	}

	//obs_data_t *cover_settings = obs_source_get_settings(source->cover_source.source);
	//obs_data_set_bool(cover_settings, IS_LOCAL_FILE, true);
	//obs_data_set_string(cover_settings, "local_file", path);
	obs_data_t *image_settings = obs_data_create();
	obs_data_set_string(image_settings, "file", path);
	obs_data_set_string(image_settings, "thumbnail_file", path);
	obs_source_update(source->cover_source.source, image_settings);
	obs_data_release(image_settings);

	bfree(path);

	//obs_source_update(source->cover_source.source, cover_settings);
	//obs_data_release(cover_settings);
}

static void prism_bgm_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "noLabelHeader", true);

	//default text
	obs_data_t *font_obj = obs_data_create();
	obs_data_set_default_string(font_obj, "face", "Arial");
	obs_data_set_default_int(font_obj, "size", 36);
	obs_data_set_default_string(font_obj, "style", "Regular");
	obs_data_set_default_obj(settings, NAME_FONT, font_obj);
	obs_data_set_default_int(settings, NAME_COLOR, 0xFFFFFF);
	obs_data_set_default_int(settings, NAME_OPACITY, 100);
	obs_data_release(font_obj);

	obs_data_set_default_bool(settings, IS_LOOP, true);
	obs_data_set_default_bool(settings, IS_SHOW, false);
	obs_data_set_default_bool(settings, SCENE_ENABLE, false);
	obs_data_set_default_bool(settings, PLAY_IN_ORDER, true);
}

static bool prism_add_bgm_music_list(const void *, const obs_properties_t *, obs_property_t *list, obs_data_t *settings)
{
	pls_property_bgm_list_clear(list);

	obs_data_array_t *play_list_array = obs_data_get_array(settings, PLAY_LIST);
	size_t music_list_count = obs_data_array_count(play_list_array);
	for (int i = 0; i < music_list_count; i++) {
		obs_data_t *play_list = obs_data_array_item(play_list_array, i);
		string producer = obs_data_get_string(play_list, PRODUCER);
		string duration = obs_data_get_string(play_list, DURATION);
		string duration_type = obs_data_get_string(play_list, DURATION_TYPE);
		string title = obs_data_get_string(play_list, TITLE);
		string url = obs_data_get_string(play_list, MUSIC);

		pls_property_bgm_list_add_item(list, title.c_str(), producer.c_str(), url.c_str(), atoi(duration.c_str()), atoi(duration_type.c_str()), nullptr);
		obs_data_release(play_list);
	}

	return true;
}

static inline void set_render_size(uint32_t width, uint32_t height)
{
	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, width, height);
}

static gs_texture_t *render_source_internal(obs_source_t *source, gs_texture_t *texture, uint32_t &width, uint32_t &height)
{
	if (!source)
		return texture;
	uint32_t source_width = obs_source_get_width(source);
	uint32_t source_height = obs_source_get_height(source);

	if (0 == source_width || 0 == source_height)
		return texture;

	width = source_width;
	height = source_height;

	if (!texture) {
		texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	} else {
		uint32_t texture_width = gs_texture_get_width(texture);
		uint32_t texture_height = gs_texture_get_height(texture);
		if (texture_width != source_width || texture_height != source_height) {
			gs_texture_destroy(texture);
			texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
		}
	}

	if (!texture)
		return texture;

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_viewport_push();
	gs_projection_push();

	gs_texture_t *pre_target = gs_get_render_target();
	gs_set_render_target(texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	set_render_size(source_width, source_height);

	if (obs_source_removed(source)) {
		obs_source_release(source);
	} else {
		obs_source_video_render(source);
	}

	gs_set_render_target(pre_target, nullptr);
	gs_projection_pop();
	gs_viewport_pop();

	return texture;
}

static void render_source(prism_source_wrapper *source, bool crop, const uint32_t &max_width)
{
	if (!source->texture)
		return;

	uint32_t source_width = gs_texture_get_width(source->texture);
	uint32_t source_height = gs_texture_get_height(source->texture);

	const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_viewport_push();
	gs_projection_push();

	int offset_x = source->targetRect.x;
	int offset_y = source->targetRect.y;
	int rt_cx = source->targetRect.cx;
	int rt_cy = source->targetRect.cy;

	gs_set_viewport(offset_x, offset_y, rt_cx, rt_cy);

	gs_enable_depth_test(false);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(param, source->texture);
	gs_matrix_push();
	gs_matrix_identity();

	if (crop) {
		gs_ortho(0.0f, (float)max_width, 0.0f, (float)source_height, -100.0f, 100.0f);
		gs_draw_sprite_subregion(source->texture, 0, 0, 0, max_width, source_height);
	} else {
		gs_ortho(0.0f, (float)source_width, 0.0f, (float)source_height, -100.0f, 100.0f);
		gs_draw_sprite(source->texture, 0, source_width, source_height);
	}

	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_projection_pop();
	gs_viewport_pop();
}

static void prism_bgm_render_bg_rect(int offset_x, int offset_y, int cx, int cy, gs_vertbuffer_t *vert_buf, const struct vec4 *bg_color)
{
	if (!vert_buf)
		return;

	gs_viewport_push();
	gs_projection_push();

	gs_set_viewport(offset_x, offset_y, cx, cy);
	gs_enable_depth_test(false);

	const gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	gs_effect_set_vec4(color, bg_color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(cx), float(cy), 1.0f);
	gs_ortho((float)0.0, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

	gs_load_vertexbuffer(vert_buf);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_load_vertexbuffer(nullptr);
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

static void prism_bgm_render_rects(void *data)
{
	auto source = (prism_bgm_source *)data;
	if (!source)
		return;

	if (!source->rect_vert) {
		obs_enter_graphics();

		gs_render_start(true);
		gs_vertex2f(0.0f, 0.0f);
		gs_vertex2f(0.0f, 1.0f);
		gs_vertex2f(1.0f, 1.0f);
		gs_vertex2f(1.0f, 0.0f);
		gs_vertex2f(0.0f, 0.0f);
		source->rect_vert = gs_render_save();

		obs_leave_graphics();
	}

	struct vec4 bg_color;
	vec4_set(&bg_color, BG_COLOR_R, BG_COLOR_G, BG_COLOR_B, BG_COLOR_A);

	int width = BASE_WIDTH - SOURCE_LEFT_RIGHT_MARGIN * 2;
	int height = BASE_HEIGHT - SOURCE_TOP_MARGIN - SOURCE_BOTTOM_MARGIN - INNER_RADIUS * 2;
	int offset_x = SOURCE_LEFT_RIGHT_MARGIN;
	int offset_y = SOURCE_TOP_MARGIN + INNER_RADIUS;
	prism_bgm_render_bg_rect(offset_x, offset_y, width, height, source->rect_vert, &bg_color);

	width = BASE_WIDTH - INNER_RADIUS * 2 - SOURCE_LEFT_RIGHT_MARGIN * 2;
	height = BASE_HEIGHT - SOURCE_TOP_MARGIN - SOURCE_BOTTOM_MARGIN;
	offset_x = INNER_RADIUS + SOURCE_LEFT_RIGHT_MARGIN;
	offset_y = SOURCE_TOP_MARGIN;
	prism_bgm_render_bg_rect(offset_x, offset_y, width, height, source->rect_vert, &bg_color);
}

static void prism_bgm_render_single_arc(int pt_x, int pt_y, gs_vertbuffer_t *vert_buf, const struct vec4 *bg_color)
{
	if (!vert_buf)
		return;

	gs_viewport_push();
	gs_projection_push();

	gs_matrix_push();
	gs_matrix_identity();

	gs_set_viewport(pt_x - INNER_RADIUS, pt_y - INNER_RADIUS, INNER_RADIUS * 2, INNER_RADIUS * 2);
	gs_ortho(0.0f, (float)INNER_RADIUS * 2, 0.0f, (float)INNER_RADIUS * 2, -100.0f, 100.0f);

	const gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_effect_set_vec4(color, bg_color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_load_vertexbuffer(vert_buf);

	gs_draw(GS_TRIS, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_load_vertexbuffer(nullptr);
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

static void prism_bgm_render_arcs(void *data)
{
	auto source = (prism_bgm_source *)data;
	if (!source)
		return;

	if (!source->arc_vert) {
		vector<vec2> vec_arc_pos;
		for (int i = 0; i <= 360; i++) {
			auto angle = static_cast<float>(PI / 180.f * i);
			vec2 pt{};
			pt.x = static_cast<float>((cos(angle) * 0.5 + 0.5) * INNER_RADIUS * 2);
			pt.y = static_cast<float>((0.5 - sin(angle) * 0.5) * INNER_RADIUS * 2);
			vec_arc_pos.push_back(pt);
		}

		gs_render_start(true);
		auto size = (int)vec_arc_pos.size();
		for (int i = 0; i < size - 1; i++) {
			vec2 pt0 = vec_arc_pos.at(i);
			vec2 pt1 = vec_arc_pos.at(i + 1);
			gs_vertex2f(INNER_RADIUS, INNER_RADIUS);
			gs_vertex2f(pt0.x, pt0.y);
			gs_vertex2f(pt1.x, pt1.y);
		}
		source->arc_vert = gs_render_save();
		vec_arc_pos.clear();
	}

	struct vec4 bg_color;
	vec4_set(&bg_color, BG_COLOR_R, BG_COLOR_G, BG_COLOR_B, BG_COLOR_A);
	//left top
	int pos_x = INNER_RADIUS + SOURCE_LEFT_RIGHT_MARGIN;
	int pos_y = INNER_RADIUS + SOURCE_TOP_MARGIN;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);

	//right top
	pos_x = BASE_WIDTH - INNER_RADIUS - SOURCE_LEFT_RIGHT_MARGIN;
	pos_y = INNER_RADIUS + SOURCE_TOP_MARGIN;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);

	//left bottom
	pos_x = INNER_RADIUS + SOURCE_LEFT_RIGHT_MARGIN;
	pos_y = BASE_HEIGHT - INNER_RADIUS - SOURCE_BOTTOM_MARGIN;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);

	//right bottom
	pos_x = BASE_WIDTH - INNER_RADIUS - SOURCE_LEFT_RIGHT_MARGIN;
	pos_y = BASE_HEIGHT - INNER_RADIUS - SOURCE_BOTTOM_MARGIN;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);
}

static void prism_bgm_render_background(void *data)
{
	auto source = (prism_bgm_source *)data;
	if (!source)
		return;

	gs_blend_state_push();
	gs_enable_blending(false);
	gs_set_cull_mode(GS_NEITHER);

	//render two rects
	prism_bgm_render_rects(data);

	//render four arcs
	prism_bgm_render_arcs(data);

	gs_blend_state_pop();
}

static void fill_cover_mask_arcs(prism_bgm_source *source)
{
	if (!source)
		return;

	if (!source->arc_vert) {
		vector<vec2> vec_arc_pos;
		for (int i = 0; i <= 360; i++) {
			auto angle = static_cast<float>(PI / 180.f * i);
			vec2 pt{};
			pt.x = static_cast<float>((cos(angle) * 0.5 + 0.5) * INNER_RADIUS * 2);
			pt.y = static_cast<float>((0.5 - sin(angle) * 0.5) * INNER_RADIUS * 2);
			vec_arc_pos.push_back(pt);
		}

		gs_render_start(true);
		auto size = (int)vec_arc_pos.size();
		for (int i = 0; i < size - 1; i++) {
			vec2 pt0 = vec_arc_pos.at(i);
			vec2 pt1 = vec_arc_pos.at(i + 1);
			gs_vertex2f(INNER_RADIUS, INNER_RADIUS);
			gs_vertex2f(pt0.x, pt0.y);
			gs_vertex2f(pt1.x, pt1.y);
		}
		source->arc_vert = gs_render_save();
		vec_arc_pos.clear();
	}

	struct vec4 bg_color;
	vec4_set(&bg_color, 0.0, 0.0, 0.0, 1.0);
	//left top
	int pos_x = INNER_RADIUS;
	int pos_y = INNER_RADIUS;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);

	//right top
	pos_x = COVER_IMAGE_WIDTH - INNER_RADIUS;
	pos_y = INNER_RADIUS;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);

	//left bottom
	pos_x = INNER_RADIUS;
	pos_y = COVER_IMAGE_HEIGHT - INNER_RADIUS;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);

	//right bottom
	pos_x = COVER_IMAGE_WIDTH - INNER_RADIUS;
	pos_y = COVER_IMAGE_HEIGHT - INNER_RADIUS;
	prism_bgm_render_single_arc(pos_x, pos_y, source->arc_vert, &bg_color);
}

static void fill_cover_mask_rects(prism_bgm_source *source)
{
	if (!source)
		return;

	if (!source->rect_vert) {
		obs_enter_graphics();

		gs_render_start(true);
		gs_vertex2f(0.0f, 0.0f);
		gs_vertex2f(0.0f, 1.0f);
		gs_vertex2f(1.0f, 1.0f);
		gs_vertex2f(1.0f, 0.0f);
		gs_vertex2f(0.0f, 0.0f);
		source->rect_vert = gs_render_save();

		obs_leave_graphics();
	}

	struct vec4 bg_color;
	vec4_set(&bg_color, 0.0, 0.0, 0.0, 1.0);

	int width = COVER_IMAGE_WIDTH;
	int height = COVER_IMAGE_HEIGHT - INNER_RADIUS * 2;
	int offset_x = 0;
	int offset_y = INNER_RADIUS;
	prism_bgm_render_bg_rect(offset_x, offset_y, width, height, source->rect_vert, &bg_color);

	width = COVER_IMAGE_WIDTH - INNER_RADIUS * 2;
	height = COVER_IMAGE_HEIGHT;
	offset_x = INNER_RADIUS;
	offset_y = 0;
	prism_bgm_render_bg_rect(offset_x, offset_y, width, height, source->rect_vert, &bg_color);
}

static void fill_cover_mask(prism_bgm_source *source)
{
	if (!source)
		return;

	gs_texture_t *pre_rt = gs_get_render_target();

	gs_blend_state_push();
	gs_reset_blend_state();
	gs_enable_depth_test(false);

	gs_set_render_target(source->cover_source.mask, nullptr);
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	fill_cover_mask_arcs(source);
	fill_cover_mask_rects(source);

	gs_blend_state_pop();
	gs_set_render_target(pre_rt, nullptr);
}

static void update_cover_mask(void *data, uint32_t width, uint32_t height)
{
	auto *source = (prism_bgm_source *)data;

	if (source->cover_source.mask)
		return;

	source->cover_source.mask = gs_texture_create(width, height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	if (!source->cover_source.mask) {
		PLS_PLUGIN_WARN("cover mask texture is not created.");
		return;
	}

	if (!source->mask_mixer_effect) {
		char *filename = obs_module_file("mask-mixer.effect");
		source->mask_mixer_effect = gs_effect_create_from_file(filename, nullptr);
		bfree(filename);

		if (!source->mask_mixer_effect) {
			PLS_PLUGIN_WARN("mask mixer effect is not created.");
			return;
		}
	}
	fill_cover_mask(source);
}

static void render_cover_source(void *data)
{
	auto source = (prism_bgm_source *)data;

	if (!source || !source->cover_source.texture)
		return;

	uint32_t source_width = gs_texture_get_width(source->cover_source.texture);
	uint32_t source_height = gs_texture_get_height(source->cover_source.texture);
	uint32_t target_width = source->cover_source.targetRect.cx;
	uint32_t target_height = source->cover_source.targetRect.cy;

	update_cover_mask(source, target_width, target_height);

	if (!source->mask_mixer_effect) {
		return;
	}

	gs_technique_t *tech = gs_effect_get_technique(source->mask_mixer_effect, "Draw");

	gs_viewport_push();
	gs_projection_push();

	int offset_x = source->cover_source.targetRect.x;
	int offset_y = source->cover_source.targetRect.y;
	int rt_cx = source->cover_source.targetRect.cx;
	int rt_cy = source->cover_source.targetRect.cy;

	gs_set_viewport(offset_x, offset_y, rt_cx, rt_cy);

	gs_enable_depth_test(false);
	gs_blend_state_push();
	gs_reset_blend_state();

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *param = gs_effect_get_param_by_name(source->mask_mixer_effect, "image");
	gs_effect_set_texture(param, source->cover_source.texture);
	gs_eparam_t *mask_param = gs_effect_get_param_by_name(source->mask_mixer_effect, "mask");
	gs_effect_set_texture(mask_param, source->cover_source.mask);
	gs_matrix_push();
	gs_matrix_identity();

	gs_ortho(0.0f, (float)source_width, 0.0f, (float)source_height, -100.0f, 100.0f);
	gs_draw_sprite(source->cover_source.texture, 0, source_width, source_height);

	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

static void prism_bgm_tick(void *data, float)
{
	auto source = static_cast<prism_bgm_source *>(data);

	latest_url url;
	bool need_update = need_update_queue_first_url(source, url);
	if (need_update) {
		source->playing_url = url.url;
		source->playing_id = url.id;

		// obs cannot seek network stream, so always play through local files
		obs_data_t *media_settings = obs_source_get_settings(source->media_source);
		obs_data_set_string(media_settings, "local_file", url.url.c_str());
		obs_data_set_bool(media_settings, IS_LOCAL_FILE, true);
		obs_source_update(source->media_source, media_settings);
		obs_data_release(media_settings);

		OBSDataAutoRelease settings = obs_source_get_settings(source->source);
		obs_source_update(source->source, settings);

		if (url.url.empty()) {
			pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_STATE_CHANGED, OBS_MEDIA_STATE_STOPPED);
			erase_queue_first_url(source);
		}
	}

	obs_enter_graphics();
	if (!source->output_texture) {
		source->output_texture = gs_texture_create(source->output_width, source->output_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}
	if (!source->output_texture) {
		PLS_PLUGIN_WARN("Fail to create texture for prism bgm source, w/h : %d/%d", source->output_width, source->output_height);
		obs_leave_graphics();
		return;
	}

	gs_matrix_push();

	source->name_source.texture = render_source_internal(source->name_source.source, source->name_source.texture, source->name_source.width, source->name_source.height);
	int name_width = 0;
	if (source->name_source.texture) {
		name_width = source->name_source.width;
		int name_height = source->name_source.height;
		int margin_top = SOURCE_TOP_MARGIN + COVER_TOP_BOTTOM_MARGIN + NAME_PRODUCER_HEIGHT - name_height;
		if (name_width <= NAME_WIDTH) {
			source->name_source.targetRect = {COVER_IMAGE_WIDTH + TEXT_LEFT_MARGIN + COVER_LEFT_RIGHT_MARGIN, margin_top, name_width, name_height};
		} else {
			source->name_source.targetRect = {COVER_IMAGE_WIDTH + TEXT_LEFT_MARGIN + COVER_LEFT_RIGHT_MARGIN, margin_top, NAME_WIDTH, name_height};
		}
		update_scroll_source(&source->name_source.source, &source->name_scroll_source, NAME_WIDTH);
	}

	source->producer_source.texture = render_source_internal(source->producer_source.source, source->producer_source.texture, source->producer_source.width, source->producer_source.height);
	int producer_width = 0;
	if (source->producer_source.texture) {
		producer_width = source->producer_source.width;
		int producer_height = source->producer_source.height;
		int margin_top = SOURCE_TOP_MARGIN + COVER_TOP_BOTTOM_MARGIN + NAME_PRODUCER_HEIGHT + PRODUCER_MARGIN_TOP;
		if (producer_width <= PRODUCER_WIDTH) {
			source->producer_source.targetRect = {COVER_IMAGE_WIDTH + TEXT_LEFT_MARGIN + COVER_LEFT_RIGHT_MARGIN, margin_top, producer_width, producer_height};
		} else {
			source->producer_source.targetRect = {COVER_IMAGE_WIDTH + TEXT_LEFT_MARGIN + COVER_LEFT_RIGHT_MARGIN, margin_top, PRODUCER_WIDTH, producer_height};
		}
		update_scroll_source(&source->producer_source.source, &source->producer_scroll_source, PRODUCER_WIDTH);
	}

	source->cover_source.texture = render_source_internal(source->cover_source.source, source->cover_source.texture, source->cover_source.width, source->cover_source.height);

	if (!source->name_source.texture || !source->producer_source.texture || !source->cover_source.texture) {
		gs_matrix_pop();
		obs_leave_graphics();
		return;
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_set_render_target(source->output_texture, nullptr);

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_blend_state_push();
	gs_reset_blend_state();

	prism_bgm_render_background(data);

	render_source(&source->name_source, name_width > NAME_WIDTH, NAME_WIDTH);
	render_source(&source->producer_source, producer_width > PRODUCER_WIDTH, PRODUCER_WIDTH);
	render_cover_source(data);

	gs_set_render_target(pre_rt, nullptr);
	gs_matrix_pop();
	gs_blend_state_pop();
	obs_leave_graphics();
}

static void prism_bgm_render(void *data, gs_effect_t *effect_)
{
	UNUSED_PARAMETER(effect_);

	auto source = static_cast<prism_bgm_source *>(data);

	if (!source->is_show || source->select_title.empty() || source->select_producer.empty()) {
		return;
	}

	gs_blend_state_push();
	gs_reset_blend_state();

	const gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), source->output_texture);
	gs_draw_sprite(source->output_texture, 0, source->output_width, source->output_height);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
}

static uint32_t prism_bgm_width(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	return source->output_width;
}

static uint32_t prism_bgm_height(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);
	return source->output_height;
}

static void prism_bgm_play_pause(void *data, bool pause)
{
	auto source = static_cast<prism_bgm_source *>(data);
	obs_source_media_play_pause(source->media_source, pause);
}

static void prism_bgm_stop(void *data)
{
	auto s = static_cast<prism_bgm_source *>(data);
	s->real_stop = true;
	obs_source_media_stop(s->media_source);
}

static void bgm_bgm_restart(void *data)
{
	auto s = (prism_bgm_source *)data;
	bgm_source_start(s);

	obs_data_t *settings = obs_source_get_settings(s->source);
	obs_source_update(s->source, settings);
	obs_data_release(settings);
}

static int64_t prism_bgm_get_duration(void *data)
{
	auto *source = static_cast<prism_bgm_source *>(data);

	return obs_source_media_get_duration(source->media_source);
}

static int64_t prism_bgm_get_time(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	return obs_source_media_get_time(source->media_source);
}

static void prism_bgm_set_time(void *data, int64_t ms)
{
	auto source = static_cast<prism_bgm_source *>(data);
	obs_source_media_set_time(source->media_source, ms);
}

static enum obs_media_state prism_bgm_get_state(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	obs_media_state state = obs_source_media_get_state(source->media_source);
	if (state == OBS_MEDIA_STATE_PLAYING && (source->playing_url.empty() || source->playing_id.empty())) {
		return OBS_MEDIA_STATE_NONE;
	}

	return obs_source_media_get_state(source->media_source);
}

static obs_properties_t *prism_bgm_properties(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);
	obs_properties_t *props = obs_properties_create();

	obs_data_t *settings = obs_source_get_private_settings(source->source);

	obs_data_array_t *playListArray = obs_data_get_array(settings, PLAY_LIST);
	size_t musicListCount = obs_data_array_count(playListArray);
	stringstream ss;
	ss << musicListCount;
	string count = ss.str() + TEXT_SONG_COUNT;
	obs_property_t *tips_prop = pls_properties_bgm_add_tips(props, count.c_str(), TEXT_TIPS);
	pls_property_set_flags(tips_prop, PROPERTY_FLAG_NO_LABEL_HEADER);

	obs_property_t *p = pls_properties_bgm_add_list(props, MUSIC, TEXT_MUSIC);
	obs_property_set_enabled(p, true);

	prism_add_bgm_music_list(source, props, p, settings);
	obs_data_release(settings);

	obs_properties_add_button(props, "open_button", TEXT_OPEN_BUTTON, nullptr);

	obs_properties_add_bool(props, IS_SHOW, TEXT_SHOW);
	obs_property_t *prop = obs_properties_add_bool(props, SCENE_ENABLE, TEXT_SCENE_ENABLE);
	obs_property_set_long_description(prop, obs_module_text("PlayWhenActivate.ToolTip"));
	return props;
}

static void prism_bgm_activate(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	if (obs_source_media_get_state(source->media_source) == OBS_MEDIA_STATE_PAUSED) {
		if (source->set_auto_pause) {
			obs_source_media_play_pause(source->media_source, false);
			source->set_auto_pause = false;
		}
	}
}

static void prism_bgm_deactivate(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	if (source->scene_enable && obs_source_media_get_state(source->media_source) == OBS_MEDIA_STATE_PLAYING) {
		obs_source_media_play_pause(source->media_source, true);
		source->set_auto_pause = true;
	}
}

static void delete_source_settings_data(const struct prism_bgm_source *source, const string &url, const string &id)
{
	if (url.empty()) {
		return;
	}

	obs_data_t *settings = obs_source_get_private_settings(source->source);
	if (!settings) {
		return;
	}

	obs_data_array_t *playListArray = obs_data_get_array(settings, PLAY_LIST);
	size_t item_count = obs_data_array_count(playListArray);
	for (int i = 0; i < item_count; i++) {
		obs_data_t *playList = obs_data_array_item(playListArray, i);
		string playlist_url = obs_data_get_string(playList, MUSIC);
		string url_id = obs_data_get_string(playList, DURATION_TYPE);
		if (0 == url.compare(playlist_url) && url_id == id) {
			obs_data_array_erase(playListArray, i);
			obs_data_release(playList);
			break;
		}
		obs_data_release(playList);
	}
	obs_data_array_release(playListArray);
	obs_data_release(settings);
}

static bool bgm_source_setting_changed(const struct prism_bgm_source *source, obs_data_t *settings)
{
	bool is_show = obs_data_get_bool(settings, IS_SHOW);
	if (is_show != source->is_show) {
		return true;
	}
	bool is_enable = obs_data_get_bool(settings, SCENE_ENABLE);
	if (is_enable != source->scene_enable) {
		return true;
	}

	return false;
}

static void audio_capture(void *param, obs_source_t *src, const struct audio_data *data, bool muted)
{
	if (!param || !src || !data || muted)
		return;

	auto source = static_cast<prism_bgm_source *>(param);
	struct obs_source_audio source_data = {};

	for (int i = 0; i < MAX_AV_PLANES; i++)
		source_data.data[i] = data->data[i];

	source_data.frames = data->frames;
	source_data.timestamp = data->timestamp;

	uint32_t samples_per_sec = 0;
	int speakers = 0;

	//obs_audio_output_get_info(&samples_per_sec, &speakers);
	const audio_output_info *mainAOI = audio_output_get_info(obs_get_audio());
	if (!mainAOI) {
		return;
	}
	source_data.samples_per_sec = mainAOI->samples_per_sec;
	source_data.speakers = mainAOI->speakers;
	source_data.format = AUDIO_FORMAT_FLOAT_PLANAR;

	obs_source_output_audio(source->source, &source_data);
	return;
}

static void create_media_source(struct prism_bgm_source *source)
{
	if (!source) {
		return;
	}

	if (source->media_source) {
		return;
	}

	obs_data_t *ffmpeg_settings = obs_data_create();
	obs_data_set_bool(ffmpeg_settings, "bgm_source", true);
	obs_data_set_bool(ffmpeg_settings, "looping", false);

	source->media_source = obs_source_create_private("ffmpeg_source", "prism_bgm_play_source", ffmpeg_settings);
	obs_source_add_audio_capture_callback(source->media_source, audio_capture, source);

	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_load", media_load, source);
	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_skipped", media_skip, source);

	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_pause", media_pause, source);
	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_restart", media_restart, source);
	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_stopped", media_stopped, source);
	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_started", media_started, source);
	signal_handler_connect_ref(obs_source_get_signal_handler(source->media_source), "media_ended", media_ended, source);

	obs_source_inc_active(source->media_source);
	obs_data_release(ffmpeg_settings);
}

static void prism_bgm_update(void *data, obs_data_t *settings)
{
	auto source = static_cast<prism_bgm_source *>(data);

	create_media_source(source);

	source->is_show = obs_data_get_bool(settings, IS_SHOW);
	source->scene_enable = obs_data_get_bool(settings, SCENE_ENABLE);

	update_name(source, settings);
	update_producer(source, settings);
	update_cover(source, settings);
}

static int prism_get_valid_font_index()
{
	QStringList fontList = QFontDatabase::families();
	std::vector<string> vecFontFamily;
	for (int i = 0; i < fontList.size(); i++) {
		vecFontFamily.push_back(fontList[i].toStdString());
	}

	for (int i = 0; i < VALID_FONT_SIZE; ++i) {
		auto iter1 = find(vecFontFamily.begin(), vecFontFamily.end(), font_array[i]);
		if (iter1 != vecFontFamily.end()) {
			return i;
		}
	}
	return -1;
}

static void bgm_insert_playlist(struct prism_bgm_source *source, obs_data_t *private_data)
{
	obs_data_array_t *playListArray = obs_data_get_array(private_data, PLAY_LIST);
	if (playListArray) {
		vector<bgm_url_info> urls = source->urls;
		size_t musicListCount = obs_data_array_count(playListArray);
		for (int i = 0; i < musicListCount; i++) {
			bgm_url_info info{};
			obs_data_t *play_list = obs_data_array_item(playListArray, i);
			info.producer = obs_data_get_string(play_list, PRODUCER);
			info.duration_type = obs_data_get_string(play_list, DURATION_TYPE);
			info.title = obs_data_get_string(play_list, TITLE);
			info.url = obs_data_get_string(play_list, MUSIC);
			info.duration = obs_data_get_string(play_list, DURATION);
			info.group = obs_data_get_string(play_list, GROUP);
			info.is_local_file = obs_data_get_bool(play_list, IS_LOCAL_FILE);
			info.has_cover = obs_data_get_bool(play_list, HAS_COVER);
			info.cover_path = obs_data_get_string(play_list, COVER_PATH);
			info.is_disable = obs_data_get_bool(play_list, IS_DISABLE);
			info.is_current = obs_data_get_bool(play_list, IS_CURRENT);

			urls.emplace(urls.begin(), info);
			obs_data_release(play_list);
		}
		source->urls = urls;
		update_source_settings_playlist_data(source);
		obs_data_array_release(playListArray);
	}
}

static void bgm_add_playlist(struct prism_bgm_source *source, obs_data_t *private_data)
{
	obs_data_array_t *playListArray = obs_data_get_array(private_data, PLAY_LIST);
	if (playListArray) {
		vector<bgm_url_info> urls = source->urls;
		size_t musicListCount = obs_data_array_count(playListArray);
		for (int i = 0; i < musicListCount; i++) {
			bgm_url_info info{};
			obs_data_t *play_list = obs_data_array_item(playListArray, i);
			info.producer = obs_data_get_string(play_list, PRODUCER);
			info.duration_type = obs_data_get_string(play_list, DURATION_TYPE);
			info.title = obs_data_get_string(play_list, TITLE);
			info.url = obs_data_get_string(play_list, MUSIC);
			info.duration = obs_data_get_string(play_list, DURATION);
			info.group = obs_data_get_string(play_list, GROUP);
			info.is_local_file = obs_data_get_bool(play_list, IS_LOCAL_FILE);
			info.has_cover = obs_data_get_bool(play_list, HAS_COVER);
			info.cover_path = obs_data_get_string(play_list, COVER_PATH);
			info.is_disable = obs_data_get_bool(play_list, IS_DISABLE);
			info.is_current = false;

			urls.push_back(info);
			obs_data_release(play_list);
		}
		source->urls = urls;
		update_source_settings_playlist_data(source);
		obs_data_array_release(playListArray);
	}
}

static void *prism_bgm_create(obs_data_t *settings, obs_source_t *source_)
{
	auto source = pls_new<prism_bgm_source>();
	source->source = source_;

	source->output_width = BASE_WIDTH;
	source->output_height = BASE_HEIGHT;

	source->name_source.targetRect = {COVER_IMAGE_WIDTH + TEXT_LEFT_MARGIN + COVER_LEFT_RIGHT_MARGIN, SOURCE_TOP_MARGIN + COVER_TOP_BOTTOM_MARGIN, NAME_WIDTH, NAME_HEIGHT};

	source->cover_source.targetRect = {COVER_LEFT_RIGHT_MARGIN, SOURCE_TOP_MARGIN + COVER_TOP_BOTTOM_MARGIN, COVER_IMAGE_WIDTH, COVER_IMAGE_WIDTH};
	source->cover_source.is_cover = true;

	source->producer_source.targetRect = {COVER_IMAGE_WIDTH + TEXT_LEFT_MARGIN + COVER_LEFT_RIGHT_MARGIN, COVER_IMAGE_WIDTH / 2, PRODUCER_WIDTH, PRODUCER_HEIGHT};

	int font_index = prism_get_valid_font_index();
	if (font_index >= 0) {
		auto _fontName = font_array[font_index];
		source->valid_font_name = _fontName == "" ? "Arial" : _fontName;
	} else {
		source->valid_font_name = "Arial";
	}

	source->arc_vert = nullptr;
	source->rect_vert = nullptr;

	obs_source_set_monitoring_type(source->source, OBS_MONITORING_TYPE_MONITOR_ONLY);

	obs_source_update(source_, settings);
	return source;
}

static void prism_bgm_destroy(void *data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	obs_enter_graphics();

	if (source->output_texture) {
		gs_texture_destroy(source->output_texture);
	}

	if (source->name_source.texture) {
		gs_texture_destroy(source->name_source.texture);
	}

	if (source->producer_source.texture) {
		gs_texture_destroy(source->producer_source.texture);
	}

	if (source->cover_source.texture) {
		gs_texture_destroy(source->cover_source.texture);
	}

	if (source->cover_source.mask) {
		gs_texture_destroy(source->cover_source.mask);
		source->cover_source.mask = nullptr;
	}
	if (source->mask_mixer_effect) {
		gs_effect_destroy(source->mask_mixer_effect);
		source->mask_mixer_effect = nullptr;
	}

	if (source->arc_vert) {
		gs_vertexbuffer_destroy(source->arc_vert);
		source->arc_vert = nullptr;
	}

	if (source->rect_vert) {
		gs_vertexbuffer_destroy(source->rect_vert);
		source->rect_vert = nullptr;
	}

	obs_leave_graphics();

	if (source->name_source.source) {
		obs_source_release(source->name_source.source);
	}

	if (source->producer_source.source) {
		obs_source_release(source->producer_source.source);
	}

	if (source->name_scroll_source) {
		obs_source_release(source->name_scroll_source);
	}
	if (source->producer_scroll_source) {
		obs_source_release(source->producer_scroll_source);
	}

	if (source->cover_source.default_source) {
		obs_source_release(source->cover_source.default_source);
	}

	if (source->media_source) {
		obs_source_media_stop(source->source);
		obs_source_dec_active(source->media_source);
		obs_source_remove_audio_capture_callback(source->media_source, audio_capture, source);

		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_load", media_load, source);
		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_skipped", media_skip, source);

		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_pause", media_pause, source);
		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_restart", media_restart, source);
		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_stopped", media_stopped, source);
		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_started", media_started, source);
		signal_handler_disconnect(obs_source_get_signal_handler(source->media_source), "media_ended", media_ended, source);

		obs_source_release(source->media_source);
	}

	if (source->cover_source.source) {
		source->cover_source.source = nullptr;
	}

	if (source) {
		pls_delete(source);
		source = nullptr;
	}
}

static void bgm_row_changed(struct prism_bgm_source *source, obs_data_t *private_data)
{
	long long srcIndex = obs_data_get_int(private_data, "src_index");
	long long destIndex = obs_data_get_int(private_data, "dest_index");
	if (srcIndex - destIndex != 0) {
		playlist_row_changed(source, (int)srcIndex, (int)destIndex);
		update_source_settings_playlist_data(source);
	}
}

static void bgm_remove_url(struct prism_bgm_source *source, const string &url, const string &id)
{
	for (auto iter = source->urls.begin(); iter != source->urls.end(); ++iter) {
		if ((*iter).url == url && (*iter).duration_type == id) {
			source->urls.erase(iter);
			break;
		}
	}
}

static void bgm_remove(struct prism_bgm_source *source, obs_data_t *private_data)
{
	string remove_url = obs_data_get_string(private_data, "remove_url");
	if (remove_url.empty()) {
		return;
	}

	string url_id = obs_data_get_string(private_data, DURATION_TYPE);
	if (remove_url == source->select_music && url_id == source->select_id) {
		source->remove_url = remove_url;
		source->remove_id = url_id;
		prism_bgm_switch_to_next_song(source);
	}

	bgm_remove_url(source, remove_url, url_id);
	delete_source_settings_data(source, remove_url, url_id);
}

static void bgm_loop(struct prism_bgm_source *source, obs_data_t *private_data)
{
	obs_data_t *settings = obs_source_get_private_settings(source->source);
	bool is_loop = obs_data_get_bool(private_data, IS_LOOP);
	obs_data_set_bool(settings, IS_LOOP, is_loop);

	source->played_urls.clear();

	bool is_random = obs_data_get_bool(settings, RANDOM_PLAY);
	obs_data_release(settings);

	if (is_random && !source->select_music.empty() && !source->select_id.empty()) {
		source->played_urls.insert(source->select_music + source->select_id);
	}

	pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_LOOP_STATE_CHANGED, static_cast<int>(is_loop));
}

static void bgm_play_mode(struct prism_bgm_source *source, obs_data_t *private_data)
{
	obs_data_t *settings = obs_source_get_private_settings(source->source);
	bool is_play_in_order = obs_data_get_bool(private_data, PLAY_IN_ORDER);
	obs_data_set_bool(settings, PLAY_IN_ORDER, is_play_in_order);

	bool is_random = obs_data_get_bool(private_data, RANDOM_PLAY);
	obs_data_set_bool(settings, RANDOM_PLAY, is_random);
	obs_data_release(settings);

	pls_source_send_notify(source->source, OBS_SOURCE_MUSIC_MODE_STATE_CHANGED, is_play_in_order ? 0 : 1);

	source->played_urls.clear();
	if (!is_random) {
		return;
	}

	if (!source->select_music.empty() && !source->select_id.empty()) {
		source->played_urls.insert(source->select_music + source->select_id);
	}
}

static void bgm_play(const struct prism_bgm_source *source, obs_data_t *private_data)
{
	obs_data_t *settings = obs_source_get_private_settings(source->source);

	obs_data_set_string(settings, TITLE, obs_data_get_string(private_data, TITLE));
	obs_data_set_string(settings, PRODUCER, obs_data_get_string(private_data, PRODUCER));
	obs_data_set_string(settings, MUSIC, obs_data_get_string(private_data, MUSIC));
	obs_data_set_string(settings, DURATION_TYPE, obs_data_get_string(private_data, DURATION_TYPE));
	obs_data_set_string(settings, DURATION, obs_data_get_string(private_data, DURATION));
	obs_data_set_string(settings, GROUP, obs_data_get_string(private_data, GROUP));
	obs_data_set_bool(settings, IS_LOCAL_FILE, obs_data_get_bool(private_data, IS_LOCAL_FILE));
	obs_data_set_bool(settings, IS_CURRENT, obs_data_get_bool(private_data, IS_CURRENT));
	obs_data_set_bool(settings, IS_DISABLE, obs_data_get_bool(private_data, IS_DISABLE));
	obs_data_set_bool(settings, HAS_COVER, obs_data_get_bool(private_data, HAS_COVER));
	obs_data_set_string(settings, COVER_PATH, obs_data_get_string(private_data, COVER_PATH));

	obs_data_release(settings);
}

static void bgm_disable(struct prism_bgm_source *source, obs_data_t *private_data)
{
	source->disable_url = obs_data_get_string(private_data, MUSIC);
	source->disable_id = obs_data_get_string(private_data, DURATION_TYPE);
	set_disable_url(source, source->disable_url, source->disable_id);
	update_source_settings_playlist_data(source);

	bool gotoNext = obs_data_get_bool(private_data, "goto_next_songs");
	if (gotoNext) {
		prism_bgm_switch_to_next_song(source);
	}
}

static void bgm_enable(struct prism_bgm_source *source, obs_data_t *private_data)
{
	obs_data_array_t *arrays = obs_data_get_array(private_data, URLS);
	for (int i = 0; i < obs_data_array_count(arrays); i++) {
		obs_data_t *data = obs_data_array_item(arrays, i);
		const char *url = obs_data_get_string(data, MUSIC);
		const char *id = obs_data_get_string(data, DURATION_TYPE);
		for (auto &info : source->urls) {
			if (0 == info.url.compare(url) && 0 == info.duration_type.compare(id)) {
				info.is_disable = false;
			}
		}
		obs_data_release(data);
	}
	obs_data_array_release(arrays);
	update_source_settings_playlist_data(source);
}

static void bgm_update_cover_path(struct prism_bgm_source *source, obs_data_t *private_data)
{
	string urls = obs_data_get_string(private_data, MUSIC);
	string id = obs_data_get_string(private_data, DURATION_TYPE);
	string cover_path = obs_data_get_string(private_data, COVER_PATH);
	for (auto &url : source->urls) {
		if (url.url == urls && url.duration_type == id) {
			url.cover_path = cover_path;
			break;
		}
	}
	update_source_settings_playlist_data(source);
}

static void prism_bgm_set_private_data(void *data, obs_data_t *private_data)
{
	auto source = static_cast<prism_bgm_source *>(data);

	string method = obs_data_get_string(private_data, "method");
	if (method == "bgm_remove") {
		bgm_remove(source, private_data);
	} else if (method == "bgm_row_changed") {
		bgm_row_changed(source, private_data);
	} else if (method == "bgm_insert_playlist") {
		bgm_insert_playlist(source, private_data);
	} else if (method == "bgm_loop") {
		bgm_loop(source, private_data);
	} else if (method == "bgm_play_mode") {
		bgm_play_mode(source, private_data);
	} else if (method == "bgm_play") {
		bgm_play(source, private_data);
	} else if (method == "bgm_disable") {
		bgm_disable(source, private_data);
	} else if (method == "bgm_enable") {
		bgm_enable(source, private_data);
	} else if (method == "bgm_update_cover_path") {
		bgm_update_cover_path(source, private_data);
	} else if (method == "bgm_get_opening") {
		bgm_erase_queue_first_url(source);
		source->restart = false;
	}
}

static void prism_bgm_get_private_data(void *data, obs_data_t *private_data)
{
	if (!private_data) {
		return;
	}

	auto source = static_cast<prism_bgm_source *>(data);
	obs_data_t *settings = obs_source_get_private_settings(source->source);
	vector<bgm_url_info> urls = source->urls;
	if (urls.empty()) {
		bgm_add_playlist(source, settings);
	}

	string method = obs_data_get_string(private_data, "method");
	if (method == "get_select_url") {

		bgm_url_info url_info = get_current_url_info(source);
		for (const auto &url : source->urls) {
			if (url.url == source->select_music && url.duration_type == source->select_id) {
				url_info = url;
			}
		}
		set_settings_from_urls_info(private_data, url_info);
	} else {

		obs_data_set_string(private_data, "method", "get_current_url");
		bgm_url_info url_info = get_current_url_info(source);
		set_settings_from_urls_info(private_data, url_info);

		obs_data_set_bool(private_data, IS_SHOW, obs_data_get_bool(settings, IS_SHOW));
		obs_data_set_bool(private_data, IS_LOOP, obs_data_get_bool(settings, IS_LOOP));
		obs_data_set_bool(private_data, PLAY_IN_ORDER, obs_data_get_bool(settings, PLAY_IN_ORDER));
		obs_data_set_bool(private_data, RANDOM_PLAY, obs_data_get_bool(settings, RANDOM_PLAY));
		obs_data_release(settings);

		obs_data_array_t *data_array = obs_data_array_create();
		for (const auto &url : urls) {

			obs_data_t *data_ = obs_data_create();
			set_settings_from_urls_info(data_, url);
			obs_data_array_push_back(data_array, data_);
			obs_data_release(data_);
		}
		obs_data_set_array(private_data, PLAY_LIST, data_array);
		obs_data_array_release(data_array);
	}
}

//PRISM/ZengQin/20210604/#none/Get properties parameters
static OBSDataAutoRelease prism_bgm_get_props_params(void *data)
{
	if (!data)
		return nullptr;

	auto pSource = static_cast<prism_bgm_source *>(data);
	obs_data_t *private_settings = obs_source_get_private_settings(pSource->source);
	bool loop = obs_data_get_bool(private_settings, IS_LOOP);
	bool play_in_order = obs_data_get_bool(private_settings, PLAY_IN_ORDER);
	bool is_random = obs_data_get_bool(private_settings, RANDOM_PLAY);
	bool is_enable = obs_data_get_bool(private_settings, SCENE_ENABLE);
	obs_data_release(private_settings);

	OBSDataAutoRelease params = obs_data_create();
	obs_data_set_bool(params, IS_LOOP, loop);
	obs_data_set_bool(params, PLAY_IN_ORDER, play_in_order);
	obs_data_set_bool(params, RANDOM_PLAY, is_random);
	obs_data_set_bool(params, SCENE_ENABLE, is_enable);
	return params;
}

void register_prism_bgm_source()
{
	obs_source_info info = {};
	info.id = "prism_bgm_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_CONTROLLABLE_MEDIA;
	info.get_name = prism_bgm_get_name;
	info.create = prism_bgm_create;
	info.destroy = prism_bgm_destroy;
	info.update = prism_bgm_update;
	info.get_defaults = prism_bgm_defaults;
	info.get_properties = prism_bgm_properties;
	info.activate = prism_bgm_activate;
	info.deactivate = prism_bgm_deactivate;
	info.video_tick = prism_bgm_tick;
	info.video_render = prism_bgm_render;
	info.get_width = prism_bgm_width;
	info.get_height = prism_bgm_height;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_BGM);
	info.media_restart = bgm_bgm_restart;
	info.media_play_pause = prism_bgm_play_pause;
	info.media_stop = prism_bgm_stop;
	info.media_next = prism_bgm_switch_to_next_song;
	info.media_previous = prism_bgm_switch_to_previous_song;
	info.media_get_duration = prism_bgm_get_duration;
	info.media_get_time = prism_bgm_get_time;
	info.media_set_time = prism_bgm_set_time;
	info.media_get_state = prism_bgm_get_state;

	pls_source_info prism_info = {};
	prism_info.set_private_data = prism_bgm_set_private_data;
	prism_info.get_private_data = prism_bgm_get_private_data;
	register_pls_source_info(&info, &prism_info);

	obs_register_source(&info);
}

void release_prism_monitor_data() {}
