#include "crash-writer.h"
#include "obs.h"
#include "obs-internal.h"
#include "util/platform.h"

#define name2str(name) (#name)

static char config_path[512] = {0};
static obs_data_t *jsonData = NULL;
static obs_data_t *currentObj = NULL;
static obs_data_array_t *modules = NULL;
static char *fileMutexId = NULL;

static char *convert_thread_type(enum cw_thread_type type)
{
	switch (type) {
	case CW_CAPTURE_VIDEO:
		return name2str(CW_CAPTURE_VIDEO);
	case CW_CAPTURE_AUDIO:
		return name2str(CW_CAPTURE_AUDIO);
	case CW_PUSH_VIDEO:
		return name2str(CW_PUSH_VIDEO);
	case CW_PUSH_AUDIO:
		return name2str(CW_PUSH_AUDIO);
	case CW_RECONNECT_VIDEO:
		return name2str(CW_RECONNECT_VIDEO);
	case CW_RECONNECT_AUDIO:
		return name2str(CW_RECONNECT_AUDIO);
	default:
		break;
	}
	return "";
}

static void update_data_to_module(crash_writer_t *writer, obs_data_t *module)
{
	if (!writer || !module) {
		return;
	}

	obs_data_set_string(module, "guid", writer->guid);
	obs_data_set_string(module, "sourceName", writer->source_name);
	obs_data_set_string(module, "sourceId", writer->source_id);
	obs_data_set_string(module, "moduleName", writer->module_name);
	obs_data_set_bool(module, "invoking", writer->invoking);
	obs_data_set_bool(module, "enumerating", writer->enumerating);
	obs_data_set_int(module, "invokeThreadId", writer->invokeThreadId);
	obs_data_set_int(module, "enumThreadId", writer->enumThreadId);
	obs_data_set_int(module, "type", writer->type);
	obs_data_set_array(module, "threads", writer->threads);
}

static bool save_data_start()
{
	jsonData = obs_data_create_from_json_file_safe(config_path, "bak");
	if (!jsonData)
		return false;

	currentObj = obs_data_get_obj(jsonData, "currentPrism");
	if (!currentObj) {
		obs_data_release(jsonData);
		return false;
	}

	modules = obs_data_get_array(currentObj, "modules");
	if (!modules)
		modules = obs_data_array_create();
	return true;
}

static void save_data_end()
{
	if (!obs_data_save_json_safe(jsonData, config_path, "tmp", "bak"))
		plog(LOG_WARNING, "CW: Could not save data to crash json.");

	obs_data_array_release(modules);
	obs_data_release(currentObj);
	obs_data_release(jsonData);
}

static void erase_module_by_id(char *guid)
{
	if (!guid) {
		return;
	}

	size_t count = obs_data_array_count(modules);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(modules, i);
		if (!item)
			continue;

		if (0 == strcmp(guid, obs_data_get_string(item, "guid"))) {
			obs_data_array_erase(modules, i);
			obs_data_release(item);
			break;
		}
		obs_data_release(item);
	}
}

static void save_data_to_file(crash_writer_t *writer)
{
	if (!writer || !obs) {
		return;
	}

	os_mutex_handle_lock(fileMutexId);
	if (!save_data_start()) {
		os_mutex_handle_unlock(fileMutexId);
		return;
	}

	erase_module_by_id(writer->guid);

	obs_data_t *module = obs_data_create();
	if (module) {
		update_data_to_module(writer, module);
		obs_data_array_push_back(modules, module);
		obs_data_release(module);
	}

	save_data_end();
	os_mutex_handle_unlock(fileMutexId);
}

void cw_create(crash_writer_t *writer, enum cw_crash_type type)
{
	if (!writer) {
		return;
	}

	memset(writer, 0, sizeof(crash_writer_t));

	if (os_get_config_path(config_path, sizeof(config_path),
			       "PRISMLiveStudio/crashDump/crash.json") <= 0)
		return;

	GUID guid;
	CoCreateGuid(&guid);

	char buf[64] = {0};
	sprintf_s(buf, sizeof(buf) / sizeof(buf[0]),
		  "{%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X}", guid.Data1,
		  guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
		  guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
		  guid.Data4[6], guid.Data4[7]);

	writer->guid = bstrdup(buf);
	writer->type = type;

	if (!writer->threads)
		writer->threads = obs_data_array_create();

	pthread_mutex_init_value(&writer->mutex);
	if (pthread_mutex_init(&writer->mutex, NULL) != 0) {
		cw_free(writer);
		plog(LOG_WARNING, "CW: Failed to init mutex.");
		return;
	}
	writer->invoking = false;
	writer->enumerating = false;
}

void cw_free(crash_writer_t *writer)
{
	if (!writer || !obs) {
		return;
	}

	os_mutex_handle_lock(fileMutexId);
	if (save_data_start()) {
		erase_module_by_id(writer->guid);
		save_data_end();
	}
	os_mutex_handle_unlock(fileMutexId);

	pthread_mutex_destroy(&writer->mutex);
	bfree(writer->guid);
	bfree(writer->module_name);
	bfree(writer->source_name);
}

