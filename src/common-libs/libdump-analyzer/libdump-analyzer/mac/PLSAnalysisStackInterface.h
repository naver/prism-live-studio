#pragma once
#include <string>
#include <vector>
#include <set>
#include "PLSProcessInfo.h"

void mac_install_crash_reporter(ProcessInfo const &info);

void mac_get_latest_dump_data(ProcessInfo const &info, std::string &dump_data, std::string &location, std::string &stack_hash, std::set<std::map<std::string, std::string>> &module_names);

bool mac_send_data(std::string post_body);

bool mac_remove_crash_logs(ProcessInfo const &info);
