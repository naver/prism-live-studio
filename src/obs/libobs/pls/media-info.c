#include "obs.h"
#include "obs-internal.h"
#include "pls/media-info.h"

#include <libswscale/swscale.h>
#include <util/threading.h>
#include <libavutil/imgutils.h>

#define FFMPEG_OPEN_TIMEOUT_NS 1000000000 // opening timeout
#define ID3V2_HEADER_SIZE 10
#define ID3v2_DEFAULT_MAGIC "ID3"
#define USING_FFMPEG 1

#define SAFE_FREE_STRING(STR_PTR)        \
	{                                \
		if (*STR_PTR) {          \
			bfree(*STR_PTR); \
			*STR_PTR = NULL; \
		}                        \
	}

#if LIBAVCODEC_VERSION_MAJOR >= 58
#define CODEC_FLAG_GLOBAL_H AV_CODEC_FLAG_GLOBAL_HEADER
#else
#define CODEC_FLAG_GLOBAL_H CODEC_FLAG_GLOBAL_HEADER
#endif

/* clang-format off */
static const mi_option mi_options[] = {
	/* options retrieved in format of string */
	{"title",		"title field for mp3 id3",				MI_TYPE_STRING},
	{"artist",		"artist field for mp3 id3",				MI_TYPE_STRING},
	{"album",		"album field for mp3 id3",				MI_TYPE_STRING},
	{"date",		"date field for mp3 id3",				MI_TYPE_STRING},
	{"comment",		"comment field for mp3 id3",				MI_TYPE_STRING},
	{"genre",		"genre field for mp3 id3",				MI_TYPE_STRING},
	{"copyright",		"copyright field for mp3 id3",				MI_TYPE_STRING},
	{"language",		"language field for mp3 id3",				MI_TYPE_STRING},
	{"album_artist",	"album_artist field for mp3 id3",			MI_TYPE_STRING},
	{"disc",		"disc field for mp3 id3",				MI_TYPE_STRING},
	{"publisher",		"publisher field for mp3 id3",				MI_TYPE_STRING},
	{"track",		"track field for mp3 id3",				MI_TYPE_STRING},
	{"encoder",		"encoder field for mp3 id3",				MI_TYPE_STRING},

	/* options retrieved in format of int64_t */
	{"duration",		"duration of meida file, in milliseconds",		MI_TYPE_INT},
	{"width",		"width of video data",					MI_TYPE_INT},
	{"height",		"height of video data",					MI_TYPE_INT},
	{"frame_count",		"frame count for video data, estimated value, may not accurate",
											MI_TYPE_INT},

	/* options retrieved in format of boolean */
	{"has_cover",		"flag shows if an audio file has a cover attached",	MI_TYPE_BOOL},

	/* options retrieved in format of packed information object */
	{"cover_obj",		"audio cover object, defined in mi_cover_t",		MI_TYPE_OBJ},
	{"metadata_obj",	"audio metadata object, defined in mi_metadata_t",	MI_TYPE_OBJ},
	{"id3v2_obj",		"mp3 id3 v2.3 object, defined in mi_id3v23_t",		MI_TYPE_OBJ},
	{"first_frame_obj",	"first video frame, defined in mi_first_frame_t",	MI_TYPE_OBJ},
};
/* clang-format on */

static int interrupt_callback(void *data)
{
	media_info_t *mi = data;

	uint64_t ts = os_gettime_ns();
	pthread_mutex_lock(&mi->mutex);
	if (mi->open_ts > 0 && (ts - mi->open_ts) > FFMPEG_OPEN_TIMEOUT_NS) {
		pthread_mutex_unlock(&mi->mutex);
		plog(LOG_INFO, "[mi] FFmpeg interrupt for openning.");
		return true;
	}

	if (mi->io_open_ts > 0 &&
	    (ts - mi->io_open_ts) > FFMPEG_OPEN_TIMEOUT_NS) {
		pthread_mutex_unlock(&mi->mutex);
		plog(LOG_INFO, "[mi] FFmpeg interrupt for avio openning.");
		return true;
	}

	if (mi->abort) {
		pthread_mutex_unlock(&mi->mutex);
		plog(LOG_WARNING,
		     "[mi] FFmpeg interrupt (due to abort) for openning.");
		return true;
	}

	pthread_mutex_unlock(&mi->mutex);

	return false;
}

