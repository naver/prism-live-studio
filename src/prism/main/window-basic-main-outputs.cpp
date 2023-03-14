#include <string>
#include <algorithm>
#include "qt-wrappers.hpp"
#include "audio-encoders.hpp"
#include "window-basic-main.hpp"
#include "window-basic-main-outputs.hpp"
#include "alert-view.hpp"
#include "main-view.hpp"
#include "PLSServerStreamHandler.hpp"
#include "PLSChannelDataAPI.h"
#include "pls/media-info.h"
#include "platform.hpp"

using namespace std;

extern bool EncoderAvailable(const char *encoder);
extern bool file_exists(const char *path);

volatile bool streaming_active = false;
volatile bool recording_active = false;
volatile bool recording_paused = false;
volatile bool replaybuf_active = false;

/* ---------------------------------------------------------------- */
//PRISM/LiuHaibin/20200320/#865/for outro
static bool StartOutro(void)
{
	QString path;
	uint timeOut = 0;
	if (PLSServerStreamHandler::instance()->getOutroInfo(path, &timeOut)) {
		if (obs_outro_activate(path.toStdString().c_str(), timeOut * 1000))
			return true;
		else
			PLS_WARN(MAIN_OUTPUT, "Activate Outro Failed, path %s, timeout %d (ms)", GetFileName(path.toStdString()).c_str(), timeOut);
	} else
		PLS_WARN(MAIN_OUTPUT, "No available Outro file");

	return false;
}

static bool IsSyncStopping(void)
{
	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	if (recordWhenStreaming && !keepRecordingWhenStreamStops)
		return true;
	return false;
}

static bool IsOnlyStopRecording(void)
{
	return PLSCHANNELS_API->isOnlyRecordStop();
}

static void AsyncStopStreaming(OBSOutput streamOutput)
{
	if (streamOutput && obs_output_active(streamOutput) && IsSyncStopping()) {
		if (!StartOutro()) {
			PLS_INFO(MAIN_OUTPUT, "[Sync Stopping], async start outro failed, stop streaming.");
			obs_output_stop(streamOutput);
		}
	}
}

static void TryToStopStreaming(BasicOutputHandler *outputHander)
{
	if (!outputHander) {
		PLS_WARN(MAIN_OUTPUT, "Error with params for output hander");
		return;
	}

	if (IsSyncStopping()) {
		if (obs_output_active(outputHander->fileOutput)) {
			PLS_INFO(MAIN_OUTPUT, "[Sync Stopping], recording is active, WILL start outro when recording stopped.");
			return;
		} else if (!StartOutro()) {
			PLS_INFO(MAIN_OUTPUT, "[Sync Stopping], start outro failed, stop streaming.");
			obs_output_stop(outputHander->streamOutput);
		}

	} else {
		if (obs_output_active(outputHander->fileOutput)) {
			PLS_INFO(MAIN_OUTPUT, "[Async Stopping], recording is active, do not start outro.");
			obs_output_stop(outputHander->streamOutput);
		} else if (!StartOutro()) {
			PLS_INFO(MAIN_OUTPUT, "[Async Stopping], start outro failed, stop streaming.");
			obs_output_stop(outputHander->streamOutput);
		}
	}
}

//PRISM/LiuHaibin/20200320/#none/for watermark
static void ResetWatermarkStatus(BasicOutputHandler *outputHander, bool isStreaming)
{
	if (!outputHander) {
		PLS_WARN(MAIN_OUTPUT, "Error with params for output hander");
		return;
	}

	OBSOutput theOther = isStreaming ? outputHander->fileOutput : outputHander->streamOutput;

	if (!obs_output_active(theOther)) {
		PLS_INFO(MAIN_OUTPUT, "No streaming & recording is active, set watermark refresh.");
		obs_watermark_set_refresh(true);
	}
}
//End
/* ---------------------------------------------------------------- */

static void PLSStreamStarting(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	obs_output_t *obj = (obs_output_t *)calldata_ptr(params, "output");

	int sec = (int)obs_output_get_active_delay(obj);
	if (sec == 0)
		return;

	output->delayActive = true;
	QMetaObject::invokeMethod(output->main, "StreamDelayStarting", Q_ARG(int, sec));
}

static void PLSStreamStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	obs_output_t *obj = (obs_output_t *)calldata_ptr(params, "output");

	int sec = (int)obs_output_get_active_delay(obj);
	if (sec == 0)
		QMetaObject::invokeMethod(output->main, "StreamStopping");
	else
		QMetaObject::invokeMethod(output->main, "StreamDelayStopping", Q_ARG(int, sec));
}

static void PLSStartStreaming(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	output->streamingActive = true;
	os_atomic_set_bool(&streaming_active, true);

	const char *fields[][2] = {{"outputStats", "Streaming Started"}};
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' started.", obs_output_get_id(output->streamOutput), obs_output_get_name(output->streamOutput));

	QMetaObject::invokeMethod(output->main, "StreamingStart");

	UNUSED_PARAMETER(params);
}

static const char *StopCodeString(int code)
{
	switch (code) {
	case OBS_OUTPUT_BAD_PATH:
		return "Bad Path";
	case OBS_OUTPUT_CONNECT_FAILED:
		return "Connect Failed";
	case OBS_OUTPUT_INVALID_STREAM:
		return "Invalid Stream";
	case OBS_OUTPUT_ERROR:
		return "Error";
	case OBS_OUTPUT_DISCONNECTED:
		return "Disconnected";
	case OBS_OUTPUT_UNSUPPORTED:
		return "Unsupported";
	case OBS_OUTPUT_NO_SPACE:
		return "No Space";
	case OBS_OUTPUT_ENCODE_ERROR:
		return "Encoder Error";
	default:
		break;
	}
	return "Unknown";
}

static void PLSStopStreaming(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *last_error = calldata_string(params, "last_error");

	QString arg_last_error = QString::fromUtf8(last_error);

	if (code != OBS_OUTPUT_SUCCESS) {
		const char *fields[][2] = {{"outputStats", "Streaming Abnormal Stopped"}, {"outputExceptionStats", StopCodeString(code)}};
		PLS_LOGEX(PLS_LOG_WARN, MAIN_OUTPUT, fields, 2, "[Output] %s :'%s' unexpected stopped due to error %d('%s'), msg '%s'.", obs_output_get_id(output->streamOutput),
			  obs_output_get_name(output->streamOutput), code, StopCodeString(code), last_error);
	} else {
		const char *fields[][2] = {{"outputStats", "Streaming Normally Stopped"}};
		PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' successfully stopped.", obs_output_get_id(output->streamOutput), obs_output_get_name(output->streamOutput));
	}

	output->streamingActive = false;
	output->delayActive = false;
	os_atomic_set_bool(&streaming_active, false);
	QMetaObject::invokeMethod(output->main, "StreamingStop", Q_ARG(int, code), Q_ARG(QString, arg_last_error));

	//PRISM/LiuHaibin/20210621/#None/clear id3v2 queue after streaming stopped.
	mi_clear_id3v2_queue();

	//PRISM/LiuHaibin/20200320/#none/for watermark
	ResetWatermarkStatus(output, true);
}

static void PLSStartRecording(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	const char *fields[][2] = {{"outputStats", "Recording Started"}};
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' started.", obs_output_get_id(output->fileOutput), obs_output_get_name(output->fileOutput));

	output->recordingActive = true;
	os_atomic_set_bool(&recording_active, true);
	QMetaObject::invokeMethod(output->main, "RecordingStart");

	UNUSED_PARAMETER(params);
}

static void PLSStopRecording(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *last_error = calldata_string(params, "last_error");

	QString arg_last_error = QString::fromUtf8(last_error);

	if (code != OBS_OUTPUT_SUCCESS) {
		const char *fields[][2] = {{"outputStats", "Recording Abnormal Stopped"}, {"outputExceptionStats", StopCodeString(code)}};
		PLS_LOGEX(PLS_LOG_WARN, MAIN_OUTPUT, fields, 2, "[Output] %s :'%s' unexpected stopped due to error %d('%s'), msg '%s'.", obs_output_get_id(output->fileOutput),
			  obs_output_get_name(output->fileOutput), code, StopCodeString(code), last_error);
	} else {
		const char *fields[][2] = {{"outputStats", "Recording Normally Stopped"}};
		PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' successfully stopped.", obs_output_get_id(output->fileOutput), obs_output_get_name(output->fileOutput));
	}

	output->recordingActive = false;
	os_atomic_set_bool(&recording_active, false);
	os_atomic_set_bool(&recording_paused, false);
	QMetaObject::invokeMethod(output->main, "RecordingStop", Q_ARG(int, code), Q_ARG(QString, arg_last_error));

	//PRISM/LiuHaibin/20200320/#865/for outro
	if (!IsOnlyStopRecording() && OBS_OUTPUT_SUCCESS == code && !obs_output_stopped_internal(output->fileOutput))
		AsyncStopStreaming(output->streamOutput);

	//PRISM/LiuHaibin/20200320/#none/for watermark
	ResetWatermarkStatus(output, false);

	UNUSED_PARAMETER(params);
}

static void PLSRecordStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	QMetaObject::invokeMethod(output->main, "RecordStopping");

	UNUSED_PARAMETER(params);
}

static void PLSStartReplayBuffer(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	const char *fields[][2] = {{"outputStats", "ReplayBuffer Started"}};
	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' started.", obs_output_get_id(output->replayBuffer), obs_output_get_name(output->replayBuffer));

	output->replayBufferActive = true;
	os_atomic_set_bool(&replaybuf_active, true);
	QMetaObject::invokeMethod(output->main, "ReplayBufferStart");

	UNUSED_PARAMETER(params);
}

static void PLSStopReplayBuffer(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");

	if (code != OBS_OUTPUT_SUCCESS) {
		const char *fields[][2] = {{"outputStats", "ReplayBuffer Abnormal Stopped"}, {"outputExceptionStats", StopCodeString(code)}};
		PLS_LOGEX(PLS_LOG_WARN, MAIN_OUTPUT, fields, 2, "[Output] %s :'%s' unexpected stopped due to error %d('%s').", obs_output_get_id(output->replayBuffer),
			  obs_output_get_name(output->replayBuffer), code, StopCodeString(code));
	} else {
		const char *fields[][2] = {{"outputStats", "ReplayBuffer Normally Stopped"}};
		PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' successfully stopped.", obs_output_get_id(output->replayBuffer), obs_output_get_name(output->replayBuffer));
	}

	output->replayBufferActive = false;
	os_atomic_set_bool(&replaybuf_active, false);
	QMetaObject::invokeMethod(output->main, "ReplayBufferStop", Q_ARG(int, code));

	UNUSED_PARAMETER(params);
}

static void PLSReplayBufferStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	QMetaObject::invokeMethod(output->main, "ReplayBufferStopping");

	UNUSED_PARAMETER(params);
}

