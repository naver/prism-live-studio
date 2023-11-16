#pragma once

#include <stdio.h>
#include <array>
#include <functional>
#include <thread>
#include <set>

#include <Windows.h>

#include <audiopolicy.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>

#include <wrl/implements.h>
#include <wil/com.h>

#include "mixer.hpp"
#include "common.hpp"
//PRISM/Xiewei/20220729/none/add source id for crash writer
#include "pls/crash-writer.h"

using namespace Microsoft::WRL;

//PRISM/Xiewei/20220729/none/add crash writer
class CrashWriter {
public:
	CrashWriter()
	{
		cw_create(&writer, CW_DEVICE);
		cw_update_module_info(&writer, "laboratory-win-capture-audio.dll", "",
				      "audio_capture");
	}
	~CrashWriter() { cw_free(&writer); }
	crash_writer writer;
};
extern std::shared_ptr<CrashWriter> writer_ptr;
extern bool test_crash;

struct CompletionHandler : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase,
					       IActivateAudioInterfaceCompletionHandler> {
	wil::com_ptr<IAudioClient> client;

	HRESULT activate_hr = E_FAIL;
	wil::unique_event event_finished;

	CompletionHandler() { event_finished.create(); }

	STDMETHOD(ActivateCompleted)
	(IActivateAudioInterfaceAsyncOperation *operation)
	{
		//PRISM/Xiewei/20220729/none/add crash writer
		auto thread_id = GetCurrentThreadId();
		cw_update_thread_id(&writer_ptr->writer, thread_id, CW_CAPTURE_VIDEO);

		if (test_crash) {
			info("Test crash will happened");
			char *test_ptr = nullptr;
			test_ptr[10] = 10;
		}

		auto set_finished = event_finished.SetEvent_scope_exit();

		RETURN_IF_FAILED(operation->GetActivateResult(&activate_hr, client.put_unknown()));

		if (FAILED(activate_hr))
			warn("activate failed (0x%lx)", activate_hr);

		return S_OK;
	}
};

namespace HelperEvents {
enum HelperEvents {
	PacketReady,
	Shutdown,
	Count,
};
};

class AudioCaptureHelper {
private:
	wil::unique_couninitialize_call couninit{wil::CoInitializeEx()};

	DWORD pid;

	wil::critical_section mixers_section;
	std::set<Mixer *> mixers;

	wil::com_ptr<IAudioClient> client;
	wil::com_ptr<IAudioCaptureClient> capture_client;

	WAVEFORMATEX format;

	std::array<wil::unique_event, HelperEvents::Count> events;
	std::thread capture_thread;

	//PRISM/WangShaohui/20220329/none/check available
	static AUDIOCLIENT_ACTIVATION_PARAMS GetParams(DWORD process_id,
						       PROCESS_LOOPBACK_MODE mode);
	static PROPVARIANT GetPropvariant(AUDIOCLIENT_ACTIVATION_PARAMS *params);

	//PRISM/WangShaohui/20220314/none/modify log
	bool first_audio = true;
	DWORD first_time = 0;
	ULONG64 frames = 0;

	void InitClient();
	void InitCapture();

	void ForwardToMixers(UINT64 qpc_position, BYTE *data, UINT32 num_frames);
	void ForwardPacket();

	void Capture();
	void CaptureSafe();

public:
	//PRISM/WangShaohui/20220329/none/check available
	static bool TestPluginAvailable();

	//PRISM/ZengQin/20220801/none/test mode
	static void PluginCrashTest();

	DWORD GetPid() { return pid; }

	AudioCaptureHelper(Mixer *mixer, WAVEFORMATEX format, DWORD pid);
	~AudioCaptureHelper();

	void RegisterMixer(Mixer *mixer);
	bool UnRegisterMixer(Mixer *mixer);
};