static bool mi_open_internal(media_info_t *mi)
{
	if (!mi || !mi->path)
		return false;

	mi->fmt = avformat_alloc_context();
	if (!mi->fmt) {
		plog(LOG_WARNING, "[mi] Fail to alloc avformat context");
		return false;
	}

	AVDictionary *opts = NULL;
	av_dict_set(&opts, "http_persistent", "0", 0);
	av_dict_set(&opts, "reconnect", "1", 0);
	av_dict_set(&opts, "stimeout", "3000000", 0);

	mi->fmt->interrupt_callback.callback = interrupt_callback;
	mi->fmt->interrupt_callback.opaque = mi;

	pthread_mutex_lock(&mi->mutex);
	mi->open_ts = os_gettime_ns();
	pthread_mutex_unlock(&mi->mutex);

	plog(LOG_DEBUG, "[mi] %s opening media.",
	     mi->open_mode == MI_OPEN_DEFER ? "Defer" : "Directly");
	int ret = avformat_open_input(&mi->fmt, mi->path, NULL,
				      opts ? &opts : NULL);
	pthread_mutex_lock(&mi->mutex);
	mi->open_ts = 0;
	pthread_mutex_unlock(&mi->mutex);
	av_dict_free(&opts);

	if (ret < 0) {
		plog(LOG_WARNING, "[mi] Failed to open media : [%d] '%s'", ret,
		     av_err2str(ret));
		return false;
	}

	ret = avformat_find_stream_info(mi->fmt, NULL);
	if (ret < 0) {
		plog(LOG_WARNING,
		     "[mi] Failed to find stream info for : [%d] '%s'", ret,
		     av_err2str(ret));
		return false;
	}

	mi->video_index = av_find_best_stream(mi->fmt, AVMEDIA_TYPE_VIDEO, -1,
					      -1, NULL, 0);
	mi->audio_index = av_find_best_stream(mi->fmt, AVMEDIA_TYPE_AUDIO, -1,
					      -1, NULL, 0);
	mi->opened = true;

	return true;
}

static bool check_defer_open(media_info_t *mi)
{
	if (mi->open_mode == MI_OPEN_DEFER && !mi->opened &&
	    !mi_open_internal(mi)) {
		mi_free(mi);
		return false;
	}
	return true;
}

static long long get_duration_ms(media_info_t *mi)
{
	if (!mi || !mi->fmt)
		return 0;

	if (mi->fmt->duration != AV_NOPTS_VALUE)
		return mi->fmt->duration / INT64_C(1000);
	else {
		plog(LOG_WARNING, "[mi] Invalid duration.");
		return AV_NOPTS_VALUE;
	}
}

static long long estimate_frame_count(media_info_t *mi)
{
	if (!mi || !mi->fmt)
		return 0;

	int ret = av_find_best_stream(mi->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL,
				      0);
	if (ret < 0) {
		plog(LOG_INFO, "[mi] NO video stream: [%d] '%s'", ret,
		     av_err2str(ret));
		return 0;
	}

	AVStream *stream = mi->fmt->streams[ret];

	if (stream->nb_frames)
		return stream->nb_frames;
	else if (stream->avg_frame_rate.den &&
		 mi->fmt->duration != AV_NOPTS_VALUE) {
		return (long long)ceil((double)mi->fmt->duration /
				       (double)AV_TIME_BASE *
				       (double)stream->avg_frame_rate.num /
				       (double)stream->avg_frame_rate.den);
	} else
		return stream->codec_info_nb_frames;
}

static long long get_resolution(media_info_t *mi, const char *key)
{
	if (!mi || !mi->fmt || mi->video_index < 0)
		return 0;

	if (0 == strcmp(key, "width"))
		return mi->fmt->streams[mi->video_index]->codecpar->width;
	else if (0 == strcmp(key, "height"))
		return mi->fmt->streams[mi->video_index]->codecpar->height;

	return 0;
}

static enum AVCodecID get_codec_id(media_info_t *mi, bool video)
{
	if (!mi || !mi->fmt)
		return AV_CODEC_ID_NONE;

	if (video) {
		if (mi->video_index < 0) {
			return AV_CODEC_ID_NONE;
		} else {
			return mi->fmt->streams[mi->video_index]
				->codecpar->codec_id;
		}
	} else {
		if (mi->audio_index < 0) {
			return AV_CODEC_ID_NONE;
		} else {
			return mi->fmt->streams[mi->audio_index]
				->codecpar->codec_id;
		}
	}
}

static bool has_cover(media_info_t *mi)
{
	if (!mi || !mi->fmt)
		return false;

	int ret = av_find_best_stream(mi->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL,
				      0);
	if (ret < 0) {
		plog(LOG_INFO, "[mi] NO video stream (cover): [%d] '%s'", ret,
		     av_err2str(ret));
		return false;
	}

	AVStream *stream = mi->fmt->streams[ret];
	mi->has_cover = stream->disposition & AV_DISPOSITION_ATTACHED_PIC;
	if (mi->has_cover)
		mi->cover_index = ret;
	return mi->has_cover;
}

