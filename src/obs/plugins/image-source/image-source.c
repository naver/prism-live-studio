#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>
#include <util/base.h>

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
#include <pthread.h>
#include <util/circlebuf.h>
#include <pls/media-info.h>

#define plog(log_level, format, ...)                                \
	plog(log_level, "[image_source: '%s' %p] " format,          \
	     obs_source_get_name(context->source), context->source, \
	     ##__VA_ARGS__)

#define debug(format, ...) plog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) plog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) plog(LOG_WARNING, format, ##__VA_ARGS__)

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
enum task_type {
	task_load_image = 1,
	task_unload_image,
};

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
struct task_info {
	enum task_type type;
	void *params;
};

struct image_source {
	obs_source_t *source;

	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread --------------- start
	bool task_completed;
	uint32_t cx;
	uint32_t cy;
	uint32_t frame_count;
	uint64_t mem_usage;

	bool exit_task_thread;
	pthread_t task_thread;
	pthread_mutex_t task_mutex;
	struct circlebuf task_buffer;
	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread --------------- end

	char *file;
	bool persistent;
	time_t file_timestamp;
	float update_time_elapsed;
	uint64_t last_time;
	bool active;

	gs_image_file2_t if2;
};

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
void push_task(struct image_source *context, const struct task_info *info)
{
	pthread_mutex_lock(&context->task_mutex);

	context->task_completed = false;
	circlebuf_push_back(&context->task_buffer, info,
			    sizeof(struct task_info));

	pthread_mutex_unlock(&context->task_mutex);
}

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
bool pop_task(struct image_source *context, struct task_info *output)
{
	pthread_mutex_lock(&context->task_mutex);
	bool include_task = (context->task_buffer.size > 0);
	if (include_task) {
		circlebuf_pop_front(&context->task_buffer, output,
				    sizeof(struct task_info));
	}
	pthread_mutex_unlock(&context->task_mutex);
	return include_task;
}

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
static void image_source_unload_inner(struct image_source *context)
{
	obs_enter_graphics();
	gs_image_file2_free(&context->if2);
	obs_leave_graphics();
}

//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
void *image_task_thread(void *param)
{
	struct image_source *context = param;
	struct task_info task;

	//PRISM/WangChuanjing/20210913/NoIssue/thread info
	THREAD_START_LOG;

	while (!context->exit_task_thread) {
		if (!pop_task(context, &task)) {
			os_sleepto_ns(os_gettime_ns() + 10000000);
			continue;
		}

		switch (task.type) {
		case task_load_image: {
			image_source_unload_inner(context);
			if (task.params) {
				gs_image_file2_init(&context->if2,
						    (char *)task.params);

				obs_enter_graphics();
				gs_image_file2_init_texture(&context->if2);
				obs_leave_graphics();

				if (!context->if2.image.loaded) {

					char temp[256];
					os_extract_file_name(
						(char *)task.params, temp,
						ARRAY_SIZE(temp) - 1);
					warn("failed to load texture '%s'",
					     temp);
				}
			}
			break;
		}

		case task_unload_image: {
			image_source_unload_inner(context);
			break;
		}

		default: {
			warn("unkown task id in image plugin : %d",
			     (char *)task.type);
			assert(false);
			break;
		}
		}

		if (task.params) {
			bfree(task.params);
		}

		pthread_mutex_lock(&context->task_mutex);
		if (0 == context->task_buffer.size) {
			context->task_completed = true;
		}
		pthread_mutex_unlock(&context->task_mutex);
	}

	return NULL;
}

static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

static const char *image_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ImageInput");
}

