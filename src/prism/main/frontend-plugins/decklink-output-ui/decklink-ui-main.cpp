#include <Windows.h>
#include <obs-module.h>
#include <frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <util/util.hpp>
#include <util/platform.h>
#include <util/dstr.hpp>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include "DecklinkOutputUI.h"
#include "../../../plugins/decklink/const.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("decklink-output-ui", "en-US")

DecklinkOutputUI *doUI;

bool main_output_running = false;
bool preview_output_running = false;

obs_output_t *output;

struct preview_output {
	bool enabled;
	obs_source_t *current_source;
	obs_output_t *output;

	video_t *video_queue;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	uint8_t *video_data;
	uint32_t video_linesize;

	obs_video_info ovi;
};

static struct preview_output context = {0};

//PRISM/Zhangdewen/20210204/#6672/for decklink crash
struct win_event {
	HANDLE event;

	win_event() : event(CreateEventW(NULL, TRUE, FALSE, NULL)) {}
	~win_event()
	{
		if (event) {
			CloseHandle(event);
			event = NULL;
		}
	}

	bool wait(void (*output_stop)(obs_output_t *output), obs_output_t *output, int timeout = 5000 /* 5s */)
	{
		if (event) {
			ResetEvent(event);
			output_stop(output);
			return (WaitForSingleObject(event, timeout) == WAIT_OBJECT_0);
		} else {
			output_stop(output);
			return true;
		}
	}
	void notify()
	{
		if (event) {
			SetEvent(event);
		}
	}
};

OBSData load_settings()
{
	BPtr<char> path = obs_module_get_config_path(obs_current_module(), "decklinkOutputProps.json");
	BPtr<char> jsonData = os_quick_read_utf8_file(path);
	if (!!jsonData) {
		obs_data_t *data = obs_data_create_from_json(jsonData);
		OBSData dataRet(data);
		obs_data_release(data);

		return dataRet;
	}

	return nullptr;
}

//PRISM/LiuHaibin/20200416/#2413/for decklink crash
OBSData load_preview_settings();
bool is_setting_conflict()
{
	OBSData previewSettings = load_preview_settings();
	OBSData mainSettings = load_settings();
	if (mainSettings && previewSettings) {
		//It will be crashed in decklink driver if we select same device but with different keyer mode(one with disable, one with internal/external),
		//and start both main output and preview
		const char *previewDeviceHash = obs_data_get_string(previewSettings, DEVICE_HASH);
		const char *mainDeviceHash = obs_data_get_string(mainSettings, DEVICE_HASH);
		int previewKeyerMode = (int)obs_data_get_int(previewSettings, KEYER);
		int mainKeyerMode = (int)obs_data_get_int(mainSettings, KEYER);
		if (!astrcmpi(previewDeviceHash, mainDeviceHash) && mainKeyerMode != previewKeyerMode && !(mainKeyerMode & previewKeyerMode)) {
			PLS_WARN(FRONTEND_PLUGINS_DECKLINK_OUTPUT, "Using different keyer mode for same device is not safe.");
			return true;
		}
	}
	return false;
}

//PRISM/Zhangdewen/20210204/#6672/for decklink crash
static win_event output_stop_event_for_exit_app;

//PRISM/Zhangdewen/20210204/#6672/for decklink crash
void output_stop_callback(void * /*data*/, calldata_t * /*params*/)
{
	output_stop_event_for_exit_app.notify();
}

void output_stop(
	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	bool app_exit = false)
{
	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	if (!app_exit) {
		obs_output_stop(output);
	} else if (!output_stop_event_for_exit_app.wait(&obs_output_stop, output)) {
		output_stop_event_for_exit_app.wait(&obs_output_force_stop, output);
	}

	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	signal_handler_disconnect(obs_output_get_signal_handler(output), "stop", output_stop_callback, doUI);

	obs_output_release(output);
	main_output_running = false;
	// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
	doUI->OutputStateChanged(false);
}