static void PLSReplayBufferSaved(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *lastError = calldata_string(params, "last_error");

	if (code != OBS_OUTPUT_SUCCESS) {
		const char *fields[][2] = {{"outputStats", "ReplayBuffer Abnormal Saved"}, {"outputExceptionStats", StopCodeString(code)}};
		PLS_LOGEX(PLS_LOG_WARN, MAIN_OUTPUT, fields, 2, "[Output] %s :'%s' unexpected stopped due to error %d('%s'), msg '%s'.", obs_output_get_id(output->replayBuffer),
			  obs_output_get_name(output->replayBuffer), code, StopCodeString(code), lastError);
	} else {
		const char *fields[][2] = {{"outputStats", "ReplayBuffer Normally Saved"}};
		PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT, fields, 1, "[Output] %s :'%s' successfully stopped.", obs_output_get_id(output->replayBuffer), obs_output_get_name(output->replayBuffer));
	}

	QMetaObject::invokeMethod(output->main, "ReplayBufferSaved", Q_ARG(int, code), Q_ARG(QString, QString::fromUtf8(lastError)));
}

static void FindBestFilename(string &strPath, bool noSpace)
{
	int num = 2;

	if (!file_exists(strPath.c_str()))
		return;

	const char *ext = strrchr(strPath.c_str(), '.');
	if (!ext)
		return;

	int extStart = int(ext - strPath.c_str());
	for (;;) {
		string testPath = strPath;
		string numStr;

		numStr = noSpace ? "_" : " (";
		numStr += to_string(num++);
		if (!noSpace)
			numStr += ")";

		testPath.insert(extStart, numStr);

		if (!file_exists(testPath.c_str())) {
			strPath = testPath;
			break;
		}
	}
}

/* ------------------------------------------------------------------------ */

static bool CreateAACEncoder(OBSEncoder &res, string &id, int bitrate, const char *name, size_t idx)
{
	const char *id_ = GetAACEncoderForBitrate(bitrate);
	if (!id_) {
		id.clear();
		res = nullptr;
		return false;
	}

	if (id == id_)
		return true;

	id = id_;
	res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);

	if (res) {
		obs_encoder_release(res);
		return true;
	}

	return false;
}

/* ------------------------------------------------------------------------ */

struct SimpleOutput : BasicOutputHandler {
	OBSEncoder aacStreaming;
	OBSEncoder h264Streaming;
	OBSEncoder aacRecording;
	OBSEncoder h264Recording;

	string aacRecEncID;
	string aacStreamEncID;

	OBSEncoder immersiveAACStreaming[BTRS_TOTAL_TRACKS];
	OBSEncoder immersiveAACRecording[BTRS_TOTAL_TRACKS];
	string immersiveAACRecEncID[BTRS_TOTAL_TRACKS];
	string immersiveAACStreamEncID[BTRS_TOTAL_TRACKS];

	string videoEncoder;
	string videoQuality;
	bool usingRecordingPreset = false;
	bool recordingConfigured = false;
	bool ffmpegOutput = false;
	bool lowCPUx264 = false;

	explicit SimpleOutput(PLSBasic *main_);

	int CalcCRF(int crf);

	void UpdateStreamingSettings_amd(obs_data_t *settings, int bitrate);
	void UpdateRecordingSettings_x264_crf(int crf);
	void UpdateRecordingSettings_qsv11(int crf);
	void UpdateRecordingSettings_nvenc(int cqp);
	void UpdateRecordingSettings_amd_cqp(int cqp);
	void UpdateRecordingSettings();
	void UpdateRecordingAudioSettings();
	virtual void Update() override;

	void SetupOutputs();
	int GetAudioBitrate() const;

	void LoadRecordingPreset_h264(const char *encoder);
	void LoadRecordingPreset_Lossless();
	void LoadRecordingPreset();

	void LoadStreamingPreset_h264(const char *encoder);

	void UpdateRecording();
	bool ConfigureRecording(bool useReplayBuffer);

	virtual bool StartStreaming(obs_service_t *service) override;
	virtual bool StartRecording() override;
	virtual bool StartReplayBuffer() override;
	virtual void StopStreaming(bool force) override;
	virtual void StopRecording(bool force) override;
	virtual void StopReplayBuffer(bool force) override;
	virtual bool StreamingActive() const override;
	virtual bool RecordingActive() const override;
	virtual bool ReplayBufferActive() const override;

	//PRISM/LiuHaibin/20210906/Pre-check encoders
	bool CheckStreamEncoder() override;
	bool CheckRecordEncoder() override;
};

void SimpleOutput::LoadRecordingPreset_Lossless()
{
	fileOutput = obs_output_create("ffmpeg_output", "simple_ffmpeg_output", nullptr, nullptr);
	if (!fileOutput)
		throw "Failed to create recording FFmpeg output "
		      "(simple output)";
	obs_output_release(fileOutput);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "format_name", "avi");
	obs_data_set_string(settings, "video_encoder", "utvideo");
	obs_data_set_string(settings, "audio_encoder", "pcm_s16le");

	int aMixes = 1;
	obs_output_set_mixers(fileOutput, aMixes);
	obs_output_update(fileOutput, settings);
	obs_data_release(settings);
}

void SimpleOutput::LoadRecordingPreset_h264(const char *encoderId)
{
	h264Recording = obs_video_encoder_create(encoderId, "simple_h264_recording", nullptr, nullptr);
	if (!h264Recording)
		throw "Failed to create h264 recording encoder (simple output)";
	obs_encoder_release(h264Recording);
}

void SimpleOutput::LoadStreamingPreset_h264(const char *encoderId)
{
	h264Streaming = obs_video_encoder_create(encoderId, "simple_h264_stream", nullptr, nullptr);
	if (!h264Streaming)
		throw "Failed to create h264 streaming encoder (simple output)";
	obs_encoder_release(h264Streaming);
}

void SimpleOutput::LoadRecordingPreset()
{
	const char *quality = config_get_string(main->Config(), "SimpleOutput", "RecQuality");
	const char *encoder = config_get_string(main->Config(), "SimpleOutput", "RecEncoder");

	videoEncoder = encoder;
	videoQuality = quality;
	ffmpegOutput = false;

	if (strcmp(quality, "Stream") == 0) {
		h264Recording = h264Streaming;
		aacRecording = aacStreaming;

		for (size_t i = 0; i < BTRS_TOTAL_TRACKS; i++)
			immersiveAACRecording[i] = immersiveAACStreaming[i];

		usingRecordingPreset = false;
		return;

	} else if (strcmp(quality, "Lossless") == 0) {
		LoadRecordingPreset_Lossless();
		usingRecordingPreset = true;
		ffmpegOutput = true;
		return;

	} else {
		lowCPUx264 = false;

		if (strcmp(encoder, SIMPLE_ENCODER_X264) == 0) {
			LoadRecordingPreset_h264("obs_x264");
		} else if (strcmp(encoder, SIMPLE_ENCODER_X264_LOWCPU) == 0) {
			LoadRecordingPreset_h264("obs_x264");
			lowCPUx264 = true;
		} else if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
			LoadRecordingPreset_h264("obs_qsv11");
		} else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
			LoadRecordingPreset_h264("amd_amf_h264");
		} else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
			const char *id = EncoderAvailable("jim_nvenc") ? "jim_nvenc" : "ffmpeg_nvenc";
			LoadRecordingPreset_h264(id);
		}
		usingRecordingPreset = true;

		if (!CreateAACEncoder(aacRecording, aacRecEncID, 192, "simple_aac_recording", 0))
			throw "Failed to create aac recording encoder "
			      "(simple output)";

		for (size_t i = 0; i < BTRS_TOTAL_TRACKS; i++) {
			std::string name = "simple_immersive_aac_recording_" + std::to_string(i);
			if (!CreateAACEncoder(immersiveAACRecording[i], immersiveAACRecEncID[i], 192, name.c_str(), i))
				throw "Failed to create aac recording encoder "
				      "(simple output)";
		}
	}
}

SimpleOutput::SimpleOutput(PLSBasic *main_) : BasicOutputHandler(main_)
{
	const char *encoder = config_get_string(main->Config(), "SimpleOutput", "StreamEncoder");

	if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
		LoadStreamingPreset_h264("obs_qsv11");

	} else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
		LoadStreamingPreset_h264("amd_amf_h264");

	} else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
		const char *id = EncoderAvailable("jim_nvenc") ? "jim_nvenc" : "ffmpeg_nvenc";
		LoadStreamingPreset_h264(id);

	} else {
		LoadStreamingPreset_h264("obs_x264");
	}

	if (!CreateAACEncoder(aacStreaming, aacStreamEncID, GetAudioBitrate(), "simple_aac", 0))
		throw "Failed to create aac streaming encoder (simple output)";

	//Create all immersive aac encoder in constructor
	for (size_t i = 0; i < BTRS_TOTAL_TRACKS; i++) {
		std::string id;
		std::string name = "simple_immersive_aac_stream_" + std::to_string(i);
		if (!CreateAACEncoder(immersiveAACStreaming[i], id, GetAudioBitrate(), name.c_str(), i))
			throw "Failed to create streaming audio encoder "
			      "(advanced output)";
	}

	LoadRecordingPreset();

	if (!ffmpegOutput) {
		bool useReplayBuffer = config_get_bool(main->Config(), "SimpleOutput", "RecRB");
		if (useReplayBuffer) {
			const char *str = config_get_string(main->Config(), "Hotkeys", "ReplayBuffer");
			obs_data_t *hotkey = obs_data_create_from_json(str);
			replayBuffer = obs_output_create("replay_buffer", Str("ReplayBuffer"), nullptr, hotkey);

			obs_data_release(hotkey);
			if (!replayBuffer)
				throw "Failed to create replay buffer output "
				      "(simple output)";
			obs_output_release(replayBuffer);

			signal_handler_t *signal = obs_output_get_signal_handler(replayBuffer);

			startReplayBuffer.Connect(signal, "start", PLSStartReplayBuffer, this);
			stopReplayBuffer.Connect(signal, "stop", PLSStopReplayBuffer, this);
			replayBufferStopping.Connect(signal, "stopping", PLSReplayBufferStopping, this);
			replayBufferSaved.Connect(signal, "replay_buffer_saved", PLSReplayBufferSaved, this);
		}

		fileOutput = obs_output_create("ffmpeg_muxer", "simple_file_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording output "
			      "(simple output)";
		obs_output_release(fileOutput);
	}

	startRecording.Connect(obs_output_get_signal_handler(fileOutput), "start", PLSStartRecording, this);
	stopRecording.Connect(obs_output_get_signal_handler(fileOutput), "stop", PLSStopRecording, this);
	recordStopping.Connect(obs_output_get_signal_handler(fileOutput), "stopping", PLSRecordStopping, this);
}

int SimpleOutput::GetAudioBitrate() const
{
	int bitrate = (int)config_get_uint(main->Config(), "SimpleOutput", "ABitrate");

	return FindClosestAvailableAACBitrate(bitrate);
}

