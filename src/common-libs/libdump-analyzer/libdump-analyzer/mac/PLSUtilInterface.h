//
//  PLSUtilInterface.h
//  prism-live-studio
//
//  Created by Keven on 3/27/23.
//
//

#pragma once
#include <string>

extern "C" {
#include "mach_backtrace.h"
}

std::string mac_get_app_run_dir(std::string name);
std::string mac_get_app_data_dir(std::string name);

std::string mac_get_os_version();
std::string mac_get_device_model();
std::string mac_get_device_name();

std::string mac_get_threads();