void output_start()
{
	OBSData settings = load_settings();

	if (settings != nullptr) {
		//PRISM/LiuHaibin/20200416/#2413/for decklink crash
		if (preview_output_running && is_setting_conflict())
			return;

		output = obs_output_create("decklink_output", "decklink_output", settings, NULL);

		//PRISM/Zhangdewen/20210204/#6672/for decklink crash
		signal_handler_connect_ref(obs_output_get_signal_handler(output), "stop", output_stop_callback, nullptr);

		bool started = obs_output_start(output);

		main_output_running = started;

		// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
		doUI->OutputStateChanged(started);

		if (!started)
			output_stop();
	}
}

// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
void output_toggle()
{
	if (main_output_running)
		output_stop();
	else
		output_start();
}

OBSData load_preview_settings()
{
	BPtr<char> path = obs_module_get_config_path(obs_current_module(), "decklinkPreviewOutputProps.json");
	BPtr<char> jsonData = os_quick_read_utf8_file(path);
	if (!!jsonData) {
		obs_data_t *data = obs_data_create_from_json(jsonData);
		OBSData dataRet(data);
		obs_data_release(data);

		return dataRet;
	}

	return nullptr;
}

void on_preview_scene_changed(enum obs_frontend_event event, void *param);
void render_preview_source(void *param, uint32_t cx, uint32_t cy);

//PRISM/Zhangdewen/20210204/#6672/for decklink crash
static win_event preview_output_stop_event_for_exit_app;

//PRISM/Zhangdewen/20210204/#6672/for decklink crash
void preview_output_stop_callback(void * /*data*/, calldata_t * /*params*/)
{
	preview_output_stop_event_for_exit_app.notify();
}

void preview_output_stop(
	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	bool app_exit = false)
{
	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	if (!app_exit) {
		obs_output_stop(context.output);
	} else if (!preview_output_stop_event_for_exit_app.wait(&obs_output_stop, context.output)) {
		preview_output_stop_event_for_exit_app.wait(&obs_output_force_stop, context.output);
	}

	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	signal_handler_disconnect(obs_output_get_signal_handler(context.output), "stop", preview_output_stop_callback, doUI);

	// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
	obs_output_release(context.output);
	video_output_stop(context.video_queue);

	obs_remove_main_render_callback(render_preview_source, &context);
	obs_frontend_remove_event_callback(on_preview_scene_changed, &context);

	obs_source_release(context.current_source);

	obs_enter_graphics();
	gs_stagesurface_destroy(context.stagesurface);
	gs_texrender_destroy(context.texrender);
	obs_leave_graphics();

	video_output_close(context.video_queue);

	preview_output_running = false;
	// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
	doUI->PreviewOutputStateChanged(false);
}

void preview_output_start()
{
	OBSData settings = load_preview_settings();

	if (settings != nullptr) {
		//PRISM/LiuHaibin/20200416/#2413/for decklink crash
		if (main_output_running && is_setting_conflict())
			return;

		context.output = obs_output_create("decklink_output", "decklink_preview_output", settings, NULL);

		//PRISM/Zhangdewen/20210204/#6672/for decklink crash
		signal_handler_connect_ref(obs_output_get_signal_handler(context.output), "stop", preview_output_stop_callback, nullptr);

		obs_get_video_info(&context.ovi);

		uint32_t width = context.ovi.base_width;
		uint32_t height = context.ovi.base_height;

		obs_enter_graphics();
		context.texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
		context.stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
		obs_leave_graphics();

		const video_output_info *mainVOI = video_output_get_info(obs_get_video());

		video_output_info vi = {0};
		vi.format = VIDEO_FORMAT_BGRA;
		vi.width = width;
		vi.height = height;
		vi.fps_den = context.ovi.fps_den;
		vi.fps_num = context.ovi.fps_num;
		vi.cache_size = 16;
		vi.colorspace = mainVOI->colorspace;
		vi.range = mainVOI->range;
		vi.name = "decklink_preview_output";

		video_output_open(&context.video_queue, &vi);

		obs_frontend_add_event_callback(on_preview_scene_changed, &context);
		if (obs_frontend_preview_program_mode_active()) {
			context.current_source = obs_frontend_get_current_preview_scene();
		} else {
			context.current_source = obs_frontend_get_current_scene();
		}
		obs_add_main_render_callback(render_preview_source, &context);

		obs_output_set_media(context.output, context.video_queue, obs_get_audio());

		bool started = obs_output_start(context.output);

		preview_output_running = started;

		// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
		doUI->PreviewOutputStateChanged(started);

		if (!started)
			preview_output_stop();
	}
}

