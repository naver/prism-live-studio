#include <functional>
#include <Windows.h>

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <vector>
#include <stdexcept>

#include <wil/result.h>
#include <wil/result_macros.h>

#include "audio-capture-helper.hpp"
#include "format-conversion.hpp"

//PRISM/WangShaohui/20220314/none/modify log
#define do_log(level, format, ...)                                                        \
	plog(level, "[win-capture-audio] [session: %p pid:%u exe:%s] " format, this, pid, \
	     get_exe_by_pid(pid).c_str(), ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

std::shared_ptr<CrashWriter> writer_ptr;
bool test_crash = false;

//PRISM/Xiewei/20220729/none/add crash writer
void CreateCrashWriter()
{
	static bool writer_create = false;
	if (!writer_create) {
		writer_ptr = std::make_shared<CrashWriter>();
		writer_create = true;
	}
}

//PRISM/WangShaohui/20220329/none/check available
bool AudioCaptureHelper::TestPluginAvailable()
{
	static bool tested = false;
	static bool valid = true;

	if (tested)
		return valid;

	try {
		auto params = AudioCaptureHelper::GetParams(
			GetCurrentProcessId(), PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE);
		auto propvariant = AudioCaptureHelper::GetPropvariant(&params);

		wil::com_ptr<IActivateAudioInterfaceAsyncOperation> async_op;
		CompletionHandler completion_handler;

		THROW_IF_FAILED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
							    __uuidof(IAudioClient), &propvariant,
							    &completion_handler, &async_op));

		completion_handler.event_finished.wait();
		THROW_IF_FAILED(completion_handler.activate_hr);

		plog(LOG_INFO, "[win-capture-audio] Plugin can be used");
		valid = true;

	} catch (wil::ResultException e) {
		plog(LOG_WARNING, "[win-capture-audio] Plugin can't be used");
		valid = false;
	}

	tested = true;
	return valid;
}

void AudioCaptureHelper::PluginCrashTest()
{
	auto params = AudioCaptureHelper::GetParams(
		GetCurrentProcessId(), PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE);
	auto propvariant = AudioCaptureHelper::GetPropvariant(&params);

	wil::com_ptr<IActivateAudioInterfaceAsyncOperation> async_op;
	CompletionHandler completion_handler;

	THROW_IF_FAILED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
						    __uuidof(IAudioClient), &propvariant,
						    &completion_handler, &async_op));

	completion_handler.event_finished.wait();
	THROW_IF_FAILED(completion_handler.activate_hr);

	plog(LOG_INFO, "[win-capture-audio] Plugin test crash");
}

//PRISM/WangShaohui/20220329/none/check available
AUDIOCLIENT_ACTIVATION_PARAMS AudioCaptureHelper::GetParams(DWORD process_id,
							    PROCESS_LOOPBACK_MODE mode)
{
	return {
		.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK,
		.ProcessLoopbackParams =
			{
				.TargetProcessId = process_id,
				.ProcessLoopbackMode = mode,
			},
	};
}

PROPVARIANT
AudioCaptureHelper::GetPropvariant(AUDIOCLIENT_ACTIVATION_PARAMS *params)
{
	return {
		.vt = VT_BLOB,
		.blob =
			{
				.cbSize = sizeof(*params),
				.pBlobData = (BYTE *)params,
			},
	};
}

void AudioCaptureHelper::InitClient()
{
	//PRISM/WangShaohui/20220329/none/check available
	auto params = AudioCaptureHelper::GetParams(
		pid, PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE);
	auto propvariant = AudioCaptureHelper::GetPropvariant(&params);

	wil::com_ptr<IActivateAudioInterfaceAsyncOperation> async_op;
	CompletionHandler completion_handler;

	THROW_IF_FAILED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
						    __uuidof(IAudioClient), &propvariant,
						    &completion_handler, &async_op));

	completion_handler.event_finished.wait();
	THROW_IF_FAILED(completion_handler.activate_hr);

	client = completion_handler.client;

	THROW_IF_FAILED(
		client->Initialize(AUDCLNT_SHAREMODE_SHARED,
				   AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				   5 * 10000000, 0, &format, NULL));

	THROW_IF_FAILED(client->SetEventHandle(events[HelperEvents::PacketReady].get()));
}

