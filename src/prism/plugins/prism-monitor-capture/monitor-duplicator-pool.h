#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include <mutex>
#include "monitor-duplicator.h"

using namespace std;

class PLSMonitorDuplicatorPool {
public:
	PLSMonitorDuplicatorPool();
	~PLSMonitorDuplicatorPool();

public:
	static PLSMonitorDuplicatorPool *get_instance();
	PLSMonitorDuplicator *get_duplicator(int adapter_id, int monitor_dev_id);
	void clear();
	bool release_duplicator(int adapter_id, int monitor_dev_id);

private:
	std::mutex mtx;
	vector<PLSMonitorDuplicator *> duplicator_vector;
};
