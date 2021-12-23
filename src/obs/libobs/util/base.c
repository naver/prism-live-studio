/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#include "c99defs.h"
#include "base.h"

#ifdef _DEBUG
static int log_output_level = LOG_DEBUG;
#else
static int log_output_level = LOG_INFO;
#endif

static int crashing = 0;
static void *log_param = NULL;
static void *crash_param = NULL;

static int LOG_DISAPPEAR = -100;

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
static void def_log_handler_ex(bool kr, int log_level, const char *format,
			       va_list args, const char *fields[][2],
			       int field_count, void *param)
{
	char out[4096];
	vsnprintf(out, sizeof(out), format, args);

	if (log_level <= log_output_level) {
		switch (log_level) {
		case LOG_DEBUG:
			fprintf(stdout, "debug: %s\n", out);
			fflush(stdout);
			break;

		case LOG_INFO:
			fprintf(stdout, "info: %s\n", out);
			fflush(stdout);
			break;

		case LOG_WARNING:
			fprintf(stdout, "warning: %s\n", out);
			fflush(stdout);
			break;

		case LOG_ERROR:
			fprintf(stderr, "error: %s\n", out);
			fflush(stderr);
		}
	}

	UNUSED_PARAMETER(kr);
	UNUSED_PARAMETER(fields);
	UNUSED_PARAMETER(field_count);
	UNUSED_PARAMETER(param);
}

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
static void def_log_handler(int log_level, const char *format, va_list args,
			    void *param)
{
	def_log_handler_ex(false, log_level, format, args, NULL, 0, param);
}

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#else
#define NORETURN __attribute__((noreturn))
#endif

NORETURN static void def_crash_handler(const char *format, va_list args,
				       void *param)
{
	vfprintf(stderr, format, args);
	exit(0);

	UNUSED_PARAMETER(param);
}

static log_handler_t log_handler = def_log_handler;
static void (*crash_handler)(const char *, va_list, void *) = def_crash_handler;

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
static log_handler_ex_t log_handler_ex = def_log_handler_ex;
static void *log_param_ex = NULL;

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
static void call_log_handler(bool kr, int log_level, const char *format,
			     va_list args, const char *fields[][2],
			     int field_count, void *param)
{
	log_handler(log_level, format, args, log_param);

	UNUSED_PARAMETER(fields);
	UNUSED_PARAMETER(field_count);
	UNUSED_PARAMETER(param);
}

void base_get_log_handler(log_handler_t *handler, void **param)
{
	if (handler)
		*handler = log_handler;
	if (param)
		*param = log_param;
}

void base_set_log_handler(log_handler_t handler, void *param)
{
	if (!handler)
		handler = def_log_handler;

	log_param = param;
	log_handler = handler;

	//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
	base_set_log_handler_ex(call_log_handler, NULL);
}

void base_set_crash_handler(void (*handler)(const char *, va_list, void *),
			    void *param)
{
	crash_param = param;
	crash_handler = handler;
}

void bcrash(const char *format, ...)
{
	va_list args;

	if (crashing) {
		fputs("Crashed in the crash handler", stderr);
		exit(2);
	}

	crashing = 1;
	va_start(args, format);
	crash_handler(format, args, crash_param);
	va_end(args);
}

#ifndef _DEBUG
void blogva(int log_level, const char *format, va_list args)
{
	///PRISM/Xiewei/20210817/#None/Does nothing in blogva to forbid third-party plugin logs.
	UNUSED_PARAMETER(log_level);
	UNUSED_PARAMETER(format);
	UNUSED_PARAMETER(args);
}
#endif

///PRISM/Xiewei/20210817/#None/Add new plogva to replace blogva.
void plogva(int log_level, const char *format, va_list args)
{
	//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
	blogvaex(log_level, format, args, NULL, 0);
}

#ifndef _DEBUG
void blog(int log_level, const char *format, ...)
{
	//PRISM/Xiewei/20210817/#None/Does nothing in plog to forbid third-party plugin logs.
	UNUSED_PARAMETER(log_level);
	UNUSED_PARAMETER(format);
}
#endif

///PRISM/Xiewei/20210817/#None/Add new plog to replace blog.
void plog(int log_level, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	plogva(log_level, format, args);
	va_end(args);
}

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
void base_get_log_handler_ex(log_handler_ex_t *handler, void **param)
{
	if (handler)
		*handler = log_handler_ex;
	if (param)
		*param = log_param_ex;
}

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
void base_set_log_handler_ex(log_handler_ex_t handler, void *param)
{
	if (!handler)
		handler = def_log_handler_ex;

	log_param_ex = param;
	log_handler_ex = handler;
}

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
void blogvaex(int log_level, const char *format, va_list args,
	      const char *fields[][2], int field_count)
{
	log_handler_ex(false, log_level, format, args, fields, field_count,
		       log_param_ex);
}

//PRISM/Zhangdewen/20210218/#/extend log support nelo fields
void blogex(bool kr, int log_level, const char *fields[][2], int field_count,
	    const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_handler_ex(kr, log_level, format, args, fields, field_count,
		       log_param_ex);
	va_end(args);
}

void bdisappear(const char *process_name, const char *pid, const char *src)
{
	const char *fields[][2] = {
		{"process", process_name}, {"pid", pid}, {"src", src}};
	blogex(false, LOG_DISAPPEAR, fields, 3, NULL);
}