void AudioCaptureHelper::InitCapture()
{
	InitClient();
	THROW_IF_FAILED(
		client->GetService(__uuidof(IAudioCaptureClient), capture_client.put_void()));
}

void AudioCaptureHelper::RegisterMixer(Mixer *mixer)
{
	auto lock = mixers_section.lock();
	mixers.insert(mixer);
}

bool AudioCaptureHelper::UnRegisterMixer(Mixer *mixer)
{
	auto lock = mixers_section.lock();
	mixers.erase(mixer);

	return mixers.size() == 0;
}

void AudioCaptureHelper::ForwardToMixers(UINT64 qpc_position, BYTE *data, UINT32 num_frames)
{
	//PRISM/WangShaohui/20220314/none/modify log
	frames += num_frames;
	if (first_audio) {
		first_audio = false;
		first_time = GetTickCount();
		info("First audio is received");
	}

	auto lock = mixers_section.lock();

	for (auto *mixer : mixers)
		mixer->SubmitPacket(qpc_position, reinterpret_cast<float *>(data), num_frames);
}

void AudioCaptureHelper::ForwardPacket()
{
	size_t frame_size = format.nBlockAlign;

	UINT32 num_frames = 0;
	THROW_IF_FAILED(capture_client->GetNextPacketSize(&num_frames));

	while (num_frames > 0) {
		BYTE *new_data;
		DWORD flags;
		UINT64 qpc_position;

		THROW_IF_FAILED(capture_client->GetBuffer(&new_data, &num_frames, &flags, NULL,
							  &qpc_position));

		if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
			ForwardToMixers(qpc_position, new_data, num_frames);

		//PRISM/Xiewei/20220622/none/remove frequently log
		//if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
		//warn("data discontinuity flag set");

		if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
			warn("timestamp error flag set");

		THROW_IF_FAILED(capture_client->ReleaseBuffer(num_frames));
		THROW_IF_FAILED(capture_client->GetNextPacketSize(&num_frames));
	}
}

void AudioCaptureHelper::Capture()
{
	InitCapture();
	THROW_IF_FAILED(client->Start());

	info("Capture is initialized");

	bool shutdown = false;
	while (!shutdown) {
		auto event_id = WaitForMultipleObjects(events.size(), events[0].addressof(), FALSE,
						       INFINITE);

		switch (event_id) {
		case HelperEvents::PacketReady:
			ForwardPacket();
			break;

		case HelperEvents::Shutdown:
			shutdown = true;
			break;

		default:
			warn("wait failed with result: %d", event_id);
			shutdown = true;
			break;
		}
	}

	THROW_IF_FAILED(client->Stop());

	//PRISM/WangShaohui/20220314/none/modify log
	if (frames > 0) {
		info("Keep capturing for %us, duration of gotten audio: %llus",
		     (GetTickCount() - first_time) / 1000, frames / (ULONG64)format.nSamplesPerSec);
	} else {
		warn("No audio is gotten");
	}
}

void AudioCaptureHelper::CaptureSafe()
{
	try {
		Capture();
	} catch (wil::ResultException e) {
		warn("Capture exception happen. %s", e.what());
	}
}

AudioCaptureHelper::AudioCaptureHelper(Mixer *mixer, WAVEFORMATEX format, DWORD pid)
	: mixers{mixer}, format{format}, pid{pid}
{
	//PRISM/WangShaohui/20220314/none/modify log
	info("Capture session is created");

	for (auto &event : events)
		event.create();

	capture_thread = std::thread(&AudioCaptureHelper::CaptureSafe, this);
}

AudioCaptureHelper::~AudioCaptureHelper()
{
	auto lock = mixers_section.lock();
	mixers.clear();
	lock.reset();

	events[HelperEvents::Shutdown].SetEvent();
	capture_thread.join();

	//PRISM/WangShaohui/20220314/none/modify log
	info("Capture session is destroied");
}