static bool mi_try_init_decoder(media_info_t *mi, enum AVMediaType type)
{
	enum AVCodecID id;
	AVStream *stream;
	AVCodec *codec;
	AVCodecContext *c;
	int ret;

	ret = av_find_best_stream(mi->fmt, type, -1, -1, NULL, 0);
	if (ret < 0) {
		plog(LOG_INFO, "MP: No %s stream.",
		     av_get_media_type_string(type));
		return false;
	}

	stream = mi->fmt->streams[ret];
	id = stream->codecpar->codec_id;
	codec = avcodec_find_decoder(id);
	if (!codec) {
		plog(LOG_WARNING, "MP: Failed to find %s codec",
		     av_get_media_type_string(type));
		return false;
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		plog(LOG_WARNING, "MP: Failed to allocate context for %s",
		     av_get_media_type_string(type));
		return false;
	}

	ret = avcodec_parameters_to_context(c, stream->codecpar);
	if (ret < 0)
		goto fail;

	if (c->thread_count == 1 && c->codec_id != AV_CODEC_ID_PNG &&
	    c->codec_id != AV_CODEC_ID_TIFF &&
	    c->codec_id != AV_CODEC_ID_JPEG2000 &&
	    c->codec_id != AV_CODEC_ID_MPEG4 && c->codec_id != AV_CODEC_ID_WEBP)
		c->thread_count = 0;

	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0)
		plog(LOG_WARNING, "MP: Failed to open %s decoder: %s",
		     av_get_media_type_string(type), av_err2str(ret));
fail:
	avcodec_close(c);
	av_free(c);
	return ret < 0 ? false : true;
}

static bool mi_try_decoder(media_info_t *mi)
{
	bool has_video, has_audio;
	has_video = mi_try_init_decoder(mi, AVMEDIA_TYPE_VIDEO);
	has_audio = mi_try_init_decoder(mi, AVMEDIA_TYPE_AUDIO);
	return has_audio || has_video;
}

bool mi_open(media_info_t *mi, const char *path, enum mi_open_mode mode)
{
	if (!mi || !path) {
		plog(LOG_WARNING, "[mi] Null ptr : mi %p", mi);
		return false;
	}

	memset(mi, 0, sizeof(media_info_t));
	pthread_mutex_init_value(&mi->mutex);
	if (pthread_mutex_init(&mi->mutex, NULL) != 0) {
		plog(LOG_WARNING, "[mi] Failed to init mutex");
		return false;
	}

	mi->path = path ? bstrdup(path) : NULL;
	mi->open_mode = mode;
	if (mode & MI_OPEN_DIRECTLY && !mi_open_internal(mi))
		goto fail;

	if (mode & MI_OPEN_TRY_DECODER && !mi_try_decoder(mi))
		goto fail;

	return true;
fail:
	mi_free(mi);
	return false;
}

void mi_free(media_info_t *mi)
{
	if (!mi)
		return;

	pthread_mutex_lock(&mi->mutex);
	mi->abort = true;
	pthread_mutex_unlock(&mi->mutex);

	SAFE_FREE_STRING(&mi->path);
	SAFE_FREE_STRING(&mi->metadata.artist);
	SAFE_FREE_STRING(&mi->metadata.title);
	SAFE_FREE_STRING(&mi->metadata.album);

	if (mi->cover.data)
		av_freep(&mi->cover.data);

	if (mi->first_frame.data)
		av_freep(&mi->first_frame.data);

	if (mi->id3v2.data)
		bfree(mi->id3v2.data);

	avformat_close_input(&mi->fmt);
	if (mi->codec_ctx) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		avcodec_free_context(&mi->codec_ctx);
#else
		avcodec_close(mi->codec_ctx);
#endif
	}
	pthread_mutex_destroy(&mi->mutex);
	memset(mi, 0, sizeof(media_info_t));
	pthread_mutex_init_value(&mi->mutex);
}

char *mi_get_string(media_info_t *mi, const char *key)
{
	if (!mi || !key)
		return NULL;

	if (!check_defer_open(mi))
		return NULL;

	// here we only support strings inside metadata,
	// if other options needed, modify this function.
	AVDictionaryEntry *e;
	e = av_dict_get(mi->fmt->metadata, key, NULL, 0);
	if (e) {
		return e->value;
	}
	return NULL;
}

long long mi_get_int(media_info_t *mi, const char *key)
{
	if (!mi || !key)
		return 0;

	if (!check_defer_open(mi))
		return 0;

	if (0 == strcmp(key, "duration"))
		return get_duration_ms(mi);
	else if (0 == strcmp(key, "width") || 0 == strcmp(key, "height"))
		return get_resolution(mi, key);
	else if (0 == strcmp(key, "frame_count"))
		return estimate_frame_count(mi);
	else if (0 == strcmp(key, "vcodec_id"))
		return get_codec_id(mi, true);
	else if (0 == strcmp(key, "acodec_id"))
		return get_codec_id(mi, false);

	// other options could be added here

	return 0;
}

