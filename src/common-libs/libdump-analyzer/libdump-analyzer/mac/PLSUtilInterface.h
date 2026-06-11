//
//  PLSUtilInterface.h
//  prism-live-studio
//
//  Created by Keven on 3/27/23.
//
//

#pragma once
#include <string>

std::string mac_get_app_run_dir(std::string name);
std::string mac_get_app_data_dir(std::string name);

std::string mac_get_os_version();
std::string mac_get_device_model();
std::string mac_get_device_name();
std::string mac_generate_dump_file(std::string info, std::string message);
bool mac_is_third_party_plugin(std::string &module_path);
std::string mac_get_plugin_version(std::string &path);