void SimpleOutput::Update()
{
	obs_data_t *h264Settings = obs_data_create();
	obs_data_t *aacSettings = obs_data_create();

	int videoBitrate = config_get_uint(main->Config(), "SimpleOutput", "VBitrate");
	int audioBitrate = GetAudioBitrate();
	bool advanced = config_get_bool(main->Config(), "SimpleOutput", "UseAdvanced");
	bool enforceBitrate = config_get_bool(main->Config(), "SimpleOutput", "EnforceBitrate");
	const char *custom = config_get_string(main->Config(), "SimpleOutput", "x264Settings");
	const char *encoder = config_get_string(main->Config(), "SimpleOutput", "StreamEncoder");

	int simpleBTRSTrack = config_get_int(main->Config(), "SimpleOutput", "TrackIndex") - 1;
	if (simpleBTRSTrack != 0)
		simpleBTRSTrack = 1;

	const char *presetType;
	const char *preset;

	if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
		presetType = "QSVPreset";

	} else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
		presetType = "AMDPreset";
		UpdateStreamingSettings_amd(h264Settings, videoBitrate);

	} else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
		presetType = "NVENCPreset";

	} else {
		presetType = "Preset";
	}

	preset = config_get_string(main->Config(), "SimpleOutput", presetType);

	obs_data_set_string(h264Settings, "rate_control", "CBR");
	obs_data_set_int(h264Settings, "bitrate", videoBitrate);

	if (advanced) {
		obs_data_set_string(h264Settings, "preset", preset);
		obs_data_set_string(h264Settings, "x264opts", custom);
	}

	obs_data_set_string(aacSettings, "rate_control", "CBR");
	obs_data_set_int(aacSettings, "bitrate", audioBitrate);

	obs_service_apply_encoder_settings(main->GetService(), h264Settings, aacSettings);

	if (advanced && !enforceBitrate) {
		obs_data_set_int(h264Settings, "bitrate", videoBitrate);
		obs_data_set_int(aacSettings, "bitrate", audioBitrate);
	}

	video_t *video = obs_get_video();
	enum video_format format = video_output_get_format(video);

	if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
		obs_encoder_set_preferred_video_format(h264Streaming, VIDEO_FORMAT_NV12);

	obs_encoder_update(h264Streaming, h264Settings);
	obs_encoder_update(aacStreaming, aacSettings);
	for (size_t i = 0; i <= simpleBTRSTrack; i++)
		obs_encoder_update(immersiveAACStreaming[i], aacSettings);

	obs_data_release(h264Settings);
	obs_data_release(aacSettings);
}

void SimpleOutput::UpdateRecordingAudioSettings()
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "bitrate", 192);
	obs_data_set_string(settings, "rate_control", "CBR");

	obs_encoder_update(aacRecording, settings);
	int simpleBTRSTrack = config_get_int(main->Config(), "SimpleOutput", "TrackIndex") - 1;
	if (simpleBTRSTrack != 0)
		simpleBTRSTrack = 1;
	for (size_t i = 0; i <= simpleBTRSTrack; i++)
		obs_encoder_update(immersiveAACRecording[i], settings);

	obs_data_release(settings);
}

#define CROSS_DIST_CUTOFF 2000.0

int SimpleOutput::CalcCRF(int crf)
{
	int cx = config_get_uint(main->Config(), "Video", "OutputCX");
	int cy = config_get_uint(main->Config(), "Video", "OutputCY");
	double fCX = double(cx);
	double fCY = double(cy);

	if (lowCPUx264)
		crf -= 2;

	double crossDist = sqrt(fCX * fCX + fCY * fCY);
	double crfResReduction = fmin(CROSS_DIST_CUTOFF, crossDist) / CROSS_DIST_CUTOFF;
	crfResReduction = (1.0 - crfResReduction) * 10.0;

	return crf - int(crfResReduction);
}

void SimpleOutput::UpdateRecordingSettings_x264_crf(int crf)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "crf", crf);
	obs_data_set_bool(settings, "use_bufsize", true);
	obs_data_set_string(settings, "rate_control", "CRF");
	obs_data_set_string(settings, "profile", "high");
	obs_data_set_string(settings, "preset", lowCPUx264 ? "ultrafast" : "veryfast");

	obs_encoder_update(h264Recording, settings);

	obs_data_release(settings);
}

static bool icq_available(obs_encoder_t *encoder)
{
	obs_properties_t *props = obs_encoder_properties(encoder);
	obs_property_t *p = obs_properties_get(props, "rate_control");
	bool icq_found = false;

	size_t num = obs_property_list_item_count(p);
	for (size_t i = 0; i < num; i++) {
		const char *val = obs_property_list_item_string(p, i);
		if (strcmp(val, "ICQ") == 0) {
			icq_found = true;
			break;
		}
	}

	obs_properties_destroy(props);
	return icq_found;
}

void SimpleOutput::UpdateRecordingSettings_qsv11(int crf)
{
	bool icq = icq_available(h264Recording);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "profile", "high");

	if (icq) {
		obs_data_set_string(settings, "rate_control", "ICQ");
		obs_data_set_int(settings, "icq_quality", crf);
	} else {
		obs_data_set_string(settings, "rate_control", "CQP");
		obs_data_set_int(settings, "qpi", crf);
		obs_data_set_int(settings, "qpp", crf);
		obs_data_set_int(settings, "qpb", crf);
	}

	obs_encoder_update(h264Recording, settings);

	obs_data_release(settings);
}

void SimpleOutput::UpdateRecordingSettings_nvenc(int cqp)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "rate_control", "CQP");
	obs_data_set_string(settings, "profile", "high");
	obs_data_set_string(settings, "preset", "hq");
	obs_data_set_int(settings, "cqp", cqp);

	obs_encoder_update(h264Recording, settings);

	obs_data_release(settings);
}

void SimpleOutput::UpdateStreamingSettings_amd(obs_data_t *settings, int bitrate)
{
	// Static Properties
	obs_data_set_int(settings, "Usage", 0);
	obs_data_set_int(settings, "Profile", 100); // High

	// Rate Control Properties
	obs_data_set_int(settings, "RateControlMethod", 3);
	obs_data_set_int(settings, "Bitrate.Target", bitrate);
	obs_data_set_int(settings, "FillerData", 1);
	obs_data_set_int(settings, "VBVBuffer", 1);
	obs_data_set_int(settings, "VBVBuffer.Size", bitrate);

	// Picture Control Properties
	obs_data_set_double(settings, "KeyframeInterval", 2.0);
	obs_data_set_int(settings, "BFrame.Pattern", 0);
}

void SimpleOutput::UpdateRecordingSettings_amd_cqp(int cqp)
{
	obs_data_t *settings = obs_data_create();

	// Static Properties
	obs_data_set_int(settings, "Usage", 0);
	obs_data_set_int(settings, "Profile", 100); // High

	// Rate Control Properties
	obs_data_set_int(settings, "RateControlMethod", 0);
	obs_data_set_int(settings, "QP.IFrame", cqp);
	obs_data_set_int(settings, "QP.PFrame", cqp);
	obs_data_set_int(settings, "QP.BFrame", cqp);
	obs_data_set_int(settings, "VBVBuffer", 1);
	obs_data_set_int(settings, "VBVBuffer.Size", 100000);

	// Picture Control Properties
	obs_data_set_double(settings, "KeyframeInterval", 2.0);
	obs_data_set_int(settings, "BFrame.Pattern", 0);

	// Update and release
	obs_encoder_update(h264Recording, settings);
	obs_data_release(settings);
}

void SimpleOutput::UpdateRecordingSettings()
{
	bool ultra_hq = (videoQuality == "HQ");
	int crf = CalcCRF(ultra_hq ? 16 : 23);

	if (astrcmp_n(videoEncoder.c_str(), "x264", 4) == 0) {
		UpdateRecordingSettings_x264_crf(crf);

	} else if (videoEncoder == SIMPLE_ENCODER_QSV) {
		UpdateRecordingSettings_qsv11(crf);

	} else if (videoEncoder == SIMPLE_ENCODER_AMD) {
		UpdateRecordingSettings_amd_cqp(crf);

	} else if (videoEncoder == SIMPLE_ENCODER_NVENC) {
		UpdateRecordingSettings_nvenc(crf);
	}
	UpdateRecordingAudioSettings();
}

inline void SimpleOutput::SetupOutputs()
{
	SimpleOutput::Update();
	obs_encoder_set_video(h264Streaming, obs_get_video());
	obs_encoder_set_audio(aacStreaming, obs_get_audio());

	int simpleBTRSTrack = config_get_int(main->Config(), "SimpleOutput", "TrackIndex") - 1;
	if (simpleBTRSTrack != 0)
		simpleBTRSTrack = 1;
	for (size_t i = 0; i <= simpleBTRSTrack; i++)
		obs_encoder_set_audio(immersiveAACStreaming[i], obs_get_audio());

	obs_output_set_media(streamOutput, obs_get_video(), obs_get_audio());
	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_set_media(replayBuffer, obs_get_video(), obs_get_audio());

	if (usingRecordingPreset) {
		if (ffmpegOutput) {
			obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
		} else {
			obs_encoder_set_video(h264Recording, obs_get_video());
			obs_encoder_set_audio(aacRecording, obs_get_audio());
			for (size_t i = 0; i <= simpleBTRSTrack; i++)
				obs_encoder_set_audio(immersiveAACRecording[i], obs_get_audio());
		}
	}
}

const char *FindAudioEncoderFromCodec(const char *type)
{
	const char *alt_enc_id = nullptr;
	size_t i = 0;

	while (obs_enum_encoder_types(i++, &alt_enc_id)) {
		const char *codec = obs_get_encoder_codec(alt_enc_id);
		if (strcmp(type, codec) == 0) {
			return alt_enc_id;
		}
	}

	return nullptr;
}

