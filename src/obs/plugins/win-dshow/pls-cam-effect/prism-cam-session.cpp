#include "prism-cam-session.h"
#include <util/platform.h>
#include <util/util.hpp>

#define CAMERA_SESSESION_TICK_TIMER 1000
#define CAMERA_SESSESION_TICK_INTERVAL_SHORT 10  // in ms
#define CAMERA_SESSESION_TICK_INTERVAL_LONG 5000 // in ms
#define CAMERA_SESSESION_RUN_PROC_INTERVAL 5000  // in ms
#define CAMERA_SESSESION_WAIT_DATA_TIME 10       // in ms
#define CAMERA_SESSESION_WAIT_ENUM_DEVICE 5000   // in ms

#define CAMERA_CRASH_DIR "PRISMLiveStudio/crashDump/"

#define do_log(level, format, ...)                    \
	plog(level, "[CamSession : '%s' %p] " format, \
	     obs_source_get_name(source), source, ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

std::string PLSCamSession::nelo_session = "";
std::string PLSCamSession::prism_version = "";
std::string PLSCamSession::prism_cfg_path = "";
std::string PLSCamSession::prism_user_id = "";
std::string PLSCamSession::prism_user_id_raw = "";
int PLSCamSession::camera_auto_restart_times = 3;
std::string PLSCamSession::prism_project_name = "";
std::string PLSCamSession::prism_project_name_kr = "";
bool PLSCamSession::prism_subprocess_rebuild = false;

bool PLSCamSession::SetCamGlobalParam(obs_data_t *data)
{
	const char *nelo = obs_data_get_string(data, "nelo");
	const char *version = obs_data_get_string(data, "version");
	const char *path = obs_data_get_string(data, "path");
	const char *userId = obs_data_get_string(data, "userId");
	const char *userId_raw = obs_data_get_string(data, "userId_raw");
	int restart_times = obs_data_get_int(data, "camera_auto_restart_times");
	const char *projectName = obs_data_get_string(data, "projectName");
	const char *projectName_kr =
		obs_data_get_string(data, "projectName_kr");
	prism_subprocess_rebuild = obs_data_get_bool(data, "SubProcessRebuild");

	if (!nelo || !version || !path || !userId || !userId_raw ||
	    !projectName) {
		plog(LOG_WARNING,
		     "[CamSession] Received incorrect global params for camera plugin!");
		assert(false);
	}

	if (nelo) {
		nelo_session = nelo;
	}
	if (version) {
		prism_version = version;
	}
	if (path) {
		prism_cfg_path = path;
	}
	if (userId)
		prism_user_id = userId;

	if (userId_raw)
		prism_user_id_raw = userId_raw;

	if (projectName) {
		prism_project_name = projectName;
	}

	if (projectName_kr) {
		prism_project_name_kr = projectName_kr;
	}

	if (restart_times > 0) {
		camera_auto_restart_times = restart_times;
	}
	plog(LOG_INFO, "[CamSession] Camera auto restart times:%d",
	     camera_auto_restart_times);
	return (nelo && version && path && userId);
}

unsigned PLSCamSession::WindowMessageLoopThread(void *pParam)
{
	PLSCamSession *self = reinterpret_cast<PLSCamSession *>(pParam);
	self->WindowLoopThreadInner();
	return 0;
}

LRESULT PLSCamSession::WindowMessageProcedure(HWND hWnd, UINT nMsg,
					      WPARAM wParam, LPARAM lParam)
{
	PLSCamSession *self =
		(PLSCamSession *)::GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (nMsg) {
	case WM_CREATE: {
		SetTimer(hWnd, CAMERA_SESSESION_TICK_TIMER,
			 CAMERA_SESSESION_TICK_INTERVAL_SHORT, NULL);
		break;
	}

	case WM_DESTROY: {
		PostQuitMessage(0); // exit message loop
		break;
	}

	case WM_TIMER: {
		if (self && wParam == CAMERA_SESSESION_TICK_TIMER) {
			self->WindowLoopTimerTick();
		}
		break;
	}

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}
	}

	return DefWindowProcA(hWnd, nMsg, wParam, lParam);
}

