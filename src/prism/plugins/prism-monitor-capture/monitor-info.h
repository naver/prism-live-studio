#pragma once
#include <dxgi.h>
#include <string>
#include <vector>
#include <mutex>
using namespace std;

struct monitor_info {
	string friendly_name;
	int monitor_id; //related to property
	int monitor_dev_id;
	int adapter_id;
	int offset_x;
	int offset_y;
	int width;
	int height;
	bool is_primary;
	int rotation;
};

class PLSMonitorManager {
public:
	PLSMonitorManager();
	~PLSMonitorManager();

public:
	static PLSMonitorManager *get_instance();
	void clear();
	void load_monitors();
	void clear_monitor_info_array(vector<monitor_info *> &monitor_info_vector);
	vector<monitor_info *> &get_monitor_info_array();
	bool enum_duplicator_array(vector<monitor_info *> &outputList);

	bool get_adapter_monitor_dev_id(int &adapter_id, int &dev_id, int monitor_id);
	bool get_monitor_detail(int &width, int &height, int &offset_x, int &offset_y, int &rotation, int monitor_id);

private:
	vector<monitor_info *> monitor_info_array;
};
