#include "prism-cam-effect.h"
#include <util/platform.h>
#include <util/util.hpp>
#include "prism-cam-ground-def.h"

#ifdef DEBUG
//#define DEBUG_EFFECT_FPS
#endif

std::string PLSCamEffect::nelo_session = "";
std::string PLSCamEffect::prism_version = "";
std::string PLSCamEffect::prism_cfg_path = "";

//----------------------------------------------------------------------
PLSFpsChecker::PLSFpsChecker(char *flag)
	: flag_name(flag ? flag : ""), start_time(0), frame_count(0)
{
}

void PLSFpsChecker::Reset()
{
#ifdef DEBUG_EFFECT_FPS
	frame_count = 0;
	start_time = 0;
#endif
}

void PLSFpsChecker::OnFrame()
{
#ifdef DEBUG_EFFECT_FPS
	if (frame_count <= 0 || start_time <= 0) {
		frame_count = 0;
		start_time = timeGetTime();
	}

	++frame_count;

	DWORD time_interval = timeGetTime() - start_time;
	if (time_interval < 3000) {
		return; // calculate fps every 3 seconds
	}

	double fps = (double(frame_count) * 1000.0) / double(time_interval);
	blog(LOG_INFO, "FPF of %s : %d", flag_name.c_str(), (int)fps);

	Reset();
#endif
}

//----------------------------------------------------------------------
void PLSCamEffect::SaveTexture(gs_texture_t *tex, char *path)
{
	obs_enter_graphics();

	uint32_t cx = gs_texture_get_width(tex);
	uint32_t cy = gs_texture_get_height(tex);
	enum gs_color_format fmt = gs_texture_get_color_format(tex);

	if (fmt == GS_RGBA || fmt == GS_BGRX || fmt == GS_BGRA) {
		gs_stagesurf_t *surface = gs_stagesurface_create(cx, cy, fmt);
		if (surface) {
			gs_stage_texture(surface, tex);

			uint8_t *data = NULL;
			uint32_t linesize = 0;
			if (gs_stagesurface_map(surface, &data, &linesize)) {
				save_as_bitmap_file(path, data, linesize, cx,
						    cy, 4, true);
				gs_stagesurface_unmap(surface);
			}

			gs_stagesurface_destroy(surface);
		}
	}

	obs_leave_graphics();
}

bool PLSCamEffect::SetGlobalParam(obs_data_t *data)
{
	const char *nelo = obs_data_get_string(data, "nelo");
	const char *version = obs_data_get_string(data, "version");
	const char *path = obs_data_get_string(data, "path");

	if (!nelo || !version || !path) {
		plog(LOG_ERROR, "Receive incorrect params for beauty!");
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

	return (nelo && version && path);
}

unsigned PLSCamEffect::WindowLoopThread(void *pParam)
{
	PLSCamEffect *self = reinterpret_cast<PLSCamEffect *>(pParam);
	self->WindowLoopThreadInner();
	return 0;
}

unsigned PLSCamEffect::MessageLoopThread(void *pParam)
{
	PLSCamEffect *self = reinterpret_cast<PLSCamEffect *>(pParam);
	self->MessageLoopThreadInner();
	return 0;
}

LRESULT PLSCamEffect::EffectWndProc(HWND hWnd, UINT nMsg, WPARAM wParam,
				    LPARAM lParam)
{
	switch (nMsg) {
	case WM_DESTROY: {
		plog(LOG_INFO, "PLSCamEffect: Window is destroied.");
		PostQuitMessage(0); // exit message loop
		break;
	}

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}

	case WM_COPYDATA: {
		PLSCamEffect *self =
			(PLSCamEffect *)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (self) {
			self->OnCopyData((COPYDATASTRUCT *)lParam);
		}
		break;
	}
	}

	return DefWindowProc(hWnd, nMsg, wParam, lParam);
}

//----------------------------------------------------------------------
PLSCamEffect::PLSCamEffect()
	: process_hwnd(0),
	  process_handle(0),
	  previous_heartbeat(),
	  output_video_handle(0),
	  output_video_tex(NULL),
	  effect_state(""),
	  fps_device("video-device"),
	  fps_push_handle("input-handle"),
	  fps_get_handle("output-handle"),
	  waiting_onoff_change(false),
	  source_hwnd(0),
	  fin_output_texture(NULL),
	  dshow_source(nullptr),
	  output_tex_flip(false)
{
	session_id = GenerateGuid();

	char temp[20];
	sprintf_s(temp, ARRAY_COUNT(temp), "%p", this);
	source_hwnd_class = session_id + std::string(temp);
	window_thread =
		(HANDLE)_beginthreadex(0, 0, WindowLoopThread, this, 0, 0);

	std::string name = session_id + std::string(VIDEO_INPUT_BUFFER);
	video_input_writer = new SharedHandleBuffer(name.c_str());
	DWORD ret = video_input_writer->InitMapBuffer();
	if (ret != 0) {
		plog(LOG_ERROR, "PLSCamEffect fail to init input map! error:%u",
		     ret);
		assert(false);
	}

	name = session_id + std::string(VIDEO_OUTPUT_BUFFER);
	video_output_reader = new SharedHandleBuffer(name.c_str());
	ret = video_output_reader->InitMapBuffer();
	if (ret != 0) {
		plog(LOG_ERROR,
		     "PLSCamEffect fail to init output map! error:%u", ret);
		assert(false);
	}

	name = session_id + std::string(SOURCE_ALIVE_FLAG);
	camera_alive_flag = CHandleWrapper::GetEvent(name.c_str(), true);

	name = session_id + std::string(SOURCE_ACTIVE_FLAG);
	camera_active_flag = CHandleWrapper::GetEvent(name.c_str(), true);
	SetCaptureState(false);

	name = session_id + std::string(PROCESS_VALID_FLAG);
	effect_valid_flag = CHandleWrapper::GetEvent(name.c_str(), true);
	::ResetEvent(effect_valid_flag);

	onoff_changed_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	::ResetEvent(onoff_changed_event);

	thread_exit_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	::ResetEvent(thread_exit_event);
	message_thread =
		(HANDLE)_beginthreadex(0, 0, MessageLoopThread, this, 0, 0);
}