bool SimpleOutput::StartStreaming(obs_service_t *service)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] SimpleOutput::StartStreaming, service(%p, %s).", service, obs_service_get_output_type(service));
	if (!Active())
		SetupOutputs();

	Auth *auth = main->GetAuth();
	if (auth)
		auth->OnStreamConfig();

	/* --------------------- */

	const char *type = obs_service_get_output_type(service);
	if (!type)
		type = "rtmp_output";

	/* XXX: this is messy and disgusting and should be refactored */
	if (outputType != type) {
		streamDelayStarting.Disconnect();
		streamStopping.Disconnect();
		startStreaming.Disconnect();
		stopStreaming.Disconnect();

		streamOutput = obs_output_create(type, "simple_stream", nullptr, nullptr);
		if (!streamOutput) {
			PLS_WARN(MAIN_OUTPUT,
				 "Creation of stream output type '%s' "
				 "failed!",
				 type);
			return false;
		}
		obs_output_release(streamOutput);

		streamDelayStarting.Connect(obs_output_get_signal_handler(streamOutput), "starting", PLSStreamStarting, this);
		streamStopping.Connect(obs_output_get_signal_handler(streamOutput), "stopping", PLSStreamStopping, this);

		startStreaming.Connect(obs_output_get_signal_handler(streamOutput), "start", PLSStartStreaming, this);
		stopStreaming.Connect(obs_output_get_signal_handler(streamOutput), "stop", PLSStopStreaming, this);

		bool isEncoded = obs_output_get_flags(streamOutput) & OBS_OUTPUT_ENCODED;

		if (isEncoded) {
			const char *codec = obs_output_get_supported_audio_codecs(streamOutput);
			if (!codec) {
				PLS_WARN(MAIN_OUTPUT, "Failed to load audio codec");
				return false;
			}

			// BTRS support FDK-AAC only
			if (!pls_is_immersive_audio()) {
				if (strcmp(codec, "aac") != 0) {
					const char *id = FindAudioEncoderFromCodec(codec);
					int audioBitrate = GetAudioBitrate();
					obs_data_t *settings = obs_data_create();
					obs_data_set_int(settings, "bitrate", audioBitrate);

					aacStreaming = obs_audio_encoder_create(id, "alt_audio_enc", nullptr, 0, nullptr);
					obs_encoder_release(aacStreaming);
					if (!aacStreaming)
						return false;

					obs_encoder_update(aacStreaming, settings);
					obs_encoder_set_audio(aacStreaming, obs_get_audio());

					obs_data_release(settings);
				}
			}
		}

		outputType = type;
	}

	obs_output_set_video_encoder(streamOutput, h264Streaming);
	// remove all audio encoders before set new audio encoders
	for (size_t i = 0; i <= MAX_AUDIO_MIXES; i++)
		obs_output_remove_audio_encoder(streamOutput, nullptr, i);
	if (pls_is_immersive_audio()) {
		int simpleBTRSTrack = config_get_int(main->Config(), "SimpleOutput", "TrackIndex") - 1;
		if (simpleBTRSTrack != 0)
			simpleBTRSTrack = 1;
		for (size_t i = 0; i <= simpleBTRSTrack; i++)
			obs_output_set_audio_encoder(streamOutput, immersiveAACStreaming[i], i);
	} else
		obs_output_set_audio_encoder(streamOutput, aacStreaming, 0);

	obs_output_set_service(streamOutput, service);

	/* --------------------- */

	bool reconnect = config_get_bool(main->Config(), "Output", "Reconnect");
	int retryDelay = config_get_uint(main->Config(), "Output", "RetryDelay");
	int maxRetries = config_get_uint(main->Config(), "Output", "MaxRetries");
	// Zhangdewen remove stream delay feature issue: 2231
	bool useDelay = false; // config_get_bool(main->Config(), "Output", "DelayEnable");
	int delaySec = config_get_int(main->Config(), "Output", "DelaySec");
	bool preserveDelay = config_get_bool(main->Config(), "Output", "DelayPreserve");
	const char *bindIP = config_get_string(main->Config(), "Output", "BindIP");
	bool enableNewSocketLoop = config_get_bool(main->Config(), "Output", "NewSocketLoopEnable");
	bool enableLowLatencyMode = config_get_bool(main->Config(), "Output", "LowLatencyEnable");
	bool enableDynBitrate = config_get_bool(main->Config(), "Output", "DynamicBitrate");

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "bind_ip", bindIP);
	obs_data_set_bool(settings, "new_socket_loop_enabled", enableNewSocketLoop);
	obs_data_set_bool(settings, "low_latency_mode_enabled", enableLowLatencyMode);
	obs_data_set_bool(settings, "dyn_bitrate", enableDynBitrate);
	obs_output_update(streamOutput, settings);
	obs_data_release(settings);

	if (!reconnect)
		maxRetries = 0;

	obs_output_set_delay(streamOutput, useDelay ? delaySec : 0, preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);

	obs_output_set_reconnect_settings(streamOutput, maxRetries, retryDelay);
	obs_output_set_immersive_audio(streamOutput, pls_is_immersive_audio());
	if (obs_output_start(streamOutput)) {
		return true;
	}

	const char *error = obs_output_get_last_error(streamOutput);
	bool hasLastError = error && *error;
	if (hasLastError)
		lastError = error;
	else
		lastError = string();

	PLS_WARN(MAIN_OUTPUT, "Stream output type '%s' failed to start!%s%s", type, hasLastError ? "  Last Error: " : "", hasLastError ? error : "");
	ResetWatermarkStatus(this, true);
	return false;
}

static void remove_reserved_file_characters(string &s)
{
	replace(s.begin(), s.end(), '/', '_');
	replace(s.begin(), s.end(), '\\', '_');
	replace(s.begin(), s.end(), '*', '_');
	replace(s.begin(), s.end(), '?', '_');
	replace(s.begin(), s.end(), '"', '_');
	replace(s.begin(), s.end(), '|', '_');
	replace(s.begin(), s.end(), ':', '_');
	replace(s.begin(), s.end(), '>', '_');
	replace(s.begin(), s.end(), '<', '_');
}

static void ensure_directory_exists(string &path)
{
	replace(path.begin(), path.end(), '\\', '/');

	size_t last = path.rfind('/');
	if (last == string::npos)
		return;

	string directory = path.substr(0, last);
	os_mkdirs(directory.c_str());
}

void SimpleOutput::UpdateRecording()
{
	if (replayBufferActive || recordingActive)
		return;

	if (usingRecordingPreset) {
		if (!ffmpegOutput)
			UpdateRecordingSettings();
	} else if (!obs_output_active(streamOutput)) {
		Update();
	}

	if (!Active())
		SetupOutputs();

	// remove all audio encoders before set new audio encoders
	for (size_t i = 0; i <= MAX_AUDIO_MIXES; i++) {
		obs_output_remove_audio_encoder(fileOutput, nullptr, i);
		if (replayBuffer)
			obs_output_remove_audio_encoder(replayBuffer, nullptr, i);
	}

	if (pls_is_immersive_audio()) {
		const char *recFormat = config_get_string(main->Config(), "SimpleOutput", "RecFormat");
		bool flv = strcmp(recFormat, "flv") == 0;
		int simpleBTRSTrack = config_get_int(main->Config(), "SimpleOutput", "TrackIndex") - 1;
		if (simpleBTRSTrack != 0)
			simpleBTRSTrack = 1;
		// flv only support one track
		if (flv)
			simpleBTRSTrack = 0;
		if (!ffmpegOutput) {
			obs_output_set_video_encoder(fileOutput, h264Recording);
			for (size_t i = 0; i <= simpleBTRSTrack; i++)
				obs_output_set_audio_encoder(fileOutput, immersiveAACRecording[i], i);
		}
		if (replayBuffer) {
			obs_output_set_video_encoder(replayBuffer, h264Recording);
			for (size_t i = 0; i <= simpleBTRSTrack; i++)
				obs_output_set_audio_encoder(replayBuffer, immersiveAACRecording[i], i);
		}
	} else {
		if (!ffmpegOutput) {
			obs_output_set_video_encoder(fileOutput, h264Recording);
			obs_output_set_audio_encoder(fileOutput, aacRecording, 0);
		}
		if (replayBuffer) {
			obs_output_set_video_encoder(replayBuffer, h264Recording);
			obs_output_set_audio_encoder(replayBuffer, aacRecording, 0);
		}
	}

	recordingConfigured = true;
}

bool SimpleOutput::ConfigureRecording(bool updateReplayBuffer)
{
	const char *path = config_get_string(main->Config(), "SimpleOutput", "FilePath");
	const char *format = config_get_string(main->Config(), "SimpleOutput", "RecFormat");
	const char *mux = config_get_string(main->Config(), "SimpleOutput", "MuxerCustom");
	bool noSpace = config_get_bool(main->Config(), "SimpleOutput", "FileNameWithoutSpace");
	const char *filenameFormat = config_get_string(main->Config(), "Output", "FilenameFormatting");
	bool overwriteIfExists = config_get_bool(main->Config(), "Output", "OverwriteIfExists");
	const char *rbPrefix = config_get_string(main->Config(), "SimpleOutput", "RecRBPrefix");
	const char *rbSuffix = config_get_string(main->Config(), "SimpleOutput", "RecRBSuffix");
	int rbTime = config_get_int(main->Config(), "SimpleOutput", "RecRBTime");
	int rbSize = config_get_int(main->Config(), "SimpleOutput", "RecRBSize");

	os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;
	if (!dir) {
		if (main->isVisible())
			PLSMessageBox::warning(main, QTStr("Output.BadPath.Title"), QTStr("Output.BadPath.Text"));
		else
			main->SysTrayNotify(QTStr("Output.BadPath.Text"), QSystemTrayIcon::Warning);
		return false;
	}

	os_closedir(dir);

	string strPath;
	strPath += path;

	char lastChar = strPath.back();
	if (lastChar != '/' && lastChar != '\\')
		strPath += "/";

	strPath += GenerateSpecifiedFilename(ffmpegOutput ? "avi" : format, noSpace, filenameFormat);
	ensure_directory_exists(strPath);
	if (!overwriteIfExists)
		FindBestFilename(strPath, noSpace);

	obs_data_t *settings = obs_data_create();
	if (updateReplayBuffer) {
		string f;

		if (rbPrefix && *rbPrefix) {
			f += rbPrefix;
			if (f.back() != ' ')
				f += " ";
		}

		f += filenameFormat;

		if (rbSuffix && *rbSuffix) {
			if (*rbSuffix != ' ')
				f += " ";
			f += rbSuffix;
		}

		remove_reserved_file_characters(f);

		obs_data_set_string(settings, "directory", path);
		obs_data_set_string(settings, "format", f.c_str());
		obs_data_set_string(settings, "extension", format);
		obs_data_set_bool(settings, "allow_spaces", !noSpace);
		obs_data_set_int(settings, "max_time_sec", rbTime);
		obs_data_set_int(settings, "max_size_mb", usingRecordingPreset ? rbSize : 0);
	} else {
		obs_data_set_string(settings, ffmpegOutput ? "url" : "path", strPath.c_str());
	}

	obs_data_set_string(settings, "muxer_settings", mux);

	if (updateReplayBuffer)
		obs_output_update(replayBuffer, settings);
	else
		obs_output_update(fileOutput, settings);

	obs_data_release(settings);
	return true;
}

bool SimpleOutput::StartRecording()
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] SimpleOutput::StartRecording.");
	UpdateRecording();
	if (!ConfigureRecording(false))
		return false;
	if (!obs_output_start(fileOutput)) {
		QString error_reason;
		const char *error = obs_output_get_last_error(fileOutput);
		if (error)
			error_reason = QT_UTF8(error);
		else
			error_reason = QTStr("Output.StartFailedGeneric");
		PLSAlertView::critical(main->getMainView(), QTStr("Output.StartRecordingFailed"), error_reason);

		PLS_WARN(MAIN_OUTPUT, "Recording for SimpleOutput failed to start!%s", error_reason.toUtf8().constData());
		ResetWatermarkStatus(this, false);
		return false;
	}

	return true;
}

