#pragma once
#include <string>
#include <vector>
#include <set>
#include "PLSProcessInfo.h"

void mac_install_crash_reporter(ProcessInfo const &info);

void mac_get_latest_dump_data(ProcessInfo const &info, std::string &dump_data, std::string &location, std::string &stack_hash, std::set<std::map<std::string, std::string>> &module_names);

bool mac_send_data(std::string post_body);

bool mac_remove_crash_logs(ProcessInfo const &info);

// Install fallback signal handlers for crash detection
void mac_install_fallback_crash_handlers();

// Check if a crash dump exists for the given process info (session + pid)
bool mac_has_crash_dump(ProcessInfo const &info);
