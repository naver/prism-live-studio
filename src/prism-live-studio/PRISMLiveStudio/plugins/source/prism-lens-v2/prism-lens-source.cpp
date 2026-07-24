#include "prism-lens-source.h"
#include "lens-logo.h"
#include "handle-wrapper.h"
#include "StringConvert.h"
#include <pls/pls-base.h>
#include <pls/pls-source.h>
#include <pls/pls-lens-event.h>
#include "util/platform.h"
#include "util/windows/win-version.h"
#include <string>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

#define do_warn(format, ...) PLS_WARN("lens capture", "[lens '%s' %p] " format, obs_source_get_name(source), source, ##__VA_ARGS__)
#define do_info(format, ...) PLS_INFO("lens capture", "[lens '%s' %p] " format, obs_source_get_name(source), source, ##__VA_ARGS__)
#define warn(format, ...) do_warn(format, ##__VA_ARGS__)
#define info(format, ...) do_info(format, ##__VA_ARGS__)

#define TEXT_PRISM_LENS_NAME "main.prism.lens.name"
#define TEXT_PRISM_LENS_MOBILE_NAME "main.prism.lens.mobile.name"

#define KEY_VIDEO_DEVICE_ID "video_device_id"   // key name in old plugin. eg: "video_device_id": "PRISM Lens 1:",
#define KEY_AUDIO_DEVICE_ID "audio_device_id"   // key name in old plugin. eg: "audio_device_id": "PRISM Lens audio 1:"
#define KEY_LENS_VIDEO_INDEX "lens_video_index" // key name in new plugin, value should be 0/1/2
#define KEY_LENS_AUDIO_INDEX "lens_audio_index" // key name in new plugin, value should be 0/1/2
#define KEY_LENS_ACTIVE "active"
#define TEXT_ACTIVATE obs_module_text("Activate")
#define TEXT_DEACTIVATE obs_module_text("Deactivate")
#define TEXT_DEVICE obs_module_text("Device")
#define USE_CUSTOM_AUDIO "use_custom_audio_device"
#define TEXT_CUSTOM_AUDIO obs_module_text("UseCustomAudioDevice")
#define FLIP_IMAGE "flip_vertically"
#define FLIP_IMAGE_H "flip_horizontally"

#define WAIT_NEW_VIDEO_DURITION 200 // in ms
#define SHORT_SLEEP_SEC 5           // in ms
#define LONG_SLEEP_SEC 100          // in ms

// this map is used to convert old device id to new index (video)
const std::map<std::string, int> map_video_id = {
	{std::string(TEXT_PRISM_LENS_1) + ":", 0}, // leftValue: old device id, rightValue: new index
	{std::string(TEXT_PRISM_LENS_2) + ":", 1},
	{std::string(TEXT_PRISM_LENS_3) + ":", 2},
};

// this map is used to convert old device id to new index (audio)
const std::map<std::string, int> map_audio_id = {
	{std::string(TEXT_PRISM_LENS_AUDIO_1) + ":", 0}, // leftValue: old device id, rightValue: new index
	{std::string(TEXT_PRISM_LENS_AUDIO_2) + ":", 1},
	{std::string(TEXT_PRISM_LENS_AUDIO_3) + ":", 2},
};

const std::map<LENS_STATUS, std::string> map_status_desc = {
	{LENS_STATUS::STATUS_LENS_NOT_RUNNING, "lens_not_running"},
	{LENS_STATUS::STATUS_INVALID_INDEX, "invalid_lens_index"},
	{LENS_STATUS::STATUS_LENS_NO_DATA, "lens_no_data"},
	{LENS_STATUS::STATUS_IPC_FAILED, "ipc_failed"},
	{LENS_STATUS::STATUS_CAPTURED, "capture_successed"},
	{LENS_STATUS::STATUS_EXITING, "exiting"},
	{LENS_STATUS::STATUS_DEACTIVED, "deactived"},
	{LENS_STATUS::STATUS_WAIT_DATA, "wait_data"},
};

// this is for checking output status of lens
HANDLE lens_video_valid[MAX_LENS_COUNT] = {};
HANDLE lens_audio_valid[MAX_LENS_COUNT] = {};

wchar_t proc_path[MAX_PATH] = {};

//---------------------------------------------------------------------------------------------------------
// defined in LENS with same function name
static std::string GetFileMappingNamePrefix(int lens_index)
{
	auto fileMapingName = std::string("camera_studio_output_") + std::to_string(lens_index);
	return fileMapingName;
}

bool ICaptureSession::is_lens_running()
{
	HANDLE prism_cam_running_handle = CHandleWrapper::GetAlreadyEvent(PRISM_CAM_RUNNING_FLAG);
	if (!CHandleWrapper::IsHandleValid(prism_cam_running_handle)) {
		return false;
	}
	CHandleWrapper::CloseHandleEx(prism_cam_running_handle);
	return true;
}