bool SimpleOutput::StartReplayBuffer()
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] SimpleOutput::StartReplayBuffer.");
	UpdateRecording();
	if (!ConfigureRecording(true))
		return false;
	if (!obs_output_start(replayBuffer)) {
		PLSAlertView::critical(main->getMainView(), QTStr("Output.StartReplayFailed"), QTStr("Output.StartFailedGeneric"));
		return false;
	}

	return true;
}

void SimpleOutput::StopStreaming(bool force)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] SimpleOutput::StopStreaming is called, force = %s.", (force ? "true" : "false"));
	if (force)
		obs_output_force_stop(streamOutput);
	else
		TryToStopStreaming(this);
}

void SimpleOutput::StopRecording(bool force)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] SimpleOutput::StopRecording is called, force = %s.", (force ? "true" : "false"));
	if (force)
		obs_output_force_stop(fileOutput);
	else
		obs_output_stop(fileOutput);
}

void SimpleOutput::StopReplayBuffer(bool force)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] SimpleOutput::StopReplayBuffer is called, force = %s.", (force ? "true" : "false"));
	if (force)
		obs_output_force_stop(replayBuffer);
	else
		obs_output_stop(replayBuffer);
}

bool SimpleOutput::StreamingActive() const
{
	return obs_output_active(streamOutput);
}

bool SimpleOutput::RecordingActive() const
{
	return obs_output_active(fileOutput);
}

bool SimpleOutput::ReplayBufferActive() const
{
	return obs_output_active(replayBuffer);
}

//PRISM/LiuHaibin/20210906/Pre-check encoders
bool SimpleOutput::CheckStreamEncoder()
{
	// NOTE!!!
	// FFmpegOutput is disabled currently, modify this function when it's enabled.

	if (streamEncoderChecked) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for streaming is already checked, availability : %s.", __FUNCTION__, streamEncoderAvailable ? "true" : "false");
		return streamEncoderAvailable;
	}

	streamEncoderChecked = true;
	streamEncoderAvailable = true;

	// encoder has already been running for stream output, return true directly.
	if (obs_output_active(streamOutput)) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for streaming is available because it's already been running.", __FUNCTION__);
		return streamEncoderAvailable;
	}

	if (!usingRecordingPreset && obs_output_active(fileOutput)) {
		/* When usingRecordingPreset == false the recording/replaybuffer and streaming are using the same encoder,
		 * if file output is active, it means stream encoder has already been running,
		 * so there is no need to check it again. */
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for streaming is available because it's already been running with fileOutput.", __FUNCTION__);
		return streamEncoderAvailable;
	}

	SimpleOutput::Update();
	obs_encoder_set_video(h264Streaming, obs_get_video());
	obs_encoder_set_audio(aacStreaming, obs_get_audio());

	int simpleBTRSTrack = config_get_int(main->Config(), "SimpleOutput", "TrackIndex") - 1;
	if (simpleBTRSTrack != 0)
		simpleBTRSTrack = 1;
	for (size_t i = 0; i <= simpleBTRSTrack; i++)
		obs_encoder_set_audio(immersiveAACStreaming[i], obs_get_audio());

	obs_output_set_media(streamOutput, obs_get_video(), obs_get_audio());

	// audio encoder is barely failed, so we only need to check video encoder here.
	if (!obs_encoder_avaliable(h264Streaming)) {
		QString id = obs_encoder_get_id(h264Streaming);
		QString name = obs_encoder_get_name(h264Streaming);
		PLS_WARN(MAIN_OUTPUT, "[%s] Encoder '%s: %s' for SimpleOutput is not available.", __FUNCTION__, id.toStdString().c_str(), name.toStdString().c_str());
		streamEncoderAvailable = false;
		return streamEncoderAvailable;
	}

	return streamEncoderAvailable;
}

//PRISM/LiuHaibin/20210906/Pre-check encoders
bool SimpleOutput::CheckRecordEncoder()
{
	// NOTE!!!
	// FFmpegOutput is disabled currently, modify this function when it's enabled.

	if (recordEncoderChecked) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for recording is already checked, availability : %s.", __FUNCTION__, recordEncoderAvailable ? "true" : "false");
		return recordEncoderAvailable;
	}

	if (!usingRecordingPreset) {
		/* When usingRecordingPreset == false the recording/replaybuffer are using the same encoder with streaming,
		 * and encoder has already been running for stream output, return true directly. */
		if (obs_output_active(streamOutput)) {
			PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for recording is available because it use the same encoder with the running streamOutput.", __FUNCTION__);
			return true;
		}

		return CheckStreamEncoder();
	}

	recordEncoderChecked = true;
	recordEncoderAvailable = true;

	if (obs_output_active(fileOutput)) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for recording is available because it's already been running.", __FUNCTION__);
		return recordEncoderAvailable;
	}

	UpdateRecording();

	if (!obs_encoder_avaliable(h264Recording)) {
		QString id = obs_encoder_get_id(h264Recording);
		QString name = obs_encoder_get_name(h264Recording);
		PLS_WARN(MAIN_OUTPUT, "[%s] Encoder '%s: %s' for SimpleOutput is not available.", __FUNCTION__, id.toStdString().c_str(), name.toStdString().c_str());
		recordEncoderAvailable = false;
		return recordEncoderAvailable;
	}

	return recordEncoderAvailable;
}

/* ------------------------------------------------------------------------ */

struct AdvancedOutput : BasicOutputHandler {
	OBSEncoder streamAudioEnc;
	OBSEncoder aacTrack[MAX_AUDIO_MIXES];
	OBSEncoder h264Streaming;
	OBSEncoder h264Recording;
	OBSEncoder immersiveStreamAudioEnc[BTRS_TOTAL_TRACKS];
	OBSEncoder immersiveRecordAudioEnc[BTRS_TOTAL_TRACKS];
	string immersiveAACEncoderID[BTRS_TOTAL_TRACKS];

	bool ffmpegOutput;
	bool ffmpegRecording;
	bool useStreamEncoder;
	bool usesBitrate = false;

	string aacEncoderID[MAX_AUDIO_MIXES];

	explicit AdvancedOutput(PLSBasic *main_);

	inline void UpdateStreamSettings();
	inline void UpdateRecordingSettings();
	inline void UpdateAudioSettings();
	virtual void Update() override;

	inline void SetupStreaming();
	inline void SetupRecording();
	inline void SetupFFmpeg();
	void SetupOutputs();
	int GetAudioBitrate(size_t i, bool immersive) const;

	virtual bool StartStreaming(obs_service_t *service) override;
	virtual bool StartRecording() override;
	virtual bool StartReplayBuffer() override;
	virtual void StopStreaming(bool force) override;
	virtual void StopRecording(bool force) override;
	virtual void StopReplayBuffer(bool force) override;
	virtual bool StreamingActive() const override;
	virtual bool RecordingActive() const override;
	virtual bool ReplayBufferActive() const override;

	//PRISM/LiuHaibin/20210906/Pre-check encoders
	bool CheckStreamEncoder() override;
	bool CheckRecordEncoder() override;
};

OBSData GetDataFromJsonFile(const char *jsonFile)
{
	char fullPath[512];
	obs_data_t *data = nullptr;

	int ret = GetProfilePath(fullPath, sizeof(fullPath), jsonFile);
	if (ret > 0) {
		BPtr<char> jsonData = os_quick_read_utf8_file(fullPath);
		if (!!jsonData) {
			data = obs_data_create_from_json(jsonData);
		}
	}

	if (!data)
		data = obs_data_create();
	OBSData dataRet(data);
	obs_data_release(data);
	return dataRet;
}

static void ApplyEncoderDefaults(OBSData &settings, const obs_encoder_t *encoder)
{
	OBSData dataRet = obs_encoder_get_defaults(encoder);
	obs_data_release(dataRet);

	if (!!settings)
		obs_data_apply(dataRet, settings);
	settings = std::move(dataRet);
}

