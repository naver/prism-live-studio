#pragma once
#include <functional>
#include <memory>

class ITaskPool {
public:
	virtual ~ITaskPool() = default;

	virtual void push_task(const std::function<void()> &func, uint64_t key = 0) = 0;

	virtual void clear_tasks(uint64_t key) = 0;
	virtual void clear_all_tasks() = 0;

	virtual bool is_task_empty() = 0;
	virtual void run_all_tasks() = 0;
};

std::shared_ptr<ITaskPool> create_task_pool();