//--------------------------------------------------------------------------------------
PLSCamSession::PLSCamSession(ICamSessionCb *cb, obs_source_t *src)
	: callback(cb), source(src)
{
	session_id = GenerateGuid();
	info("%s Session GUID: %s", __FUNCTION__, session_id.c_str());

	video_enum_done = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	audio_enum_done = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	char temp[20];
	sprintf_s(temp, ARRAY_COUNT(temp), "%p", this);
	local_wnd_class_name = session_id + std::string(temp);
	local_wnd_loop_thread = (HANDLE)_beginthreadex(
		0, 0, WindowMessageLoopThread, this, 0, 0);

	std::string name = session_id + std::string(VIDEO_OUTPUT_BUFFER);
	video_reader = std::shared_ptr<SharedHandleBuffer>(
		new SharedHandleBuffer(name.c_str()));
	DWORD ret = video_reader->InitMapBuffer();
	if (ret != 0) {
		warn("Failed to init map for video! error:%u", ret);
		assert(false);
	}

	name = session_id + std::string(AUDIO_OUTPUT_BUFFER);
	audio_reader = std::shared_ptr<AudioCircleBuffer>(
		new AudioCircleBuffer(name.c_str()));
	ret = audio_reader->InitMapBuffer();
	if (ret != 0) {
		warn("Failed to init map for audio! error:%u", ret);
		assert(false);
	}

	name = session_id + std::string(SOURCE_ALIVE_FLAG);
	camera_source_alive_flag = CHandleWrapper::GetEvent(name.c_str(), true);
}

PLSCamSession::~PLSCamSession()
{
	info("%s", __FUNCTION__);

	if (::IsWindow(local_wnd_handle)) {
		SendMessage(local_wnd_handle, WM_CLOSE, 0, 0);
	}

	CHandleWrapper::WaitThreadEnd(local_wnd_loop_thread);

	CHandleWrapper::CloseHandleEx(child_process_handle);
	CHandleWrapper::CloseHandleEx(camera_source_alive_flag);
	CHandleWrapper::CloseHandleEx(video_enum_done);
	CHandleWrapper::CloseHandleEx(audio_enum_done);
}

void PLSCamSession::SetDShowConfig(obs_data_t *settings)
{
	EnableRestore();

	std::string dev = obs_data_get_string(settings, "video_device_id");
	if (dev != video_device_id) {
		child_exited_count = 0;
		video_device_id = dev;
	}

	const char *json = obs_data_get_json(settings);
	if (json) {
		message_queue.PushMessage(effect_message_dshow_config, json,
					  true);
	}
}

void PLSCamSession::SetBeautyConfig(obs_data_t *settings)
{
	const char *json = obs_data_get_json(settings);
	if (json) {
		message_queue.PushMessage(effect_message_beauty_config, json,
					  true);
	}
}

void PLSCamSession::SetSegmenConfig(obs_data_t *settings)
{
	const char *json = obs_data_get_json(settings);
	if (json) {
		message_queue.PushMessage(effect_message_segmen_config, json,
					  true);
	}
}

void PLSCamSession::SetAudioSettings(struct obs_audio_info &oai)
{
	uint32_t channel = get_audio_channels(oai.speakers);
	if (channel > IPC_AUDIO_MAX_CHANNEL) {
		channel = IPC_AUDIO_MAX_CHANNEL;
	}

	uint32_t samplerate = oai.samples_per_sec;
	if (samplerate > IPC_AUDIO_MAX_SAMPLERATE) {
		samplerate = IPC_AUDIO_MAX_SAMPLERATE;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, MSGKEY_AUDIO_SAMPLERATE, samplerate);
	obs_data_set_int(settings, MSGKEY_AUDIO_CHANNEL, channel);
	obs_data_set_int(settings, MSGKEY_AUDIO_FORMAT,
			 TransFormatToFfmpeg(AUDIO_FORMAT_FLOAT_PLANAR));

	const char *json = obs_data_get_json(settings);
	message_queue.PushMessage(effect_message_audio_config, json, true);

	obs_data_release(settings);
}