AdvancedOutput::AdvancedOutput(PLSBasic *main_) : BasicOutputHandler(main_)
{
	const char *recType = config_get_string(main->Config(), "AdvOut", "RecType");
	const char *streamEncoder = config_get_string(main->Config(), "AdvOut", "Encoder");
	const char *recordEncoder = config_get_string(main->Config(), "AdvOut", "RecEncoder");

	ffmpegOutput = astrcmpi(recType, "FFmpeg") == 0;
	ffmpegRecording = ffmpegOutput && config_get_bool(main->Config(), "AdvOut", "FFOutputToFile");
	useStreamEncoder = astrcmpi(recordEncoder, "none") == 0;

	OBSData streamEncSettings = GetDataFromJsonFile("streamEncoder.json");
	OBSData recordEncSettings = GetDataFromJsonFile("recordEncoder.json");

	const char *rate_control = obs_data_get_string(useStreamEncoder ? streamEncSettings : recordEncSettings, "rate_control");
	if (!rate_control)
		rate_control = "";
	usesBitrate = astrcmpi(rate_control, "CBR") == 0 || astrcmpi(rate_control, "VBR") == 0 || astrcmpi(rate_control, "ABR") == 0;

	if (ffmpegOutput) {
		fileOutput = obs_output_create("ffmpeg_output", "adv_ffmpeg_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording FFmpeg output "
			      "(advanced output)";
		obs_output_release(fileOutput);
	} else {
		bool useReplayBuffer = config_get_bool(main->Config(), "AdvOut", "RecRB");
		if (useReplayBuffer) {
			const char *str = config_get_string(main->Config(), "Hotkeys", "ReplayBuffer");
			obs_data_t *hotkey = obs_data_create_from_json(str);
			replayBuffer = obs_output_create("replay_buffer", Str("ReplayBuffer"), nullptr, hotkey);

			obs_data_release(hotkey);
			if (!replayBuffer)
				throw "Failed to create replay buffer output "
				      "(simple output)";
			obs_output_release(replayBuffer);

			signal_handler_t *signal = obs_output_get_signal_handler(replayBuffer);

			startReplayBuffer.Connect(signal, "start", PLSStartReplayBuffer, this);
			stopReplayBuffer.Connect(signal, "stop", PLSStopReplayBuffer, this);
			replayBufferStopping.Connect(signal, "stopping", PLSReplayBufferStopping, this);
			replayBufferSaved.Connect(signal, "replay_buffer_saved", PLSReplayBufferSaved, this);
		}

		fileOutput = obs_output_create("ffmpeg_muxer", "adv_file_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording output "
			      "(advanced output)";
		obs_output_release(fileOutput);

		if (!useStreamEncoder) {
			h264Recording = obs_video_encoder_create(recordEncoder, "recording_h264", recordEncSettings, nullptr);
			if (!h264Recording)
				throw "Failed to create recording h264 "
				      "encoder (advanced output)";
			obs_encoder_release(h264Recording);
		}
	}

	h264Streaming = obs_video_encoder_create(streamEncoder, "streaming_h264", streamEncSettings, nullptr);
	if (!h264Streaming)
		throw "Failed to create streaming h264 encoder "
		      "(advanced output)";
	obs_encoder_release(h264Streaming);

	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		char name[9];
		sprintf(name, "adv_aac%d", i);

		if (!CreateAACEncoder(aacTrack[i], aacEncoderID[i], GetAudioBitrate(i, false), name, i))
			throw "Failed to create audio encoder "
			      "(advanced output)";
	}

	int streamTrack = config_get_int(main->Config(), "AdvOut", "TrackIndex") - 1;
	std::string id;
	if (!CreateAACEncoder(streamAudioEnc, id, GetAudioBitrate(streamTrack, false), "avc_aac_stream", streamTrack))
		throw "Failed to create streaming audio encoder "
		      "(advanced output)";

	// for immersive audio, create it even not enabled
	for (size_t i = 0; i < BTRS_TOTAL_TRACKS; i++) {
		std::string id;
		std::string name = "adv_immersive_aac_stream_" + std::to_string(i);
		if (!CreateAACEncoder(immersiveStreamAudioEnc[i], id, GetAudioBitrate(i, true), name.c_str(), i))
			throw "Failed to create streaming audio encoder (advanced output)";

		name = "adv_immersive_aac_record_" + std::to_string(i);
		if (!CreateAACEncoder(immersiveRecordAudioEnc[i], immersiveAACEncoderID[i], GetAudioBitrate(i, true), name.c_str(), i))
			throw "Failed to create audio encoder "
			      "(advanced output)";
	}

	startRecording.Connect(obs_output_get_signal_handler(fileOutput), "start", PLSStartRecording, this);
	stopRecording.Connect(obs_output_get_signal_handler(fileOutput), "stop", PLSStopRecording, this);
	recordStopping.Connect(obs_output_get_signal_handler(fileOutput), "stopping", PLSRecordStopping, this);
}

void AdvancedOutput::UpdateStreamSettings()
{
	bool applyServiceSettings = config_get_bool(main->Config(), "AdvOut", "ApplyServiceSettings");
	bool dynBitrate = config_get_bool(main->Config(), "Output", "DynamicBitrate");
	const char *streamEncoder = config_get_string(main->Config(), "AdvOut", "Encoder");

	OBSData settings = GetDataFromJsonFile("streamEncoder.json");
	ApplyEncoderDefaults(settings, h264Streaming);

	if (applyServiceSettings)
		obs_service_apply_encoder_settings(main->GetService(), settings, nullptr);

	if (dynBitrate && astrcmpi(streamEncoder, "jim_nvenc") == 0)
		obs_data_set_bool(settings, "lookahead", false);

	video_t *video = obs_get_video();
	enum video_format format = video_output_get_format(video);

	if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
		obs_encoder_set_preferred_video_format(h264Streaming, VIDEO_FORMAT_NV12);

	obs_encoder_update(h264Streaming, settings);
}

inline void AdvancedOutput::UpdateRecordingSettings()
{
	OBSData settings = GetDataFromJsonFile("recordEncoder.json");
	obs_encoder_update(h264Recording, settings);
}

void AdvancedOutput::Update()
{
	UpdateStreamSettings();
	if (!useStreamEncoder && !ffmpegOutput)
		UpdateRecordingSettings();
	UpdateAudioSettings();
}

inline void AdvancedOutput::SetupStreaming()
{
	bool rescale = config_get_bool(main->Config(), "AdvOut", "Rescale");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "RescaleRes");
	int streamTrack = config_get_int(main->Config(), "AdvOut", "TrackIndex") - 1;
	uint32_t caps = obs_encoder_get_caps(h264Streaming);
	unsigned int cx = 0;
	unsigned int cy = 0;

	if ((caps & OBS_ENCODER_CAP_PASS_TEXTURE) != 0) {
		rescale = false;
	}

	if (rescale && rescaleRes && *rescaleRes) {
		if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
			cx = 0;
			cy = 0;
		}
	}

	// remove all audio encoders before set new audio encoders
	for (size_t i = 0; i <= MAX_AUDIO_MIXES; i++)
		obs_output_remove_audio_encoder(streamOutput, nullptr, i);
	if (pls_is_immersive_audio()) {
		int immersiveTrack = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex") - 1;
		// 0: Stereo Only, 1: Stereo/Immersive
		if (immersiveTrack != 0)
			immersiveTrack = 1;
		for (size_t i = 0; i <= immersiveTrack; i++)
			obs_output_set_audio_encoder(streamOutput, immersiveStreamAudioEnc[i], i);
	} else
		obs_output_set_audio_encoder(streamOutput, streamAudioEnc, streamTrack);
	obs_encoder_set_scaled_size(h264Streaming, cx, cy);
	obs_encoder_set_video(h264Streaming, obs_get_video());
}

inline void AdvancedOutput::SetupRecording()
{
	const char *path = config_get_string(main->Config(), "AdvOut", "RecFilePath");
	const char *mux = config_get_string(main->Config(), "AdvOut", "RecMuxerCustom");
	bool rescale = config_get_bool(main->Config(), "AdvOut", "RecRescale");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "RecRescaleRes");
	int tracks;

	const char *recFormat = config_get_string(main->Config(), "AdvOut", "RecFormat");

	bool flv = strcmp(recFormat, "flv") == 0;

	if (flv)
		tracks = config_get_int(main->Config(), "AdvOut", "FLVTrack");
	else
		tracks = config_get_int(main->Config(), "AdvOut", "RecTracks");

	obs_data_t *settings = obs_data_create();
	unsigned int cx = 0;
	unsigned int cy = 0;
	int idx = 0;

	if (tracks == 0)
		tracks = config_get_int(main->Config(), "AdvOut", "TrackIndex");

	if (useStreamEncoder) {
		obs_output_set_video_encoder(fileOutput, h264Streaming);
		if (replayBuffer)
			obs_output_set_video_encoder(replayBuffer, h264Streaming);
	} else {
		uint32_t caps = obs_encoder_get_caps(h264Recording);
		if ((caps & OBS_ENCODER_CAP_PASS_TEXTURE) != 0) {
			rescale = false;
		}

		if (rescale && rescaleRes && *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
		}

		obs_encoder_set_scaled_size(h264Recording, cx, cy);
		obs_encoder_set_video(h264Recording, obs_get_video());
		obs_output_set_video_encoder(fileOutput, h264Recording);
		if (replayBuffer)
			obs_output_set_video_encoder(replayBuffer, h264Recording);
	}

	if (pls_is_immersive_audio()) {
		tracks = config_get_int(main->Config(), "AdvOut", "ImmersiveRecTracks");
		// ImmersiveRecTracks can only be 1 or 2
		if (tracks == 0)
			tracks = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex");

		// flv only support 1 track
		if (flv)
			tracks = 1;
	}

	// remove all audio encoders before set new audio encoders
	for (size_t i = 0; i <= MAX_AUDIO_MIXES; i++) {
		obs_output_remove_audio_encoder(fileOutput, nullptr, i);
		if (replayBuffer)
			obs_output_remove_audio_encoder(replayBuffer, nullptr, i);
	}

	if (!flv) {
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			if ((tracks & (1 << i)) != 0) {
				obs_output_set_audio_encoder(fileOutput, aacTrack[i], idx);
				if (replayBuffer)
					obs_output_set_audio_encoder(replayBuffer, aacTrack[i], idx);
				idx++;
			}
		}
	} else if (flv && tracks != 0) {
		obs_output_set_audio_encoder(fileOutput, aacTrack[tracks - 1], idx);

		if (replayBuffer)
			obs_output_set_audio_encoder(replayBuffer, aacTrack[tracks - 1], idx);
	}

	obs_data_set_string(settings, "path", path);
	obs_data_set_string(settings, "muxer_settings", mux);
	obs_output_update(fileOutput, settings);
	if (replayBuffer)
		obs_output_update(replayBuffer, settings);
	obs_data_release(settings);
}

inline void AdvancedOutput::SetupFFmpeg()
{
	const char *url = config_get_string(main->Config(), "AdvOut", "FFURL");
	int vBitrate = config_get_int(main->Config(), "AdvOut", "FFVBitrate");
	int gopSize = config_get_int(main->Config(), "AdvOut", "FFVGOPSize");
	bool rescale = config_get_bool(main->Config(), "AdvOut", "FFRescale");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "FFRescaleRes");
	const char *formatName = config_get_string(main->Config(), "AdvOut", "FFFormat");
	const char *mimeType = config_get_string(main->Config(), "AdvOut", "FFFormatMimeType");
	const char *muxCustom = config_get_string(main->Config(), "AdvOut", "FFMCustom");
	const char *vEncoder = config_get_string(main->Config(), "AdvOut", "FFVEncoder");
	int vEncoderId = config_get_int(main->Config(), "AdvOut", "FFVEncoderId");
	const char *vEncCustom = config_get_string(main->Config(), "AdvOut", "FFVCustom");
	int aBitrate = config_get_int(main->Config(), "AdvOut", "FFABitrate");
	int aMixes = config_get_int(main->Config(), "AdvOut", "FFAudioMixes");
	const char *aEncoder = config_get_string(main->Config(), "AdvOut", "FFAEncoder");
	int aEncoderId = config_get_int(main->Config(), "AdvOut", "FFAEncoderId");
	const char *aEncCustom = config_get_string(main->Config(), "AdvOut", "FFACustom");
	obs_data_t *settings = obs_data_create();

	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "format_name", formatName);
	obs_data_set_string(settings, "format_mime_type", mimeType);
	obs_data_set_string(settings, "muxer_settings", muxCustom);
	obs_data_set_int(settings, "gop_size", gopSize);
	obs_data_set_int(settings, "video_bitrate", vBitrate);
	obs_data_set_string(settings, "video_encoder", vEncoder);
	obs_data_set_int(settings, "video_encoder_id", vEncoderId);
	obs_data_set_string(settings, "video_settings", vEncCustom);
	obs_data_set_int(settings, "audio_bitrate", aBitrate);
	obs_data_set_string(settings, "audio_encoder", aEncoder);
	obs_data_set_int(settings, "audio_encoder_id", aEncoderId);
	obs_data_set_string(settings, "audio_settings", aEncCustom);

	if (rescale && rescaleRes && *rescaleRes) {
		int width;
		int height;
		int val = sscanf(rescaleRes, "%dx%d", &width, &height);

		if (val == 2 && width && height) {
			obs_data_set_int(settings, "scale_width", width);
			obs_data_set_int(settings, "scale_height", height);
		}
	}

	obs_output_set_mixers(fileOutput, aMixes);
	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_update(fileOutput, settings);

	obs_data_release(settings);
}

static inline void SetEncoderName(obs_encoder_t *encoder, const char *name, const char *defaultName)
{
	obs_encoder_set_name(encoder, (name && *name) ? name : defaultName);
}

