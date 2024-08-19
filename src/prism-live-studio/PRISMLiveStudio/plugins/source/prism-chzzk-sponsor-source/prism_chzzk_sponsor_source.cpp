#include "prism_chzzk_sponsor_source.h"

#include <obs.h>
#include <stdio.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <sys/stat.h>
#include <filesystem>
#include <string>
#include <codecvt>
#include <util/threading.h>
#include <log.h>
#include <libutils-api.h>
#include <frontend-api.h>
#include <qcolor.h>
#include <qdir.h>
#include <optional>
#include <qurlquery.h>
#include <pls/pls-obs-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <libui.h>

#define debug(format, ...) PLS_PLUGIN_DEBUG(format, ##__VA_ARGS__)
#define info(format, ...) PLS_PLUGIN_INFO(format, ##__VA_ARGS__)
#define warn(format, ...) PLS_PLUGIN_WARN(format, ##__VA_ARGS__)
#define error(format, ...) PLS_PLUGIN_ERROR(format, ##__VA_ARGS__)

#define S_CHZZK_SOURCE_DISPLAY "CHZZK.Display"
#define S_CHZZK_SOURCE_URL_TYPE "CHZZK.Sponsor.Url.Type"

const auto VC_DEFAULT_WIDTH = 800;
const auto VC_DEFAULT_HEIGHT = 600;

static inline uint32_t GetAudioChannels(enum speaker_layout speakers)
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
		return 0;
	}
}

struct chzzk_sponsor_source {
	obs_source_t *m_source = nullptr;
	int m_width = 0;
	int m_height = 0;
	QString m_channelId;
	QString m_chatDonationUrl;
	QString m_videoDonationUrl;
	QString m_missionDonationUrl;

	obs_source_t *m_browser = nullptr;
	gs_texture_t *m_source_texture = nullptr;
	QObject *audioWork = nullptr;
	QThread *audioConvertThread = nullptr;

	chzzk_sponsor_source()
	{
		audioConvertThread = pls_new<QThread>();
		audioWork = pls_new<QObject>();
		audioWork->moveToThread(audioConvertThread);
		audioConvertThread->start();
		QObject::connect(audioConvertThread, &QThread::finished, [audioWork = audioWork]() { pls_delete(audioWork); });
	}
	~chzzk_sponsor_source() { pls_delete_thread(audioConvertThread); }

	void updateSettings(obs_data_t *settings)
	{
		m_channelId = obs_data_get_string(settings, "channelId");
		auto browser_settings = obs_source_get_settings(m_browser);
		auto url = getUrl();
		obs_data_set_string(browser_settings, "url", url.toUtf8().constData());
		obs_source_update(m_browser, browser_settings);
		obs_data_release(browser_settings);
	}

	void setSetting(obs_data_t *settings)
	{
		m_channelId = obs_data_get_string(settings, "channelId");
		m_chatDonationUrl = obs_data_get_string(settings, "chatDonationUrl");
		m_videoDonationUrl = obs_data_get_string(settings, "videoDonationUrl");
		m_missionDonationUrl = obs_data_get_string(settings, "missionDonationUrl");
	}

	static QString encodePercent(const QString &val) { return QUrl::toPercentEncoding(val); }

	QString getUrl()
	{
		obs_data_t *settings = obs_source_get_settings(m_source);

		obs_data_release(settings);
		int type = obs_data_get_int(settings, S_CHZZK_SOURCE_URL_TYPE);
		QString urlstr;
		switch (type) {
		case 0:
			urlstr = obs_data_get_string(settings, "chatDonationUrl");
			break;
		case 1:
			urlstr = obs_data_get_string(settings, "videoDonationUrl");
			break;
		case 2:
			urlstr = obs_data_get_string(settings, "missionDonationUrl");
			break;
		default:
			break;
		}
		return urlstr;
	}
};

static const char *chzzk_sponsor_source_get_name(void *param)
{
	pls_unused(param);
	return obs_module_text("ChzzkSponsor.SourceName");
}

static obs_properties_t *chzzk_sponsor_source_properties(void *data)
{
	pls_unused(data);

	obs_properties_t *props = pls_properties_create();
	pls_properties_add_chzzk_sponsor(props, S_CHZZK_SOURCE_DISPLAY);
	return props;
}