PLSCamEffect::~PLSCamEffect()
{
	::SetEvent(thread_exit_event);
	CHandleWrapper::WaitThreadEnd(message_thread);
	CHandleWrapper::CloseHandleEx(thread_exit_event);
	CHandleWrapper::CloseHandleEx(onoff_changed_event);

	if (::IsWindow(source_hwnd)) {
		SendMessage(source_hwnd, WM_CLOSE, 0, 0);
	}

	CHandleWrapper::WaitThreadEnd(window_thread);

	ClearOutputVideo();

	CHandleWrapper::CloseHandleEx(process_handle);

	SAFE_DELETE_PTR(video_input_writer);
	SAFE_DELETE_PTR(video_output_reader);

	CHandleWrapper::CloseHandleEx(camera_alive_flag);
	CHandleWrapper::CloseHandleEx(camera_active_flag);
	CHandleWrapper::CloseHandleEx(effect_valid_flag);

	back_ground.reset();
	fore_ground.reset();
	obs_enter_graphics();
	if (fin_output_texture) {
		gs_texture_destroy(fin_output_texture);
		fin_output_texture = nullptr;
	}
	obs_leave_graphics();
}

bool PLSCamEffect::SetEffectParam(const char *method, obs_data_t *data,
				  bool &effect_onoff_changed)
{
	effect_onoff_changed = false;

	bool seg_effect_changed = false;
	bool ret = false;
	if (0 == strcmp(method, "beauty")) {
		ret = ParseBeautyParam(data, effect_onoff_changed);
	} else {
		plog(LOG_ERROR, "Receive unknown effect type!");
		assert(false);
		ret = false;
	}

	if (effect_onoff_changed && waiting_onoff_change && IsEffectOn()) {
		::SetEvent(onoff_changed_event);
	}

	return ret;
}

size_t PLSCamEffect::PopEvents(std::vector<PLSEventInfo> &output)
{
	output.clear();

	if (!IsEffectOn()) {
		return 0;
	}

	{
		CAutoLockCS autoLock(exception_lock);

		std::map<obs_source_event_type, PLSEventInfo>::iterator itr =
			exceptions.begin();

		while (itr != exceptions.end()) {
			if (itr->second.event_happen &&
			    !itr->second.notify_done) {
				output.push_back(itr->second);
				itr->second.notify_done =
					true; // make sure one exception won't be push again
			}
			++itr;
		}
	}

	return output.size();
}

bool PLSCamEffect::IsEffectOn()
{
	CAutoLockCS autoLock(param_lock);
	if (beauty_params.enable || seg_message.enable) {
		return true;
	} else {
		return false;
	}
}

bool PLSCamEffect::ParseBeautyParam(obs_data_t *data,
				    bool &effect_onoff_changed)
{
	face_beauty_message *temp = new face_beauty_message();
	EFFECT_MESSAGE_PTR msg(temp);

	const char *id = obs_data_get_string(data, "id");
	assert(id);
	if (id) {
		int len = min(ARRAY_COUNT(temp->id), strlen(id) + 1);
		memmove(temp->id, id, len);
	}

	const char *category = obs_data_get_string(data, "category");
	assert(category);
	if (category) {
		int len =
			min(ARRAY_COUNT(temp->category), strlen(category) + 1);
		memmove(temp->category, category, len);
	}

	temp->chin = obs_data_get_double(data, "chin");
	temp->cheek = obs_data_get_double(data, "cheek");
	temp->cheekbone = obs_data_get_double(data, "cheekbone");
	temp->eyes = obs_data_get_double(data, "eyes");
	temp->nose = obs_data_get_double(data, "nose");
	temp->smooth = obs_data_get_double(data, "smooth");
	temp->enable = obs_data_get_bool(data, "enable");

	int len = min(ARRAY_COUNT(temp->guid), session_id.length() + 1);
	memmove(temp->guid, session_id.c_str(), len);

	bool output_clear = !temp->enable;

	{
		CAutoLockCS autoLock(param_lock);
		effect_onoff_changed = (temp->enable != beauty_params.enable) &&
				       !seg_message.enable;
		memmove(&beauty_params, temp, sizeof(face_beauty_message));
		output_clear = output_clear && !seg_message.enable;
	}

	if (effect_onoff_changed) {
		PushEmptyInputVideo();
		ClearOutputVideo();

		if (temp->enable) {
			plog(LOG_INFO, "PLSCamEffect: Face beauty is enable");
		} else {
			plog(LOG_INFO, "PLSCamEffect: Face beauty is disable");
		}
	}

	if (output_clear) {
		ClearOutputVideo();
	}

	message_queue.PushMessage(msg, sizeof(face_beauty_message), true);
	return true;
}

