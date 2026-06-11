#include "libutils-api.h"
#include "libutils-api-log.h"

namespace pls {
namespace {
struct LocalGlobalVars {
	static int s_log_error_lvl;
	static int s_log_warn_lvl;
	static int s_log_info_lvl;
	static int s_log_debug_lvl;
	static log_cb_t s_log_error_cb;
	static log_cb_t s_log_warn_cb;
	static log_cb_t s_log_info_cb;
	static log_cb_t s_log_debug_cb;
	static logex_cb_t s_logex_cb;
};

void default_cb(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	pls_unused(kr, module_name, file_name, file_line, arg_count, format);
}
void default_logex(bool kr, int log_level, const char *module_name, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, int arg_count,
		   const char *format, ...)
{
	pls_unused(kr, log_level, module_name, file_name, file_line, fields, arg_count, format);
}

int LocalGlobalVars::s_log_error_lvl = 0;
int LocalGlobalVars::s_log_warn_lvl = 0;
int LocalGlobalVars::s_log_info_lvl = 0;
int LocalGlobalVars::s_log_debug_lvl = 0;

log_cb_t LocalGlobalVars::s_log_error_cb = &default_cb;
log_cb_t LocalGlobalVars::s_log_warn_cb = &default_cb;
log_cb_t LocalGlobalVars::s_log_info_cb = &default_cb;
log_cb_t LocalGlobalVars::s_log_debug_cb = &default_cb;
logex_cb_t LocalGlobalVars::s_logex_cb = &default_logex;
}

LIBUTILSAPI_API void set_log_levels(int error_lvl, int warn_lvl, int info_lvl, int debug_lvl)
{
	LocalGlobalVars::s_log_error_lvl = error_lvl;
	LocalGlobalVars::s_log_warn_lvl = warn_lvl;
	LocalGlobalVars::s_log_info_lvl = info_lvl;
	LocalGlobalVars::s_log_debug_lvl = debug_lvl;
}
LIBUTILSAPI_API int log_error_lvl()
{
	return LocalGlobalVars::s_log_error_lvl;
}
LIBUTILSAPI_API int log_warn_lvl()
{
	return LocalGlobalVars::s_log_warn_lvl;
}
LIBUTILSAPI_API int log_info_lvl()
{
	return LocalGlobalVars::s_log_info_lvl;
}
LIBUTILSAPI_API int log_debug_lvl()
{
	return LocalGlobalVars::s_log_debug_lvl;
}
LIBUTILSAPI_API void set_log_cbs(log_cb_t error_cb, log_cb_t warn_cb, log_cb_t info_cb, log_cb_t debug_cb, logex_cb_t logex_cb)
{
	LocalGlobalVars::s_log_error_cb = error_cb ? error_cb : &default_cb;
	LocalGlobalVars::s_log_warn_cb = warn_cb ? warn_cb : &default_cb;
	LocalGlobalVars::s_log_info_cb = info_cb ? info_cb : &default_cb;
	LocalGlobalVars::s_log_debug_cb = debug_cb ? debug_cb : &default_cb;
	LocalGlobalVars::s_logex_cb = logex_cb ? logex_cb : &default_logex;
}
LIBUTILSAPI_API log_cb_t log_error_cb()
{
	return LocalGlobalVars::s_log_error_cb;
}
LIBUTILSAPI_API log_cb_t log_warn_cb()
{
	return LocalGlobalVars::s_log_warn_cb;
}
LIBUTILSAPI_API log_cb_t log_info_cb()
{
	return LocalGlobalVars::s_log_info_cb;
}
LIBUTILSAPI_API log_cb_t log_debug_cb()
{
	return LocalGlobalVars::s_log_debug_cb;
}
LIBUTILSAPI_API logex_cb_t logex_cb()
{
	return LocalGlobalVars::s_logex_cb;
}
}
