#pragma once

#include <string>

enum ENC_CHECKER { ENCODER_CHECK_STREAM = 1, ENCODER_CHECK_RECORD = ENCODER_CHECK_STREAM << 1, ENCODER_CHECK_ALL = (ENCODER_CHECK_STREAM | ENCODER_CHECK_RECORD) };

class PLSBasic;

struct BasicOutputHandler {
	OBSOutput fileOutput;
	OBSOutput streamOutput;
	OBSOutput replayBuffer;
	bool streamingActive = false;
	bool recordingActive = false;
	bool delayActive = false;
	bool replayBufferActive = false;

	PLSBasic *main;

	std::string outputType;
	std::string lastError;

	OBSSignal startRecording;
	OBSSignal stopRecording;
	OBSSignal startReplayBuffer;
	OBSSignal stopReplayBuffer;
	OBSSignal startStreaming;
	OBSSignal stopStreaming;
	OBSSignal streamDelayStarting;
	OBSSignal streamStopping;
	OBSSignal recordStopping;
	OBSSignal replayBufferStopping;
	OBSSignal replayBufferSaved;

	inline BasicOutputHandler(PLSBasic *main_) : main(main_) {}

	virtual ~BasicOutputHandler(){};

	virtual bool StartStreaming(obs_service_t *service) = 0;
	virtual bool StartRecording() = 0;
	virtual bool StartReplayBuffer() { return false; }
	virtual void StopStreaming(bool force = false) = 0;
	virtual void StopRecording(bool force = false) = 0;
	virtual void StopReplayBuffer(bool force = false) { (void)force; }
	virtual bool StreamingActive() const = 0;
	virtual bool RecordingActive() const = 0;
	virtual bool ReplayBufferActive() const { return false; }

	virtual void Update() = 0;

	//PRISM/LiuHaibin/20210906/#None/Pre-check encoders
	bool CheckEncoders(ENC_CHECKER flag);
	virtual bool CheckStreamEncoder() = 0;
	virtual bool CheckRecordEncoder() = 0;
	bool streamEncoderChecked = false;
	bool streamEncoderAvailable = false;
	bool recordEncoderChecked = false;
	bool recordEncoderAvailable = false;

	inline bool Active() const { return streamingActive || recordingActive || delayActive || replayBufferActive; }
};

BasicOutputHandler *CreateSimpleOutputHandler(PLSBasic *main);
BasicOutputHandler *CreateAdvancedOutputHandler(PLSBasic *main);