void PLSCamEffect::SetCaptureState(bool actived)
{
	if (actived) {
		if (!CHandleWrapper::IsHandleSigned(camera_active_flag)) {
			plog(LOG_INFO, "Capture state is active");
		}

		fps_device.OnFrame();

		// should clear cache when receive first video
		if (!CHandleWrapper::IsHandleSigned(camera_active_flag)) {
			PushEmptyInputVideo();
			ClearOutputVideo();
		}

		::SetEvent(camera_active_flag);

	} else {
		fps_device.Reset();
		fps_push_handle.Reset();
		fps_get_handle.Reset();

		if (CHandleWrapper::IsHandleSigned(camera_active_flag)) {
			plog(LOG_INFO, "Capture state is deactive");
		}

		::ResetEvent(camera_active_flag);

		PushEmptyInputVideo();
		ClearOutputVideo();
	}
}

bool PLSCamEffect::IsCaptureNormal()
{
	return CHandleWrapper::IsHandleSigned(camera_active_flag);
}

// return true if effect process is ready && effect is on
bool PLSCamEffect::UseVideoEffect()
{
	return false;
	// effect is off
	if (!IsEffectOn()) {
		SetEffectState("beaufy-off");
		return false;
	}

	// circle buffer is not valid
	if (!video_input_writer->IsBufferValid() ||
	    !video_output_reader->IsBufferValid()) {
		SetEffectState("buffer-invalid");
		return false;
	}

	// effect process is not running, not exited
	if (!CHandleWrapper::IsHandleValid(process_handle) ||
	    CHandleWrapper::IsHandleSigned(process_handle)) {
		SetEffectState("process-not-run");
		return false;
	}

	// cann't find process window
	if (!::IsWindow(process_hwnd)) {
		SetEffectState("process-nowindow");
		return false;
	}

	// effect process is not ready
	if (!CHandleWrapper::IsHandleSigned(effect_valid_flag)) {
		SetEffectState("process-not-ready");
		return false;
	}

	// effect process may be blocked or exception happens
	if (GetTickCount() - previous_heartbeat >= HEART_BEAT_TIMEOUT) {
		SetEffectState("process-noheartbeat");
		return false;
	}

	return true;
}

void PLSCamEffect::EffectTick()
{
	if (!CHandleWrapper::IsHandleSigned(camera_active_flag)) {
		SetEffectState("camera-nodata");
		PushEmptyInputVideo();
		ClearOutputVideo();
	}

	if (back_ground) {
		back_ground->Tick();
	}

	if (fore_ground) {
		fore_ground->Tick();
	}
}

void PLSCamEffect::SetInputVideo(shared_handle_header &hdr,
				 shared_handle_sample &body)
{
	if (!IsEffectOn()) {
		return;
	}

	if (body.handle) {
		fps_push_handle.OnFrame();
	}

	video_input_writer->WriteItemData(&hdr, sizeof(hdr), &body,
					  sizeof(body));
}

gs_texture *PLSCamEffect::GetOutputVideo()
{
	if (!IsEffectOn()) {
		return NULL;
	}

	if (!CHandleWrapper::IsHandleSigned(camera_active_flag)) {
		return NULL; // currently no video, we should not render any frame
	}

	shared_handle_header header;
	shared_handle_sample sample;
	sample.handle = 0;

	bool bOk = video_output_reader->ReadItemData(&header, sizeof(header),
						     &sample, sizeof(sample));
	if (bOk) {
		if (header.is_handle_available()) {
			if (output_video_handle != sample.handle) {
				plog(LOG_INFO,
				     "Received new video handle. video:%llu",
				     sample.handle);
			}

			output_video_handle = sample.handle;
			output_tex_flip = header.flip;
			if (output_video_handle) {
				fps_get_handle.OnFrame();
			}
		} else {
			plog(LOG_INFO,
			     "Received obsoleted video handle(%llu). It is updated before %ums.",
			     sample.handle, GetTickCount() - header.push_time);
		}
	}

	if (!output_video_handle) {
		ClearOutputVideo();
		return NULL;
	}

	if (output_video_tex) {
		obs_enter_graphics();
		ULONG64 current_handle =
			(ULONG64)gs_texture_get_shared_handle(output_video_tex);
		obs_leave_graphics();

		// Output video never change, use the openned texture directly.
		if (current_handle == output_video_handle) {
			SetEffectState("sharedhandle-ok");
			//render & mix ground
			obs_enter_graphics();
			UpdateFinOutputTexture(output_video_tex);
			RenderAllLayers(output_tex_flip);
			obs_leave_graphics();
			return fin_output_texture;
			//return output_video_tex;
		}

		// Output video is changed, release previou texture.
		ReleaseOpennedTexture();
	}

	obs_enter_graphics();
	output_video_tex =
		gs_texture_open_shared((uint32_t)output_video_handle);

	if (!output_video_tex) {
		SetEffectState("sharedhandle-invalid");
		plog(LOG_ERROR, "Fail to open shared handle! HANDLE:%llu",
		     output_video_handle);
		//assert(output_video_tex && "fail to open output handle");
	}
	//render & mix ground
	UpdateFinOutputTexture(output_video_tex);
	RenderAllLayers(output_tex_flip);
	obs_leave_graphics();
	return fin_output_texture;
	//return output_video_tex;
}