void cw_update_module_info(crash_writer_t *writer, const char *module_name,
			   const char *source_name, const char *source_id)
{
	if (!writer || !module_name || !source_name || !source_id)
		return;

	pthread_mutex_lock(&writer->mutex);
	bool changed = false;

	if (!writer->module_name ||
	    0 != strcmp(module_name, writer->module_name)) {
		changed = true;
		bfree(writer->module_name);
		writer->module_name = bstrdup(module_name);
	}
	if (!writer->source_name ||
	    0 != strcmp(source_name, writer->source_name)) {
		changed = true;
		bfree(writer->source_name);
		writer->source_name = bstrdup(source_name);
	}
	if (!writer->source_id || 0 != strcmp(source_id, writer->source_id)) {
		changed = true;
		bfree(writer->source_id);
		writer->source_id = bstrdup(source_id);
	}

	if (changed) {
		save_data_to_file(writer);
		plog(LOG_DEBUG, "CW: Updated momdule info. SourceId is %s",
		     source_id);
	}
	pthread_mutex_unlock(&writer->mutex);
}

void cw_update_thread_id(crash_writer_t *writer, unsigned long value,
			 enum cw_thread_type type)
{
	if (!writer) {
		return;
	}
	if (!writer->threads)
		writer->threads = obs_data_array_create();

	pthread_mutex_lock(&writer->mutex);
	size_t count = obs_data_array_count(writer->threads);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(writer->threads, i);
		if (!item)
			continue;
		if (type == obs_data_get_int(item, "threadType") &&
		    value == obs_data_get_int(item, "threadId")) {
			obs_data_release(item);
			pthread_mutex_unlock(&writer->mutex);
			return;
		}
		obs_data_release(item);
	}

	obs_data_t *id = obs_data_create();
	obs_data_set_int(id, "threadType", type);
	obs_data_set_int(id, "threadId", value);
	obs_data_array_push_back(writer->threads, id);
	obs_data_release(id);

	save_data_to_file(writer);
	plog(LOG_DEBUG,
	     "CW: Updated threadId. ThreadId is %d, threadType is %s, sourceId is %s",
	     value, convert_thread_type(type), writer->source_id);

	pthread_mutex_unlock(&writer->mutex);
}

void cw_remove_threads(crash_writer_t *writer)
{
	if (!writer) {
		return;
	}

	if (!writer->threads)
		return;

	pthread_mutex_lock(&writer->mutex);
	size_t count = obs_data_array_count(writer->threads);
	obs_data_array_release(writer->threads);

	writer->threads = obs_data_array_create();
	count = obs_data_array_count(writer->threads);

	save_data_to_file(writer);
	plog(LOG_DEBUG, "CW: Removed threads. SourceId is %s",
	     writer->source_id);
	pthread_mutex_unlock(&writer->mutex);
}

void cw_remove_thread_id(crash_writer_t *writer, unsigned long value)
{
	if (!writer) {
		return;
	}

	if (!writer->threads)
		return;

	pthread_mutex_lock(&writer->mutex);
	size_t count = obs_data_array_count(writer->threads);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(writer->threads, i);
		if (!item)
			continue;

		if (value == obs_data_get_int(item, "threadId")) {
			int type = obs_data_get_int(item, "threadType");
			obs_data_array_erase(writer->threads, i);
			obs_data_release(item);
			save_data_to_file(writer);
			plog(LOG_DEBUG,
			     "CW: Removed threadId by id. ThreadId is %d, threadType is %s, sourceId is %s",
			     value, convert_thread_type(type),
			     writer->source_id);
			break;
		}
		obs_data_release(item);
	}

	pthread_mutex_unlock(&writer->mutex);
}

void cw_remove_thread_id_by_type(crash_writer_t *writer,
				 enum cw_thread_type type)
{
	if (!writer) {
		return;
	}

	if (!writer->threads)
		return;

	pthread_mutex_lock(&writer->mutex);
	bool changed = false;
	size_t count = obs_data_array_count(writer->threads);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(writer->threads, i);
		if (!item)
			continue;

		if (type == obs_data_get_int(item, "threadType")) {
			int id = obs_data_get_int(item, "threadId");
			obs_data_array_erase(writer->threads, i);
			obs_data_release(item);
			changed = true;
			plog(LOG_DEBUG,
			     "CW: Removed threadId by type. ThreadId is %d, threadType is %s, sourceId is %s",
			     id, convert_thread_type(type), writer->source_id);
			continue;
		}
		obs_data_release(item);
	}

	if (changed) {
		save_data_to_file(writer);
	}
	pthread_mutex_unlock(&writer->mutex);
}

void cw_update_invoke_mark(crash_writer_t *writer, bool invoking)
{
	if (!writer) {
		return;
	}

	pthread_mutex_lock(&writer->mutex);
	if (writer->invoking != invoking) {
		writer->invoking = invoking;
		writer->invokeThreadId = GetCurrentThreadId();
		save_data_to_file(writer);
		plog(LOG_DEBUG,
		     "CW: Updated invoke mark. Invoking status is %d, sourceId is %s",
		     invoking, writer->source_id);
	}
	pthread_mutex_unlock(&writer->mutex);
}

void cw_update_enum_mark(crash_writer_t *writer, bool enumerating)
{
	if (!writer) {
		return;
	}

	pthread_mutex_lock(&writer->mutex);
	if (writer->enumerating != enumerating) {
		writer->enumerating = enumerating;
		writer->enumThreadId = GetCurrentThreadId();
		save_data_to_file(writer);
		plog(LOG_DEBUG,
		     "CW: Updated enum device mark. Enumerating status is %d, sourceId is %s",
		     enumerating, writer->source_id);
	}
	pthread_mutex_unlock(&writer->mutex);
}

void cw_set_file_mutex_uuid(const char *uuid)
{
	if (uuid) {
		if (fileMutexId) {
			bfree(fileMutexId);
		}
		fileMutexId = bstrdup(uuid);
		memcpy(fileMutexId, uuid, sizeof(uuid));
	}
}
