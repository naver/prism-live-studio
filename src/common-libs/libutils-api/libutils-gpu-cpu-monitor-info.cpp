//
//  gpu_info_collector.h
//  pls-gpu-info
//
//  Created by Zhong Ling on 2023/3/2.
//

#include "libutils-gpu-cpu-monitor-info.h"

#include <sstream>

std::string pls_print_basic_graphics_info(const pls_gpu_basic_info &info)
{
	std::string result;

	result = "GPU Info: \nMachine Model Name is " + info.machine_model_name + "\n" + "Machine Model Version is " + info.machine_model_version + "\n";

	for (auto it = info.gpus.begin(); it != info.gpus.end(); ++it) {
		result += "GPU device id: " + std::to_string(it->device_id) + ", vendor id: " + std::to_string(it->vendor_id) + "\n";
	}
	return result;
}

std::string pls_print_cpu_info(const pls_cpu_info &info)
{
	std::string result;
	result = "CPU Info: \nCPU Name: " + info.name + "\nHZ: " + std::to_string(info.hz) + "\nMicro Seconds: " + std::to_string(info.tick) + "\nFree Memory:" + std::to_string(info.free_mem) +
		 " MB\nTotal Memory: " + std::to_string(info.total_mem) + " MB\nLogical Cores: " + std::to_string(info.logical_cores) + "\nPhysical Cores:" + std::to_string(info.physical_cores);
	return result;
}

std::string pls_print_monitors_info(const std::vector<pls_monitor_info> &infos)
{
	std::stringstream ss;
	ss << "Monitors Info:" << std::endl;
	for (int i = 0; i < infos.size(); i++) {
		ss << "Monitor ID: " << infos[i].monitor_id << std::endl
		   << "Bounds: " << infos[i].bounds.x << ", " << infos[i].bounds.y << ", " << infos[i].bounds.w << ", " << infos[i].bounds.h << std::endl
		   << "Dip to pixel scale: " << infos[i].dip_to_pixel_scale << std::endl
		   << "Is builtin: " << infos[i].is_builtin << std::endl;
	}
	return ss.str();
}