void PLSCamSession::SetDirectX(struct gs_luid &luid)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, MSGKEY_D3D11_LOW_PART, luid.low_part);
	obs_data_set_int(settings, MSGKEY_D3D11_HIGH_PART, luid.high_part);

	info("SetDirectX data: low_part[%ld], high_part[%ld]", luid.low_part,
	     luid.high_part);

	const char *json = obs_data_get_json(settings);
	message_queue.PushMessage(effect_message_d3d11_config, json, true);

	obs_data_release(settings);
}

void PLSCamSession::SetDShowActive(bool active)
{
	if (active) {
		EnableRestore();
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, MSGKEY_ACTIVE_FLAG, active);

	const char *json = obs_data_get_json(settings);
	message_queue.PushMessage(effect_message_dshow_active, json, true);

	obs_data_release(settings);
}

void PLSCamSession::OpenDShowDialog(HWND parent, enum DShow::DialogType type)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, MSGKEY_OPEN_DIALOG_PARENT,
			 (long long)parent);
	obs_data_set_int(settings, MSGKEY_OPEN_DIALOG_TYPE, (long long)type);

	const char *json = obs_data_get_json(settings);
	message_queue.PushMessage(effect_message_open_dialog, json, true);

	obs_data_release(settings);
}

void PLSCamSession::RequestEnumDevice(bool request_video)
{
	if (request_video) {
		message_queue.PushMessage(
			effect_message_enum_video_device_request, "", true);
		video_list_json = "";
		::ResetEvent(video_enum_done);
	} else {
		message_queue.PushMessage(
			effect_message_enum_audio_device_request, "", true);
		audio_list_json = "";
		::ResetEvent(audio_enum_done);
	}
}

void PLSCamSession::SetExtraInfo()
{
	obs_data_t *settings = obs_data_create();
	//TODO add some other info
	const char *json = obs_data_get_json(settings);
	message_queue.PushMessage(effect_message_extra_info, json, true);
	obs_data_release(settings);
}

void PLSCamSession::EnableRestore()
{
	retry_run_child = true;
}

bool PLSCamSession::IsChildProcessNormal()
{
	return child_process_running;
}

std::string PLSCamSession::ReadEnumDeviceList(bool request_video)
{
	if (!IsChildProcessNormal()) {
		info("Child process is invalid and enum device locally.");
		return "";
	}

	if (request_video) {
		if (CHandleWrapper::IsHandleSigned(
			    video_enum_done,
			    CAMERA_SESSESION_WAIT_ENUM_DEVICE)) {
			return video_list_json;
		} else {
			info("Timeout to wait video device from sub-process and to enum list locally.");
			return "";
		}
	} else {
		if (CHandleWrapper::IsHandleSigned(
			    audio_enum_done,
			    CAMERA_SESSESION_WAIT_ENUM_DEVICE)) {
			return audio_list_json;
		} else {
			info("Timeout to wait audio device from sub-process and to enum list locally.");
			return "";
		}
	}
}

bool PLSCamSession::ReadOutputAudio(audio_item_header &header,
				    audio_item_sample &sample)
{
	if (!audio_reader->IsBufferValid()) {
		return false;
	} else {
		audio_reader->WaitBufferChanged(
			CAMERA_SESSESION_WAIT_DATA_TIME);
		return audio_reader->ReadItemData(&header, sizeof(header),
						  &sample, sizeof(sample));
	}
}

int PLSCamSession::MapOutputVideo(shared_handle_header &header,
				  shared_handle_sample &sample)
{
	if (!video_reader->IsBufferValid()) {
		return -1;
	} else {
		video_reader->WaitBufferChanged(
			CAMERA_SESSESION_WAIT_DATA_TIME);
		return video_reader->MapItemData(&header, sizeof(header),
						 &sample, sizeof(sample));
	}
}

void PLSCamSession::UnmapOutputVideo(int index)
{
	if (video_reader->IsBufferValid()) {
		video_reader->UnmapItemData(index);
	}
}

void PLSCamSession::PushEvent(obs_source_event_type type, int sub_code,
			      bool only_send_once)
{
	{
		CAutoLockCS autoLock(exception_lock);

		if (only_send_once) {
			std::map<obs_source_event_type, PLSEventInfo>::iterator
				itr = exceptions.find(type);
			if (itr != exceptions.end() &&
			    itr->second.notify_done) {
				return; // exception has been notified before
			}
		}

		PLSEventInfo info;
		info.type = type;
		info.sub_code = sub_code;
		if (type == OBS_SOURCE_EXCEPTION_SENSETIME) {
			info.notify_done = false;
		} else
			info.notify_done = true;

		exceptions[type] = info;
	}

	callback->OnEventHappenned(type, sub_code);
}

