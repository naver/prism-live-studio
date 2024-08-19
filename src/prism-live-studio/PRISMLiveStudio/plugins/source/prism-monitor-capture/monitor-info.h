#pragma once
#include <dxgi.h>
#include <string>
#include <vector>
#include <mutex>

struct monitor_info {
	HMONITOR handle = nullptr;
	std::string friendly_name = "";
	bool is_primary = false;
	int rotation = 0;

	int display_id = 0; // using to check monitor remove. For Windows, its value should be more than 0 if it is valid
	int adapter_index = -1;
	int adapter_output_index = -1;

	int offset_x = 0;
	int offset_y = 0;

	int width = 0;
	int height = 0;
};

class PLSMonitorManager {
public:
	PLSMonitorManager();
	~PLSMonitorManager() = default;

	static PLSMonitorManager *get_instance();

	void reload_monitor(bool save_log = false);
	bool find_monitor(int index, int display_id, monitor_info &info);
	std::vector<monitor_info> get_monitor();

private:
	bool enum_duplicator_array(std::vector<monitor_info> &outputList) const;

	//-----------------------------------------------------
	std::mutex list_lock;
	std::vector<monitor_info> monitor_info_array;
};