void init_process_path()
{
	GetModuleFileNameW(NULL, proc_path, MAX_PATH);
	if (wcsstr(proc_path, L"obs64.exe")) {
		if (wcsstr(proc_path, L"PRISMLiveStudio") || wcsstr(proc_path, L"prism-live-studio")) {
			// If path contains "PRISMLiveStudio" or "prism-live-studio", we think it is a prism process
			PathRemoveFileSpecW(proc_path);
			swprintf_s(proc_path, MAX_PATH, L"%s\\PRISMLiveStudio.exe", proc_path);
		}
	}
}

void ICaptureSession::init_lens_module()
{
	init_process_path();

	std::string name;
	for (size_t i = 0; i < MAX_LENS_COUNT; i++) {
		name = std::string(FILEMAP_OUTPUT_STARTED_EVENT) + "-" + std::to_string(i);
		lens_video_valid[i] = CHandleWrapper::GetEvent(name.c_str());
		assert(CHandleWrapper::IsHandleValid(lens_video_valid[i]));

		name = std::string(FILEMAP_OUTPUT_STARTED_EVENT_AUDIO) + "-" + std::to_string(i);
		lens_audio_valid[i] = CHandleWrapper::GetEvent(name.c_str());
		assert(CHandleWrapper::IsHandleValid(lens_audio_valid[i]));
	}
}

void ICaptureSession::uninit_lens_module()
{
	for (size_t i = 0; i < MAX_LENS_COUNT; i++) {
		CHandleWrapper::CloseHandleEx(lens_video_valid[i]);
		CHandleWrapper::CloseHandleEx(lens_audio_valid[i]);
	}
}

ICaptureSession::ICaptureSession(obs_source_t *s, HANDLE &exit_evt, bool is_audio) : source(s), exit_event(exit_evt), is_audio_capture(is_audio)
{
	async_task = create_task_pool();
}

void ICaptureSession::set_active(bool is_active)
{
	actived = is_active;
}

void ICaptureSession::set_device(uint32_t index)
{
	async_task->push_task([=]() {
		if (selected_len != index) {
			selected_len = index;
			data_invalid_count = 0;
			info("[%s] lens index is changed to %u", get_session_type(), index);

			CHandleWrapper::CloseHandleEx(flag_capturing);

			auto active_event = std::string(get_capture_flag()) + "-" + std::to_string(index);
			flag_capturing = CHandleWrapper::GetEvent(active_event.c_str());
		}
	});
}

void ICaptureSession::start_capture()
{
	capture_thread = std::thread(&ICaptureSession::lens_capture_thread, this);
}

void ICaptureSession::stop_capture()
{
	if (capture_thread.joinable()) {
		capture_thread.join();
	}

	uninit_ipc_buffer();

	CHandleWrapper::CloseHandleEx(flag_capturing);
}

void ICaptureSession::force_new_timestamp()
{
	async_task->push_task([=]() {
		is_first_data = true;
		first_timestamp = 0;
	});
}

void ICaptureSession::lens_capture_thread()
{
	info("[%s] %s enter", get_session_type(), __FUNCTION__);

	while (!CHandleWrapper::IsHandleSigned(exit_event, SHORT_SLEEP_SEC)) {
		async_task->run_all_tasks();

		HANDLE events[2] = {exit_event};
		DWORD event_count = 1;
		if (data_reader && data_reader->IsBufferValid()) {
			events[1] = data_reader->bufUpdateEvent;
			++event_count;
		}
		if (WAIT_OBJECT_0 == WaitForMultipleObjects(event_count, events, FALSE, SHORT_SLEEP_SEC)) {
			break;
		}

		if (!actived.load()) {
			uninit_ipc_buffer();
			update_status(LENS_STATUS::STATUS_DEACTIVED);
			continue;
		}

		auto new_index = selected_len.load();
		if (new_index >= MAX_LENS_COUNT) { // invalid index
			uninit_ipc_buffer();
			update_status(LENS_STATUS::STATUS_INVALID_INDEX);
			CHandleWrapper::IsHandleSigned(exit_event, LONG_SLEEP_SEC);
			continue;
		}

		if (!is_lens_output_valid(new_index)) {
			continue;
		}

		if (new_index != used_lens) { // user changed index for capturing
			uninit_ipc_buffer(true);
		}

		if (!is_ipc_buffer_ready()) {
			bool successed = init_ipc_buffer(new_index);
			if (!successed) {
				assert(false);
				uninit_ipc_buffer();
				update_status(LENS_STATUS::STATUS_IPC_FAILED);
				CHandleWrapper::IsHandleSigned(exit_event, LONG_SLEEP_SEC);
				continue;
			}
		}

		if (GetTickCount64() - previous_heartbeat >= PRISM_VCAM_HEARTBEAT_INTERVAL_MS) {
			request_heartbeat();
		}

		read_buffer_data();
	}

	uninit_ipc_buffer();
	update_status(LENS_STATUS::STATUS_EXITING);

	info("[%s] %s leave", get_session_type(), __FUNCTION__);
}

