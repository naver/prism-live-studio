#include <stdlib.h>
#include <util/dstr.h>
#include "dc-capture.h"
#include "window-helpers.h"
#include "../../libobs/util/platform.h"
#include "../../libobs-winrt/winrt-capture.h"

//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
#include <Windows.h>

/* clang-format off */

#define TEXT_WINDOW_CAPTURE obs_module_text("WindowCapture")
#define TEXT_WINDOW         obs_module_text("WindowCapture.Window")
#define TEXT_METHOD         obs_module_text("WindowCapture.Method")
#define TEXT_METHOD_AUTO    obs_module_text("WindowCapture.Method.Auto")
#define TEXT_METHOD_BITBLT  obs_module_text("WindowCapture.Method.BitBlt")
#define TEXT_METHOD_WGC     obs_module_text("WindowCapture.Method.WindowsGraphicsCapture")
#define TEXT_MATCH_PRIORITY obs_module_text("WindowCapture.Priority")
#define TEXT_MATCH_TITLE    obs_module_text("WindowCapture.Priority.Title")
#define TEXT_MATCH_CLASS    obs_module_text("WindowCapture.Priority.Class")
#define TEXT_MATCH_EXE      obs_module_text("WindowCapture.Priority.Exe")
#define TEXT_CAPTURE_CURSOR obs_module_text("CaptureCursor")
#define TEXT_COMPATIBILITY  obs_module_text("Compatibility")
#define TEXT_CLIENT_AREA    obs_module_text("ClientArea")

/* clang-format on */

//PRISM/WangShaohui/20200117/#281/for source unavailable
#define CHECK_WINDOW_INTERVAL 1000 // in milliseconds

struct winrt_exports {
	BOOL *(*winrt_capture_supported)();
	BOOL *(*winrt_capture_cursor_toggle_supported)();
	struct winrt_capture *(*winrt_capture_init_window)(BOOL cursor,
							   HWND window,
							   BOOL client_area);
	void (*winrt_capture_free)(struct winrt_capture *capture);
	BOOL *(*winrt_capture_active)(const struct winrt_capture *capture);
	void (*winrt_capture_show_cursor)(struct winrt_capture *capture,
					  BOOL visible);
	void (*winrt_capture_render)(struct winrt_capture *capture,
				     gs_effect_t *effect);
	uint32_t (*winrt_capture_width)(const struct winrt_capture *capture);
	uint32_t (*winrt_capture_height)(const struct winrt_capture *capture);
};

enum window_capture_method {
	METHOD_AUTO,
	METHOD_BITBLT,
	METHOD_WGC,
};

typedef DPI_AWARENESS_CONTEXT(WINAPI *PFN_SetThreadDpiAwarenessContext)(
	DPI_AWARENESS_CONTEXT);
typedef DPI_AWARENESS_CONTEXT(WINAPI *PFN_GetThreadDpiAwarenessContext)(VOID);
typedef DPI_AWARENESS_CONTEXT(WINAPI *PFN_GetWindowDpiAwarenessContext)(HWND);

struct window_capture {
	obs_source_t *source;

	char *title;
	char *class;
	char *executable;
	enum window_capture_method method;
	enum window_priority priority;
	bool cursor;
	bool compatibility;
	bool client_area;
	bool use_wildcards; /* TODO */

	struct dc_capture capture;

	bool wgc_supported;
	bool previously_failed;
	void *winrt_module;
	struct winrt_exports exports;
	struct winrt_capture *capture_winrt;

	float resize_timer;
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	DWORD previous_check_time; // in millisecond
	float cursor_check_time;

	//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
	CRITICAL_SECTION window_str_mutex;

	HWND window;
	RECT last_rect;

	PFN_SetThreadDpiAwarenessContext set_thread_dpi_awareness_context;
	PFN_GetThreadDpiAwarenessContext get_thread_dpi_awareness_context;
	PFN_GetWindowDpiAwarenessContext get_window_dpi_awareness_context;
};

static const char *wgc_partial_match_classes[] = {
	"Chrome",
	"Mozilla",
	NULL,
};

