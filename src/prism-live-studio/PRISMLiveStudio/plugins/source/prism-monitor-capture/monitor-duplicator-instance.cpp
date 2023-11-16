#include "monitor-duplicator-instance.h"
#include <util/platform.h>
#include <assert.h>
#include <process.h>
#include <array>
#include <liblog.h>
#include <obs.h>

const auto MONITOR_ACTIVE_INTERVAL = 2000;
const auto MONITOR_EXCEPTION_SLEEP = 2000;

PLSDuplicatorInstance::PLSDuplicatorInstance(DuplicatorType type, int adapter, int output_id, int did) : display_id(did), monitor_capture(type, adapter, output_id)
{
	exit_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	active_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	::ResetEvent(exit_event);
	capture_thread = (HANDLE)_beginthreadex(nullptr, 0, thread_func, this, 0, nullptr);
}

PLSDuplicatorInstance::~PLSDuplicatorInstance()
{
	release_frame();

	::SetEvent(exit_event);
	::WaitForSingleObject(capture_thread, INFINITE);

	::CloseHandle(capture_thread);
	::CloseHandle(exit_event);
	::CloseHandle(active_event);
}

bool is_handle_signed(const HANDLE &hEvent, DWORD dwMilliSecond)
{
	if (!hEvent)
		return false;

	DWORD res = WaitForSingleObject(hEvent, dwMilliSecond);
	return (res == WAIT_OBJECT_0);
}

unsigned __stdcall PLSDuplicatorInstance::thread_func(void *pParam)
{
	auto self = static_cast<PLSDuplicatorInstance *>(pParam);
	CoInitialize(nullptr);
	self->thread_func_inner();
	CoUninitialize();
	return 0;
}

void PLSDuplicatorInstance::thread_func_inner()
{
	if (!monitor_capture.init()) {
		PLS_ERROR("monitor capture", "Failed to init prism duplicator. adp:%d output:%d", monitor_capture.adapter, monitor_capture.adapter_output);
	}

	std::vector<HANDLE> events = {
		active_event,
		exit_event,
	};

	DWORD render_fps = 100; // default fps for monitor
	uint64_t ns_to_ms = 1000000;
	uint64_t wait_time = 0;

	while (!is_handle_signed(exit_event, (DWORD)wait_time)) {
		uint64_t capture_time = os_gettime_ns() / ns_to_ms;
		uint64_t next_capture = capture_time + (1000 / render_fps);

		if (!need_update_frame()) {
			WaitForMultipleObjects((DWORD)events.size(), events.data(), FALSE, MONITOR_EXCEPTION_SLEEP);
		}

		if (!monitor_capture.check_update_texture()) {
			WaitForSingleObject(exit_event, MONITOR_EXCEPTION_SLEEP);
		}

		wait_time = 0;
		uint64_t current_time = os_gettime_ns() / ns_to_ms;
		if (current_time < next_capture) {
			wait_time = next_capture - current_time;
			if (wait_time < 5)
				wait_time = 0;
		}
	}

	monitor_capture.uninit();
}

int PLSDuplicatorInstance::get_hardware_id() const
{
	return display_id;
}

int PLSDuplicatorInstance::get_adapter_id() const
{
	return monitor_capture.adapter;
}

int PLSDuplicatorInstance::get_monitor_dev_id() const
{
	return monitor_capture.adapter_output;
}

bool PLSDuplicatorInstance::is_adapter_valid() const
{
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return true;

	DuplicatorType type;
	if (ovi.adapter == get_adapter_id())
		type = DuplicatorType::DUPLICATOR_SHARED_HANDLE;
	else
		type = DuplicatorType::DUPLICATOR_MEMORY_COPY;

	return (type == monitor_capture.type);
}

bool PLSDuplicatorInstance::update_frame()
{
	previous_active_time = GetTickCount();
	::SetEvent(active_event); // enable event after set active time

	PLSAutoLockRender alr;

	bool is_ok;
	switch (monitor_capture.type) {
	case DuplicatorType::DUPLICATOR_MEMORY_COPY:
		is_ok = monitor_capture.upload_memory_texture(gs_frame);
		break;

	case DuplicatorType::DUPLICATOR_SHARED_HANDLE:
		is_ok = monitor_capture.upload_shared_texture(gs_frame, shared_handle);
		break;

	default:
		break;
	}

	if (!is_ok)
		release_frame();

	return is_ok;
}

void PLSDuplicatorInstance::set_removed()
{
	removed = true;
}

bool PLSDuplicatorInstance::is_removed() const
{
	return removed;
}

gs_texture *PLSDuplicatorInstance::get_texture()
{
	previous_active_time = GetTickCount();
	::SetEvent(active_event); // enable event after set active time

	PLSAutoLockRender alr;
	return gs_frame;
}

bool PLSDuplicatorInstance::need_update_frame() const
{
	DWORD current = GetTickCount();
	DWORD previous = previous_active_time.load();
	if (current > previous && (current - previous) > MONITOR_ACTIVE_INTERVAL)
		return false;
	else
		return true;
}

void PLSDuplicatorInstance::release_frame()
{
	PLSAutoLockRender alr;

	shared_handle = nullptr;
	if (gs_frame) {
		gs_texture_destroy(gs_frame);
		gs_frame = nullptr;
	}
}
