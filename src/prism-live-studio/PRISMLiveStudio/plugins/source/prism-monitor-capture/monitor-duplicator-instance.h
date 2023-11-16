#pragma once
#include "monitor-info.h"
#include "graphics/graphics.h"
#include "monitor-duplicator-core.h"
#include <atomic>

class PLSDuplicatorInstance {
public:
	PLSDuplicatorInstance(DuplicatorType type, int adapter, int output_id, int did);
	virtual ~PLSDuplicatorInstance();

	PLSDuplicatorInstance(const PLSDuplicatorInstance &) = delete;
	PLSDuplicatorInstance &operator=(const PLSDuplicatorInstance &) = delete;
	PLSDuplicatorInstance(PLSDuplicatorInstance &&) = delete;
	PLSDuplicatorInstance &operator=(PLSDuplicatorInstance &&) = delete;

	int get_hardware_id() const;
	int get_adapter_id() const;
	int get_monitor_dev_id() const;
	bool is_adapter_valid() const;
	bool update_frame();

	void set_removed();
	bool is_removed() const;

	// Warning :
	// while invoking this func, you should request obs_enter_graphics() firstly and call obs_leave_graphics() after using returned texture.
	gs_texture *get_texture();

private:
	static unsigned __stdcall thread_func(void *pParam);
	void thread_func_inner();

	bool need_update_frame() const;
	void release_frame();

	//-----------------------------------------------------
	std::atomic<bool> removed = false;

	int display_id = 0;

	HANDLE active_event;
	HANDLE exit_event;
	HANDLE capture_thread;
	PLSDuplicatorCore monitor_capture;

	gs_texture *gs_frame = nullptr; // texture on system graphics
	HANDLE shared_handle = nullptr;

	std::atomic<DWORD> previous_active_time = GetTickCount();
};

using DUPLICATOR_PTR = std::shared_ptr<PLSDuplicatorInstance>;