bool ICaptureSession::init_ipc_buffer(uint32_t index)
{
	check_alignment();

	if (!init_ipc_buffer_impl(index)) {
		uninit_ipc_buffer();
		return false;
	}

	assert(is_start_event_sent == false && instance_guid.empty());
	update_status(LENS_STATUS::STATUS_WAIT_DATA);

	instance_guid = str::GenerateGuid();
	used_lens = index;

	request_start();
	return true;
}

void ICaptureSession::check_alignment()
{
	ipc_alignment = 64;

	QString path;
	if (pls_is_install_cam_studio(path)) {
		win_version_info version;
		if (get_dll_ver(path.toStdWString().c_str(), &version)) {
			if (version.major == 1 && version.minor == 0 && version.build < 8)
				ipc_alignment = 32; // before lens-1.0.8, alignment is 32
		}
	}
}

void ICaptureSession::uninit_ipc_buffer(bool keepVideo)
{
	request_stop();

	used_lens = uint32_t(-1);
	data_reader = nullptr;
}

bool ICaptureSession::is_ipc_buffer_ready()
{
	return data_reader && data_reader->IsBufferValid();
}

bool ICaptureSession::is_lens_output_valid(uint32_t index)
{
	if (!ICaptureSession::is_lens_running()) { // lens app is not running
		update_status(LENS_STATUS::STATUS_LENS_NOT_RUNNING);
		uninit_ipc_buffer();
		return false;
	}

	if (is_audio_capture) {
		if (CHandleWrapper::IsHandleSigned(lens_audio_valid[index], 0)) {
			return true; // only supported by lens-2.0 or higher version
		}
	}

	if (CHandleWrapper::IsHandleSigned(lens_video_valid[index], 0)) {
		return true;

	} else {
		update_status(LENS_STATUS::STATUS_LENS_NO_DATA);

		// After changing lens index, maybe lens is not sending data because no app was capturing lens before.
		// However maybe data will be sent soon since lens app detects we need data.
		// So here we wait for new data with litter time and will not display logo during waiting time.
		// This help reduces render flicker of logo.
		++data_invalid_count;
		if (data_invalid_count < (WAIT_NEW_VIDEO_DURITION / SHORT_SLEEP_SEC)) {
			uninit_ipc_buffer(true);
		} else {
			uninit_ipc_buffer();
		}

		return false; // lens app does not send av data
	}
}

void ICaptureSession::update_status(LENS_STATUS s)
{
	if (source_status == s)
		return;

	source_status = s;

	std::string state = "";
	auto itr = map_status_desc.find(s);
	if (itr != map_status_desc.end()) {
		state = itr->second;
	} else {
		state = std::to_string((int)s);
		assert(false && "lost handler for new status");
	}

	info("[%s] capture status is changed to %s", get_session_type(), state.c_str());
}

void ICaptureSession::request_start()
{
	info("[%s] %s is called", get_session_type(), __FUNCTION__);

	if (!pipe_client) {
		pipe_client = create_pipe_client(PRISM_VCAM_PIPE_NAME);
		assert(pipe_client);
	}

	if (pipe_client && !is_start_event_sent) {
		named_pipe_pkg_t pkg = {};
		memcpy(pkg.proc_name, proc_path, MAX_PATH * sizeof(wchar_t));
		memcpy(pkg.guid, instance_guid.c_str(), PRISM_VCAM_GUID_SIZE);
		pkg.pid = GetCurrentProcessId();
		pkg.type = PKG_START;
		pkg.cam_idx = used_lens.load();

		if (!pipe_send_pkg(pipe_client, &pkg)) {
			warn("[%s] Failed to send start request to lens app, error: %u", get_session_type(), GetLastError());
			destroy_pipe_client(&pipe_client);
		} else {
			info("[%s] %s is called to notify lens, guid: %s, cam_idx: %d", get_session_type(), __FUNCTION__, instance_guid.c_str(), pkg.cam_idx);
			is_start_event_sent = true;
			previous_heartbeat = 0;
		}
	}
}