std::string PLSCamEffect::GenerateGuid()
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

std::wstring PLSCamEffect::GetProcessPath()
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
	       std::wstring(CAMERA_EFFECT_PROCESS_NAME);
}

HWND PLSCamEffect::CreateMessageWindow()
{
	WNDCLASSA wc = {0};
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = EffectWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = source_hwnd_class.c_str();

	ATOM nAtom = RegisterClassA(&wc);
	if (0 == nAtom) {
		plog(LOG_ERROR,
		     "PLSCamEffect failed to register window! error:%u",
		     GetLastError());

		assert(false);
		return 0;
	}

	HWND hWnd = CreateWindowA(source_hwnd_class.c_str(),
				  source_hwnd_class.c_str(), WS_POPUPWINDOW, 0,
				  0, 0, 0, NULL, NULL, GetModuleHandle(NULL),
				  NULL);
	if (!::IsWindow(hWnd)) {
		UnregisterClassA(source_hwnd_class.c_str(),
				 GetModuleHandle(NULL));

		plog(LOG_ERROR,
		     "PLSCamEffect failed to create window! error:%u",
		     GetLastError());

		assert(false);
		return 0;
	}

	::SetWindowLongPtr(hWnd, GWLP_USERDATA,
			   reinterpret_cast<LONG_PTR>(this));
	return hWnd;
}

void PLSCamEffect::ClearOutputVideo()
{
	if (output_video_handle) {
		obs_enter_graphics();
		ReleaseOpennedTexture();
		output_video_handle = 0;
		obs_leave_graphics();
	}
}

void PLSCamEffect::ReleaseOpennedTexture()
{
	if (output_video_tex) {
		obs_enter_graphics();
		if (output_video_tex) {
			gs_texture_destroy(output_video_tex);
			output_video_tex = NULL;
		}
		if (fin_output_texture) {
			gs_texture_destroy(fin_output_texture);
			fin_output_texture = NULL;
		}
		obs_leave_graphics();
	}
}

void PLSCamEffect::PushEmptyInputVideo()
{
	shared_handle_header hdr = {};
	shared_handle_sample body = {};
	body.handle = 0;
	SetInputVideo(hdr, body);
}

void PLSCamEffect::SetEffectState(std::string state)
{
	// "effect_state" has been removed in latest version, so we just comment it here for releasing hotfix

	//if (effect_state != state) {
	//	effect_state = state;
	//	blog(LOG_INFO, "Update effect state : %s", state.c_str());
	//}
}

void PLSCamEffect::PushRestoreMessage()
{
	face_beauty_message *temp = new face_beauty_message();
	EFFECT_MESSAGE_PTR msg(temp);

	{
		CAutoLockCS autoLock(param_lock);
		memmove(temp, &beauty_params, sizeof(face_beauty_message));
	}

	message_queue.PushMessage(msg, sizeof(face_beauty_message), true);
}

void PLSCamEffect::PushRestoreSegMessage()
{
	//for segmentation
	if (seg_message.enable) {
		segmentation_message *temp_seg = new segmentation_message();
		EFFECT_MESSAGE_PTR seg_msg(temp_seg);
		{
			CAutoLockCS autoLock(param_lock);
			memmove(temp_seg, &seg_message,
				sizeof(segmentation_message));
		}
		message_queue.PushMessage(seg_msg, sizeof(segmentation_message),
					  true);
	}
}