inline void AdvancedOutput::UpdateAudioSettings()
{
	bool applyServiceSettings = config_get_bool(main->Config(), "AdvOut", "ApplyServiceSettings");
	int streamTrackIndex = config_get_int(main->Config(), "AdvOut", "TrackIndex");

	obs_data_t *settings[MAX_AUDIO_MIXES];
	obs_data_t *immersive_settings[BTRS_TOTAL_TRACKS];

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		settings[i] = obs_data_create();
		obs_data_set_int(settings[i], "bitrate", GetAudioBitrate(i, false));
	}

	for (size_t i = 0; i < BTRS_TOTAL_TRACKS; i++) {
		immersive_settings[i] = obs_data_create();
		obs_data_set_int(immersive_settings[i], "bitrate", GetAudioBitrate(i, true));
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		string cfg_name = "Track";
		cfg_name += to_string((int)i + 1);
		cfg_name += "Name";
		const char *name = config_get_string(main->Config(), "AdvOut", cfg_name.c_str());

		string def_name = "Track";
		def_name += to_string((int)i + 1);
		SetEncoderName(aacTrack[i], name, def_name.c_str());
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		obs_encoder_update(aacTrack[i], settings[i]);

		if ((int)(i + 1) == streamTrackIndex) {
			if (applyServiceSettings) {
				obs_service_apply_encoder_settings(main->GetService(), nullptr, settings[i]);
			}

			obs_encoder_update(streamAudioEnc, settings[i]);
		}

		obs_data_release(settings[i]);
	}

	for (size_t i = 0; i < BTRS_TOTAL_TRACKS; i++) {
		obs_encoder_update(immersiveStreamAudioEnc[i], immersive_settings[i]);
		obs_encoder_update(immersiveRecordAudioEnc[i], immersive_settings[i]);
		obs_data_release(immersive_settings[i]);
	}
}

void AdvancedOutput::SetupOutputs()
{
	obs_encoder_set_video(h264Streaming, obs_get_video());
	if (h264Recording)
		obs_encoder_set_video(h264Recording, obs_get_video());
	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++)
		obs_encoder_set_audio(aacTrack[i], obs_get_audio());
	obs_encoder_set_audio(streamAudioEnc, obs_get_audio());

	//for immversive audio
	int immersiveTrack = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex") - 1;
	// 0: Stereo Only, 1: Stereo/Immersive
	if (immersiveTrack != 0)
		immersiveTrack = 1;
	for (size_t i = 0; i <= immersiveTrack; i++)
		obs_encoder_set_audio(immersiveStreamAudioEnc[i], obs_get_audio());

	obs_output_set_media(streamOutput, obs_get_video(), obs_get_audio());
	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_set_media(replayBuffer, obs_get_video(), obs_get_audio());

	SetupStreaming();

	if (ffmpegOutput)
		SetupFFmpeg();
	else
		SetupRecording();
}

int AdvancedOutput::GetAudioBitrate(size_t i, bool immersive) const
{
	int bitrate;
	if (immersive) {
		static const char *names[] = {
			"TrackStereoBitrate",
			"TrackImmersiveBitrate",
		};
		bitrate = (int)config_get_uint(main->Config(), "AdvOut", names[i]);
	} else {
		static const char *names[] = {
			"Track1Bitrate", "Track2Bitrate", "Track3Bitrate", "Track4Bitrate", "Track5Bitrate", "Track6Bitrate",
		};
		bitrate = (int)config_get_uint(main->Config(), "AdvOut", names[i]);
	}

	return FindClosestAvailableAACBitrate(bitrate);
}

bool AdvancedOutput::StartStreaming(obs_service_t *service)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] AdvancedOutput::StartStreaming, service(%p, %s).", service, obs_service_get_output_type(service));
	int streamTrack = config_get_int(main->Config(), "AdvOut", "TrackIndex") - 1;

	if (!useStreamEncoder || (!ffmpegOutput && !obs_output_active(fileOutput))) {
		UpdateStreamSettings();
	}

	UpdateAudioSettings();

	if (!Active())
		SetupOutputs();

	Auth *auth = main->GetAuth();
	if (auth)
		auth->OnStreamConfig();

	/* --------------------- */

	const char *type = obs_service_get_output_type(service);
	if (!type)
		type = "rtmp_output";

	/* XXX: this is messy and disgusting and should be refactored */
	if (outputType != type) {
		streamDelayStarting.Disconnect();
		streamStopping.Disconnect();
		startStreaming.Disconnect();
		stopStreaming.Disconnect();

		streamOutput = obs_output_create(type, "adv_stream", nullptr, nullptr);
		if (!streamOutput) {
			PLS_WARN(MAIN_OUTPUT,
				 "Creation of stream output type '%s' "
				 "failed!",
				 type);
			return false;
		}
		obs_output_release(streamOutput);

		streamDelayStarting.Connect(obs_output_get_signal_handler(streamOutput), "starting", PLSStreamStarting, this);
		streamStopping.Connect(obs_output_get_signal_handler(streamOutput), "stopping", PLSStreamStopping, this);

		startStreaming.Connect(obs_output_get_signal_handler(streamOutput), "start", PLSStartStreaming, this);
		stopStreaming.Connect(obs_output_get_signal_handler(streamOutput), "stop", PLSStopStreaming, this);

		bool isEncoded = obs_output_get_flags(streamOutput) & OBS_OUTPUT_ENCODED;

		if (isEncoded) {
			const char *codec = obs_output_get_supported_audio_codecs(streamOutput);
			if (!codec) {
				PLS_WARN(MAIN_OUTPUT, "Failed to load audio codec");
				return false;
			}

			if (!pls_is_immersive_audio()) {
				if (strcmp(codec, "aac") != 0) {
					OBSData settings = obs_encoder_get_settings(streamAudioEnc);
					obs_data_release(settings);

					const char *id = FindAudioEncoderFromCodec(codec);

					streamAudioEnc = obs_audio_encoder_create(id, "alt_audio_enc", nullptr, streamTrack, nullptr);

					if (!streamAudioEnc)
						return false;

					obs_encoder_release(streamAudioEnc);
					obs_encoder_update(streamAudioEnc, settings);
					obs_encoder_set_audio(streamAudioEnc, obs_get_audio());
				}
			}
		}

		outputType = type;
	}

	obs_output_set_video_encoder(streamOutput, h264Streaming);
	// remove all audio encoders before set new audio encoders
	for (size_t i = 0; i <= MAX_AUDIO_MIXES; i++)
		obs_output_remove_audio_encoder(streamOutput, nullptr, i);
	if (pls_is_immersive_audio()) {
		int immersiveStreamTrack = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex") - 1;
		// 0: Stereo Only, 1: Stereo/Immersive
		if (immersiveStreamTrack != 0)
			immersiveStreamTrack = 1;
		for (size_t i = 0; i <= immersiveStreamTrack; i++)
			obs_output_set_audio_encoder(streamOutput, immersiveStreamAudioEnc[i], i);
	} else {
		obs_output_set_audio_encoder(streamOutput, streamAudioEnc, 0);
	}

	/* --------------------- */

	obs_output_set_service(streamOutput, service);

	bool reconnect = config_get_bool(main->Config(), "Output", "Reconnect");
	int retryDelay = config_get_int(main->Config(), "Output", "RetryDelay");
	int maxRetries = config_get_int(main->Config(), "Output", "MaxRetries");
	// Zhangdewen remove stream delay feature issue: 2231
	bool useDelay = false; // config_get_bool(main->Config(), "Output", "DelayEnable");
	int delaySec = config_get_int(main->Config(), "Output", "DelaySec");
	bool preserveDelay = config_get_bool(main->Config(), "Output", "DelayPreserve");
	const char *bindIP = config_get_string(main->Config(), "Output", "BindIP");
	bool enableNewSocketLoop = config_get_bool(main->Config(), "Output", "NewSocketLoopEnable");
	bool enableLowLatencyMode = config_get_bool(main->Config(), "Output", "LowLatencyEnable");
	bool enableDynBitrate = config_get_bool(main->Config(), "Output", "DynamicBitrate");

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "bind_ip", bindIP);
	obs_data_set_bool(settings, "new_socket_loop_enabled", enableNewSocketLoop);
	obs_data_set_bool(settings, "low_latency_mode_enabled", enableLowLatencyMode);
	obs_data_set_bool(settings, "dyn_bitrate", enableDynBitrate);
	obs_output_update(streamOutput, settings);
	obs_data_release(settings);

	if (!reconnect)
		maxRetries = 0;

	obs_output_set_delay(streamOutput, useDelay ? delaySec : 0, preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);

	obs_output_set_reconnect_settings(streamOutput, maxRetries, retryDelay);
	obs_output_set_immersive_audio(streamOutput, pls_is_immersive_audio());
	if (obs_output_start(streamOutput)) {
		return true;
	}

	const char *error = obs_output_get_last_error(streamOutput);
	bool hasLastError = error && *error;
	if (hasLastError)
		lastError = error;
	else
		lastError = string();

	PLS_WARN(MAIN_OUTPUT, "Stream output type '%s' failed to start!%s%s", type, hasLastError ? "  Last Error: " : "", hasLastError ? error : "");
	ResetWatermarkStatus(this, true);
	return false;
}

bool AdvancedOutput::StartRecording()
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] AdvancedOutput::StartRecording.");
	const char *path;
	const char *recFormat;
	const char *filenameFormat;
	bool noSpace = false;
	bool overwriteIfExists = false;

	if (!useStreamEncoder) {
		if (!ffmpegOutput) {
			UpdateRecordingSettings();
		}
	} else if (!obs_output_active(streamOutput)) {
		UpdateStreamSettings();
	}

	UpdateAudioSettings();

	if (!Active())
		SetupOutputs();

	if (!ffmpegOutput || ffmpegRecording) {
		path = config_get_string(main->Config(), "AdvOut", ffmpegRecording ? "FFFilePath" : "RecFilePath");
		recFormat = config_get_string(main->Config(), "AdvOut", ffmpegRecording ? "FFExtension" : "RecFormat");
		filenameFormat = config_get_string(main->Config(), "Output", "FilenameFormatting");
		overwriteIfExists = config_get_bool(main->Config(), "Output", "OverwriteIfExists");
		noSpace = config_get_bool(main->Config(), "AdvOut", ffmpegRecording ? "FFFileNameWithoutSpace" : "RecFileNameWithoutSpace");

		os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;

		if (!dir) {
			if (main->isVisible())
				PLSMessageBox::warning(main, QTStr("Output.BadPath.Title"), QTStr("Output.BadPath.Text"));
			else
				main->SysTrayNotify(QTStr("Output.BadPath.Text"), QSystemTrayIcon::Warning);
			return false;
		}

		os_closedir(dir);

		string strPath;
		strPath += path;

		char lastChar = strPath.back();
		if (lastChar != '/' && lastChar != '\\')
			strPath += "/";

		strPath += GenerateSpecifiedFilename(recFormat, noSpace, filenameFormat);
		ensure_directory_exists(strPath);
		if (!overwriteIfExists)
			FindBestFilename(strPath, noSpace);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, ffmpegRecording ? "url" : "path", strPath.c_str());

		obs_output_update(fileOutput, settings);

		obs_data_release(settings);
	}

	if (!obs_output_start(fileOutput)) {
		QString error_reason;
		const char *error = obs_output_get_last_error(fileOutput);
		if (error)
			error_reason = QT_UTF8(error);
		else
			error_reason = QTStr("Output.StartFailedGeneric");
		PLSAlertView::critical(main->getMainView(), QTStr("Output.StartRecordingFailed"), error_reason);

		PLS_WARN(MAIN_OUTPUT, "Recording for AdvancedOutput failed to start!%s", error_reason.toUtf8().constData());
		ResetWatermarkStatus(this, false);
		return false;
	}

	return true;
}

