#pragma once
#include "prism-msg-queue.h"
#include "prism-ipc-buffer.h"
#include "prism-handle-wrapper.h"
#include <obs.hpp>
#include <Windows.h>
#include <process.h>
#include <map>
#include "prism-cam-ground.h"

class PLSFpsChecker {
public:
	PLSFpsChecker(char *flag);

	void Reset();
	void OnFrame();

private:
	std::string flag_name;
	DWORD start_time;
	DWORD frame_count;
};

struct PLSEventInfo {
	bool event_happen;
	bool notify_done;

	obs_source_event_type type;
	int sub_code;

	PLSEventInfo() : event_happen(true), notify_done(false), sub_code(0), type(OBS_SOURCE_EXCEPTION_NONE) {}
};

class PLSCamEffect {
public:
	static std::string nelo_session;
	static std::string prism_version;
	static std::string prism_cfg_path;

	static void SaveTexture(gs_texture_t *tex, char *path);
	static bool SetGlobalParam(obs_data_t *data);
	static unsigned WindowLoopThread(void *pParam);
	static unsigned MessageLoopThread(void *pParam);
	static LRESULT EffectWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);

public:
	PLSCamEffect();
	virtual ~PLSCamEffect();

	bool SetEffectParam(const char *method, obs_data_t *data, bool &effect_onoff_changed);

	size_t PopEvents(std::vector<PLSEventInfo> &output);

	void SetCaptureState(bool actived);
	bool IsCaptureNormal();

	bool UseVideoEffect();
	void EffectTick();
	void SetInputVideo(shared_handle_header &hdr, shared_handle_sample &body);
	gs_texture *GetOutputVideo();

	//for segmentation
	bool ParseSegmentationParam(obs_data_t *data, bool &seg_onoff_changed);
	void SetDShowSource(obs_source_t *source) { dshow_source = source; };
	void ClearResWhenEffectOff();

private:
	bool IsEffectOn();
	bool ParseBeautyParam(obs_data_t *data, bool &effect_onoff_changed);
	std::string GenerateGuid();
	std::wstring GetProcessPath();
	HWND CreateMessageWindow();
	void ClearOutputVideo();
	void ReleaseOpennedTexture();
	void PushEmptyInputVideo();
	void SetEffectState(std::string state);

	void PushRestoreMessage();
	void PushRestoreSegMessage();
	bool RunEffectProcess();
	bool CheckFindWindow();
	bool CheckRunProcess();
	void CheckHeartBeat(bool &heartbeat_timeout);
	void TrySendMessage(cam_effect_message_info &info);
	void MessageLoopThreadInner();
	void WindowLoopThreadInner();

	void OnCopyData(COPYDATASTRUCT *cd);
	void PushEvent(PLSEventInfo &ecp);

	void UpdateFinOutputTexture(const gs_texture *tex);
	void RenderAllLayers(bool flip);
	static void GroundExceptionHandler(void *data, int type, int subcode);

private:
	std::string session_id;
	std::string effect_state;

	std::string source_hwnd_class;
	HWND source_hwnd;

	HANDLE process_handle;
	HWND process_hwnd;
	DWORD previous_heartbeat;

	CCriticalSection param_lock;
	face_beauty_message beauty_params;

	CCriticalSection exception_lock;
	std::map<obs_source_event_type, PLSEventInfo> exceptions;

	SharedHandleBuffer *video_input_writer;
	SharedHandleBuffer *video_output_reader;
	ULONG64 output_video_handle;
	gs_texture *output_video_tex;

	HANDLE camera_alive_flag;
	HANDLE camera_active_flag;
	HANDLE effect_valid_flag;

	bool waiting_onoff_change;
	HANDLE onoff_changed_event;

	HANDLE thread_exit_event;
	HANDLE message_thread;
	HANDLE window_thread;
	PLSMsgQueue message_queue;

	PLSFpsChecker fps_device;
	PLSFpsChecker fps_push_handle;
	PLSFpsChecker fps_get_handle;

	std::shared_ptr<PLSCamGround> back_ground;
	std::shared_ptr<PLSCamGround> fore_ground;
	gs_texture *fin_output_texture;
	segmentation_message seg_message;
	obs_source_t *dshow_source;
	bool output_tex_flip;
};