bool PLSCamEffect::RunEffectProcess()
{
	assert(!CHandleWrapper::IsHandleValid(process_handle));
	CHandleWrapper::CloseHandleEx(process_handle);
	process_hwnd = 0;

	::ResetEvent(
		effect_valid_flag); // Effect process will sign this flag if success to init
	PushRestoreMessage(); // send effect params to effect process for restoring
	//PushRestoreSegMessage(); //restore segmentation message
	PushEmptyInputVideo();
	ClearOutputVideo();

	if (!::IsWindow(source_hwnd)) {
		CHandleWrapper::IsHandleSigned(thread_exit_event, 1000);
		return false;
	}

	std::wstring process_path = GetProcessPath();
	if (process_path.empty()) {
		plog(LOG_ERROR, "Get beauty process path failed");
		CHandleWrapper::IsHandleSigned(thread_exit_event, 5000);
		assert(false);
		return false;
	}

	if (!os_is_file_exist_ex(process_path.c_str())) {
		PLSEventInfo ecp;
		ecp.event_happen = true;
		ecp.sub_code = 0;
		ecp.type = OBS_SOURCE_EXCEPTION_NO_FILE;
		PushEvent(ecp);

		plog(LOG_ERROR, "Beauty process file not exist");
		CHandleWrapper::IsHandleSigned(thread_exit_event, 5000);
		return false;
	}

	{
		CAutoLockCS autoLock(exception_lock);
		exceptions.clear();
	}

	wchar_t cmd[2048];
	swprintf_s(cmd, ARRAY_COUNT(cmd),
		   L"\"%s\" \"%hs\" \"%hs\" \"%hs\" \"%hs\" %d",
		   process_path.c_str(), session_id.c_str(),
		   PLSCamEffect::prism_version.c_str(),
		   PLSCamEffect::nelo_session.c_str(),
		   PLSCamEffect::prism_cfg_path.c_str(), source_hwnd);

	PROCESS_INFORMATION pi = {};
	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK;
	si.wShowWindow = SW_HIDE;

	BOOL bOK = CreateProcessW((LPWSTR)process_path.c_str(), cmd, NULL, NULL,
				  TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	if (!bOK) {
		plog(LOG_ERROR, "Fail to run effect process! error:%u",
		     GetLastError());
		CHandleWrapper::IsHandleSigned(thread_exit_event, 5000);
		assert(false);
		return false;
	}

	CloseHandle(pi.hThread);
	process_handle = pi.hProcess;

	plog(LOG_INFO, "Success to run effect process. HANDLE:%llu PID:%u",
	     pi.hProcess, pi.dwProcessId);

	return true;
}

bool PLSCamEffect::CheckFindWindow()
{
	if (::IsWindow(process_hwnd)) {
		return true;
	}

	std::string process_hwnd_class = session_id;
	process_hwnd = ::FindWindowA(process_hwnd_class.c_str(), NULL);
	if (::IsWindow(process_hwnd)) {
		plog(LOG_INFO,
		     "Find window of effect process. HANDLE:%llu HWND:%llu",
		     process_handle, process_hwnd);
		previous_heartbeat = GetTickCount();
		return true;
	} else {
		return false;
	}
}

bool PLSCamEffect::CheckRunProcess()
{
	if (CHandleWrapper::IsHandleValid(process_handle)) {
		if (!CHandleWrapper::IsHandleSigned(process_handle)) {
			// effect process is running
			return CheckFindWindow();
		}

		// effect process exited because of an exception !

		DWORD exit_code = 0;
		GetExitCodeProcess(process_handle, &exit_code);

		plog(LOG_ERROR,
		     "Effect process exited because of an exception! code:%u HANDLE:%llu",
		     exit_code, process_handle);

		PLSEventInfo ecp;
		ecp.event_happen = true;
		ecp.sub_code = exit_code;
		ecp.type = OBS_SOURCE_EXCEPTION_VIDEO_DEVICE;
		PushEvent(ecp);

		process_hwnd = 0;
		CHandleWrapper::CloseHandleEx(process_handle);

		HANDLE evt[] = {
			thread_exit_event,
			onoff_changed_event,
		};

		waiting_onoff_change = true;
		WaitForMultipleObjects(ARRAY_COUNT(evt), evt, FALSE, INFINITE);
		waiting_onoff_change = false;

		if (CHandleWrapper::IsHandleSigned(thread_exit_event, 0)) {
			return false;
		}
	}

	// Won't return true until find HWND of effect process
	RunEffectProcess();

	return false;
}

void PLSCamEffect::CheckHeartBeat(bool &heartbeat_timeout)
{
	bool timeout =
		(GetTickCount() - previous_heartbeat >= HEART_BEAT_TIMEOUT);

	if (heartbeat_timeout != timeout) {
		heartbeat_timeout = timeout;
		if (heartbeat_timeout) {
			plog(LOG_ERROR, "Heartbeat is timeout. HANDLE:%llu",
			     process_handle);
		} else {
			plog(LOG_INFO, "Heartbeat is normal. HANDLE:%llu",
			     process_handle);
		}
	}
}

void PLSCamEffect::TrySendMessage(cam_effect_message_info &info)
{
	COPYDATASTRUCT temp = {};
	temp.dwData = info.msg->type;
	temp.lpData = info.msg.get();
	temp.cbData = info.length;

	LRESULT res = ::SendMessageTimeout(process_hwnd, WM_COPYDATA, NULL,
					   (LPARAM)&temp,
					   SMTO_BLOCK | SMTO_ABORTIFHUNG,
					   MESSAGE_TIMEOUT_MS, NULL);
	if (!res) {
		plog(LOG_ERROR,
		     "Timeout while sending message! hr:%X error:%u HANDLE:%llu",
		     res, GetLastError(), process_handle);

		if (info.msg->type == effect_message_face_beauty) {
			PushRestoreMessage(); // push another message with latest params
		} else if (info.msg->type == effect_message_segmentation) {
			PushRestoreSegMessage();
		} else {
			message_queue.PushMessage(info.msg, info.length, false);
		}

		CHandleWrapper::IsHandleSigned(thread_exit_event, 1000);
	}
}

void PLSCamEffect::WindowLoopThreadInner()
{
	source_hwnd = CreateMessageWindow();
	if (!::IsWindow(source_hwnd)) {
		return;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClassA(source_hwnd_class.c_str(), GetModuleHandle(NULL));
}

void PLSCamEffect::MessageLoopThreadInner()
{
	bool heartbeat_timeout = false;
	while (!CHandleWrapper::IsHandleSigned(thread_exit_event, 50)) {
		if (!CheckRunProcess()) {
			heartbeat_timeout = false; // reset flag
			continue;
		}

		CheckHeartBeat(heartbeat_timeout);
		if (!heartbeat_timeout) {
			cam_effect_message_info info;
			if (message_queue.PopMessage(info)) {
				TrySendMessage(info);
			}
		}
	}
}

obs_source_event_type TransException(cam_effect_exception_type src)
{
	switch (src) {
	case effect_exception_sensetime:
		return OBS_SOURCE_EXCEPTION_SENSETIME;

	case effect_exception_d3d:
		return OBS_SOURCE_EXCEPTION_D3D;

	case effect_exception_unknown:
	default:
		assert(false);
		return OBS_SOURCE_EXCEPTION_NONE;
	}
}

void PLSCamEffect::OnCopyData(COPYDATASTRUCT *cd)
{
	int min_msg_len = sizeof(cam_effect_message);
	if (!cd->lpData || (cd->cbData < min_msg_len)) {
		return;
	}

	cam_effect_message *msg = (cam_effect_message *)cd->lpData;
	std::string temp = msg->guid;
	if (temp != session_id) {
		return;
	}

	if (msg->type != cd->dwData) {
		assert(false && "incorrect message type");
		return;
	}

	switch (cd->dwData) {
	case effect_message_process_heartbeat:
		assert(cd->cbData == sizeof(heart_beat_message));
		if (cd->cbData == sizeof(heart_beat_message)) {
			previous_heartbeat = GetTickCount();
		}
		break;

	case effect_message_process_exception:
		assert(cd->cbData == sizeof(process_exception_message));
		if (cd->cbData == sizeof(process_exception_message)) {
			process_exception_message *src =
				(process_exception_message *)cd->lpData;

			obs_source_event_type type =
				TransException(src->exception_type);

			if (type != OBS_SOURCE_EXCEPTION_NONE) {
				PLSEventInfo ecp;
				ecp.event_happen = true;
				ecp.type = type;
				ecp.sub_code = src->sub_code;
				PushEvent(ecp);
			} else {
				plog(LOG_WARNING,
				     "Received unknown exception type. type:%d",
				     type);
			}
		}
		break;
	case effect_message_status_action:
		assert(cd->cbData == sizeof(process_status_message));
		if (cd->cbData == sizeof(process_status_message)) {
			process_status_message *src =
				(process_status_message *)cd->lpData;
			if (src->type == effect_message_status_action) {
				PLSEventInfo ecp;
				ecp.event_happen = true;
				ecp.type = OBS_SOURCE_SENSEAR_ACTION;
				ecp.sub_code = 0;
				PushEvent(ecp);
			}
		}
		break;

	default:
		plog(LOG_WARNING, "Received unknown message type. type:%d",
		     (int)cd->dwData);
		break;
	}
}

void PLSCamEffect::PushEvent(PLSEventInfo &ecp)
{
	CAutoLockCS autoLock(exception_lock);

	PLSEventInfo info = exceptions[ecp.type];
	if (info.event_happen && info.notify_done &&
	    ecp.type != OBS_SOURCE_EXCEPTION_BG_FILE_ERROR &&
	    ecp.type != OBS_SOURCE_EXCEPTION_BG_FILE_NETWORK_ERROR) {
		return; // exception has been notified before
	}

	exceptions[ecp.type] = ecp;
}

void PLSCamEffect::UpdateFinOutputTexture(const gs_texture *tex)
{
	if (!tex)
		return;
	uint32_t width = gs_texture_get_width(tex);
	uint32_t height = gs_texture_get_height(tex);
	gs_color_format fmt = gs_texture_get_color_format(tex);
	if (fin_output_texture &&
	    (gs_texture_get_width(fin_output_texture) != width ||
	     gs_texture_get_height(fin_output_texture) != height ||
	     fmt != gs_texture_get_color_format(fin_output_texture))) {
		gs_texture_destroy(fin_output_texture);
		fin_output_texture = NULL;
	}

	if (!fin_output_texture) {
		fin_output_texture = gs_texture_create(width, height, fmt, 1,
						       NULL, GS_RENDER_TARGET);
	}
}

void PLSCamEffect::RenderAllLayers(bool flip)
{
	if (!fin_output_texture) {
		return;
	}

	obs_enter_graphics();

	if (!seg_message.enable) {
		gs_copy_texture(fin_output_texture, output_video_tex);
		obs_leave_graphics();
		return;
	}

	bool res = false;
	if (back_ground) {
		res = back_ground->RenderGround(fin_output_texture, flip);
	}

	if (res) {
		//mix output_video_tex to fin_output_texture
		gs_texture_t *pre_rt_tex = gs_get_render_target();

		gs_viewport_push();
		gs_projection_push();
		gs_matrix_push();
		gs_matrix_identity();
		uint32_t width = gs_texture_get_width(fin_output_texture);
		uint32_t height = gs_texture_get_height(fin_output_texture);
		gs_set_render_target(fin_output_texture, NULL);
		gs_set_viewport(0, 0, width, height);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f,
			 100.0f);
		gs_blend_state_push();
		gs_reset_blend_state();

		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);

		gs_eparam_t *param_image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(param_image, output_video_tex);

		gs_draw_sprite(output_video_tex, 0, 0, 0);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		gs_blend_state_pop();
		gs_set_render_target(pre_rt_tex, NULL);
		gs_matrix_pop();
		gs_projection_pop();
		gs_viewport_pop();
	} else {
		gs_copy_texture(fin_output_texture, output_video_tex);
	}

	if (fore_ground) {
		fore_ground->RenderGround(fin_output_texture, flip);
	}

	obs_leave_graphics();
}

