#pragma once
#include <vector>
#include "monitor-duplicator-instance.h"

using namespace std;

class PLSMonitorDuplicatorPool {
public:
	virtual ~PLSMonitorDuplicatorPool() {}

public:
	static PLSMonitorDuplicatorPool *get_instance();

	void clear();
	DUPLICATOR_PTR get_duplicator(int adapter_index, int output_index, int display_id, bool enable_dif_adapter);

private:
	std::mutex mtx;
	vector<DUPLICATOR_PTR> duplicator_vector;
};
