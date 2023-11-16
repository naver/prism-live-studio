#pragma once
#include <vector>
#include "monitor-duplicator-instance.h"

class PLSMonitorDuplicatorPool {
public:
	virtual ~PLSMonitorDuplicatorPool() = default;

	static PLSMonitorDuplicatorPool *get_instance();

	void clear();
	DUPLICATOR_PTR get_duplicator(int adapter_index, int output_index, int display_id, bool enable_dif_adapter);

private:
	std::recursive_mutex mtx;
	std::vector<DUPLICATOR_PTR> duplicator_vector;
};