void ICaptureSession::request_heartbeat()
{
	if (pipe_client) {
		previous_heartbeat = GetTickCount64();

		named_pipe_pkg_t pkg = {};
		memcpy(pkg.proc_name, proc_path, MAX_PATH * sizeof(wchar_t));
		memcpy(pkg.guid, instance_guid.c_str(), PRISM_VCAM_GUID_SIZE);
		pkg.pid = GetCurrentProcessId();
		pkg.type = PKT_HEARTBEAT;
		pkg.cam_idx = used_lens.load();

		if (!pipe_send_pkg(pipe_client, &pkg)) {
			warn("[%s] Failed to send heartbeat to lens app, error: %u", get_session_type(), GetLastError());
			destroy_pipe_client(&pipe_client);
		}
	}
}

void ICaptureSession::request_stop()
{
	if (pipe_client && is_start_event_sent) {
		named_pipe_pkg_t pkg = {};
		memcpy(pkg.proc_name, proc_path, MAX_PATH * sizeof(wchar_t));
		memcpy(pkg.guid, instance_guid.c_str(), PRISM_VCAM_GUID_SIZE);
		pkg.pid = GetCurrentProcessId();
		pkg.type = PKG_STOP;
		pkg.cam_idx = used_lens.load();

		pipe_send_pkg(pipe_client, &pkg);
		info("[%s] %s is called to notify lens, guid: %s, cam_idx: %d", get_session_type(), __FUNCTION__, instance_guid.c_str(), pkg.cam_idx);
	}

	instance_guid = "";
	is_start_event_sent = false;
	if (pipe_client) {
		destroy_pipe_client(&pipe_client);
	}
}

//---------------------------------------------------------------------------------------------------------
static void update_reroute_audio(void *data, calldata_t *cd)
{
	auto session = static_cast<AudioCaptureSession *>(data);
	if (!session)
		return;

	obs_source_t *target = nullptr;
	calldata_get_ptr(cd, "target", &target);
	session->set_reroute_target(target);
}

static void on_force_new_timestamp(void *data, calldata_t *cd)
{
	auto session = static_cast<AudioCaptureSession *>(data);
	if (session)
		session->force_new_timestamp();
}

AudioCaptureSession::AudioCaptureSession(obs_source_t *s, HANDLE &exit_evt) : ICaptureSession(s, exit_evt, true)
{
	proc_handler_t *ph = obs_source_get_proc_handler(s);
	if (ph) {
		proc_handler_add(ph, "void reroute_audio(in ptr target)", update_reroute_audio, this);
		proc_handler_add(ph, "void force_new_timestamp()", on_force_new_timestamp, this);
	}
}

AudioCaptureSession::~AudioCaptureSession()
{
	stop_capture();
	clear_reroute_target();
}

void AudioCaptureSession::set_use_audio(bool used)
{
	async_task->push_task([=]() {
		obs_source_set_audio_active(source, used);
		use_audio = used;
	});
}

void AudioCaptureSession::set_reroute_target(obs_source_t *target)
{
	std::lock_guard<std::recursive_mutex> lock(lock_reroute);
	clear_reroute_target();
	if (target)
		reroute_target = obs_source_get_weak_source(target);
}

bool AudioCaptureSession::init_ipc_buffer_impl(uint32_t index)
{
	auto name = GetFileMappingNamePrefix(index) + std::string(AUDIO_OUTPUT_BUFFER);
	data_reader = std::make_shared<AudioCircleBuffer>(name.c_str(), ipc_alignment);

	std::string error{};
	auto ret = data_reader->InitMapBuffer(error);
	if (ret != 0) {
		warn("Failed to init audio buffer: %s with lastError=%u, index=%u", error.c_str(), ret, index);
		return false;
	}

	return true;
}

void AudioCaptureSession::read_buffer_data()
{
	while (!CHandleWrapper::IsHandleSigned(exit_event, 0)) {
		if (!use_audio) {
			data_reader->DiscardCurrentData();
			break;
		}

		audio_item_header *header = nullptr;
		audio_item_sample *sample = nullptr;
		int index = data_reader->MapRead((void **)&header, sizeof(audio_item_header), (void **)&sample, sizeof(audio_item_sample));
		if (index >= 0) {
			obs_source_audio data = {};
			data.data[0] = (const uint8_t *)sample->data;   // the data is saved as packet mode
			data.frames = header->frames;                   // frames in per channel
			data.samples_per_sec = header->samples_per_sec; // defined as DEFAULT_AUDIO_SAMPLERATE in lens
			data.speakers = SPEAKERS_STEREO;                // defined as DEFAULT_AUDIO_CHANNEL in lens
			data.format = AUDIO_FORMAT_16BIT;               // defined as DEFAULT_AUDIO_FORMAT in lens
			data.timestamp = header->timestamp;             // in nanoseconds

			if (is_first_data) {
				is_first_data = false;
				first_timestamp = header->timestamp;
				info("[%s] first audio data received, frames: %u, samples_per_sec: %u, speakers: %d, format: %d, ts: %llu", get_session_type(), data.frames, data.samples_per_sec,
				     (int)data.speakers, (int)data.format, header->timestamp);
			}

			obs_source_t *target = get_reroute_target_ref();
			if (target) {
				// it is for dshow source, timestamp should be started with 0
				data.timestamp = header->timestamp - first_timestamp;

				obs_source_output_audio(target, &data);
				obs_source_release(target);
			} else {
				obs_source_output_audio(source, &data);
			}

			data_reader->UnmapRead(index);

			assert(header->channel == 2);
			update_status(LENS_STATUS::STATUS_CAPTURED);

		} else {
			break;
		}
	}
}

