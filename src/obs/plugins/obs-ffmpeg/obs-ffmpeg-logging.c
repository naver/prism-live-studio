#include <obs-module.h>
#include <util/darray.h>
#include <util/threading.h>
#include <libavutil/log.h>

static DARRAY(struct log_context {
	void *context;
	char str[4096];
	int print_prefix;
} *) active_log_contexts;
static DARRAY(struct log_context *) cached_log_contexts;
pthread_mutex_t log_contexts_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct log_context *create_or_fetch_log_context(void *context)
{
	pthread_mutex_lock(&log_contexts_mutex);
	for (size_t i = 0; i < active_log_contexts.num; i++) {
		if (context == active_log_contexts.array[i]->context) {
			pthread_mutex_unlock(&log_contexts_mutex);
			return active_log_contexts.array[i];
		}
	}

	struct log_context *new_log_context = NULL;

	size_t cnt = cached_log_contexts.num;
	if (!!cnt) {
		new_log_context = cached_log_contexts.array[cnt - 1];
		da_pop_back(cached_log_contexts);
	}

	if (!new_log_context)
		new_log_context = bzalloc(sizeof(struct log_context));

	new_log_context->context = context;
	new_log_context->str[0] = '\0';
	new_log_context->print_prefix = 1;

	da_push_back(active_log_contexts, &new_log_context);

	pthread_mutex_unlock(&log_contexts_mutex);

	return new_log_context;
}

static void destroy_log_context(struct log_context *log_context)
{
	pthread_mutex_lock(&log_contexts_mutex);
	da_erase_item(active_log_contexts, &log_context);
	da_push_back(cached_log_contexts, &log_context);
	pthread_mutex_unlock(&log_contexts_mutex);
}

//PRISM/LiuHaibin/20210208/#None/ignore some ffmpeg log
/* clang-format off */
#define END_MARK "The-End-of-PRISM-FFmpeg-Log-Filter"
static const char *const ffmpeg_log_filter[] = {
	"No JPEG data found in image\n",
	"Found EOI before SOI, skipping\n",

	END_MARK
};
/* clang-format on */

static bool filter_ffmpeg_log(const char *msg)
{
	if (msg) {
		int i = 0;
		while (0 != strcmp(ffmpeg_log_filter[i], END_MARK)) {
			if (0 == strcmp(ffmpeg_log_filter[i], msg))
				return true;
			++i;
		}
		return false;
	}
	return true;
}

char *ff_log_level_string(int level)
{
	switch (level) {
	case AV_LOG_PANIC:
		return "PANIC  ";
	case AV_LOG_FATAL:
		return "FATAL  ";
	case AV_LOG_ERROR:
		return "ERROR  ";
	case AV_LOG_WARNING:
		return "WARNING";
	case AV_LOG_INFO:
		return "INFO   ";
	case AV_LOG_VERBOSE:
		return "VERBOSE";
	case AV_LOG_DEBUG:
		return "DEBUG  ";
	case AV_LOG_TRACE:
		return "TRACE  ";
	default:
		return "WARNING";
	}
}

static void ffmpeg_log_callback(void *context, int level, const char *format,
				va_list args)
{
	if (format == NULL)
		return;

	if (level >= AV_LOG_INFO)
		return;

	if (filter_ffmpeg_log(format))
		return;

	struct log_context *log_context = create_or_fetch_log_context(context);

	char *str = log_context->str;

	av_log_format_line(context, level, format, args, str + strlen(str),
			   (int)(sizeof(log_context->str) - strlen(str)),
			   &log_context->print_prefix);

	int obs_level;
	switch (level) {
	case AV_LOG_PANIC:
	case AV_LOG_FATAL:
		obs_level = LOG_WARNING;
		break;
	case AV_LOG_ERROR:
	case AV_LOG_WARNING:
	default:
		obs_level = LOG_DEBUG;
		break;
	}

	if (!log_context->print_prefix)
		return;

	char *str_end = str + strlen(str) - 1;
	while (str < str_end) {
		if (*str_end != '\n')
			break;
		*str_end-- = '\0';
	}

	if (str_end <= str)
		goto cleanup;

	//PRISM/Wangshaohui/20210913/#9672/send log to KR nelo
	blogex(true, obs_level, NULL, 0, "[FFmpeg] %s: %s",
	       ff_log_level_string(level), str);

cleanup:
	destroy_log_context(log_context);
}

static bool logging_initialized = false;

void obs_ffmpeg_load_logging(void)
{
	da_init(active_log_contexts);
	da_init(cached_log_contexts);

	if (pthread_mutex_init(&log_contexts_mutex, NULL) == 0) {
		av_log_set_level(AV_LOG_WARNING);
		av_log_set_callback(ffmpeg_log_callback);
		logging_initialized = true;
	}
}

void obs_ffmpeg_unload_logging(void)
{
	if (!logging_initialized)
		return;

	logging_initialized = false;

	av_log_set_callback(av_log_default_callback);

	//PRISM/LiuHaibin/20211018/#10017/lock contexts when release them, and destroy mutex after
	pthread_mutex_lock(&log_contexts_mutex);
	for (size_t i = 0; i < active_log_contexts.num; i++) {
		bfree(active_log_contexts.array[i]);
	}
	for (size_t i = 0; i < cached_log_contexts.num; i++) {
		bfree(cached_log_contexts.array[i]);
	}

	da_free(active_log_contexts);
	da_free(cached_log_contexts);
	pthread_mutex_unlock(&log_contexts_mutex);
	pthread_mutex_destroy(&log_contexts_mutex);
}
