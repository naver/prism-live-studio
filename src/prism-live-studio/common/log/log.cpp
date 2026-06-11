#include "log.h"

#include <obs-module.h>

void pls_log(bool kr, pls_log_level_t log_level, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, log_level, obs_module_name(), file_name, file_line, arg_count, format, args);
	va_end(args);
}
void pls_logex(bool kr, pls_log_level_t log_level, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logvaex(kr, log_level, obs_module_name(), file_name, file_line, fields, arg_count, format, args);
	va_end(args);
}
void pls_error(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_ERROR, obs_module_name(), file_name, file_line, arg_count, format, args);
	va_end(args);
}
void pls_warn(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_WARN, obs_module_name(), file_name, file_line, arg_count, format, args);
	va_end(args);
}
void pls_info(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_INFO, obs_module_name(), file_name, file_line, arg_count, format, args);
	va_end(args);
}
void pls_debug(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_DEBUG, obs_module_name(), file_name, file_line, arg_count, format, args);
	va_end(args);
}
void pls_ui_step(bool kr, const char *controls, const char *action, const char *file_name, int file_line)
{
	pls_ui_step(kr, obs_module_name(), controls, action, file_name, file_line);
}
