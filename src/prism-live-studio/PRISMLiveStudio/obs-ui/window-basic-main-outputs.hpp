#pragma once

#include <future>
#include <memory>
#include <string>

#include "multitrack-video-output.hpp"

#define MAIN_OUTPUT_IDX 0
#define VERTICAL_OUTPUT_IDX 1

class OBSBasic;
class PLSOutputHandler;

using SetupStreamingContinuation_t = std::function<void(bool)>;

extern void obs_output_release2(obs_output_t *output);
using OBSOutputAutoRelease2 = OBSRefAutoRelease<obs_output_t *, obs_output_release2>;

struct BasicOutputHandler {
	OBSOutputAutoRelease2 fileOutput;
	OBSOutputAutoRelease2 streamOutput;
	OBSOutputAutoRelease2 replayBuffer;
	OBSOutputAutoRelease2 virtualCam;
	bool streamingActive = false;
	bool recordingActive = false;
	bool delayActive = false;
	bool replayBufferActive = false;
	bool virtualCamActive = false;
	OBSBasic *main;
	PLSOutputHandler *manager;
	size_t outputIndex;
	bool vertical;

	std::unique_ptr<MultitrackVideoOutput> multitrackVideo;
	bool multitrackVideoActive = false;

	OBSOutputAutoRelease StreamingOutput() const
	{
		return (multitrackVideo && multitrackVideoActive)
			       ? multitrackVideo->StreamingOutput()
			       : OBSOutputAutoRelease{obs_output_get_ref(streamOutput)};
	}

	QTimer delayTimer;

	obs_view_t *virtualCamView = nullptr;
	video_t *virtualCamVideo = nullptr;
	obs_scene_t *vCamSourceScene = nullptr;
	obs_sceneitem_t *vCamSourceSceneItem = nullptr;

	std::string outputType;
	std::string lastError;

	std::string lastRecordingPath;

	OBSSignal startRecording;
	OBSSignal stopRecording;
	OBSSignal startReplayBuffer;
	OBSSignal stopReplayBuffer;
	OBSSignal startStreaming;
	OBSSignal stopStreaming;
	OBSSignal reconnectStreaming;
	OBSSignal reconnectedStreaming;
	OBSSignal startVirtualCam;
	OBSSignal stopVirtualCam;
	OBSSignal deactivateVirtualCam;
	OBSSignal streamDelayStarting;
	OBSSignal streamStopping;
	OBSSignal recordStopping;
	OBSSignal recordFileChanged;
	OBSSignal replayBufferStopping;
	OBSSignal replayBufferSaved;

	uint64_t outputLastTotalFrame = 0;
	std::chrono::steady_clock::time_point outputFpsLastRequestTime;

	uint64_t recordLastTotalFrame = 0;
	std::chrono::steady_clock::time_point recordFpsLastRequestTime;

	inline BasicOutputHandler(OBSBasic *main_, PLSOutputHandler *manager_, size_t outputIndex);

	virtual ~BasicOutputHandler() {};

	virtual std::shared_future<void> SetupStreaming(obs_service_t *service,
							SetupStreamingContinuation_t continuation) = 0;
	virtual bool StartStreaming(obs_service_t *service) = 0;
	virtual bool StartRecording() = 0;
	virtual bool StartReplayBuffer() { return false; }
	virtual bool StartVirtualCam();
	virtual void StopStreaming(bool force = false) = 0;
	virtual void StopRecording(bool force = false) = 0;
	virtual void StopReplayBuffer(bool force = false) { (void)force; }
	virtual void StopVirtualCam();
	virtual bool StreamingActive() const = 0;
	virtual bool RecordingActive() const = 0;
	virtual bool ReplayBufferActive() const { return false; }
	virtual bool VirtualCamActive() const;

	virtual void Update() = 0;
	virtual void SetupOutputs() = 0;

	virtual void UpdateVirtualCamOutputSource();
	virtual void DestroyVirtualCamView();
	virtual void DestroyVirtualCameraScene();

	bool IsVertical() const { return vertical; };

	inline bool Active() const
	{
		return streamingActive || recordingActive || delayActive || replayBufferActive || virtualCamActive ||
		       multitrackVideoActive;
	}

	void CheckStreamingSettings(bool useStreamEncoder);
	void OptimizeStreamingVideoSettings(obs_encoder_t *encoder, obs_data_t *settings);
	bool IsWhip();
	const char *GetEncoderBFrameSettingName(obs_encoder_t *encoder);

protected:
	void SetupAutoRemux(const char *&container);
	std::string GetRecordingFilename(const char *path, const char *container, bool noSpace, bool overwrite,
					 const char *format, bool ffmpeg);

	std::shared_future<void> SetupMultitrackVideo(obs_service_t *service, std::string audio_encoder_id,
						      size_t main_audio_mixer, std::optional<size_t> vod_track_mixer,
						      std::function<void(std::optional<bool>)> continuation);
	OBSDataAutoRelease GenerateMultitrackVideoStreamDumpConfig();
};

BasicOutputHandler *CreateSimpleOutputHandler(OBSBasic *main, PLSOutputHandler *manager, bool vertical);
BasicOutputHandler *CreateAdvancedOutputHandler(OBSBasic *main, PLSOutputHandler *manager, bool vertical);
