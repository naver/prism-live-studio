#include "graphics.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <util/platform.h>

#include "../obs-ffmpeg-compat.h"

struct ffmpeg_image {
	const char *file;
	AVFormatContext *fmt_ctx;
	AVCodecContext *decoder_ctx;
	AVCodec *decoder;
	AVStream *stream;
	int stream_idx;

	int cx, cy;
	enum AVPixelFormat format;
};

static bool ffmpeg_image_open_decoder_context(struct ffmpeg_image *info)
{
	int ret = av_find_best_stream(info->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, 1,
				      NULL, 0);
	if (ret < 0) {
		plog(LOG_WARNING, "Couldn't find video stream : %s",
		     av_err2str(ret));
		return false;
	}

	info->stream_idx = ret;
	info->stream = info->fmt_ctx->streams[ret];
	info->decoder_ctx = info->stream->codec;
	info->decoder = avcodec_find_decoder(info->decoder_ctx->codec_id);

	if (!info->decoder) {
		plog(LOG_WARNING, "Failed to find decoder.");
		return false;
	}

	ret = avcodec_open2(info->decoder_ctx, info->decoder, NULL);
	if (ret < 0) {
		plog(LOG_WARNING,
		     "Failed to open video codec : "
		     "%s",
		     av_err2str(ret));
		return false;
	}

	return true;
}

static void ffmpeg_image_free(struct ffmpeg_image *info)
{
	avcodec_close(info->decoder_ctx);
	avformat_close_input(&info->fmt_ctx);
}

static bool ffmpeg_image_init(struct ffmpeg_image *info, const char *file)
{
	int ret;

	if (!file || !*file)
		return false;

	memset(info, 0, sizeof(struct ffmpeg_image));
	info->file = file;
	info->stream_idx = -1;

	ret = avformat_open_input(&info->fmt_ctx, file, NULL, NULL);
	if (ret < 0) {
		plog(LOG_WARNING, "Failed to open file : %s", av_err2str(ret));
		return false;
	}

	ret = avformat_find_stream_info(info->fmt_ctx, NULL);
	if (ret < 0) {
		plog(LOG_WARNING,
		     "Could not find stream info for file :"
		     " %s",
		     av_err2str(ret));
		goto fail;
	}

	if (!ffmpeg_image_open_decoder_context(info))
		goto fail;

	info->cx = info->decoder_ctx->width;
	info->cy = info->decoder_ctx->height;
	info->format = info->decoder_ctx->pix_fmt;
	return true;

fail:
	ffmpeg_image_free(info);
	return false;
}

static bool ffmpeg_image_reformat_frame(struct ffmpeg_image *info,
					AVFrame *frame, uint8_t *out,
					int linesize)
{
	struct SwsContext *sws_ctx = NULL;
	int ret = 0;

	if (info->format == AV_PIX_FMT_RGBA ||
	    info->format == AV_PIX_FMT_BGRA ||
	    info->format == AV_PIX_FMT_BGR0) {

		if (linesize != frame->linesize[0]) {
			int min_line = linesize < frame->linesize[0]
					       ? linesize
					       : frame->linesize[0];

			for (int y = 0; y < info->cy; y++)
				memcpy(out + y * linesize,
				       frame->data[0] + y * frame->linesize[0],
				       min_line);
		} else {
			memcpy(out, frame->data[0], linesize * info->cy);
		}

	} else {
		//PRISM/LiuHaibin/20210118/#None/Merge from OBS https://github.com/obsproject/obs-studio/pull/2836
		static const enum AVPixelFormat format = AV_PIX_FMT_BGRA;

		sws_ctx = sws_getContext(info->cx, info->cy, info->format,
					 info->cx, info->cy, format, SWS_POINT,
					 NULL, NULL, NULL);
		if (!sws_ctx) {
			plog(LOG_WARNING, "Failed to create scale context.");
			return false;
		}

		uint8_t *pointers[4];
		int linesizes[4];
		ret = av_image_alloc(pointers, linesizes, info->cx, info->cy,
				     format, 32);
		if (ret < 0) {
			plog(LOG_WARNING, "av_image_alloc failed : %s",
			     av_err2str(ret));
			sws_freeContext(sws_ctx);
			return false;
		}

		ret = sws_scale(sws_ctx, (const uint8_t *const *)frame->data,
				frame->linesize, 0, info->cy, pointers,
				linesizes);
		sws_freeContext(sws_ctx);

		if (ret < 0) {
			plog(LOG_WARNING, "sws_scale failed for : %s",
			     av_err2str(ret));
			av_freep(pointers);
			return false;
		}

		for (size_t y = 0; y < (size_t)info->cy; y++)
			memcpy(out + y * linesize,
			       pointers[0] + y * linesizes[0], linesize);

		av_freep(pointers);

		info->format = format;
	}

	return true;
}

static bool ffmpeg_image_decode(struct ffmpeg_image *info, uint8_t *out,
				int linesize)
{
	AVPacket packet = {0};
	bool success = false;
	AVFrame *frame = av_frame_alloc();
	int got_frame = 0;
	int ret;

	if (!frame) {
		plog(LOG_WARNING, "Failed to create frame data.");
		return false;
	}

	ret = av_read_frame(info->fmt_ctx, &packet);
	if (ret < 0) {
		plog(LOG_WARNING, "Failed to read image frame : %s",
		     av_err2str(ret));
		goto fail;
	}

	while (!got_frame) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 40, 101)
		ret = avcodec_send_packet(info->decoder_ctx, &packet);
		if (ret == 0)
			ret = avcodec_receive_frame(info->decoder_ctx, frame);

		got_frame = (ret == 0);

		if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			ret = 0;