static void image_source_load(struct image_source *context)
{
	char *file = context->file;

	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread

	bool file_valid = (file && *file);
	if (file_valid) {
		context->file_timestamp = get_modified_timestamp(file);
		context->update_time_elapsed = 0;

		media_info_t mi;
		if (!mi_open(&mi, file, MI_OPEN_DIRECTLY)) {
			context->cx = 0;
			context->cy = 0;
			context->frame_count = 0;
			context->mem_usage = 0;

			char temp[256];
			os_extract_file_name(file, temp, ARRAY_SIZE(temp) - 1);
			warn("failed to call mi_open for '%s'", temp);
		} else {
			long long cx = mi_get_int(&mi, "width");
			long long cy = mi_get_int(&mi, "height");
			long long frame_count = mi_get_int(&mi, "frame_count");
			int codec_id = mi_get_int(&mi, "vcodec_id");

			//PRISM/WangShaohui/20210802/NoIssue/scale large image
			gs_image_convert_resolution(cx, cy, &cx, &cy);

			context->cx = cx;
			context->cy = cy;
			context->frame_count = frame_count;
			context->mem_usage = (cx * cy * 4) * frame_count;

			mi_free(&mi);

			char temp[256];
			os_extract_file_name(file, temp, ARRAY_SIZE(temp) - 1);
			info("loaded image. %lldx%lld frames:%lld codec_id:%d '%s'",
			     cx, cy, frame_count, codec_id, temp);
		}
	}

	struct task_info task;
	task.type = task_load_image;
	task.params = file_valid ? bstrdup(file) : NULL;
	push_task(context, &task);
}

static void image_source_unload(struct image_source *context)
{
	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	struct task_info task;
	task.type = task_unload_image;
	task.params = NULL;
	push_task(context, &task);
}

static void image_source_update(void *data, obs_data_t *settings)
{
	struct image_source *context = data;
	const char *file = obs_data_get_string(settings, "file");
	const bool unload = obs_data_get_bool(settings, "unload");

	//PRISM/WangShaohui/20210607/NoIssue/Check whether to reload image
	if (file && context->file && 0 == strcmp(file, context->file)) {
		context->persistent = !unload;
		if (!obs_source_showing(context->source)) {
			if (context->persistent)
				image_source_load(data);
			else
				image_source_unload(data);
		}
		return;
	}

	if (context->file)
		bfree(context->file);
	context->file = bstrdup(file);
	context->persistent = !unload;

	/* Load the image if the source is persistent or showing */
	if (context->persistent || obs_source_showing(context->source))
		image_source_load(data);
	else
		image_source_unload(data);
}

static void image_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "unload", false);
}

static void image_source_show(void *data)
{
	struct image_source *context = data;

	if (!context->persistent)
		image_source_load(context);
}

static void image_source_hide(void *data)
{
	struct image_source *context = data;

	if (!context->persistent)
		image_source_unload(context);
}

static void *image_source_create(obs_data_t *settings, obs_source_t *source)
{
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	struct image_source *context = bzalloc(sizeof(struct image_source));
	context->source = source;

	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	context->exit_task_thread = false;
	pthread_mutex_init(&context->task_mutex, NULL);
	pthread_create(&context->task_thread, NULL, image_task_thread, context);

	image_source_update(context, settings);
	return context;
}

static void image_source_destroy(void *data)
{
	struct image_source *context = data;

	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread --------------- start
	void *thread_retval;
	context->exit_task_thread = true;
	pthread_join(context->task_thread, &thread_retval);

	image_source_unload_inner(context);

	struct task_info task;
	while (pop_task(context, &task)) {
		if (task.params) {
			bfree(task.params);
		}
	}

	circlebuf_free(&context->task_buffer);
	pthread_mutex_destroy(&context->task_mutex);
	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread --------------- end

	if (context->file)
		bfree(context->file);
	bfree(context);
}

static uint32_t image_source_getwidth(void *data)
{
	struct image_source *context = data;
	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	return context->cx;
}

static uint32_t image_source_getheight(void *data)
{
	struct image_source *context = data;
	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	return context->cy;
}

static void image_source_render(void *data, gs_effect_t *effect)
{
	struct image_source *context = data;

	//PRISM/WangShaohui/20200117/#281/for source unavailable
	if (context->if2.image.texture ||
	    (!context->file || 0 == strlen(context->file))) {
		obs_source_set_capture_valid(context->source, true,
					     OBS_SOURCE_ERROR_OK);
	} else {
		if (context->task_completed) {
			obs_source_set_capture_valid(
				context->source, false,
				os_is_file_exist(context->file)
					? OBS_SOURCE_ERROR_UNKNOWN
					: OBS_SOURCE_ERROR_NOT_FOUND);
		}
	}

	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	if (!context->if2.image.texture || !context->task_completed)
		return;

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			      context->if2.image.texture);
	gs_draw_sprite(context->if2.image.texture, 0, context->if2.image.cx,
		       context->if2.image.cy);
}

