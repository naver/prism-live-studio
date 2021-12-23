#pragma once

#include "obs.h"
#include <util/threading.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* crash writer */

enum cw_thread_type {
	CW_CAPTURE_VIDEO = 1,
	CW_CAPTURE_AUDIO,
	CW_PUSH_VIDEO,
	CW_PUSH_AUDIO,
	CW_RECONNECT_VIDEO,
	CW_RECONNECT_AUDIO
};

enum cw_crash_type {
	CW_DEVICE = 1,
};

typedef struct crash_writer {
	char *guid;
	char *module_name;
	char *source_name;
	char *source_id;
	bool invoking;
	bool enumerating;
	unsigned long enumThreadId;
	unsigned long invokeThreadId;
	obs_data_array_t *threads;
	pthread_mutex_t mutex;
	enum cw_crash_type type;
} crash_writer_t;

EXPORT void cw_create(crash_writer_t *writer, enum cw_crash_type type);

EXPORT void cw_free(crash_writer_t *writer);

EXPORT void cw_update_module_info(crash_writer_t *writer,
				  const char *crash_module_name,
				  const char *source_name,
				  const char *source_id);

EXPORT void cw_update_thread_id(crash_writer_t *writer, unsigned long value,
				enum cw_thread_type type);

EXPORT void cw_remove_threads(crash_writer_t *writer);

EXPORT void cw_remove_thread_id(crash_writer_t *writer, unsigned long value);

EXPORT void cw_remove_thread_id_by_type(crash_writer_t *writer,
					enum cw_thread_type type);

EXPORT void cw_update_invoke_mark(crash_writer_t *writer, bool invoking);

EXPORT void cw_update_enum_mark(crash_writer_t *writer, bool enumerating);

EXPORT void cw_set_file_mutex_uuid(const char *uuid);

#ifdef __cplusplus
}
#endif