static std::pair<obs_source_audio, std::shared_ptr<uint8_t>> ConvertAudio(obs_source_t *source, const obs_source_audio &audio)
{

	uint64_t linesize = static_cast<uint64_t>(audio.frames) * 4 /*AUDIO_FORMAT_FLOAT_PLANAR is 4*/;
	uint32_t channels = GetAudioChannels(audio.speakers);
	if (!linesize || !channels)
		return {};

	auto data = (uint8_t *)bmalloc(linesize * channels);
	if (!data)
		return {};

	std::shared_ptr<uint8_t> ptr(data, [](uint8_t *p) { bfree(p); });

	obs_source_audio tmp = audio;
	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		tmp.data[i] = nullptr;

	uint8_t *plane = data;
	for (size_t i = 0; i < channels; i++) {
		memcpy(plane, audio.data[i], linesize);
		tmp.data[i] = plane;
		plane += linesize;
	}
	return {tmp, ptr};
}
static void audio_capture(void *param, obs_source_t *src, const struct audio_data *data, bool muted)
{
	if (!param || !src || !data || muted)
		return;

	auto source = static_cast<chzzk_sponsor_source *>(param);
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

	auto tempdata = ConvertAudio(source->m_source, source_data);
	if (tempdata.second) {
		pls_async_call(source->audioWork, [source, tempdata]() { obs_source_output_audio(source->m_source, &tempdata.first); });
	}
	return;
}

static void init_browser_source(struct chzzk_sponsor_source *context)
{
	if (!context || context->m_browser) {
		return;
	}

	// init image source
	obs_data_t *browser_settings = obs_data_create();
	obs_data_set_string(browser_settings, "url", context->getUrl().toUtf8().constData());
	obs_data_set_bool(browser_settings, "is_local_file", false);
	obs_data_set_bool(browser_settings, "reroute_audio", true);
	obs_data_set_bool(browser_settings, "ignore_reload", true);
	obs_data_set_int(browser_settings, "width", context->m_width);
	obs_data_set_int(browser_settings, "height", context->m_height);
	context->m_browser = obs_source_create_private("browser_source", "prism_chzzk_sponsor_browser_source", browser_settings);
	obs_data_release(browser_settings);
	obs_source_add_audio_capture_callback(context->m_browser, audio_capture, context);
	obs_source_inc_active(context->m_browser);
	obs_source_inc_showing(context->m_browser);
}

static void chzzk_sponsor_source_update(void *data, obs_data_t *settings)
{
	auto context = (chzzk_sponsor_source *)(data);
	context->updateSettings(settings);
}

static void chzzk_sponsor_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_CHZZK_SOURCE_URL_TYPE, 0);
}

static void chzzk_sponsor_source_set_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}
	auto context = static_cast<struct chzzk_sponsor_source *>(data);
	QString method = obs_data_get_string(private_data, "method");
	if (method == "default_properties") {
		auto settings = obs_source_get_settings(context->m_source);
		obs_data_set_string(settings, "channelId", context->m_channelId.toUtf8().constData());
		obs_data_set_string(settings, "chatDonationUrl", context->m_chatDonationUrl.toUtf8().constData());
		obs_data_set_string(settings, "videoDonationUrl", context->m_videoDonationUrl.toUtf8().constData());
		obs_data_set_string(settings, "missionDonationUrl", context->m_missionDonationUrl.toUtf8().constData());
		obs_data_release(settings);
	}
	return;
}

static void source_created_notified(void *data, calldata_t *calldata)
{
	pls_used(calldata);
	auto context = static_cast<struct chzzk_sponsor_source *>(data);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || (source != (uint64_t)context->m_source) || !data)
		return;
	auto settings = obs_source_get_settings(context->m_source);
	context->setSetting(settings);
	obs_data_release(settings);
	init_browser_source(context);
}

static void *chzzk_sponsor_source_create(obs_data_t *settings, obs_source_t *source)
{

	auto context = pls_new_nothrow<chzzk_sponsor_source>();
	if (!context) {
		error("chzzk sponsor source create failed, because out of memory.");
		return nullptr;
	}

	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_created_notified, context);

	context->m_source = source;
	context->m_width = VC_DEFAULT_WIDTH;
	context->m_height = VC_DEFAULT_HEIGHT;
	context->setSetting(settings);
	return context;
}