static void image_source_tick(void *data, float seconds)
{
	struct image_source *context = data;
	uint64_t frame_time = obs_get_video_frame_time();

	context->update_time_elapsed += seconds;

	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	if (!context->task_completed) {
		return;
	}

	if (obs_source_showing(context->source)) {
		if (context->update_time_elapsed >= 1.0f) {
			time_t t = get_modified_timestamp(context->file);
			context->update_time_elapsed = 0.0f;

			if (context->file_timestamp != t) {
				image_source_load(context);
			}
		}
	}

	//PRISM/WangShaohui/20200303/#872/for playing gif in studio mode
	if (obs_source_showing(context->source)) {
		if (!context->active) {
			if (context->if2.image.is_animated_gif)
				context->last_time = frame_time;
			context->active = true;
		}

	} else {
		if (context->active) {
			if (context->if2.image.is_animated_gif) {
				context->if2.image.cur_frame = 0;
				context->if2.image.cur_loop = 0;
				context->if2.image.cur_time = 0;

				obs_enter_graphics();
				gs_image_file2_update_texture(&context->if2);
				obs_leave_graphics();
			}

			context->active = false;
		}

		return;
	}

	if (context->last_time && context->if2.image.is_animated_gif) {
		uint64_t elapsed = frame_time - context->last_time;
		bool updated = gs_image_file2_tick(&context->if2, elapsed);

		if (updated) {
			obs_enter_graphics();
			gs_image_file2_update_texture(&context->if2);
			obs_leave_graphics();
		}
	}

	context->last_time = frame_time;
}

static const char *image_filter =
	"All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.psd);;"
	"BMP Files (*.bmp);;"
	"Targa Files (*.tga);;"
	"PNG Files (*.png);;"
	"JPEG Files (*.jpeg *.jpg);;"
	"GIF Files (*.gif);;"
	"PSD Files (*.psd);;"
	//PRISM/Liuying/20210323/#None/add JFIF Files
	"JFIF Files (*.jfif);;"
	"All Files (*.*)";

static obs_properties_t *image_source_properties(void *data)
{
	struct image_source *s = data;
	struct dstr path = {0};

	obs_properties_t *props = obs_properties_create();

	if (s && s->file && *s->file) {
		const char *slash;

		dstr_copy(&path, s->file);
		dstr_replace(&path, "\\", "/");
		slash = strrchr(path.array, '/');
		if (slash)
			dstr_resize(&path, slash - path.array + 1);
	}

	obs_properties_add_path(props, "file", obs_module_text("File"),
				OBS_PATH_FILE, image_filter, path.array);
	obs_properties_add_bool(props, "unload",
				obs_module_text("UnloadWhenNotShowing"));
	dstr_free(&path);

	return props;
}

uint64_t image_source_get_memory_usage(void *data)
{
	struct image_source *s = data;
	//PRISM/WangShaohui/20210526/NoIssue/load/unload image in worker thread
	return s->mem_usage;
}

//PRISM/ZengQin/20210604/#none/Get properties parameters
static obs_data_t *image_source_props_params(void *data)
{
	if (!data)
		return NULL;

	struct image_source *s = data;
	obs_data_t *params = obs_data_create();
	obs_data_set_int(params, "width", s->cx);
	obs_data_set_int(params, "height", s->cy);
	obs_data_set_int(params, "frames", s->frame_count);

	return params;
}

static struct obs_source_info image_source_info = {
	.id = "image_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = image_source_get_name,
	.create = image_source_create,
	.destroy = image_source_destroy,
	.update = image_source_update,
	.get_defaults = image_source_defaults,
	.show = image_source_show,
	.hide = image_source_hide,
	.get_width = image_source_getwidth,
	.get_height = image_source_getheight,
	.video_render = image_source_render,
	.video_tick = image_source_tick,
	.get_properties = image_source_properties,
	.icon_type = OBS_ICON_TYPE_IMAGE,
	//PRISM/ZengQin/20210604/#none/Get properties parameters
	.props_params = image_source_props_params,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("image-source", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Image/color/slideshow sources";
}

extern struct obs_source_info slideshow_info;
extern struct obs_source_info color_source_info;

bool obs_module_load(void)
{
	obs_register_source(&image_source_info);
	obs_register_source(&color_source_info);
	obs_register_source(&slideshow_info);
	return true;
}
