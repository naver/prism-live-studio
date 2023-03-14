#pragma once
#include "prism-msg-queue.h"
#include "prism-ipc-buffer.h"
#include "prism-handle-wrapper.h"
#include "prism-effect-ipc-define.h"
#include "../libdshowcapture/dshowcapture.hpp"
#include <pls/pipe/pipe-io.h>
#include <obs.hpp>
#include <Windows.h>
#include <process.h>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
}

class ICamSessionCb {
public:
	virtual ~ICamSessionCb() {}

	virtual void OnValidStateChanged(bool valid, obs_source_error code) = 0;
	virtual void OnImageNormalChanged(bool normal) = 0;
	virtual void OnEventHappenned(obs_source_event_type type, int code) = 0;
	virtual void OnNotifyActionEvent(const char *e1, const char *e2,
					 const char *e3, const char *tgt) = 0;
	virtual void OnSetupBuffering(bool useBuffering, bool isDecoupled) = 0;
};

class PLSCamSession : public PipeCallback {
public:
	static std::string nelo_session;
	static std::string prism_version;
	static std::string prism_cfg_path;
	static std::string prism_user_id;
	static std::string prism_user_id_raw;
	static int camera_auto_restart_times;
	static std::string prism_project_name;
	static std::string prism_project_name_kr;
	static bool prism_subprocess_rebuild;

	static bool SetCamGlobalParam(obs_data_t *data);
	static unsigned WindowMessageLoopThread(void *pParam);
	static LRESULT WindowMessageProcedure(HWND, UINT, WPARAM, LPARAM);

public:
	PLSCamSession(ICamSessionCb *cb, obs_source_t *src);
	virtual ~PLSCamSession();

	// Remote requestion
	void SetDShowConfig(obs_data_t *settings);
	void SetBeautyConfig(obs_data_t *settings);
	void SetSegmenConfig(obs_data_t *settings);
	void SetAudioSettings(struct obs_audio_info &oai);
	void SetDirectX(struct gs_luid &luid);
	void SetDShowActive(bool active);
	void OpenDShowDialog(HWND parent, enum DShow::DialogType type);
	void RequestEnumDevice(bool request_video);
	void SetExtraInfo();

	void EnableRestore();
	bool IsChildProcessNormal();
	std::string ReadEnumDeviceList(bool request_video);
	bool ReadOutputAudio(audio_item_header &, audio_item_sample &);
	int MapOutputVideo(shared_handle_header &, shared_handle_sample &);
	void UnmapOutputVideo(int index);

	void PushEvent(obs_source_event_type type, int sub_code,
		       bool only_send_once = true);

	// PipeCallback
	virtual void OnMessage(PipeMessageHeader, std::shared_ptr<char>);
	virtual void OnError(pipe_error_type, unsigned);

	bool IsSubprocessRebuild();

private:
	std::string GenerateGuid();
	std::wstring GetProcessPath();
	HWND CreateLocalWindow();
	void ClearPipe();
	obs_source_event_type TransException(cam_effect_exception_type src);
	obs_source_error TransInvalidCode(cam_source_invalid_code src);
	enum AVSampleFormat TransFormatToFfmpeg(enum audio_format format);

	void WindowLoopThreadInner();
	void WindowLoopTimerTick();
	bool CheckCameraProcess();
	bool RunCameraProcess();
	bool CheckFindWindow();
	bool CheckHeartBeatTimeout();
	bool TrySendMessage(message_info &info);

	void OnException(std::shared_ptr<char> body);
	void OnImageNormal(std::shared_ptr<char> body);
	void OnDShowValid(std::shared_ptr<char> body);
	void OnSetupBuffering(std::shared_ptr<char> body);

	void OnParseCrashAndSendAction();
	long long TransExceptionCode(long long code);

private:
	struct PLSEventInfo {
		obs_source_event_type type;
		int sub_code;
		bool notify_done = false;
	};

	CCriticalSection exception_lock;
	std::map<obs_source_event_type, PLSEventInfo> exceptions;

	obs_source_t *source = NULL;
	ICamSessionCb *callback = NULL;
	std::string session_id = "";
	HANDLE camera_source_alive_flag = NULL;

	HANDLE video_enum_done = 0;
	HANDLE audio_enum_done = 0;
	std::string video_list_json = "";
	std::string audio_list_json = "";

	std::shared_ptr<AudioCircleBuffer> audio_reader;
	std::shared_ptr<SharedHandleBuffer> video_reader;

	std::string local_wnd_class_name = "";
	HANDLE local_wnd_loop_thread = NULL;
	HWND local_wnd_handle = NULL;
	PLSMsgQueue message_queue;

	std::atomic<unsigned int> child_exited_count = 0;
	bool retry_run_child = true;
	bool child_process_running = false;
	DWORD previous_run_time = 0;
	DWORD child_process_id = 0;
	HANDLE child_process_handle = NULL;
	HWND child_process_wnd_handle = NULL;
	bool heartbeat_timeout = false;
	DWORD previous_heartbeat = GetTickCount();

	std::shared_ptr<PipeServer> pipe_server;
	std::shared_ptr<PipeIO> pipe_io;

	std::string device_crash_path = "";
	std::string video_device_id = "";
};