std::string PLSCamSession::GenerateGuid()
{
	GUID guid;
	CoCreateGuid(&guid);

	char buf[64] = {0};
	sprintf_s(buf, ARRAY_COUNT(buf),
		  "{%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X}", guid.Data1,
		  guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
		  guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
		  guid.Data4[6], guid.Data4[7]);

	return std::string(buf);
}

std::wstring PLSCamSession::GetProcessPath()
{
	WCHAR szFilePath[MAX_PATH] = {};
	GetModuleFileNameW(NULL, szFilePath, MAX_PATH);

	int nLen = (int)wcslen(szFilePath);
	for (int i = nLen - 1; i >= 0; --i) {
		if (szFilePath[i] == '\\') {
			szFilePath[i + 1] = 0;
			break;
		}
	}

	return std::wstring(szFilePath) +
	       std::wstring(CAMERA_CHILD_PROCESS_NAME);
}

HWND PLSCamSession::CreateLocalWindow()
{
	WNDCLASSA wc = {0};
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = WindowMessageProcedure;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = local_wnd_class_name.c_str();

	ATOM nAtom = RegisterClassA(&wc);
	if (0 == nAtom) {
		warn("Failed to register window! error:%u", GetLastError());

		assert(false);
		return 0;
	}

	HWND hWnd = CreateWindowA(local_wnd_class_name.c_str(),
				  local_wnd_class_name.c_str(), WS_POPUPWINDOW,
				  0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL),
				  NULL);
	if (!::IsWindow(hWnd)) {
		UnregisterClassA(local_wnd_class_name.c_str(),
				 GetModuleHandle(NULL));

		warn("Failed to create window! error:%u", GetLastError());

		assert(false);
		return 0;
	}

	::SetWindowLongPtr(hWnd, GWLP_USERDATA,
			   reinterpret_cast<LONG_PTR>(this));
	return hWnd;
}

void PLSCamSession::ClearPipe()
{
	if (pipe_server.get()) {
		pipe_server->UninitPipeIO();
	}

	if (pipe_io.get()) {
		pipe_io->Stop();
	}

	pipe_io.reset(); // release pipe-io firstly
	pipe_server.reset();
}

obs_source_event_type
PLSCamSession::TransException(cam_effect_exception_type src)
{
	switch (src) {
	case effect_exception_sensetime:
		return OBS_SOURCE_EXCEPTION_SENSETIME;

	case effect_exception_d3d:
		return OBS_SOURCE_EXCEPTION_D3D;

	case effect_exception_video_device:
		return OBS_SOURCE_EXCEPTION_VIDEO_DEVICE;

	case effect_exception_unknown:
	default:
		assert(false);
		return OBS_SOURCE_EXCEPTION_NONE;
	}
}

obs_source_error PLSCamSession::TransInvalidCode(cam_source_invalid_code src)
{
	switch (src) {
	case invalid_code_ok:
		return OBS_SOURCE_ERROR_OK;

	case invalid_code_unknown:
		return OBS_SOURCE_ERROR_UNKNOWN;

	case invalid_code_not_found_device:
		return OBS_SOURCE_ERROR_NOT_FOUND;

	case invalid_code_be_using:
		return OBS_SOURCE_ERROR_BE_USING;

	default:
		assert(false);
		return OBS_SOURCE_ERROR_UNKNOWN;
	}
}

