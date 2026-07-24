#include "task-pool.h"

#include <list>
#include <mutex>
#include <assert.h>

class TaskPool : public ITaskPool {
public:
	TaskPool() = default;

	~TaskPool() override { clear_all_tasks(); }

	void push_task(const std::function<void()> &func, uint64_t key = 0) override
	{
		if (!func)
			return;

		TaskInfo info;
		info.key = key;
		info.func = func;

		std::scoped_lock<std::recursive_mutex> autoLock(m_lockTask);
		m_taskList.push_back(info);
	}

	bool is_task_empty() override
	{
		std::scoped_lock<std::recursive_mutex> autoLock(m_lockTask);
		return m_taskList.empty();
	}

	void run_all_tasks() override
	{
		std::list<TaskPool::TaskInfo> temTasks;

		{
			std::scoped_lock<std::recursive_mutex> autoLock(m_lockTask);
			temTasks.swap(m_taskList);
		}

		for (const auto &item : temTasks) {
			item.func();
		}
	}

	void clear_tasks(uint64_t key) override
	{
		std::scoped_lock<std::recursive_mutex> autoLock(m_lockTask);

		auto itr = m_taskList.begin();
		while (itr != m_taskList.end()) {
			if (itr->key == key) {
				itr = m_taskList.erase(itr);
				continue;
			}

			++itr;
		}
	}

	void clear_all_tasks() override
	{
		std::scoped_lock<std::recursive_mutex> autoLock(m_lockTask);
		m_taskList.clear();
	}

protected:
	struct TaskInfo {
		uint64_t key = 0;
		std::function<void()> func;
	};

	std::recursive_mutex m_lockTask;
	std::list<TaskInfo> m_taskList;
};

std::shared_ptr<ITaskPool> create_task_pool()
{
	return std::make_shared<TaskPool>();
}