void PLSCamEffect::GroundExceptionHandler(void *data, int type, int subcode)
{
	PLSCamEffect *cam_effect = (PLSCamEffect *)data;
	PLSEventInfo ecp;
	ecp.event_happen = true;
	ecp.sub_code = subcode;
	ecp.type = static_cast<obs_source_event_type>(type);
	plog(LOG_INFO, "Ground Item Exception, type:%d, subcode:%d", type,
	     subcode);
	cam_effect->PushEvent(ecp);
}

enum seg_method {
	seg_method_chromakey = 1,
	seg_method_sensetime = 2,
};

bool PLSCamEffect::ParseSegmentationParam(obs_data_t *data,
					  bool &seg_onoff_changed)
{
	if (!data) {
		return false;
	}

	ground_private_data ground_data;
	ground_data.blur_enable = obs_data_get_bool(data, GROUND_BLUR_ENABLE);
	ground_data.blur = obs_data_get_double(data, GROUND_BLUR);
	ground_data.ground_seg_type = static_cast<GROUND_SEG_TYPE>(
		obs_data_get_int(data, GROUND_SELECT_TYPE));
	ground_data.temp_origin =
		obs_data_get_int(data, GROUND_IS_TEMP_ORIGINAL);

	if (ground_data.ground_seg_type == GROUND_SEG_TYPE_UNKNOWN) {
		plog(LOG_WARNING, "Ground type error: unknown");
		return false;
	}

	item_private_data item_data;
	std::string str_type = obs_data_get_string(data, GROUND_BG_ITEM_TYPE);
	if (ground_data.ground_seg_type == GROUND_SEG_TYPE_ADD_BG) {
		if (str_type.compare("image") == 0) {
			item_data.item_type = ITEM_TYPE_IMAGE;
		} else if (str_type.compare("video") == 0) {
			item_data.item_type = ITEM_TYPE_VIDEO;
		} else {
			plog(LOG_INFO, "Background type[%s] invalid", str_type);
			return false;
		}
	}

	item_data.path = obs_data_get_string(data, GROUND_PATH);
	item_data.stop_moiton_path =
		obs_data_get_string(data, GROUND_STOPMOTION_PATH);
	item_data.rt_source.x = obs_data_get_double(data, GROUND_SRC_RECT_X);
	item_data.rt_source.y = obs_data_get_double(data, GROUND_SRC_RECT_Y);
	item_data.rt_source.cx = obs_data_get_double(data, GROUND_SRC_RECT_CX);
	item_data.rt_source.cy = obs_data_get_double(data, GROUND_SRC_RECT_CY);

	item_data.rt_target.x = obs_data_get_double(data, GROUND_DST_RECT_X);
	item_data.rt_target.y = obs_data_get_double(data, GROUND_DST_RECT_Y);
	item_data.rt_target.cx = obs_data_get_double(data, GROUND_DST_RECT_CX);
	item_data.rt_target.cy = obs_data_get_double(data, GROUND_DST_RECT_CY);

	item_data.motion = obs_data_get_bool(data, GROUND_MOTION);
	item_data.h_flip = obs_data_get_bool(data, GROUND_H_FLIP);
	item_data.v_flip = obs_data_get_bool(data, GROUND_V_FLIP);
	item_data.ui_motion_id = obs_data_get_string(data, GROUND_UI_MOTION_ID);
	item_data.thumbnail_file_path =
		obs_data_get_string(data, GROUND_THUMBNAIL_FILE_PATH);
	item_data.is_prism_resource =
		obs_data_get_bool(data, GROUND_IS_PRISM_RESOURCE);

	bool ck_enable = obs_data_get_bool(data, CK_CAPTURE);

	if (!back_ground) {
		ground_exception callback =
			(ground_exception)&PLSCamEffect::GroundExceptionHandler;
		back_ground = std::make_shared<PLSCamGround>(
			dshow_source, this, callback, GROUND_TYPE_BG);
	}
	back_ground->SetBackgroundInfo(ground_data, item_data);

	segmentation_message *temp = new segmentation_message();
	EFFECT_MESSAGE_PTR msg(temp);
	int len = min(ARRAY_COUNT(temp->guid), session_id.length() + 1);
	memmove(temp->guid, session_id.c_str(), len);

	temp->blur = ground_data.blur;
	//temp->use_original_bg = (ground_data.ground_seg_type == GROUND_SEG_TYPE_ORIGINAL);
	temp->segment_type = cam_seg_type_original_total;
	if (ground_data.temp_origin == 1 &&
	    ground_data.ground_seg_type == GROUND_SEG_TYPE_ORIGINAL) {
		temp->segment_type = cam_seg_type_original_total;
	} else {
		if (ground_data.ground_seg_type == GROUND_SEG_TYPE_ORIGINAL) {
			temp->segment_type = cam_seg_type_original;
		} else if (ground_data.ground_seg_type ==
				   GROUND_SEG_TYPE_ADD_BG ||
			   ground_data.ground_seg_type ==
				   GROUND_SEG_TYPE_DEL_BG) {
			temp->segment_type = cam_seg_type_remvoe_bg;
		}
	}
	temp->blur_enable = ground_data.blur_enable;

	plog(LOG_INFO, "ground type:%d", ground_data.ground_seg_type);

	//disable only blur disable and use original background
	temp->enable = ground_data.blur_enable ||
		       ground_data.ground_seg_type !=
			       GROUND_SEG_TYPE_ORIGINAL ||
		       ground_data.temp_origin == 1;

	temp->seg_method = seg_method_sensetime;
	temp->ck_info.enable = false;
	/*temp->ck_info.enable = ck_enable;
	if (ck_enable) {
		temp->seg_method = 1;
		temp->ck_info.color = obs_data_get_int(data, CK_COLOR);
		temp->ck_info.similarity =
			obs_data_get_int(data, CK_SIMILARITY);
		temp->ck_info.smooth = obs_data_get_int(data, CK_SMOOTH);
		temp->ck_info.spill = obs_data_get_int(data, CK_SPILL);
		temp->ck_info.opacity = obs_data_get_int(data, CK_OPACITY);
		temp->ck_info.contrast = obs_data_get_double(data, CK_CONTRAST);
		temp->ck_info.brightness =
			obs_data_get_double(data, CK_BRIGHTNESS);
		temp->ck_info.gamma = obs_data_get_double(data, CK_GAMMA);
	}*/

	bool output_clear = !temp->enable;
	{
		CAutoLockCS autoLock(param_lock);
		seg_onoff_changed = (temp->enable != seg_message.enable) &&
				    !beauty_params.enable;
		output_clear = output_clear && !beauty_params.enable;
		memmove(&seg_message, temp, sizeof(segmentation_message));
	}

	if (seg_onoff_changed) {
		PushEmptyInputVideo();
		ClearOutputVideo();
		if (back_ground) {
			back_ground->SetClearAllTextureToken();
		}
	}

	if (output_clear) {
		ClearOutputVideo();
	}

	if (!fore_ground) {
		ground_exception callback =
			(ground_exception)&PLSCamEffect::GroundExceptionHandler;
		fore_ground = std::make_shared<PLSCamGround>(
			dshow_source, this, callback, GROUND_TYPE_FG);
	}

	item_private_data it;
	it.path = obs_data_get_string(data, FORE_GROUND_PATH);
	it.stop_moiton_path =
		obs_data_get_string(data, FORE_GROUND_STATIC_PATH);
	it.item_type = item_data.item_type;
	it.ui_motion_id = item_data.ui_motion_id;
	it.motion = item_data.motion;
	it.is_prism_resource = item_data.is_prism_resource;
	if (it.path.empty() || it.stop_moiton_path.empty()) {
		ground_data.enable = false;
	}
	fore_ground->SetBackgroundInfo(ground_data, it);

	message_queue.PushMessage(msg, sizeof(segmentation_message), true);

	if (seg_onoff_changed && waiting_onoff_change && IsEffectOn()) {
		::SetEvent(onoff_changed_event);
	}

	return true;
}

void PLSCamEffect::ClearResWhenEffectOff()
{
	if (back_ground) {
		back_ground->ClearResWhenEffectOff();
	}

	if (fore_ground) {
		fore_ground->ClearResWhenEffectOff();
	}
}