enum AVSampleFormat PLSCamSession::TransFormatToFfmpeg(enum audio_format format)
{
	switch (format) {
	case AUDIO_FORMAT_UNKNOWN:
		return AV_SAMPLE_FMT_S16;
	case AUDIO_FORMAT_U8BIT:
		return AV_SAMPLE_FMT_U8;
	case AUDIO_FORMAT_16BIT:
		return AV_SAMPLE_FMT_S16;
	case AUDIO_FORMAT_32BIT:
		return AV_SAMPLE_FMT_S32;
	case AUDIO_FORMAT_FLOAT:
		return AV_SAMPLE_FMT_FLT;
	case AUDIO_FORMAT_U8BIT_PLANAR:
		return AV_SAMPLE_FMT_U8P;
	case AUDIO_FORMAT_16BIT_PLANAR:
		return AV_SAMPLE_FMT_S16P;
	case AUDIO_FORMAT_32BIT_PLANAR:
		return AV_SAMPLE_FMT_S32P;
	case AUDIO_FORMAT_FLOAT_PLANAR:
		return AV_SAMPLE_FMT_FLTP;
	}

	/* shouldn't get here */
	return AV_SAMPLE_FMT_S16;
}

void PLSCamSession::WindowLoopThreadInner()
{
	local_wnd_handle = CreateLocalWindow();
	if (!::IsWindow(local_wnd_handle)) {
		assert(false);
		return;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClassA(local_wnd_class_name.c_str(), GetModuleHandle(NULL));
}

void PLSCamSession::WindowLoopTimerTick()
{
	if (!CheckCameraProcess()) {
		// child process is not ready
		heartbeat_timeout = false;
		return;
	}

	bool timeout = CheckHeartBeatTimeout();
	if (timeout) {
#ifndef DEBUG
		BOOL res = TerminateProcess(child_process_handle,
					    cam_effect_heartbeat_timeout);
		if (res) {
			warn("Terminate camera process with code=%d because heartbeat is timeout. [sucessed] pid:%u pHandle:%X",
			     cam_effect_heartbeat_timeout, child_process_id,
			     child_process_handle);
		} else {
			warn("Terminate camera process with code=%d because heartbeat is timeout. [Failed: %u] pid:%u pHandle:%X",
			     cam_effect_heartbeat_timeout, GetLastError(),
			     child_process_id, child_process_handle);
		}
#endif
		return;
	}

	while (true) {
		message_info info;
		if (!message_queue.PopMessage(info)) {
			break;
		}
		if (!TrySendMessage(info)) {
			break;
		}
	}
}

bool PLSCamSession::CheckCameraProcess()
{
	if (!CHandleWrapper::IsHandleValid(child_process_handle)) {
		if (RunCameraProcess()) {
			SetTimer(local_wnd_handle, CAMERA_SESSESION_TICK_TIMER,
				 CAMERA_SESSESION_TICK_INTERVAL_SHORT, NULL);
		} else {
			// can't run child process, so we increase the retry interval
			SetTimer(local_wnd_handle, CAMERA_SESSESION_TICK_TIMER,
				 CAMERA_SESSESION_TICK_INTERVAL_LONG, NULL);

			callback->OnImageNormalChanged(false);
			callback->OnValidStateChanged(false,
						      OBS_SOURCE_ERROR_UNKNOWN);
		}
		return false;
	} else {
		if (!CHandleWrapper::IsHandleSigned(child_process_handle)) {
			return CheckFindWindow(); // camera process is running
		}

		// camera process exited because of an exception!
		++child_exited_count;
		if (child_exited_count > camera_auto_restart_times) {
			retry_run_child = false;
		}

		DWORD exit_code = 0;
		GetExitCodeProcess(child_process_handle, &exit_code);

		bool disappear =
			!(exit_code == cam_effect_normal_exit ||
			  (exit_code >= cam_effect_process_param_error &&
			   exit_code < cam_effect_diasppear_flag) ||
			  exit_code == EXCEPTION_EXECUTE_HANDLER);
		char src[64];
		sprintf(src, "%p", source);
		if (disappear)
			bdisappear(CAMERA_CHILD_PROCESS_NAME_U,
				   std::to_string(
					   GetProcessId(child_process_handle))
					   .c_str(),
				   src);

		warn("Child process exited! exit_code:%u exited_count:%u disappear:%s pid:%u pHandle:%X",
		     exit_code, child_exited_count.load(),
		     disappear ? "true" : "false", child_process_id,
		     child_process_handle);

		PushEvent(OBS_SOURCE_DEVICE_UNSTABLE, 0, false);

		ClearPipe();

		CHandleWrapper::CloseHandleEx(child_process_handle);
		child_process_wnd_handle = 0;
		child_process_running = false;
		callback->OnImageNormalChanged(false);
		callback->OnValidStateChanged(false, OBS_SOURCE_ERROR_UNKNOWN);

		OnParseCrashAndSendAction();

		return false;
	}
}

bool PLSCamSession::RunCameraProcess()
{
	CHandleWrapper::CloseHandleEx(child_process_handle);
	child_process_wnd_handle = 0;

	DWORD current_time = GetTickCount();
	bool wait_run_time = (current_time > previous_run_time &&
			      (current_time - previous_run_time) <
				      CAMERA_SESSESION_RUN_PROC_INTERVAL);
	if (wait_run_time || !retry_run_child) {
		return true;
	}

	previous_run_time = current_time;

	std::wstring process_path = GetProcessPath();
	if (process_path.empty()) {
		warn("Failed to get path of child process.");
		assert(false);
		return false;
	}

	if (!os_is_file_exist_ex(process_path.c_str())) {
		PushEvent(OBS_SOURCE_EXCEPTION_NO_FILE, 0);

		warn("Failed to find file of child process.");
		assert(false);
		return false;
	}

	{
		CAutoLockCS autoLock(exception_lock);
		exceptions.clear();
	}

	pipe_server = std::shared_ptr<PipeServer>(new PipeServer);
	pipe_server->InitPipeIO();

	pipe_io = std::shared_ptr<PipeIO>(new PipeIO);
	pipe_io->StartServer(pipe_server.get(), this);

	HANDLE client_write_handle;
	HANDLE client_read_handle;
	pipe_server->GetClientHandle(client_write_handle, client_read_handle);

	std::string filePath = CAMERA_CRASH_DIR + GenerateGuid() + ".json";
	char path[512] = {0};
	if (os_get_config_path(path, sizeof(path), filePath.c_str()) > 0)
		device_crash_path = path;

	wchar_t cmd[2048];
	swprintf_s(
		cmd, ARRAY_COUNT(cmd),
		L"\"%s\" \"%hs\" \"%hs\" \"%hs\" \"%hs\" \"%hs\" \"%hs\" %llu %p %llu %llu %llu \"%hs\" \"%hs\" \"%hs\"",
		process_path.c_str(), session_id.c_str(),
		PLSCamSession::prism_version.c_str(),
		PLSCamSession::nelo_session.c_str(),
		PLSCamSession::prism_user_id.c_str(),
		PLSCamSession::prism_cfg_path.c_str(),
		device_crash_path.c_str(), (ULONG64)local_wnd_handle, source,
		(ULONG64)GetCurrentProcess(), (ULONG64)client_read_handle,
		(ULONG64)client_write_handle, prism_project_name.c_str(),
		PLSCamSession::prism_project_name_kr.c_str(),
		PLSCamSession::prism_user_id_raw.c_str());

	PROCESS_INFORMATION pi = {};
	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK;
	si.wShowWindow = SW_HIDE;

	BOOL bOK = CreateProcessW((LPWSTR)process_path.c_str(), cmd, NULL, NULL,
				  TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	if (!bOK) {
		ClearPipe();
		warn("Failed to run child process! error:%u", GetLastError());
		assert(false);
		return false;
	}

	CloseHandle(pi.hThread);
	child_process_handle = pi.hProcess;
	child_process_id = pi.dwProcessId;

	info("Success to run child process. pid:%u pHandle:%X", pi.dwProcessId,
	     pi.hProcess);

	return true;
}

bool PLSCamSession::CheckFindWindow()
{
	if (::IsWindow(child_process_wnd_handle)) {
		return true; // window handle of child process is found already
	} else {
		child_process_wnd_handle =
			::FindWindowA(session_id.c_str(), NULL);
		if (::IsWindow(child_process_wnd_handle)) {
			info("Found window of child process. HWND:%X",
			     child_process_wnd_handle);

			child_process_running = true;
			heartbeat_timeout = false;
			previous_heartbeat = GetTickCount();
			message_queue.PushMessageForRestore(); // restore status
		}

		return false;
	}
}

bool PLSCamSession::CheckHeartBeatTimeout()
{
	bool timeout =
		(GetTickCount() - previous_heartbeat >= HEART_BEAT_TIMEOUT);

	if (heartbeat_timeout != timeout) {
		heartbeat_timeout = timeout;

		if (timeout) {
			warn("Heartbeat of child process is timeout.");
		} else {
			info("Heartbeat of child process is normal.");
		}
	}

	return timeout;
}

bool PLSCamSession::TrySendMessage(message_info &info)
{
	PipeMessageHeader hdr;
	hdr.msg_id = info.msg_id;
	if (info.msg_json.empty()) {
		hdr.body_len = 0;
	} else {
		hdr.body_len = info.msg_json.length() + 1;
	}

	pipe_io->PushSendMsg(hdr, info.msg_json.c_str());
	return true;
}

void PLSCamSession::OnMessage(PipeMessageHeader hdr, std::shared_ptr<char> body)
{
	cam_session_message msg = (cam_session_message)hdr.msg_id;

	if (msg != effect_message_process_heartbeat) {
		info("PRISM received message from camera process : %s",
		     GetMessageName(msg).c_str());
	}

	switch (msg) {
	case effect_message_process_heartbeat: {
		previous_heartbeat = GetTickCount();
		break;
	}

	case effect_message_sensetime_inited: {
		PushEvent(OBS_SOURCE_SENSEAR_ACTION,
			  0); // event for sensetime's inited
		break;
	}

	case effect_message_dshow_valid: {
		OnDShowValid(body);
		break;
	}

	case effect_message_image_normal: {
		OnImageNormal(body);
		break;
	}

	case effect_message_process_exception: {
		OnException(body);
		break;
	}

	case effect_message_enum_video_device_response: {
		video_list_json = body != NULL ? (char *)body.get() : "";
		::SetEvent(video_enum_done);
		break;
	}

	case effect_message_enum_audio_device_response: {
		audio_list_json = body != NULL ? (char *)body.get() : "";
		::SetEvent(audio_enum_done);
		break;
	}

	case effect_message_setupbuffering: {
		OnSetupBuffering(body);
		break;
	}

	default: {
		warn("Received unknown message type : %d", msg);
		break;
	}
	}
}

void PLSCamSession::OnError(pipe_error_type type, unsigned code)
{
	warn("<Prism-Endpoint> Pipe error happened. error:%s code:%u",
	     GetPipeError(type).c_str(), code);
}

bool PLSCamSession::IsSubprocessRebuild()
{
	return PLSCamSession::prism_subprocess_rebuild;
}

long long PLSCamSession::TransExceptionCode(long long code)
{
	switch (code) {
	case SENSETIME_ERROR_LICENSE_NOT_EXIST:
		return OBS_SOURCE_EXCEPTION_SUB_CODE_LICENSE_NOT_EXIST;

	case SENSETIME_ERROR_LICENSE_EXPIRE:
		return OBS_SOURCE_EXCEPTION_SUB_CODE_LICENSE_EXPIRE;
	case SENSETIME_ERROR_NETWORK_ERROR:
		return OBS_SOURCE_EXCEPTION_SUB_CODE_NETWORK_ERROR;

	default:
		return code;
	}
}

void PLSCamSession::OnException(std::shared_ptr<char> body)
{
	if (body.get()) {
		obs_data_t *data =
			obs_data_create_from_json((char *)body.get());
		assert(data);
		if (data) {
			cam_effect_exception_type type =
				(cam_effect_exception_type)obs_data_get_int(
					data, MSGKEY_EXCEPTION_TYPE);
			long long sub_code =
				obs_data_get_int(data, MSGKEY_EXCEPTION_CODE);
			obs_data_release(data);

			if (type == effect_exception_sensetime) {
				sub_code = TransExceptionCode(sub_code);
			}

			obs_source_event_type event = TransException(type);
			assert(event != OBS_SOURCE_EXCEPTION_NONE);
			if (event != OBS_SOURCE_EXCEPTION_NONE) {
				PushEvent(event, (int)sub_code);
				return;
			}
		}
	}

	warn("Received invalid body for exception.");
}

void PLSCamSession::OnImageNormal(std::shared_ptr<char> body)
{
	if (body.get()) {
		obs_data_t *data =
			obs_data_create_from_json((char *)body.get());
		assert(data);
		if (data) {
			bool valid = obs_data_get_bool(
				data, MSGKEY_IMAGE_NORMAL_FLAG);
			obs_data_release(data);

			callback->OnImageNormalChanged(valid);
			return;
		}
	}

	warn("Received invalid body for image-normal.");
}

void PLSCamSession::OnDShowValid(std::shared_ptr<char> body)
{
	if (body.get()) {
		obs_data_t *data =
			obs_data_create_from_json((char *)body.get());
		assert(data);
		if (data) {
			bool valid = obs_data_get_bool(data, MSGKEY_VALID_FLAG);
			cam_source_invalid_code error_code =
				(cam_source_invalid_code)obs_data_get_int(
					data, MSGKEY_VALID_ERROR_CODE);
			obs_data_release(data);

			callback->OnValidStateChanged(
				valid, TransInvalidCode(error_code));
			return;
		}
	}

	warn("Received invalid body for camera-valid.");
}

void PLSCamSession::OnSetupBuffering(std::shared_ptr<char> body)
{
	if (body.get()) {
		obs_data_t *data =
			obs_data_create_from_json((char *)body.get());
		assert(data);
		if (data) {
			bool useBuffering = obs_data_get_bool(
				data, MSGKEY_VIDEO_USING_BUFFERING_FLAG);
			bool isDecoupled = obs_data_get_bool(
				data, MSGKEY_VIDEO_DECOUPLED_FLAG);
			obs_data_release(data);

			callback->OnSetupBuffering(useBuffering, isDecoupled);
			return;
		}
	}

	warn("Received invalid body for setup buffering.");
}

void PLSCamSession::OnParseCrashAndSendAction()
{
	if (device_crash_path.empty()) {
		warn("[PLSCamSession] Device crash file is empty. obs_source: %p",
		     source);
		return;
	}

	obs_data_t *crashData = obs_data_create_from_json_file_safe(
		device_crash_path.c_str(), "bak");
	if (!crashData) {
		warn("[PLSCamSession] Device crash file data is error. obs_source: %p",
		     source);
		return;
	}
	int crashThreadId = obs_data_get_int(crashData, "crashThreadId");
	if (!crashThreadId) {
		obs_data_release(crashData);
		goto remove;
	}

	const char *deviceId = obs_data_get_string(crashData, "deviceId");
	const char *deviceName = obs_data_get_string(crashData, "deviceName");
	bool invoking = obs_data_get_bool(crashData, "invoking");
	bool enumerating = obs_data_get_bool(crashData, "enumerating");
	int invokeThreadId = obs_data_get_int(crashData, "invokeThreadId");
	int enumThreadId = obs_data_get_int(crashData, "enumThreadId");
	obs_data_array_t *threads = obs_data_get_array(crashData, "threads");
	int deviceDriver = obs_data_get_int(crashData, "deviceDriver");

	bool isSend = false;

	if (deviceDriver) {
		isSend = true;
	} else if (enumerating && crashThreadId == enumThreadId) {
		deviceName = "DeviceEnumerating";
		isSend = true;
	} else if (invoking && crashThreadId == enumThreadId) {
		isSend = true;
	} else {
		size_t num = obs_data_array_count(threads);
		for (int i = 0; i < num; ++i) {
			obs_data_t *thread = obs_data_array_item(threads, i);
			int id = obs_data_get_int(thread, "id");
			int type = obs_data_get_int(thread, "type");
			obs_data_release(thread);
			if (crashThreadId == id) {
				isSend = true;
				break;
			}
		}
	}

	if (isSend) {
		callback->OnNotifyActionEvent("crash", "device", "cam_device",
					      deviceName);
		info("[PLSCamSession] Child process crashed! Send action log : device name is %s. obs_source: %p",
		     deviceName, source);
	} else {
		info("[PLSCamSession] Child process crashed! Crash not caused by the device. Not Send action log : device name is %s. obs_source: %p",
		     deviceName, source);
	}

	obs_data_array_release(threads);
	obs_data_release(crashData);

remove:
	//remove file.
	if (remove(device_crash_path.c_str()) == -1)
		warn("[PLSCamSession] Remove the json file to clear device information failed.");
}