static const char *wgc_whole_match_classes[] = {
	"ApplicationFrameWindow",
	"Windows.UI.Core.CoreWindow",
	"XLMAIN",        /* excel*/
	"PPTFrameClass", /* powerpoint */
	"OpusApp",       /* word */
	NULL,
};

//PRISM/WangShaohui/20200525/#2928/for AUTO window capture
static const char *wgc_whole_match_exe[] = {
	"vlc.exe",
	NULL,
};

//PRISM/WangShaohui/20200525/#2928/for AUTO window capture
static enum window_capture_method
choose_method(enum window_capture_method method, bool wgc_supported,
	      const char *current_class, const char *current_exe)
{
	if (!wgc_supported)
		return METHOD_BITBLT;

	if (method != METHOD_AUTO)
		return method;

	if (!current_class)
		return METHOD_BITBLT;

	const char **class = wgc_partial_match_classes;
	while (*class) {
		if (astrstri(current_class, *class) != NULL) {
			return METHOD_WGC;
		}
		class ++;
	}

	class = wgc_whole_match_classes;
	while (*class) {
		if (astrcmpi(current_class, *class) == 0) {
			return METHOD_WGC;
		}
		class ++;
	}

	//PRISM/WangShaohui/20200525/#2928/for AUTO window capture
	if (current_exe) {
		const char **exe_name = wgc_whole_match_exe;
		while (*exe_name) {
			if (astrstri(current_exe, *exe_name) != NULL) {
				return METHOD_WGC;
			}
			exe_name++;
		}
	}

	return METHOD_BITBLT;
}

static void update_settings(struct window_capture *wc, obs_data_t *s)
{
	int method = (int)obs_data_get_int(s, "method");
	const char *window = obs_data_get_string(s, "window");
	int priority = (int)obs_data_get_int(s, "priority");

	//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
	{
		EnterCriticalSection(&wc->window_str_mutex);

		if (wc->title)
			bfree(wc->title);
		if (wc->class)
			bfree(wc->class);
		if (wc->executable)
			bfree(wc->executable);

		build_window_strings(window, &wc->class, &wc->title,
				     &wc->executable);

		if (wc->title != NULL) {
			//PRISM/WangShaohui/20201102/#NoIssue/for debug log
			char temp[256];
			os_extract_file_name(wc->executable, temp,
					     ARRAY_SIZE(temp) - 1);
			plog(LOG_INFO,
			     "[window-capture: '%s'] [obs_source:%p plugin_obj:%p] window updated : \n"
			     "\t executable: %s \n"
			     "\t class: %s \n"
			     "\t title: %s \n"
			     "\t priority: %d \n"
			     "\t method: %d",
			     obs_source_get_name(wc->source), wc->source, wc,
			     temp, wc->class ? wc->class : "",
			     wc->title ? wc->title : "", priority, method);
		}

		//PRISM/WangShaohui/20200525/#2928/for AUTO window capture
		wc->method = choose_method(method, wc->wgc_supported, wc->class,
					   wc->executable);

		LeaveCriticalSection(&wc->window_str_mutex);
	}

	wc->priority = (enum window_priority)priority;
	wc->cursor = obs_data_get_bool(s, "cursor");
	wc->use_wildcards = obs_data_get_bool(s, "use_wildcards");
	wc->compatibility = obs_data_get_bool(s, "compatibility");
	wc->client_area = obs_data_get_bool(s, "client_area");
}

/* ------------------------------------------------------------------------- */

static const char *wc_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return TEXT_WINDOW_CAPTURE;
}