#else
		ret = avcodec_decode_video2(info->decoder_ctx, frame,
					    &got_frame, &packet);
#endif
		if (ret < 0) {
			plog(LOG_WARNING, "Failed to decode frame : %s",
			     av_err2str(ret));
			goto fail;
		}
	}

	//PRISM/WangShaohui/20210915/#9682/check resolution
	if (info->cx != frame->width || info->cy != frame->height) {
		success = false;

		char name[260] = {0};
		os_extract_file_name(info->file, name, ARRAY_SIZE(name));

		plog(LOG_WARNING,
		     "Resolution incorrect with %s! %dx%d in decode_context while %dx%x in AVFrame",
		     name, info->cx, info->cy, frame->width, frame->height);
	} else {
		success =
			ffmpeg_image_reformat_frame(info, frame, out, linesize);
	}

fail:
	av_packet_unref(&packet);
	av_frame_free(&frame);
	return success;
}

void gs_init_image_deps(void)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif
}

void gs_free_image_deps(void) {}

static inline enum gs_color_format convert_format(enum AVPixelFormat format)
{
	switch ((int)format) {
	case AV_PIX_FMT_RGBA:
		return GS_RGBA;
	case AV_PIX_FMT_BGRA:
		return GS_BGRA;
	case AV_PIX_FMT_BGR0:
		return GS_BGRX;
	}

	return GS_BGRX;
}

uint8_t *gs_create_texture_file_data(const char *file,
				     enum gs_color_format *format,
				     uint32_t *cx_out, uint32_t *cy_out)
{
	struct ffmpeg_image image;
	uint8_t *data = NULL;
	enum AVPixelFormat ffmpeg_format;

	if (ffmpeg_image_init(&image, file)) {
		//PRISM/WangShaohui/20210427/NoIssue/fix breakpoint
		//data = bmalloc(image.cx * image.cy * 4);
		MALLOC(data, image.cx * image.cy * 4);
		if (data) {
			if (ffmpeg_image_decode(&image, data, image.cx * 4)) {
				ffmpeg_format = image.format;
				*format = convert_format(image.format);
				*cx_out = (uint32_t)image.cx;
				*cy_out = (uint32_t)image.cy;
			} else {
				bfree(data);
				data = NULL;
			}
		}

		ffmpeg_image_free(&image);
	}

	//PRISM/WangShaohui/20210802/NoIssue/scale large image
	struct SwsContext *sws_ctx = NULL;
	do {
		if (!data) {
			break;
		}

		uint32_t cx = 0;
		uint32_t cy = 0;
		if (!gs_image_convert_resolution(*cx_out, *cy_out, &cx, &cy)) {
			break;
		}

		sws_ctx = sws_getContext(*cx_out, *cy_out, ffmpeg_format, cx,
					 cy, ffmpeg_format, SWS_BICUBIC, NULL,
					 NULL, NULL);
		if (!sws_ctx) {
			plog(LOG_WARNING,
			     "[scale-image] Failed to create scale context. %dx%d",
			     *cx_out, *cy_out);
			break;
		}

		uint8_t *dest_buffer = NULL;
		MALLOC(dest_buffer, cx * cy * 4);
		if (!dest_buffer) {
			plog(LOG_WARNING,
			     "[scale-image] Failed to malloc memory");
			break;
		}

		uint8_t *src_data[AV_NUM_DATA_POINTERS] = {data};
		int src_linesize[AV_NUM_DATA_POINTERS] = {(*cx_out) * 4};

		uint8_t *dest_data[AV_NUM_DATA_POINTERS] = {dest_buffer};
		int dest_linesizes[AV_NUM_DATA_POINTERS] = {cx * 4};

		int ret = sws_scale(sws_ctx, src_data, src_linesize, 0, *cy_out,
				    dest_data, dest_linesizes);
		if (ret < 0) {
			plog(LOG_WARNING,
			     "[scale-image] Failed to scale image. error:%d",
			     ret);
			bfree(dest_buffer);
			break;
		}

		bfree(data);

		data = dest_buffer;
		*cx_out = cx;
		*cy_out = cy;

	} while (false);
	if (sws_ctx) {
		sws_freeContext(sws_ctx);
	}

	return data;
}

//PRISM/WangShaohui/20210802/NoIssue/scale large image
bool gs_image_convert_resolution(uint32_t src_cx, uint32_t src_cy,
				 uint32_t *dest_cx, uint32_t *dest_cy)
{
	uint32_t sz = max(src_cx, src_cy);
	if (sz <= MAX_IMAGE_RESOLUTIN) {
		return false;
	} else {
		float scale = (float)(MAX_IMAGE_RESOLUTIN) / (float)sz;
		*dest_cx = (uint32_t)(scale * (float)src_cx);
		*dest_cy = (uint32_t)(scale * (float)src_cy);
		return true;
	}
}