bool mi_get_bool(media_info_t *mi, const char *key)
{
	if (!mi || !key)
		return false;

	if (!check_defer_open(mi))
		return false;

	if (0 == strcmp(key, "has_cover"))
		return has_cover(mi);

	// other options could be added here

	return false;
}

static bool init_decoder(media_info_t *mi, int stream_index)
{
	AVStream *stream = mi->fmt->streams[stream_index];

	enum AVCodecID id = stream->codecpar->codec_id;
	AVCodec *codec = NULL;
	if (id == AV_CODEC_ID_VP8 || id == AV_CODEC_ID_VP9) {
		AVDictionaryEntry *tag = NULL;
		tag = av_dict_get(stream->metadata, "alpha_mode", tag,
				  AV_DICT_IGNORE_SUFFIX);

		if (tag && strcmp(tag->value, "1") == 0) {
			char *codec_str = (id == AV_CODEC_ID_VP8)
						  ? "libvpx"
						  : "libvpx-vp9";
			codec = avcodec_find_decoder_by_name(codec_str);
		}
	}

	if (!codec)
		codec = avcodec_find_decoder(id);

	if (!codec) {
		plog(LOG_WARNING, "[mi] Failed to find codec for cover");
		return false;
	}

	AVCodecContext *ctx = avcodec_alloc_context3(codec);
	if (!ctx) {
		plog(LOG_WARNING, "[mi] Failed to allocate context");
		return false;
	}

	int ret = avcodec_parameters_to_context(ctx, stream->codecpar);
	if (ret < 0) {
		plog(LOG_WARNING,
		     "[mi] Failed to pass parameters to context: %d, '%s'", ret,
		     av_err2str(ret));
		goto fail;
	}

	AVDictionary *opts = NULL;
	av_dict_set(&opts, "threads", "1", 0);
	ret = avcodec_open2(ctx, codec, &opts);
	av_dict_free(&opts);
	if (ret < 0) {
		plog(LOG_WARNING,
		     "[mi] Failed to open codec context : %d, '%s'", ret,
		     av_err2str(ret));
		goto fail;
	}

	mi->codec_ctx = ctx;
	return true;
fail:
	avcodec_close(ctx);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
	av_free(mi->codec_ctx);
	mi->codec_ctx = NULL;
#endif
	return false;
}

static bool convert_to_rgba(media_info_t *mi, AVFrame *frame, char **out,
			    int *out_size)
{
	char *buffer[MAX_AV_PLANES] = {0};
	int linesize[MAX_AV_PLANES] = {0};

	int ret = av_image_alloc(buffer, linesize, frame->width, frame->height,
				 AV_PIX_FMT_RGBA, 1);
	if (ret < 0) {
		plog(LOG_WARNING,
		     "[mi] Failed to alloc image buffer : %d, '%s'", ret,
		     av_err2str(ret));
		return false;
	}

	int size = ret;

	if (frame->format != AV_PIX_FMT_RGBA) {
		struct SwsContext *swsctx = sws_getContext(
			frame->width, frame->height, frame->format,
			frame->width, frame->height, AV_PIX_FMT_RGBA, SWS_POINT,
			NULL, NULL, NULL);
		if (!swsctx) {
			plog(LOG_WARNING,
			     "[mi] Failed to create scale context : %d, '%s'",
			     ret, av_err2str(ret));
			goto fail;
		}

		ret = sws_scale(swsctx, (const uint8_t *const *)frame->data,
				frame->linesize, 0, frame->height, buffer,
				linesize);
		sws_freeContext(swsctx);
		swsctx = NULL;
		if (ret < 0) {
			plog(LOG_WARNING,
			     "[mi] Failed to create scale context : %d, '%s'",
			     ret, av_err2str(ret));
			goto fail;
		}
	} else {
		ret = av_image_copy_to_buffer(
			buffer[0], size, (const uint8_t *const *)frame->data,
			(const int *)frame->linesize, frame->format,
			frame->width, frame->height, 1);
		if (ret < 0) {
			plog(LOG_WARNING,
			     "[mi] Can not copy image to buffer : %d, '%s'",
			     ret, av_err2str(ret));
			goto fail;
		}
	}

	*out = buffer[0];
	*out_size = size;

	return true;
fail:
	av_freep(&buffer);
	return false;
}