// Zhang dewen issue:#2416 merge OBS v25.0.8 code.
void preview_output_toggle()
{
	if (preview_output_running)
		preview_output_stop();
	else
		preview_output_start();
}

void on_preview_scene_changed(enum obs_frontend_event event, void *param)
{
	auto ctx = (struct preview_output *)param;
	switch (event) {
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		obs_source_release(ctx->current_source);
		ctx->current_source = obs_frontend_get_current_preview_scene();
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		obs_source_release(ctx->current_source);
		ctx->current_source = obs_frontend_get_current_scene();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		if (!obs_frontend_preview_program_mode_active()) {
			obs_source_release(ctx->current_source);
			ctx->current_source = obs_frontend_get_current_scene();
		}
		break;
	default:
		break;
	}
}

void render_preview_source(void *param, uint32_t cx, uint32_t cy)
{
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);

	auto ctx = (struct preview_output *)param;

	if (!ctx->current_source)
		return;

	uint32_t width = obs_source_get_base_width(ctx->current_source);
	uint32_t height = obs_source_get_base_height(ctx->current_source);

	gs_texrender_reset(ctx->texrender);

	if (gs_texrender_begin(ctx->texrender, width, height)) {
		struct vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(ctx->current_source);

		gs_blend_state_pop();
		gs_texrender_end(ctx->texrender);

		struct video_frame output_frame;
		if (video_output_lock_frame(ctx->video_queue, &output_frame, 1, os_gettime_ns())) {
			gs_stage_texture(ctx->stagesurface, gs_texrender_get_texture(ctx->texrender));

			if (gs_stagesurface_map(ctx->stagesurface, &ctx->video_data, &ctx->video_linesize)) {
				uint32_t linesize = output_frame.linesize[0];
				for (uint32_t i = 0; i < ctx->ovi.base_height; i++) {
					uint32_t dst_offset = linesize * i;
					uint32_t src_offset = ctx->video_linesize * i;
					memcpy(output_frame.data[0] + dst_offset, ctx->video_data + src_offset, linesize);
				}

				gs_stagesurface_unmap(ctx->stagesurface);
				ctx->video_data = nullptr;
			}

			video_output_unlock_frame(ctx->video_queue);
		}
	}
}

void addOutputUI(void)
{
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(obs_module_text("DecklinkOutput"));
	pls_add_tools_menu_seperator();

	QMainWindow *window = (QMainWindow *)obs_frontend_get_main_window();

	obs_frontend_push_ui_translation(obs_module_get_string);
	doUI = new DecklinkOutputUI(window);
	obs_frontend_pop_ui_translation();

	auto cb = []() { doUI->ShowHideDialog(); };

	action->connect(action, &QAction::triggered, cb);
}

static void OBSEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		OBSData settings = load_settings();

		if (settings && obs_data_get_bool(settings, "auto_start"))
			output_start();

		OBSData previewSettings = load_preview_settings();

		if (previewSettings && obs_data_get_bool(previewSettings, "auto_start"))
			preview_output_start();
	}
	//PRISM/Zhangdewen/20210204/#6672/for decklink crash
	else if (event == OBS_FRONTEND_EVENT_EXIT) {
		// main window close
		if (main_output_running) {
			output_stop(true);
		}

		if (preview_output_running) {
			preview_output_stop(true);
		}
	}
}

bool obs_module_load(void)
{
	addOutputUI();

	obs_frontend_add_event_callback(OBSEvent, nullptr);

	return true;
}

const char *obs_module_name(void)
{
	return "Decklink Output";
}