bool AdvancedOutput::StartReplayBuffer()
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] AdvancedOutput::StartRecording.");
	const char *path;
	const char *recFormat;
	const char *filenameFormat;
	bool noSpace = false;
	bool overwriteIfExists = false;
	const char *rbPrefix;
	const char *rbSuffix;
	int rbTime;
	int rbSize;

	if (!useStreamEncoder) {
		if (!ffmpegOutput)
			UpdateRecordingSettings();
	} else if (!obs_output_active(streamOutput)) {
		UpdateStreamSettings();
	}

	UpdateAudioSettings();

	if (!Active())
		SetupOutputs();

	if (!ffmpegOutput || ffmpegRecording) {
		path = config_get_string(main->Config(), "AdvOut", ffmpegRecording ? "FFFilePath" : "RecFilePath");
		recFormat = config_get_string(main->Config(), "AdvOut", ffmpegRecording ? "FFExtension" : "RecFormat");
		filenameFormat = config_get_string(main->Config(), "Output", "FilenameFormatting");
		overwriteIfExists = config_get_bool(main->Config(), "Output", "OverwriteIfExists");
		noSpace = config_get_bool(main->Config(), "AdvOut", ffmpegRecording ? "FFFileNameWithoutSpace" : "RecFileNameWithoutSpace");
		rbPrefix = config_get_string(main->Config(), "SimpleOutput", "RecRBPrefix");
		rbSuffix = config_get_string(main->Config(), "SimpleOutput", "RecRBSuffix");
		rbTime = config_get_int(main->Config(), "AdvOut", "RecRBTime");
		rbSize = config_get_int(main->Config(), "AdvOut", "RecRBSize");

		os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;

		if (!dir) {
			if (main->isVisible())
				PLSMessageBox::warning(main, QTStr("Output.BadPath.Title"), QTStr("Output.BadPath.Text"));
			else
				main->SysTrayNotify(QTStr("Output.BadPath.Text"), QSystemTrayIcon::Warning);
			return false;
		}

		os_closedir(dir);

		string strPath;
		strPath += path;

		char lastChar = strPath.back();
		if (lastChar != '/' && lastChar != '\\')
			strPath += "/";

		strPath += GenerateSpecifiedFilename(recFormat, noSpace, filenameFormat);
		ensure_directory_exists(strPath);
		if (!overwriteIfExists)
			FindBestFilename(strPath, noSpace);

		obs_data_t *settings = obs_data_create();
		string f;

		if (rbPrefix && *rbPrefix) {
			f += rbPrefix;
			if (f.back() != ' ')
				f += " ";
		}

		f += filenameFormat;

		if (rbSuffix && *rbSuffix) {
			if (*rbSuffix != ' ')
				f += " ";
			f += rbSuffix;
		}

		remove_reserved_file_characters(f);

		obs_data_set_string(settings, "directory", path);
		obs_data_set_string(settings, "format", f.c_str());
		obs_data_set_string(settings, "extension", recFormat);
		obs_data_set_bool(settings, "allow_spaces", !noSpace);
		obs_data_set_int(settings, "max_time_sec", rbTime);
		obs_data_set_int(settings, "max_size_mb", usesBitrate ? 0 : rbSize);

		obs_output_update(replayBuffer, settings);

		obs_data_release(settings);
	}

	if (!obs_output_start(replayBuffer)) {
		PLSAlertView::critical(main->getMainView(), QTStr("Output.StartRecordingFailed"), QTStr("Output.StartFailedGeneric"));
		return false;
	}

	return true;
}

void AdvancedOutput::StopStreaming(bool force)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] AdvancedOutput::StopStreaming is called, force = %s.", (force ? "true" : "false"));

	if (force)
		obs_output_force_stop(streamOutput);
	else
		TryToStopStreaming(this);
}

void AdvancedOutput::StopRecording(bool force)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] AdvancedOutput::StopRecording is called, force = %s.", (force ? "true" : "false"));
	if (force)
		obs_output_force_stop(fileOutput);
	else
		obs_output_stop(fileOutput);
}

void AdvancedOutput::StopReplayBuffer(bool force)
{
	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT, "[Output] AdvancedOutput::StopReplayBuffer is called, force = %s.", (force ? "true" : "false"));
	if (force)
		obs_output_force_stop(replayBuffer);
	else
		obs_output_stop(replayBuffer);
}

bool AdvancedOutput::StreamingActive() const
{
	return obs_output_active(streamOutput);
}

bool AdvancedOutput::RecordingActive() const
{
	return obs_output_active(fileOutput);
}

bool AdvancedOutput::ReplayBufferActive() const
{
	return obs_output_active(replayBuffer);
}

//PRISM/LiuHaibin/20210906/Pre-check encoders
bool AdvancedOutput::CheckStreamEncoder()
{
	// NOTE!!!
	// FFmpegOutput is disabled currently, modify this function when it's enabled.

	if (streamEncoderChecked) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for streaming is already checked, availability : %s.", __FUNCTION__, streamEncoderAvailable ? "true" : "false");
		return streamEncoderAvailable;
	}

	streamEncoderChecked = true;
	streamEncoderAvailable = true;

	// encoder has already been running for stream output, return true directly.
	if (obs_output_active(streamOutput)) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for streaming is available because it's already been running.", __FUNCTION__);
		return streamEncoderAvailable;
	}

	// stream encoder has already been running for file output, return true directly.
	if (useStreamEncoder && obs_output_active(fileOutput)) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for streaming is available because it's already been running with fileOutput.", __FUNCTION__);
		return streamEncoderAvailable;
	}

	UpdateStreamSettings();
	obs_encoder_set_video(h264Streaming, obs_get_video());
	obs_encoder_set_audio(streamAudioEnc, obs_get_audio());

	// for immversive audio
	int immersiveTrack = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex") - 1;
	// 0: Stereo Only, 1: Stereo/Immersive
	if (immersiveTrack != 0)
		immersiveTrack = 1;
	for (size_t i = 0; i <= immersiveTrack; i++)
		obs_encoder_set_audio(immersiveStreamAudioEnc[i], obs_get_audio());

	obs_output_set_media(streamOutput, obs_get_video(), obs_get_audio());
	SetupStreaming();

	// audio encoder is barely failed, so we only need to check video encoder here.
	if (!obs_encoder_avaliable(h264Streaming)) {
		QString id = obs_encoder_get_id(h264Streaming);
		QString name = obs_encoder_get_name(h264Streaming);
		PLS_WARN(MAIN_OUTPUT, "[%s] Encoder '%s: %s' for AdvancedOutput is not available.", __FUNCTION__, id.toStdString().c_str(), name.toStdString().c_str());
		streamEncoderAvailable = false;
		return streamEncoderAvailable;
	}

	return streamEncoderAvailable;
}

//PRISM/LiuHaibin/20210906/Pre-check encoders
bool AdvancedOutput::CheckRecordEncoder()
{
	// NOTE!!!
	// FFmpegOutput is disabled currently, modify this function when it's enabled.

	if (recordEncoderChecked) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for recording is already checked, availability : %s.", __FUNCTION__, recordEncoderAvailable ? "true" : "false");
		return recordEncoderAvailable;
	}

	if (useStreamEncoder) {
		// encoder has already been running for stream output, return true directly.
		if (obs_output_active(streamOutput)) {
			PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for recording is available because it use the same encoder with the running streamOutput.", __FUNCTION__);
			return true;
		}

		return CheckStreamEncoder();
	}

	recordEncoderChecked = true;
	recordEncoderAvailable = true;

	if (obs_output_active(fileOutput)) {
		PLS_INFO(MAIN_OUTPUT, "[%s] Encoder for recording is available because it's already been running.", __FUNCTION__);
		return recordEncoderAvailable;
	}

	UpdateRecordingSettings();
	UpdateAudioSettings();

	obs_encoder_set_video(h264Recording, obs_get_video());
	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++)
		obs_encoder_set_audio(aacTrack[i], obs_get_audio());

	//for immversive audio
	int immersiveTrack = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex") - 1;
	// 0: Stereo Only, 1: Stereo/Immersive
	if (immersiveTrack != 0)
		immersiveTrack = 1;
	for (size_t i = 0; i <= immersiveTrack; i++)
		obs_encoder_set_audio(immersiveRecordAudioEnc[i], obs_get_audio());

	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_set_media(replayBuffer, obs_get_video(), obs_get_audio());

	SetupRecording();

	if (!obs_encoder_avaliable(h264Recording)) {
		QString id = obs_encoder_get_id(h264Recording);
		QString name = obs_encoder_get_name(h264Recording);
		PLS_WARN(MAIN_OUTPUT, "[%s] Encoder '%s: %s' for AdvancedOutput is not available.", __FUNCTION__, id.toStdString().c_str(), name.toStdString().c_str());
		recordEncoderAvailable = false;
		return recordEncoderAvailable;
	}

	return recordEncoderAvailable;
}

/* ------------------------------------------------------------------------ */

BasicOutputHandler *CreateSimpleOutputHandler(PLSBasic *main)
{
	return new SimpleOutput(main);
}

BasicOutputHandler *CreateAdvancedOutputHandler(PLSBasic *main)
{
	return new AdvancedOutput(main);
}

//PRISM/LiuHaibin/20210906/Pre-check encoders
bool BasicOutputHandler::CheckEncoders(ENC_CHECKER flag)
{
	PLS_INFO(MAIN_OUTPUT, "======================= Run Encoder Checking =======================");

	bool available = true;
	if (flag == ENCODER_CHECK_ALL)
		available = CheckRecordEncoder() & CheckStreamEncoder();
	else if (flag == ENCODER_CHECK_STREAM)
		available = CheckStreamEncoder();
	else if (flag == ENCODER_CHECK_RECORD)
		available = CheckRecordEncoder();

	if (!available) {
		auto showWarning = [this]() { PLSAlertView::warning(main->getMainView(), QTStr("Output.Encoder.CheckingFailed.Title"), QTStr("Output.Encoder.CheckingFailed.Text")); };
		QMetaObject::invokeMethod(main->getMainView(), showWarning, Qt::QueuedConnection);
	}
	PLS_INFO(MAIN_OUTPUT, "----------------------- Encoder Checking End -----------------------");
	return available;
}