void AudioCaptureSession::clear_reroute_target()
{
	std::lock_guard<std::recursive_mutex> lock(lock_reroute);
	if (reroute_target) {
		obs_weak_source_release(reroute_target);
		reroute_target = nullptr;
	}
}

obs_source_t *AudioCaptureSession::get_reroute_target_ref()
{
	obs_source_t *target = nullptr;

	std::lock_guard<std::recursive_mutex> lock(lock_reroute);
	if (reroute_target) {
		target = obs_weak_source_get_source(reroute_target);
	}

	return target;
}

//---------------------------------------------------------------------------------------------------------
bool VideoCaptureSession::init_ipc_buffer_impl(uint32_t index)
{
	auto name = GetFileMappingNamePrefix(index) + std::string(VIDEO_OUTPUT_BUFFER);
	data_reader = std::make_shared<SharedHandleBuffer>(name.c_str(), ipc_alignment);

	std::string error{};
	auto ret = data_reader->InitMapBuffer(error);
	if (ret != 0) {
		warn("Failed to init video buffer: %s with lastError=%u, index=%u", error.c_str(), ret, index);
		return false;
	}

	return true;
}

void VideoCaptureSession::uninit_ipc_buffer(bool keepVideo)
{
	__super::uninit_ipc_buffer(keepVideo);

	// avoid render flicker. this function will be called again if new device failed to init
	if (!keepVideo)
		shared_handle = 0;
}

void VideoCaptureSession::read_buffer_data()
{
	while (!CHandleWrapper::IsHandleSigned(exit_event, 0)) {
		shared_handle_header *header = nullptr;
		shared_handle_sample *sample = nullptr;
		int index = data_reader->MapRead((void **)&header, sizeof(shared_handle_header), (void **)&sample, sizeof(shared_handle_sample));
		if (index >= 0) {
			if (sample->handle != shared_handle.load()) {
				shared_handle = (uint32_t)sample->handle;
				info("[%s] video handle is changed to %llu", get_session_type(), sample->handle);
			}
			data_reader->UnmapRead(index);
			update_status(LENS_STATUS::STATUS_CAPTURED);
			pls_set_lens_resolution(used_lens.load(), header->width, header->height);

		} else {
			break;
		}
	}
}

//---------------------------------------------------------------------------------------------------------
prism_lens_source::prism_lens_source(obs_data_t *settings, obs_source_t *source_) : source(source_)
{
	exit_event = CHandleWrapper::GetEvent(nullptr, true, nullptr);
	ResetEvent(exit_event); // firstly reset exit event, then startup thread

	audio_session = std::make_shared<AudioCaptureSession>(source, exit_event);
	audio_session->start_capture();

	video_session = std::make_shared<VideoCaptureSession>(source, exit_event);
	video_session->start_capture();

	bool actived_temp = obs_data_get_bool(settings, KEY_LENS_ACTIVE);
	audio_session->set_active(actived_temp);
	video_session->set_active(actived_temp);

	update(settings);
}

prism_lens_source::~prism_lens_source()
{
	SetEvent(exit_event); // firstly signal exit event to stop ipc thread
	audio_session->stop_capture();
	video_session->stop_capture();

	CHandleWrapper::CloseHandleEx(exit_event);
	clear_texture();
}

uint32_t prism_lens_source::get_video_width()
{
	if (!video_session->is_active())
		return 0;

	if (tex_width > 0)
		return tex_width;

	if (selected_video.load() >= MAX_LENS_COUNT)
		return 0;

	int width, height;
	pls_get_lens_resolution(selected_video.load(), &width, &height);

	return width;
}

uint32_t prism_lens_source::get_video_height()
{
	if (!video_session->is_active())
		return 0;

	if (tex_height > 0)
		return tex_height;

	if (selected_video.load() >= MAX_LENS_COUNT)
		return 0;

	int width, height;
	pls_get_lens_resolution(selected_video.load(), &width, &height);

	return height;
}