static void chzzk_sponsor_source_destroy(void *data)
{
	auto context = (chzzk_sponsor_source *)(data);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_created_notified, context);

	if (context->m_browser) {
		obs_source_dec_active(context->m_browser);
		obs_source_release(context->m_browser);
	}

	if (context->m_source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->m_source_texture);
		context->m_source_texture = nullptr;
		obs_leave_graphics();
	}

	pls_delete(context);
}

static uint32_t chzzk_sponsor_source_get_width(void *data)
{
	auto context = (chzzk_sponsor_source *)(data);
	return context->m_width;
}

static uint32_t chzzk_sponsor_source_get_height(void *data)
{
	auto context = (chzzk_sponsor_source *)(data);
	return context->m_height;
}

static void chzzk_sponsor_source_clear_texture(gs_texture_t *tex)
{
	if (!tex) {
		return;
	}
	obs_enter_graphics();
	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_set_render_target(tex, nullptr);
	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	gs_set_render_target(pre_rt, nullptr);
	gs_projection_pop();
	obs_leave_graphics();
}

static void chzzk_sponsor_source_video_render(void *data, gs_effect_t *effect)
{
	auto context = (chzzk_sponsor_source *)(data);

	gs_blend_state_push();
	gs_reset_blend_state();

	const gs_effect_t *def_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(def_effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_effect_set_texture(gs_effect_get_param_by_name(def_effect, "image"), context->m_source_texture);
	gs_draw_sprite(context->m_source_texture, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
}

static void chzzk_sponsor_source_render(void *data, obs_source_t *source)
{
	auto vc_source = (chzzk_sponsor_source *)(data);
	uint32_t source_width = obs_source_get_width(source);
	uint32_t source_height = obs_source_get_height(source);
	if (source_width <= 0 || source_height <= 0) {
		chzzk_sponsor_source_clear_texture(vc_source->m_source_texture);
		return;
	}
	obs_enter_graphics();
	if (vc_source->m_source_texture) {
		uint32_t tex_width = gs_texture_get_width(vc_source->m_source_texture);
		uint32_t tex_height = gs_texture_get_height(vc_source->m_source_texture);
		if (tex_width != source_width || tex_height != source_height) {
			gs_texture_destroy(vc_source->m_source_texture);
			vc_source->m_source_texture = nullptr;
		}
	}

	if (!vc_source->m_source_texture) {
		vc_source->m_source_texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_set_render_target(vc_source->m_source_texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);
	gs_set_viewport(0, 0, source_width, source_height);

	gs_ortho(0.0f, float(source_width), 0.0f, float(source_height), -100.0f, 100.0f);
	obs_source_video_render(source);

	gs_set_render_target(pre_rt, nullptr);
	gs_viewport_pop();
	gs_projection_pop();

	obs_leave_graphics();
}

static void chzzk_sponsor_source_video_tick(void *data, float /*seconds*/)
{
	if (auto context = (chzzk_sponsor_source *)(data); context->m_browser) {
		chzzk_sponsor_source_render(data, context->m_browser);
	}
}

static void chzzk_sponsor_source_activate(void *data)
{
	if (auto context = (chzzk_sponsor_source *)(data); context->m_browser) {
		obs_source_inc_active(context->m_browser);
	}
}

static void chzzk_sponsor_source_deactivate(void *data)
{
	if (auto context = (chzzk_sponsor_source *)(data); context->m_browser) {
		obs_source_dec_active(context->m_browser);
	}
}

void register_prism_chzzk_sponsor_source()
{
	obs_source_info info;
	memset(&info, 0, sizeof(info));
	info.id = "prism_chzzk_sponsor";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO;
	info.get_name = chzzk_sponsor_source_get_name;
	info.create = chzzk_sponsor_source_create;
	info.destroy = chzzk_sponsor_source_destroy;
	info.get_defaults = chzzk_sponsor_source_defaults;
	info.get_properties = chzzk_sponsor_source_properties;
	info.activate = chzzk_sponsor_source_activate;
	info.deactivate = chzzk_sponsor_source_deactivate;
	info.video_render = chzzk_sponsor_source_video_render;
	info.video_tick = chzzk_sponsor_source_video_tick;
	info.get_width = chzzk_sponsor_source_get_width;
	info.get_height = chzzk_sponsor_source_get_height;
	info.update = chzzk_sponsor_source_update;

	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_CHZZK_SPONSOR);
	pls_source_info prism_info = {};
	prism_info.set_private_data = chzzk_sponsor_source_set_private_data;
	register_pls_source_info(&info, &prism_info);

	obs_register_source(&info);
}
