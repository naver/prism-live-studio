#pragma once
#include <atomic>
#include <memory>
#include <Windows.h>

#include <obs-module.h>
#include <obs.hpp>
#include <liblog.h>
#include <libutils-api.h>
#include <frontend-api.h>
#include <pls/pls-obs-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>

#include "ipc-define.h"
#include "task-pool.h"
#include "pipe/prism-named-pipe.h"

// defined in lens app
#define FILEMAP_OUTPUT_STARTED_EVENT "output-started-event"                       // whether lens is sending video
#define FILEMAP_OUTPUT_STARTED_EVENT_AUDIO "output-started-event-audio"           // whether lens is sending audio
#define FILEMAP_OUTPUT_VIDEO_RUNNING_EVENT "prismlens-video-output-running-event" // whether video is necessary
#define FILEMAP_OUTPUT_AUDIO_RUNNING_EVENT "prismlens-audio-output-running-event" // whether audio is necessary

enum class LENS_STATUS {
	STATUS_NONE = 0,

	// if status is between STATUS_EXCEPTION_BEGIN~STATUS_EXCEPTION_END, logo of lens will be displayed soon
	STATUS_EXCEPTION_BEGIN = 100,
	STATUS_LENS_NOT_RUNNING,
	STATUS_INVALID_INDEX,
	STATUS_LENS_NO_DATA,
	STATUS_IPC_FAILED,
	STATUS_EXCEPTION_END = 200,

	// normal status
	STATUS_DEACTIVED,
	STATUS_WAIT_DATA,
	STATUS_CAPTURED,
	STATUS_EXITING,
};

//----------------------------------------------------------- capture implement ------------------------------------------------------------
class ICaptureSession {
public:
	static bool is_lens_running();
	static void init_lens_module();
	static void uninit_lens_module();

	ICaptureSession(obs_source_t *s, HANDLE &exit_evt, bool is_audio);
	virtual ~ICaptureSession() { assert(!capture_thread.joinable()); }

	bool is_active() { return actived.load(); }
	LENS_STATUS get_status() const { return source_status.load(); }

	void set_active(bool is_active);
	void set_device(uint32_t index);

	void start_capture();
	void stop_capture();

	void force_new_timestamp();

	std::shared_ptr<ITaskPool> get_task_pool() { return async_task; }

	virtual const char *get_session_type() = 0;
	virtual const char *get_capture_flag() = 0;
	virtual bool init_ipc_buffer_impl(uint32_t index) = 0;
	virtual void read_buffer_data() = 0;

protected:
	void lens_capture_thread();

	bool init_ipc_buffer(uint32_t index);
	void check_alignment();
	virtual void uninit_ipc_buffer(bool keepVideo = false);
	bool is_ipc_buffer_ready();

	bool is_lens_output_valid(uint32_t index);
	void update_status(LENS_STATUS s);

	void request_start();
	void request_heartbeat();
	void request_stop();

protected:
	obs_source_t *source = nullptr;
	HANDLE &exit_event; // only referencing object hold by prism_lens_source

	// Its value depends on the lens app and needs to be consistent with the lens
	// Before lens-1.0.8, alignment is 32 and it is changed to 64 since lens-1.0.8
	int ipc_alignment = 64;

	const bool is_audio_capture = false;
	std::atomic<LENS_STATUS> source_status = LENS_STATUS::STATUS_NONE; // save the status reason for capture thread
	std::atomic<bool> actived = false;

	std::thread capture_thread;
	std::shared_ptr<ITaskPool> async_task; // all async tasks are done in capture thread

	// ------------------- updated in capture thread ------------------
	bool is_first_data = true;
	uint64_t first_timestamp = 0;
	HANDLE flag_capturing = 0;       // tell lens app to send data
	uint32_t data_invalid_count = 0; // count how many times lens app does not send data
	std::atomic<uint32_t> selected_len = uint32_t(-1);
	std::atomic<uint32_t> used_lens = uint32_t(-1);
	std::shared_ptr<CircleBufferIPC> data_reader = nullptr; // only used in capture thread

	// --------------------- only used in capture thread for IPC -----------------------
	std::string instance_guid = "";
	ULONGLONG previous_heartbeat = 0; // last heartbeat time in ms
	bool is_start_event_sent = false; // whether PKG_START is sent
	named_pipe_client_t *pipe_client = nullptr;
};

class AudioCaptureSession : public ICaptureSession {
public:
	AudioCaptureSession(obs_source_t *s, HANDLE &exit_evt);
	~AudioCaptureSession() override;

	void set_use_audio(bool used);
	void set_reroute_target(obs_source_t *target);

	// implement ICaptureSession
	const char *get_session_type() override { return "audio_session"; }
	const char *get_capture_flag() override { return FILEMAP_OUTPUT_AUDIO_RUNNING_EVENT; }
	bool init_ipc_buffer_impl(uint32_t index) override;
	void read_buffer_data() override;

private:
	void clear_reroute_target();
	obs_source_t *get_reroute_target_ref();

protected:
	std::atomic<bool> use_audio = true;

	std::recursive_mutex lock_reroute;
	obs_weak_source_t *reroute_target = nullptr;
};

class VideoCaptureSession : public ICaptureSession {
public:
	VideoCaptureSession(obs_source_t *s, HANDLE &exit_evt) : ICaptureSession(s, exit_evt, false) {}
	~VideoCaptureSession() override { stop_capture(); }

	uint32_t get_video_handle() const { return shared_handle.load(); }

	// implement ICaptureSession
	const char *get_session_type() override { return "video_session"; }
	const char *get_capture_flag() override { return FILEMAP_OUTPUT_VIDEO_RUNNING_EVENT; }
	bool init_ipc_buffer_impl(uint32_t index) override;
	void uninit_ipc_buffer(bool keepVideo = false) override;
	void read_buffer_data() override;

protected:
	std::atomic<uint32_t> shared_handle = 0; // updated in capture thread and readed in render thread
};

//----------------------------------------------------------- plugin implement ------------------------------------------------------------
class prism_lens_source {
	obs_source_t *source = nullptr;

	HANDLE exit_event = 0;
	std::shared_ptr<AudioCaptureSession> audio_session = nullptr;
	std::shared_ptr<VideoCaptureSession> video_session = nullptr;

	std::atomic<uint32_t> selected_video = uint32_t(-1);
	std::atomic<bool> flip_video = false;
	std::atomic<bool> flip_video_h = false;

	// ------------------- updated in render thread -------------------
	gs_texture_t *shared_texture = nullptr;
	gs_texture_t *copy_texture = nullptr;
	bool timeout_happened = false;
	std::atomic<uint32_t> tex_width = 0;
	std::atomic<uint32_t> tex_height = 0;

	DWORD64 update_time = 0; // the time when video device is changed

public:
	static obs_properties_t *get_properties(void *obj, const char *keyTip);
	static bool on_device_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings);
	static bool activate_clicked(obs_properties_t *, obs_property_t *p, void *data);

	prism_lens_source(obs_data_t *settings, obs_source_t *source_);
	virtual ~prism_lens_source();

	uint32_t get_video_width();
	uint32_t get_video_height();
	void set_active(bool active_);

	void tick();
	void render();
	void update(obs_data_t *settings);

private:
	void settings_old_to_new(obs_data_t *settings, const char *old_key, const char *new_key, const std::map<std::string, int> &map_rule);

	void render_texture(gs_texture_t *draw_texture);
	void clear_texture();
};