void prism_lens_source::tick()
{
	uint32_t handle = video_session->get_video_handle();
	if (!handle) {
		clear_texture();
		return;
	}

	obs_enter_graphics();
	std::shared_ptr<int> auto_clear(nullptr, [this](int *) { obs_leave_graphics(); });

	if (shared_texture) {
		if (gs_texture_get_shared_handle(shared_texture) == handle) {
			return; // handle is not changed
		}

		clear_texture(); // handle is changed, clear previous one
	}

	shared_texture = gs_texture_open_shared(handle);
	if (shared_texture) {
		tex_width = gs_texture_get_width(shared_texture);
		tex_height = gs_texture_get_height(shared_texture);
		auto fmt = gs_texture_get_color_format(shared_texture);

		copy_texture = gs_texture_create(tex_width, tex_height, fmt, 1, nullptr, 0);
		assert(copy_texture);

		timeout_happened = false;
		info("shared texture is created, handle: %u, size: %ux%u", handle, tex_width.load(), tex_height.load());
	}
}

void prism_lens_source::render()
{
	if (!video_session->is_active()) {
		pls_on_source_property_render(source, 0);
		return;
	}

	if (shared_texture && copy_texture) {
		pls_begin_taken_time(source, obs_source_get_id(source), "gs_texture_acquire_sync");
		auto code = gs_texture_acquire_sync(shared_texture, 0, 200); // 200ms timeout
		pls_end_taken_time(source, obs_source_get_id(source), "gs_texture_acquire_sync", time_ns_3ms);

		if (ETIMEDOUT == code) {
			if (!timeout_happened) {
				timeout_happened = true;
				warn("lens v2 timeout for handle=%u, lensRunning=%d", gs_texture_get_shared_handle(shared_texture), pls_is_lens_running());
			}

			// Even if it fails to acquire (except for ETIMEDOUT), we still try to render the shared texture,
			// because in the future the lens texture may remove the keyed mutex.
			return;
		}

		gs_copy_texture(copy_texture, shared_texture);
		if (0 == code)
			gs_texture_release_sync(shared_texture, 0);

		pls_begin_taken_time(source, obs_source_get_id(source), "render_lens_texture");
		render_texture(copy_texture);
		pls_end_taken_time(source, obs_source_get_id(source), "render_lens_texture", time_ns_3ms);

		pls_on_source_property_render(source, PROPERTY_RENDER_TIMEOUT);

	} else if (selected_video.load() < MAX_LENS_COUNT) {
		auto current_time = GetTickCount64();
		if (current_time >= update_time && (current_time - update_time) < WAIT_NEW_VIDEO_DURITION) {
			return; // if we are waiting for new video data, do not render logo
		}

		auto status = video_session->get_status();
		if (status > LENS_STATUS::STATUS_EXCEPTION_BEGIN && status < LENS_STATUS::STATUS_EXCEPTION_END) {
			pls_begin_taken_time(source, obs_source_get_id(source), "render_logo");
			auto logo = LensLogo::get_lens_logo(selected_video.load());
			if (logo && logo->tex) {
				render_texture(logo->tex);
			}
			pls_end_taken_time(source, obs_source_get_id(source), "render_logo", time_ns_3ms);
		}
	}
}

void prism_lens_source::render_texture(gs_texture_t *draw_texture)
{
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *const image = gs_effect_get_param_by_name(effect, "image");

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_effect_set_texture(image, draw_texture);

	uint32_t flip_flag = 0;
	if (flip_video.load())
		flip_flag = flip_flag | GS_FLIP_V;

	if (flip_video_h.load())
		flip_flag = flip_flag | GS_FLIP_U;

	while (gs_effect_loop(effect, "DrawSrgbDecompress"))
		gs_draw_sprite(draw_texture, flip_flag, 0, 0);

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}

void prism_lens_source::settings_old_to_new(obs_data_t *settings, const char *old_key, const char *new_key, const std::map<std::string, int> &map_rule)
{
	if (!settings || !old_key || !new_key || map_rule.empty()) {
		assert(false);
		return;
	}

	const char *old_id = obs_data_get_string(settings, old_key);
	if (old_id && *old_id) { // This is for compatibility with old device.

		auto itr = map_rule.find(old_id); // check if old_id is in the map
		if (itr != map_rule.end()) {
			obs_data_set_int(settings, new_key, itr->second); // convert old value to new value
			obs_data_set_string(settings, old_key, "");       // clear old value
		}
	}
}