#define WINRT_IMPORT(func)                                        \
	do {                                                      \
		exports->func = os_dlsym(module, #func);          \
		if (!exports->func) {                             \
			success = false;                          \
			plog(LOG_ERROR,                           \
			     "Could not load function '%s' from " \
			     "module '%s'",                       \
			     #func, module_name);                 \
		}                                                 \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *module,
			       const char *module_name)
{
	bool success = true;

	WINRT_IMPORT(winrt_capture_supported);
	WINRT_IMPORT(winrt_capture_cursor_toggle_supported);
	WINRT_IMPORT(winrt_capture_init_window);
	WINRT_IMPORT(winrt_capture_free);
	WINRT_IMPORT(winrt_capture_active);
	WINRT_IMPORT(winrt_capture_show_cursor);
	WINRT_IMPORT(winrt_capture_render);
	WINRT_IMPORT(winrt_capture_width);
	WINRT_IMPORT(winrt_capture_height);

	return success;
}

static void *wc_create(obs_data_t *settings, obs_source_t *source)
{
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	struct window_capture *wc = bzalloc(sizeof(struct window_capture));
	wc->source = source;

	//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
	InitializeCriticalSection(&wc->window_str_mutex);

	obs_enter_graphics();
	const bool uses_d3d11 = gs_get_device_type() == GS_DEVICE_DIRECT3D_11;
	obs_leave_graphics();

	if (uses_d3d11) {
		static const char *const module = "libobs-winrt";
		bool use_winrt_capture = false;
		wc->winrt_module = os_dlopen(module);
		if (wc->winrt_module &&
		    load_winrt_imports(&wc->exports, wc->winrt_module,
				       module) &&
		    wc->exports.winrt_capture_supported()) {
			wc->wgc_supported = true;
		}

		//PRISM/WangShaohui/20200525/#2928/save log for WGC
		static bool wgc_log_saved = false;
		if (!wgc_log_saved) {
			wgc_log_saved = true;
			plog(LOG_INFO, "[window-capture] support WGC : %s",
			     wc->wgc_supported ? "YES" : "NO");
		}
	}

	const HMODULE hModuleUser32 = GetModuleHandle(L"User32.dll");
	if (hModuleUser32) {
		PFN_SetThreadDpiAwarenessContext
			set_thread_dpi_awareness_context =
				(PFN_SetThreadDpiAwarenessContext)GetProcAddress(
					hModuleUser32,
					"SetThreadDpiAwarenessContext");
		PFN_GetThreadDpiAwarenessContext
			get_thread_dpi_awareness_context =
				(PFN_GetThreadDpiAwarenessContext)GetProcAddress(
					hModuleUser32,
					"GetThreadDpiAwarenessContext");
		PFN_GetWindowDpiAwarenessContext
			get_window_dpi_awareness_context =
				(PFN_GetWindowDpiAwarenessContext)GetProcAddress(
					hModuleUser32,
					"GetWindowDpiAwarenessContext");
		if (set_thread_dpi_awareness_context &&
		    get_thread_dpi_awareness_context &&
		    get_window_dpi_awareness_context) {
			wc->set_thread_dpi_awareness_context =
				set_thread_dpi_awareness_context;
			wc->get_thread_dpi_awareness_context =
				get_thread_dpi_awareness_context;
			wc->get_window_dpi_awareness_context =
				get_window_dpi_awareness_context;
		}
	}

	update_settings(wc, settings);
	return wc;
}

static void wc_actual_destroy(void *data)
{
	struct window_capture *wc = data;

	if (wc->capture_winrt) {
		wc->exports.winrt_capture_free(wc->capture_winrt);
	}

	obs_enter_graphics();
	dc_capture_free(&wc->capture);
	obs_leave_graphics();

	bfree(wc->title);
	bfree(wc->class);
	bfree(wc->executable);

	if (wc->winrt_module)
		os_dlclose(wc->winrt_module);

	//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
	DeleteCriticalSection(&wc->window_str_mutex);

	bfree(wc);
}

static void wc_destroy(void *data)
{
	obs_queue_task(OBS_TASK_GRAPHICS, wc_actual_destroy, data, false);
}

static void wc_update(void *data, obs_data_t *settings)
{
	struct window_capture *wc = data;
	update_settings(wc, settings);

	/* forces a reset */
	wc->window = NULL;
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	wc->previous_check_time = 0;

	wc->previously_failed = false;
}

static uint32_t wc_width(void *data)
{
	struct window_capture *wc = data;
	return (wc->method == METHOD_WGC)
		       ? wc->exports.winrt_capture_width(wc->capture_winrt)
		       : wc->capture.width;
}

static uint32_t wc_height(void *data)
{
	struct window_capture *wc = data;
	return (wc->method == METHOD_WGC)
		       ? wc->exports.winrt_capture_height(wc->capture_winrt)
		       : wc->capture.height;
}

static void wc_defaults(obs_data_t *defaults)
{
	obs_data_set_default_int(defaults, "method", METHOD_AUTO);
	obs_data_set_default_bool(defaults, "cursor", true);
	obs_data_set_default_bool(defaults, "compatibility", false);
	obs_data_set_default_bool(defaults, "client_area", true);
}

static void update_settings_visibility(obs_properties_t *props,
				       struct window_capture *wc)
{
	const enum window_capture_method method = wc->method;
	const bool bitblt_options = method == METHOD_BITBLT;
	const bool wgc_options = method == METHOD_WGC;

	const bool wgc_cursor_toggle =
		wgc_options &&
		wc->exports.winrt_capture_cursor_toggle_supported();

	obs_property_t *p = obs_properties_get(props, "cursor");
	obs_property_set_visible(p, bitblt_options || wgc_cursor_toggle);

	p = obs_properties_get(props, "compatibility");
	obs_property_set_visible(p, bitblt_options);

	p = obs_properties_get(props, "client_area");
	obs_property_set_visible(p, wgc_options);
}

static bool wc_capture_method_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	struct window_capture *wc = obs_properties_get_param(props);
	update_settings(wc, settings);

	update_settings_visibility(props, wc);

	return true;
}

//PRISM/WangShaohui/20200302/#420/for not found window
static bool on_window_changed_handle(obs_properties_t *props, obs_property_t *p,
				     obs_data_t *settings)
{
	struct window_capture *wc = obs_properties_get_param(props);
	update_settings(wc, settings);

	update_settings_visibility(props, wc);

	//return on_window_changed(props, p, settings, "window", 0);

	//PRISM/Liuying/20201113/#5692/for refresh property warning ui when window changed
	on_window_changed(props, p, settings, "window", 0);
	return true;
}

static obs_properties_t *wc_properties(void *data)
{
	struct window_capture *wc = data;

	obs_properties_t *ppts = obs_properties_create();
	obs_properties_set_param(ppts, wc, NULL);

	obs_property_t *p;

	p = obs_properties_add_list(ppts, "window", TEXT_WINDOW,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	fill_window_list(p, EXCLUDE_MINIMIZED, NULL);
	//PRISM/WangShaohui/20200302/#420/for not found window
	obs_property_set_modified_callback(p, on_window_changed_handle);

	p = obs_properties_add_list(ppts, "method", TEXT_METHOD,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_METHOD_AUTO, METHOD_AUTO);
	obs_property_list_add_int(p, TEXT_METHOD_BITBLT, METHOD_BITBLT);
	obs_property_list_add_int(p, TEXT_METHOD_WGC, METHOD_WGC);
	obs_property_list_item_disable(p, 2, !wc->wgc_supported);
	obs_property_set_modified_callback(p, wc_capture_method_changed);

	p = obs_properties_add_list(ppts, "priority", TEXT_MATCH_PRIORITY,
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, TEXT_MATCH_TITLE, WINDOW_PRIORITY_TITLE);
	obs_property_list_add_int(p, TEXT_MATCH_CLASS, WINDOW_PRIORITY_CLASS);
	obs_property_list_add_int(p, TEXT_MATCH_EXE, WINDOW_PRIORITY_EXE);

	obs_properties_add_bool(ppts, "cursor", TEXT_CAPTURE_CURSOR);

	obs_properties_add_bool(ppts, "compatibility", TEXT_COMPATIBILITY);

	obs_properties_add_bool(ppts, "client_area", TEXT_CLIENT_AREA);

	return ppts;
}

static void wc_hide(void *data)
{
	struct window_capture *wc = data;

	if (wc->capture_winrt) {
		wc->exports.winrt_capture_free(wc->capture_winrt);
		wc->capture_winrt = NULL;
	}

	memset(&wc->last_rect, 0, sizeof(wc->last_rect));
}

#define RESIZE_CHECK_TIME 0.2f
#define CURSOR_CHECK_TIME 0.2f

//PRISM/WangShaohui/20200117/#281/for source unavailable
static bool is_window_valid(struct window_capture *wc)
{
	return (wc->window && IsWindow(wc->window));
}

//PRISM/WangShaohui/20200117/#281/for source unavailable
static bool should_check_window(struct window_capture *wc)
{
	DWORD current_time = GetTickCount();

	if (wc->previous_check_time > current_time) {
		wc->previous_check_time = current_time;
		return true; // system time has been changed
	}

	if ((current_time - wc->previous_check_time) >= CHECK_WINDOW_INTERVAL) {
		wc->previous_check_time = current_time;
		return true;
	}

	return false;
}

static void wc_tick(void *data, float seconds)
{
	struct window_capture *wc = data;
	RECT rect;
	bool reset_capture = false;

	if (!obs_source_showing(wc->source))
		return;

	if (!is_window_valid(wc)) {
		//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
		EnterCriticalSection(&wc->window_str_mutex);
		bool no_info = (!wc->title && !wc->class);
		LeaveCriticalSection(&wc->window_str_mutex);
		if (no_info) {
			//PRISM/WangShaohui/20200117/#281/for source unavailable
			obs_source_set_capture_valid(wc->source, true,
						     OBS_SOURCE_ERROR_OK);
			if (wc->capture.valid)
				dc_capture_free(&wc->capture);
			return;
		}

		if (wc->capture_winrt) {
			wc->exports.winrt_capture_free(wc->capture_winrt);
			wc->capture_winrt = NULL;
		}

		//PRISM/WangShaohui/20200117/#281/for source unavailable
		if (!should_check_window(wc)) {
			if (wc->capture.valid)
				dc_capture_free(&wc->capture);
			return;
		}

		//PRISM/WangShaohui/20200729/#3285/for enum window object
		//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
		EnterCriticalSection(&wc->window_str_mutex);
		wc->window = (wc->method == METHOD_WGC)
				     ? find_window_top_level(EXCLUDE_MINIMIZED,
							     wc->priority,
							     wc->class,
							     wc->title,
							     wc->executable)
				     : find_window(EXCLUDE_MINIMIZED,
						   wc->priority, wc->class,
						   wc->title, wc->executable);
		//PRISM/WangShaohui/20201012/noissue/thread safe for title/class/exe
		LeaveCriticalSection(&wc->window_str_mutex);

		if (!wc->window) {
			//PRISM/WangShaohui/20200117/#281/for source unavailable
			obs_source_set_capture_valid(
				wc->source, false, OBS_SOURCE_ERROR_NOT_FOUND);
			if (wc->capture.valid)
				dc_capture_free(&wc->capture);
			return;
		}

		wc->previously_failed = false;
		reset_capture = true;

		//PRISM/WangShaohui/20200117/#281/for source unavailable
		obs_source_set_capture_valid(wc->source, true,
					     OBS_SOURCE_ERROR_OK);

		//PRISM/WangShaohui/20210428/#5157/handle LAYERED window
		if (wc->wgc_supported) {
			obs_data_t *data = obs_source_get_settings(wc->source);
			if (data) {
				int method =
					(int)obs_data_get_int(data, "method");
				LONG exstyle =
					GetWindowLong(wc->window, GWL_EXSTYLE);
				if (method == METHOD_AUTO &&
				    (WS_EX_LAYERED & exstyle)) {
					wc->method = METHOD_WGC;
				}
				obs_data_release(data);
			}
		}

	} else if (IsIconic(wc->window)) {
		return;
	}

	wc->cursor_check_time += seconds;
	if (wc->cursor_check_time > CURSOR_CHECK_TIME) {
		DWORD foreground_pid, target_pid;

		// Can't just compare the window handle in case of app with child windows
		if (!GetWindowThreadProcessId(GetForegroundWindow(),
					      &foreground_pid))
			foreground_pid = 0;

		if (!GetWindowThreadProcessId(wc->window, &target_pid))
			target_pid = 0;

		const bool cursor_hidden = foreground_pid && target_pid &&
					   foreground_pid != target_pid;
		wc->capture.cursor_hidden = cursor_hidden;
		if (wc->capture_winrt)
			wc->exports.winrt_capture_show_cursor(wc->capture_winrt,
							      !cursor_hidden);

		wc->cursor_check_time = 0.0f;
	}

	obs_enter_graphics();

	if (wc->method == METHOD_BITBLT) {
		DPI_AWARENESS_CONTEXT previous = NULL;
		if (wc->get_window_dpi_awareness_context != NULL) {
			const DPI_AWARENESS_CONTEXT context =
				wc->get_window_dpi_awareness_context(
					wc->window);
			previous =
				wc->set_thread_dpi_awareness_context(context);
		}

		GetClientRect(wc->window, &rect);

		if (!reset_capture) {
			wc->resize_timer += seconds;

			if (wc->resize_timer >= RESIZE_CHECK_TIME) {
				if ((rect.bottom - rect.top) !=
					    (wc->last_rect.bottom -
					     wc->last_rect.top) ||
				    (rect.right - rect.left) !=
					    (wc->last_rect.right -
					     wc->last_rect.left))
					reset_capture = true;

				wc->resize_timer = 0.0f;
			}
		}

		if (reset_capture) {
			wc->resize_timer = 0.0f;
			wc->last_rect = rect;
			dc_capture_free(&wc->capture);
			dc_capture_init(&wc->capture, 0, 0,
					rect.right - rect.left,
					rect.bottom - rect.top, wc->cursor,
					wc->compatibility);
		}

		dc_capture_capture(&wc->capture, wc->window);

		if (previous)
			wc->set_thread_dpi_awareness_context(previous);

	} else if (wc->method == METHOD_WGC) {
		if (wc->window && (wc->capture_winrt == NULL)) {
			if (!wc->previously_failed) {
				wc->capture_winrt =
					wc->exports.winrt_capture_init_window(
						wc->cursor, wc->window,
						wc->client_area);

				if (!wc->capture_winrt) {
					wc->previously_failed = true;
				}
			}
		}
	}

	obs_leave_graphics();
}

static void wc_render(void *data, gs_effect_t *effect)
{
	struct window_capture *wc = data;
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	if (is_window_valid(wc)) {
		gs_effect_t *const opaque =
			obs_get_base_effect(OBS_EFFECT_OPAQUE);
		if (wc->method == METHOD_WGC) {
			if (wc->capture_winrt) {
				if (wc->exports.winrt_capture_active(
					    wc->capture_winrt)) {
					wc->exports.winrt_capture_render(
						wc->capture_winrt, opaque);
				} else {
					wc->exports.winrt_capture_free(
						wc->capture_winrt);
					wc->capture_winrt = NULL;
				}
			}
		} else {
			dc_capture_render(&wc->capture, opaque);
		}
	}

	UNUSED_PARAMETER(effect);
}

//PRISM/ZengQin/20210604/#none/Get properties parameters
static obs_data_t *wc_props_params(void *data)
{
	if (!data)
		return NULL;

	struct window_capture *wc = data;
	obs_data_t *settings = obs_source_get_settings(wc->source);
	int method = obs_data_get_int(settings, "method");
	const char *window = obs_data_get_string(settings, "window");
	obs_data_release(settings);

	obs_data_t *params = obs_data_create();
	obs_data_set_int(params, "method", method);
	obs_data_set_string(params, "window", window);

	return params;
}

struct obs_source_info window_capture_info = {
	.id = "window_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = wc_getname,
	.create = wc_create,
	.destroy = wc_destroy,
	.update = wc_update,
	.video_render = wc_render,
	.hide = wc_hide,
	.video_tick = wc_tick,
	.get_width = wc_width,
	.get_height = wc_height,
	.get_defaults = wc_defaults,
	.get_properties = wc_properties,
	.icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE,
	//PRISM/ZengQin/20210604/#none/Get properties parameters
	.props_params = wc_props_params,
};