static bool decode_video(media_info_t *mi, int stream_index,
			 struct mi_video_frame *out)
{
	AVPacket packet;
	av_init_packet(&packet);

	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		plog(LOG_WARNING, "[mi] Failed to alloc cover frame");
		return false;
	}
	bool got_frame = 0;
	bool eof = false;
	while (!got_frame) {
		int ret = 0;
		if (!eof) {
			ret = av_read_frame(mi->fmt, &packet);
			if (ret < 0) {
				if (ret == AVERROR_EOF) {
					eof = true;
					/* flush the decoder */
					packet.data = NULL;
					packet.size = 0;
					packet.stream_index = stream_index;
				} else {
					plog(LOG_WARNING,
					     "[mi] Failed to read image frame : %d, '%s'",
					     ret, av_err2str(ret));
					goto fail;
				}
			}
		}

		if (packet.stream_index == stream_index) {
			avcodec_send_packet(mi->codec_ctx, &packet);
			ret = avcodec_receive_frame(mi->codec_ctx, frame);
			if (ret < 0) {
				if (ret != AVERROR(EAGAIN)) {
					plog(LOG_WARNING,
					     "[mi] Failed to decode frame : %d, '%s'",
					     ret, av_err2str(ret));
					goto fail;
				} else if (eof) {
					plog(LOG_WARNING,
					     "[mi] Can not decode one frame before EOF.");
					goto fail;
				}

				continue;
			} else {
				got_frame = 1;
			}
		}
	}

	if (out->data)
		av_freep(&out->data);
	memset(out, 0, sizeof(*out));

	bool ret = convert_to_rgba(mi, frame, &out->data, &out->size);
	out->width = frame->width;
	out->height = frame->height;
	out->format = AV_PIX_FMT_RGBA;

fail:
	av_packet_unref(&packet);
	av_frame_free(&frame);
	if (got_frame && ret == true) {
		return true;
	} else {
		return false;
	}
}

static mi_obj get_cover(media_info_t *mi)
{
	if (!has_cover(mi))
		return NULL;

	for (int i = 0; i < mi->fmt->nb_streams; ++i) {
		if (i == mi->cover_index)
			mi->fmt->streams[i]->discard = AVDISCARD_DEFAULT;
		else
			mi->fmt->streams[i]->discard = AVDISCARD_ALL;
	}

	if (!init_decoder(mi, mi->cover_index))
		return NULL;

	bool ret = decode_video(mi, mi->cover_index, &mi->cover);

	if (mi->codec_ctx) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		avcodec_free_context(&mi->codec_ctx);
#else
		avcodec_close(mi->codec_ctx);
#endif
		mi->codec_ctx = NULL;
	}

	return ret ? &mi->cover : NULL;
}

static mi_obj get_first_frame(media_info_t *mi)
{
	if (mi->video_index < 0)
		return NULL;

	for (int i = 0; i < mi->fmt->nb_streams; ++i) {
		if (i == mi->video_index)
			mi->fmt->streams[i]->discard = AVDISCARD_DEFAULT;
		else
			mi->fmt->streams[i]->discard = AVDISCARD_ALL;
	}

	if (!init_decoder(mi, mi->video_index))
		return NULL;

	bool ret = decode_video(mi, mi->video_index, &mi->first_frame);

	if (mi->codec_ctx) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		avcodec_free_context(&mi->codec_ctx);
#else
		avcodec_close(mi->codec_ctx);
#endif
		mi->codec_ctx = NULL;
	}

	return ret ? &mi->first_frame : NULL;
}

static mi_obj get_metadata(media_info_t *mi)
{
	char *title = mi_get_string(mi, "title");
	char *artist = mi_get_string(mi, "artist");
	char *album = mi_get_string(mi, "album");
	mi->metadata.title = title ? bstrdup(title) : NULL;
	mi->metadata.artist = artist ? bstrdup(artist) : NULL;
	mi->metadata.album = album ? bstrdup(album) : NULL;

	return &mi->metadata;
}

static bool id3v2_match(const uint8_t *buf, const char *magic)
{
	return buf[0] == magic[0] && buf[1] == magic[1] && buf[2] == magic[2] &&
	       buf[3] != 0xff && buf[4] != 0xff && (buf[6] & 0x80) == 0 &&
	       (buf[7] & 0x80) == 0 && (buf[8] & 0x80) == 0 &&
	       (buf[9] & 0x80) == 0;
}