void prism_lens_source::update(obs_data_t *settings)
{
	pls_set_property_update_delay(true);

	settings_old_to_new(settings, KEY_VIDEO_DEVICE_ID, KEY_LENS_VIDEO_INDEX, map_video_id);
	settings_old_to_new(settings, KEY_AUDIO_DEVICE_ID, KEY_LENS_AUDIO_INDEX, map_audio_id);

	int selected_a = obs_data_get_int(settings, KEY_LENS_AUDIO_INDEX);
	int selected_v = obs_data_get_int(settings, KEY_LENS_VIDEO_INDEX);
	bool actived_temp = obs_data_get_bool(settings, KEY_LENS_ACTIVE);
	bool use_audio_temp = obs_data_get_bool(settings, USE_CUSTOM_AUDIO);
	flip_video = obs_data_get_bool(settings, FLIP_IMAGE);
	flip_video_h = obs_data_get_bool(settings, FLIP_IMAGE_H);
	audio_session->set_device(selected_a);
	audio_session->set_active(actived_temp);
	audio_session->set_use_audio(use_audio_temp);
	audio_session->force_new_timestamp();

	if (selected_v != selected_video.load()) {
		clear_texture();
		selected_video = selected_v;
		update_time = GetTickCount64();
	}
	video_session->set_active(actived_temp);
	video_session->set_device(selected_v);

	video_session->get_task_pool()->push_task([=]() { pls_on_source_property_updated(source); });
}

void prism_lens_source::set_active(bool active_)
{
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_set_bool(settings, KEY_LENS_ACTIVE, active_);
	obs_data_release(settings);

	audio_session->set_active(active_);
	video_session->set_active(active_);

	pls_on_source_property_updated(source);
}

void prism_lens_source::clear_texture()
{
	if (shared_texture || copy_texture) {
		obs_enter_graphics();

		if (copy_texture) {
			gs_texture_destroy(copy_texture);
			copy_texture = nullptr;
		}

		if (shared_texture) {
			gs_texture_destroy(shared_texture);
			shared_texture = nullptr;
		}

		timeout_happened = false;
		tex_width = 0;
		tex_height = 0;

		obs_leave_graphics();
	}
}

//----------------------------------------------------------------------------------------------------------------
struct properties_data {
	prism_lens_source *input = nullptr;
	uint32_t old_index_v = 0;
	uint32_t old_index_a = 0;
};

static void properties_data_destroy(void *data)
{
	delete (properties_data *)data;
}

bool prism_lens_source::on_device_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	auto *data = (properties_data *)obs_properties_get_param(props);
	if (data && data->input) {
		auto new_index_v = obs_data_get_int(settings, KEY_LENS_VIDEO_INDEX);
		auto new_index_a = obs_data_get_int(settings, KEY_LENS_AUDIO_INDEX);

		if (new_index_v != new_index_a && new_index_v != data->old_index_v && new_index_a == data->old_index_a) {
			obs_data_set_int(settings, KEY_LENS_AUDIO_INDEX, new_index_v); // update audio automatically with same index as video
			obs_source_update(data->input->source, settings);
		}

		data->input->update(settings);

		data->old_index_v = obs_data_get_int(settings, KEY_LENS_VIDEO_INDEX);
		data->old_index_a = obs_data_get_int(settings, KEY_LENS_AUDIO_INDEX);
	}

	UNUSED_PARAMETER(p);
	return true;
}

bool prism_lens_source::activate_clicked(obs_properties_t *, obs_property_t *p, void *data)
{
	prism_lens_source *input = reinterpret_cast<prism_lens_source *>(data);

	if (input->audio_session->is_active()) {
		pls_on_source_property_changed(input->source, "deactivate");
		input->set_active(false);
		obs_property_set_description(p, TEXT_ACTIVATE);
	} else {
		pls_on_source_property_changed(input->source, "activate");
		input->set_active(true);
		obs_property_set_description(p, TEXT_DEACTIVATE);
	}

	return true;
}

static bool prismLens_custom_audio_clicked(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	bool use_custom_audio = obs_data_get_bool(settings, USE_CUSTOM_AUDIO);
	p = obs_properties_get(props, KEY_LENS_AUDIO_INDEX);
	obs_property_set_enabled(p, use_custom_audio);
	return true;
}

struct lens_device_data {
	size_t idx = 0;
	std::string audio_name;
	std::string video_name;
};

void pls_enum_lens_device_callback(void *param, size_t idx, const char *audio_name, const char *video_name, bool is_enabled, int cx, int cy)
{
	if (!audio_name || !video_name || !param || idx >= MAX_LENS_COUNT) {
		assert(false);
		return;
	}
	if (!is_enabled) {
		// if device is not enabled, we do not add it to the list
		return;
	}

	std::vector<lens_device_data> *devices = (std::vector<lens_device_data> *)param;

	if (devices->size() >= MAX_LENS_COUNT) {
		assert(false);
		return;
	}

	lens_device_data temp;
	temp.idx = idx;
	temp.audio_name = audio_name;
	temp.video_name = video_name;

	devices->push_back(temp);
}

