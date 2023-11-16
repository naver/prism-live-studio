#pragma once
#include <string>
#include <vector>
#include <set>
#include "PLSProcessInfo.h"

void mac_install_crash_reporter(const std::string &process_name);

void mac_get_latest_dump_data_location(ProcessInfo const &info, std::string &dump_data, std::string &location);
void mac_get_latest_dump_data_module_names(ProcessInfo const &info, std::string &dump_data, std::set<std::string> &module_names);

bool mac_send_data(std::string post_body);

bool mac_remove_crash_logs(ProcessInfo const &info);