static mi_obj get_id3v2_from_ffmpeg(media_info_t *mi)
{
	bool succeed = false;
	char *header = NULL;
	char *buffer = NULL;
	AVIOContext *io = NULL;
	AVIOInterruptCB io_interrupt_cb = {interrupt_callback, mi};

	pthread_mutex_lock(&mi->mutex);
	mi->io_open_ts = os_gettime_ns();
	pthread_mutex_unlock(&mi->mutex);
	int ret = avio_open2(&io, mi->path, AVIO_FLAG_READ, &io_interrupt_cb,
			     NULL);
	pthread_mutex_lock(&mi->mutex);
	mi->io_open_ts = 0;
	pthread_mutex_unlock(&mi->mutex);
	if (ret < 0) {
		plog(LOG_WARNING, "[mi] Fail to open : %s.\n", av_err2str(ret));
		goto fail;
	}

	header = bmalloc(ID3V2_HEADER_SIZE);
	if (!header) {
		plog(LOG_WARNING, "[mi] Fail to malloc ID3V2 header buffer.");
		goto fail;
	}

	ret = avio_read(io, header, ID3V2_HEADER_SIZE);
	if (ret < ID3V2_HEADER_SIZE) {
		plog(LOG_WARNING, "[mi] Fail to read ID3V2 header, ret %d.",
		     ret);
		goto fail;
	}

	if (!id3v2_match(header, ID3v2_DEFAULT_MAGIC)) {
		plog(LOG_WARNING, "[mi] Not an ID3V2 header.");
		goto fail;
	}

	size_t body_size = ((header[6] & 0x7f) << 21) |
			   ((header[7] & 0x7f) << 14) |
			   ((header[8] & 0x7f) << 7) | (header[9] & 0x7f);
	if (body_size <= 0) {
		plog(LOG_WARNING, "[mi] Wrong body size.");
		goto fail;
	}

	size_t size = body_size + ID3V2_HEADER_SIZE;
	buffer = bmalloc(size);
	if (!buffer) {
		plog(LOG_WARNING, "[mi] Fail to malloc ID3V2 body buffer.");
		goto fail;
	}
	memset(buffer, 0, size);
	memmove(buffer, header, ID3V2_HEADER_SIZE);

	ret = avio_read(io, buffer + ID3V2_HEADER_SIZE, body_size);
	if (ret < body_size) {
		plog(LOG_WARNING, "[mi] Fail to read ID3V2 body, ret %d.", ret);
		goto fail;
	}
	mi->id3v2.data = buffer;
	mi->id3v2.size = size;
	plog(LOG_INFO, "[mi] Succeed to get ID3 : version %d, size %d.",
	     header[3], mi->id3v2.size);
	succeed = true;
	mi->id3v2_ready = true;
fail:
	avio_closep(&io);
	if (header)
		bfree(header);

	// buffer is reused for mi->id3v2.data if succeed
	if (succeed)
		return &mi->id3v2;

	if (buffer)
		bfree(buffer);
	return NULL;
}

static mi_obj get_id3v2(media_info_t *mi)
{
	// currently, we only support mp3 file
	if (!mi->path || NULL == astrstri(mi->path, "mp3")) {
		plog(LOG_WARNING, "[mi] Not a mp3 file.");
		return NULL;
	}

	if (mi->id3v2_ready)
		return &mi->id3v2;

	return get_id3v2_from_ffmpeg(mi);
}

mi_obj mi_get_obj(media_info_t *mi, const char *key)
{
	if (!mi || !key)
		return NULL;

	if (0 != strcmp(key, "id3v2_obj") && !check_defer_open(mi))
		return NULL;

	if (0 == strcmp(key, "cover_obj"))
		return get_cover(mi);
	else if (0 == strcmp(key, "metadata_obj"))
		return get_metadata(mi);
	else if (0 == strcmp(key, "id3v2_obj"))
		return get_id3v2(mi);
	else if (0 == strcmp(key, "first_frame_obj"))
		return get_first_frame(mi);

	return NULL;
}

void mi_send_id3v2(mi_id3v2_t *id3v2)
{
	if (!id3v2 || !id3v2->data || id3v2->size <= 0)
		return;

	mi_id3v2_t id3 = {0};
	id3.data = bmalloc(id3v2->size);
	if (!id3.data) {
		plog(LOG_WARNING,
		     "[mi] Fail to malloc new ID3V2 buffer, size %lld.",
		     id3v2->size);
		return;
	}
	memmove(id3.data, id3v2->data, id3v2->size);
	id3.size = id3v2->size;

	struct obs_core_audio *audio = &obs->audio;
	pthread_mutex_lock(&audio->id3v2_mutex);
	da_push_back(audio->id3v2_array, &id3);
	pthread_mutex_unlock(&audio->id3v2_mutex);
}

bool mi_get_id3v2(mi_id3v2_t *id3v2)
{
	if (!id3v2)
		return false;

	bool ret = false;
	struct obs_core_audio *audio = &obs->audio;
	pthread_mutex_lock(&audio->id3v2_mutex);
	if (audio->id3v2_array.num > 0) {
		struct mi_id3v2 id3 = audio->id3v2_array.array[0];
		id3v2->data = id3.data;
		id3v2->size = id3.size;
		da_erase(audio->id3v2_array, 0);
		ret = true;
	} else
		ret = false;
	pthread_mutex_unlock(&audio->id3v2_mutex);
	return ret;
}