obs_properties_t *prism_lens_source::get_properties(void *obj, const char *keyTip)
{
	pls_init_lens_resolution();

	std::vector<lens_device_data> devices;
	pls_enum_lens_devices(pls_enum_lens_device_callback, &devices);

	auto input = (prism_lens_source *)obj;

	auto *data = new properties_data;
	data->input = input;

	obs_properties_t *ppts = obs_properties_create();
	obs_properties_set_param(ppts, data, properties_data_destroy);

	QString program;
	obs_property_t *p = obs_properties_add_list(ppts, KEY_LENS_VIDEO_INDEX, TEXT_PRISM_LENS_DEVICE, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	if (pls_is_install_cam_studio(program)) {
		for (const auto &item : devices) {
			obs_property_list_add_int(p, item.video_name.c_str(), item.idx);
		}
		obs_property_set_modified_callback(p, prism_lens_source::on_device_changed);
	}

	const char *activateText = TEXT_ACTIVATE;
	if (input) {
		obs_data_t *settings = obs_source_get_settings(input->source);
		if (settings) {
			data->old_index_v = obs_data_get_int(settings, KEY_LENS_VIDEO_INDEX);
			data->old_index_a = obs_data_get_int(settings, KEY_LENS_AUDIO_INDEX);
			obs_data_release(settings);
		}

		if (input->audio_session && input->audio_session->is_active())
			activateText = TEXT_DEACTIVATE;
	}

	obs_properties_add_button(ppts, "activate", activateText, prism_lens_source::activate_clicked);
	pls_properties_add_tips(ppts, "tip", keyTip);

	p = obs_properties_add_bool(ppts, USE_CUSTOM_AUDIO, TEXT_CUSTOM_AUDIO);
	obs_property_set_modified_callback(p, prismLens_custom_audio_clicked);

	p = obs_properties_add_list(ppts, KEY_LENS_AUDIO_INDEX, TEXT_PRISM_LENS_AUDIO_DEVICE, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	if (pls_is_install_cam_studio(program)) {
		for (const auto &item : devices) {
			obs_property_list_add_int(p, item.audio_name.c_str(), item.idx);
		}
		obs_property_set_modified_callback(p, prism_lens_source::on_device_changed);
	}

	return ppts;
}

void get_defaults(obs_data_t *data, int lens_index)
{
	obs_data_set_default_int(data, KEY_LENS_VIDEO_INDEX, lens_index);
	obs_data_set_default_int(data, KEY_LENS_AUDIO_INDEX, lens_index);
	obs_data_set_default_bool(data, KEY_LENS_ACTIVE, true);
	obs_data_set_default_bool(data, USE_CUSTOM_AUDIO, true);
	obs_data_set_default_bool(data, FLIP_IMAGE, false);
	obs_data_set_default_bool(data, FLIP_IMAGE_H, false);
}

void init_common_attributes(obs_source_info &info)
{
	info.type = OBS_SOURCE_TYPE_INPUT;

	// in fact, this plugin does not need OBS_SOURCE_DO_NOT_DUPLICATE, it is kept to ensure the behavior is same as before
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;

	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * { return pls_new<prism_lens_source>(settings, source); };
	info.destroy = [](void *data) {
		auto src = static_cast<prism_lens_source *>(data);
		pls_delete(src);
	};

	info.get_width = [](void *data) {
		auto src = static_cast<prism_lens_source *>(data);
		return src->get_video_width();
	};
	info.get_height = [](void *data) {
		auto src = static_cast<prism_lens_source *>(data);
		return src->get_video_height();
	};

	info.video_tick = [](void *data, float) { static_cast<prism_lens_source *>(data)->tick(); };
	info.video_render = [](void *data, gs_effect_t *) { static_cast<prism_lens_source *>(data)->render(); };
	info.update = [](void *data, obs_data_t *settings) { static_cast<prism_lens_source *>(data)->update(settings); };
}

void register_lens_source()
{
	obs_source_info info = {};
	init_common_attributes(info);

	info.id = TEXT_PRISM_LENS_ID;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_PRISM_LENS);
	info.get_name = [](void *) { return TEXT_PRISM_LENS_NAME; };
	info.get_defaults = [](obs_data_t *data) { get_defaults(data, 0); };
	info.get_properties = [](void *data) { return prism_lens_source::get_properties(data, TEXT_PRISM_LENS_TIPS); };

	obs_register_source(&info);
}

void register_mobile_source()
{
	obs_source_info info = {};
	init_common_attributes(info);

	info.id = TEXT_PRISM_LENS_MOBILE_ID;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_PRISM_MOBILE);
	info.get_name = [](void *) { return TEXT_PRISM_LENS_MOBILE_NAME; };
	info.get_defaults = [](obs_data_t *data) { get_defaults(data, 2); };
	info.get_properties = [](void *data) { return prism_lens_source::get_properties(data, TEXT_PRISM_LENS_MOBILE_TIPS); };

	obs_register_source(&info);
}
