#pragma once

#include <liblog.h>
#include <action.h>
#include <util/base.h>

#include "module_names.h"

bool log_init();
void log_cleanup();

void set_log_handler(log_handler_t handler, void *param);