void mi_free_id3v2(mi_id3v2_t *id3v2)
{
	if (!id3v2)
		return;
	if (id3v2->data)
		bfree(id3v2->data);
	id3v2->size = 0;
}

void mi_clear_id3v2_queue()
{
	struct obs_core_audio *audio = &obs->audio;
	pthread_mutex_lock(&audio->id3v2_mutex);
	for (int i = 0; i < audio->id3v2_array.num; i++) {
		struct mi_id3v2 id3 = audio->id3v2_array.array[i];
		if (id3.data)
			bfree(id3.data);
	}
	da_free(audio->id3v2_array);
	pthread_mutex_unlock(&audio->id3v2_mutex);
}

bool mi_peek_id3v2(mi_id3v2_t *id3v2)
{
	if (!id3v2)
		return false;

	bool ret = false;
	struct obs_core_audio *audio = &obs->audio;
	pthread_mutex_lock(&audio->id3v2_mutex);
	if (audio->id3v2_array.num > 0) {
		struct mi_id3v2 id3 = audio->id3v2_array.array[0];
		id3v2->data = id3.data;
		id3v2->size = id3.size;
		ret = true;
	} else
		ret = false;
	pthread_mutex_unlock(&audio->id3v2_mutex);
	return ret;
}

bool mi_del_id3v2(mi_id3v2_t *id3v2)
{
	if (!id3v2)
		return false;

	bool ret = false;
	struct obs_core_audio *audio = &obs->audio;
	pthread_mutex_lock(&audio->id3v2_mutex);
	for (int i = 0; i < audio->id3v2_array.num; i++) {
		struct mi_id3v2 id3 = audio->id3v2_array.array[i];
		if (id3.data == id3v2->data && id3.size == id3v2->size) {
			da_erase(audio->id3v2_array, i);
			ret = true;
			break;
		}
	}
	pthread_mutex_unlock(&audio->id3v2_mutex);
	return ret;
}

bool mi_id3v2_queued()
{
	bool queued = false;
	struct obs_core_audio *audio = &obs->audio;
	pthread_mutex_lock(&audio->id3v2_mutex);
	queued = audio->id3v2_array.num > 0;
	pthread_mutex_unlock(&audio->id3v2_mutex);
	return queued;
}

static inline bool init_input(media_remux_t *mr, const char *in_filename)
{
	struct media_info *mi = bzalloc(sizeof(struct media_info));
	if (!mi_open(mi, in_filename, MI_OPEN_DIRECTLY)) {
		return false;
	}

	mi_obj obj = mi_get_obj(mi, "first_frame_obj");
	if (!obj) {
		mi_free(mi);
		return false;
	}

#ifndef _NDEBUG
	av_dump_format(mi->fmt, 0, in_filename, false);
#endif
	int stream_index = av_find_default_stream_index(mi->fmt);
	AVStream *stream = mi->fmt->streams[stream_index];
	int seek_flags;
	if (mi->fmt->duration == AV_NOPTS_VALUE)
		seek_flags = AVSEEK_FLAG_FRAME;
	else
		seek_flags = AVSEEK_FLAG_BACKWARD;

	int ret = avformat_seek_file(mi->fmt, stream_index, INT64_MIN,
				     0 + stream->start_time, INT64_MAX,
				     seek_flags);

	if (ret < 0) {
		plog(LOG_WARNING, "[mi] remux: Failed to seek: %s",
		     av_err2str(ret));
		return false;
	}

	da_push_back(mr->mis, &mi);
	return true;
}

static inline bool init_output(media_remux_t *mr, const char *out_filename)
{
	bool ret = false;
	avformat_alloc_output_context2(&mr->ofmt_ctx, NULL, NULL, out_filename);
	if (!mr->ofmt_ctx) {
		plog(LOG_ERROR, "[mi] remux : Could not create output context");
		return ret;
	}

	for (unsigned i = 0; i < mr->mis.array[0]->fmt->nb_streams; i++) {
		AVStream *in_stream = mr->mis.array[0]->fmt->streams[i];
		AVStream *out_stream = avformat_new_stream(
			mr->ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			plog(LOG_ERROR, "[mi] remux : Failed to allocate output"
					" stream");
			return false;
		}
		out_stream->avg_frame_rate = (AVRational){mr->fps, 1};
		out_stream->time_base = (AVRational){1, 1000000000};

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
		AVCodecParameters *par = avcodec_parameters_alloc();
		ret = avcodec_parameters_from_context(par, in_stream->codec);
		if (ret == 0)
			ret = avcodec_parameters_to_context(out_stream->codec,
							    par);
		avcodec_parameters_free(&par);
#else
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
#endif

		if (ret < 0) {
			plog(LOG_ERROR, "[mi] remux : Failed to copy context");
			return false;
		}

		av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);

		out_stream->codec->codec_tag = 0;
		if (mr->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_H;
	}

#ifndef _NDEBUG
	av_dump_format(mr->ofmt_ctx, 0, out_filename, true);
#endif

	if (!(mr->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&mr->ofmt_ctx->pb, out_filename,
				AVIO_FLAG_WRITE);
		if (ret < 0) {
			plog(LOG_ERROR, "[mi] remux : Failed to open output.");
			return false;
		}
	}
	return true;
}

static bool valid_extension(const char *ext)
{
	if (!ext)
		return false;
	return astrcmpi(ext, ".bmp") == 0 || astrcmpi(ext, ".tga") == 0 ||
	       astrcmpi(ext, ".png") == 0 || astrcmpi(ext, ".jpeg") == 0 ||
	       astrcmpi(ext, ".jpg") == 0 || astrcmpi(ext, ".gif") == 0;
}

static inline bool init_files(media_remux_t *mr, const char *in_files,
			      const char *out_filename)
{
	os_dir_t *dir = os_opendir(in_files);
	if (dir) {
		struct dstr dir_path = {0};
		struct os_dirent *ent;

		for (;;) {
			const char *ext;

			ent = os_readdir(dir);
			if (!ent)
				break;
			if (ent->directory)
				continue;

			ext = os_get_path_extension(ent->d_name);
			if (!valid_extension(ext))
				continue;

			dstr_copy(&dir_path, in_files);
			dstr_cat_ch(&dir_path, '/');
			dstr_cat(&dir_path, ent->d_name);
			init_input(mr, dir_path.array);
		}

		dstr_free(&dir_path);
		os_closedir(dir);
	} else {
		init_input(mr, in_files);
	}

	if (mr->mis.num <= 0) {
		plog(LOG_ERROR, "[mi] remux : No input context");
		return false;
	}

	return init_output(mr, out_filename);
}

static bool remux_internal(media_remux_t *mr)
{
	int ret = avformat_write_header(mr->ofmt_ctx, NULL);
	if (ret < 0) {
		plog(LOG_ERROR, "[mi] remux : Error opening output file: %s",
		     av_err2str(ret));
		return false;
	}

	AVPacket pkt;
	bool success = false;
	for (int i = 0; i < mr->mis.num; i++) {
		for (;;) {
			AVFormatContext *ctx = mr->mis.array[i]->fmt;
			ret = av_read_frame(ctx, &pkt);
			if (ret < 0) {
				if (ret != AVERROR_EOF) {
					plog(LOG_ERROR,
					     "[mi] remux : Error reading"
					     " packet: %s",
					     av_err2str(ret));
				}
				break;
			}

			pkt.pts = mr->ts;
			pkt.dts = mr->ts;
			mr->ts += (1000000000 / mr->fps);

			ret = av_interleaved_write_frame(mr->ofmt_ctx, &pkt);
			av_packet_unref(&pkt);

			if (ret < 0) {
				plog(LOG_ERROR,
				     "[mi] remux : Error muxing packet: %s",
				     av_err2str(ret));
				break;
			}
		}
		success = ret >= 0 || ret == AVERROR_EOF;
		if (!success)
			break;
	}

	ret = av_write_trailer(mr->ofmt_ctx);
	if (ret < 0) {
		plog(LOG_ERROR, "[mi] remux : av_write_trailer: %s",
		     av_err2str(ret));
		return false;
	}

	return success;
}

void mi_remux_free(media_remux_t *mr)
{
	if (!mr)
		return;

	if (mr->ofmt_ctx && !(mr->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_close(mr->ofmt_ctx->pb);

	if (mr->ofmt_ctx)
		avformat_free_context(mr->ofmt_ctx);

	for (int i = 0; i < mr->mis.num; i++) {
		mi_free(mr->mis.array[i]);
	}

	da_free(mr->mis);
}

bool mi_remux_do(const char *in_path, const char *out_filename,
		 unsigned int fps)
{
	if (!in_path || !out_filename || !strlen(in_path) ||
	    !strlen(out_filename))
		return false;

	bool success = false;
	media_remux_t mr;
	memset(&mr, 0, sizeof(media_remux_t));
	mr.ts = 0;
	mr.fps = fps;

	if (!init_files(&mr, in_path, out_filename)) {
		success = false;
		goto free;
	}

	success = remux_internal(&mr);
	if (!success && os_is_file_exist(out_filename))
		remove(out_filename);

free:
	mi_remux_free(&mr);
	return success;
}
