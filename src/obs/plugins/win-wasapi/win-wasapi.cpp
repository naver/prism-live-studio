#include "enum-wasapi.hpp"

#include <obs-module.h>
#include <obs.h>
#include <util/platform.h>
#include <util/windows/HRError.hpp>
#include <util/windows/ComPtr.hpp>
#include <util/windows/WinHandle.hpp>
#include <util/windows/CoTaskMemPtr.hpp>
#include <util/threading.h>
//PRISM/LiuHaibin/20200803/#None/https://github.com/obsproject/obs-studio/pull/2657
#include <util/util_uint64.h>

//PRISM/ZengQin/20210226/#none/for save device info to file.
#include "pls/crash-writer.h"

//PRISM/WangShaohui/20210406/#7526/handle default device changed
#include <memory>
#include "MMNotificationClient.h"

//PRISM/WangShaohui/20210806/noissue/save PCM
#include <atomic>
#include <shlobj_core.h>

#pragma comment(lib, "Shell32.lib")

using namespace std;

#define OPT_DEVICE_ID "device_id"
#define OPT_USE_DEVICE_TIMING "use_device_timing"

//PRISM/WangShaohui/20210611/noissue/fix bug of checking monitor self
#define OPT_REAL_DEVICE_ID "real_device_id"

static void GetWASAPIDefaults(obs_data_t *settings);

//PRISM/WangShaohui/20210406/#7526/handle default device changed
static std::string UnicodeToUtf8(const std::wstring &id);

#define OBS_KSAUDIO_SPEAKER_4POINT1 \
	(KSAUDIO_SPEAKER_SURROUND | SPEAKER_LOW_FREQUENCY)

#define do_log(level, format, ...)                                            \
	plog(level, "[%s audio : '%s' %p] " format,                           \
	     isInputDevice ? "input" : "output", obs_source_get_name(source), \
	     source, ##__VA_ARGS__)

#define err(format, ...) do_log(LOG_ERROR, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

//PRISM/WangShaohui/20201124/noissue/monitor audio delay
class WASAPISource;
struct AudioDelayMonitor {
	uint64_t totalFrames = 0;
	uint64_t delayFrames = 0;

	uint64_t preDevTs = 0;    // ms
	uint64_t preSysTs = 0;    // ms
	uint64_t preLogSysTs = 0; // ms

	void OnFrameCaptured(WASAPISource *plugin, uint64_t dev, uint64_t sys);
	void SaveDelayStatus(WASAPISource *plugin);
};

class WASAPISource : public IWASEventCallback {
	//PRISM/WangShaohui/20201124/noissue/monitor audio delay
	friend struct AudioDelayMonitor;

	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	ComPtr<IMMDeviceEnumerator> enumerator;
	ComPtr<CMMNotificationClient> eventMonitor;

	ComPtr<IMMDevice> device;
	ComPtr<IAudioClient> client;
	ComPtr<IAudioCaptureClient> capture;
	ComPtr<IAudioRenderClient> render;

	obs_source_t *source;
	string device_id;
	string device_name;
	bool isInputDevice;
	bool useDeviceTiming = false;
	bool isDefaultDevice = false;

	bool reconnecting = false;
	bool previouslyFailed = false;
	WinHandle reconnectThread;

	bool active = false;
	WinHandle captureThread;

	WinHandle stopSignal;
	WinHandle receiveSignal;

	speaker_layout speakers;
	audio_format format;
	uint32_t sampleRate;
	int channels;

	//PRISM/WangShaohui/20201124/noissue/monitor audio delay
	AudioDelayMonitor delayMonitor;

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	crash_writer writer{};

	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	std::wstring usedDefaultDev = L"";

	//PRISM/WangShaohui/20210806/noissue/save PCM
	std::atomic<bool> pcmToSave = false;
	std::atomic<unsigned long> pcmSaveDuration = 10000; // in ms
	DWORD pcmStartTime = 0;                             // in ms
	HANDLE pcmWriter = 0;

	static DWORD WINAPI ReconnectThread(LPVOID param);
	static DWORD WINAPI CaptureThread(LPVOID param);

	//PRISM/WangShaohui/20210806/noissue/save PCM
	std::wstring GetAudioPath();
	bool CheckCreatePCM();

	bool ProcessCaptureData();

	//PRISM/WangShaohui/20200210/#281/for source unavailable
	bool IsAudioExist();

	inline void Start();
	inline void Stop();
	void Reconnect();

	bool InitDevice(IMMDeviceEnumerator *enumerator);
	void InitName();
	void InitClient();
	void InitRender();
	void InitFormat(WAVEFORMATEX *wfex);
	void InitCapture();
	void Initialize();

	//PRISM/WangShaohui/20200219/#468/for checking source invalid
	void SetInvalidState();

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	void StopThread(WinHandle &thread);

	bool TryInitialize();

	void UpdateSettings(obs_data_t *settings);

	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	virtual void OnDefaultInputDeviceChanged(const std::wstring &id);
	virtual void OnDefaultOutputDeviceChanged(const std::wstring &id);
	bool DefaultDeviceChanged();

public:
	WASAPISource(obs_data_t *settings, obs_source_t *source_, bool input);
	inline ~WASAPISource();

	void Update(obs_data_t *settings);

	//PRISM/WangShaohui/20210806/noissue/save PCM
	void StartSavePCM(int sec);

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	inline crash_writer *GetCrashWriter() { return &writer; };
	//PRISM/ZengQin/20210604/#none/Get properties parameters
	inline obs_data_t *getPropsParams()
	{
		obs_data_t *settings = obs_source_get_settings(source);
		const char *device_id =
			obs_data_get_string(settings, OPT_DEVICE_ID);
		obs_data_release(settings);

		obs_data_t *params = obs_data_create();
		obs_data_set_string(params, "device_id", device_id);
		return params;
	};
};

WASAPISource::WASAPISource(obs_data_t *settings, obs_source_t *source_,
			   bool input)
	: source(source_), isInputDevice(input)
{
	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	eventMonitor = new CMMNotificationClient(this);
	// reference is set with default 1 and will be 2 after setting to ComPtr, so here we must invoke Release once
	eventMonitor->Release();

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_create(&writer, cw_crash_type::CW_DEVICE);

	UpdateSettings(settings);

	stopSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!stopSignal.Valid())
		throw "Could not create stop signal";

	receiveSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!receiveSignal.Valid())
		throw "Could not create receive signal";

	Start();
}

//PRISM/WangShaohui/20200210/#281/for source unavailable
bool WASAPISource::IsAudioExist()
{
	vector<AudioDeviceInfo> devices;
	GetWASAPIAudioDevices(devices, isInputDevice);

	size_t count = devices.size();
	for (size_t i = 0; i < count; ++i) {
		if (devices[i].id == device_id)
			return true;
	}

	return false;
}

inline void WASAPISource::Start()
{
	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_update_invoke_mark(&writer, true);
	if (!TryInitialize()) {
		info("[WASAPISource::Start] "
		     "Device '%s' not found.  Waiting for device",
		     device_id.c_str());
		Reconnect();
	}
	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_update_invoke_mark(&writer, false);
}

inline void WASAPISource::Stop()
{
	SetEvent(stopSignal);

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	//if (active) {
	//	info("WASAPI: Device '%s' Terminated",
	//	     device_name.c_str());
	//	WaitForSingleObject(captureThread, INFINITE);
	//}

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	//if (reconnecting)
	//	WaitForSingleObject(reconnectThread, INFINITE);

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	// stop reconnect thread firstly because capture thread may be created in this thread
	info("[WASAPISource::Stop] stop reconnect thread");
	StopThread(reconnectThread);
	info("[WASAPISource::Stop] stop capture thread");
	StopThread(captureThread);

	ResetEvent(stopSignal);
}

inline WASAPISource::~WASAPISource()
{
	Stop();

	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	if (enumerator) {
		enumerator->UnregisterEndpointNotificationCallback(
			eventMonitor);
	}

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_free(&writer);
}

void WASAPISource::UpdateSettings(obs_data_t *settings)
{
	//PRISM/WangShaohui/20200414/#2224/for lost audio device
	string name;
	DecodeAudioString(obs_data_get_string(settings, OPT_DEVICE_ID), name,
			  device_id);

	//PRISM/WangShaohui/20210611/noissue/fix bug of checking monitor self
	obs_data_set_string(settings, OPT_REAL_DEVICE_ID, device_id.c_str());

	useDeviceTiming = obs_data_get_bool(settings, OPT_USE_DEVICE_TIMING);
	isDefaultDevice = _strcmpi(device_id.c_str(), "default") == 0;

	//PRISM/ZengQin/20210803/#none/for save device info to file.
	cw_update_module_info(&writer, device_name.c_str(),
			      obs_source_get_name(source),
			      obs_source_get_id(source));

	//PRISM/WangShaohui/20210316/noIssue/for debugging
	info("Update settings for %s source. obs_source:%p useDeviceTiming:%s device:%s devID:%s",
	     isInputDevice ? "input" : "output", source,
	     useDeviceTiming ? "yes" : "no",
	     isDefaultDevice ? "default" : name.c_str(), device_id.c_str());
}

void WASAPISource::Update(obs_data_t *settings)
{
	//PRISM/WangShaohui/20200414/#2224/for lost audio device
	string name;
	string newDevice;
	DecodeAudioString(obs_data_get_string(settings, OPT_DEVICE_ID), name,
			  newDevice);

	//PRISM/WangShaohui/20210611/noissue/fix bug of checking monitor self
	obs_data_set_string(settings, OPT_REAL_DEVICE_ID, device_id.c_str());

	bool restart = newDevice.compare(device_id) != 0;

	if (restart)
		Stop();

	UpdateSettings(settings);

	if (restart)
		Start();
}

//PRISM/WangShaohui/20210406/#7526/handle default device changed
void WASAPISource::OnDefaultInputDeviceChanged(const std::wstring &id)
{
	if (isInputDevice && isDefaultDevice) {

		info("[WASAPISource] Default input device is changed. id:%s",
		     UnicodeToUtf8(id).c_str());
	}
}

//PRISM/WangShaohui/20210406/#7526/handle default device changed
void WASAPISource::OnDefaultOutputDeviceChanged(const std::wstring &id)
{
	if (!isInputDevice && isDefaultDevice) {
		info("[WASAPISource] Default output device is changed. id:%s",
		     UnicodeToUtf8(id).c_str());
	}
}

bool WASAPISource::InitDevice(IMMDeviceEnumerator *enumerator)
{
	HRESULT res;

	if (isDefaultDevice) {
		//PRISM/WangShaohui/20210406/#7526/handle default device changed
		std::wstring id = eventMonitor->GetDefaultDevice(isInputDevice);
		if (id.empty()) {
			res = enumerator->GetDefaultAudioEndpoint(
				isInputDevice ? eCapture : eRender,
				isInputDevice ? eCommunications : eConsole,
				device.Assign());
		} else {
			res = enumerator->GetDevice(id.c_str(),
						    device.Assign());
		}
	} else {
		wchar_t *w_id;
		os_utf8_to_wcs_ptr(device_id.c_str(), device_id.size(), &w_id);

		res = enumerator->GetDevice(w_id, device.Assign());

		bfree(w_id);
	}

	return SUCCEEDED(res);
}

#define BUFFER_TIME_100NS (5 * 10000000)

void WASAPISource::InitClient()
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT res;
	DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
			       (void **)client.Assign());
	if (FAILED(res))
		throw HRError("Failed to activate client context", res);

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		throw HRError("Failed to get mix format", res);

	InitFormat(wfex);

	if (!isInputDevice)
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

	res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
				 BUFFER_TIME_100NS, 0, wfex, nullptr);
	if (FAILED(res))
		throw HRError("Failed to get initialize audio client", res);
}

void WASAPISource::InitRender()
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	ComPtr<IAudioClient> client;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
			       (void **)client.Assign());
	if (FAILED(res))
		throw HRError("Failed to activate client context", res);

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		throw HRError("Failed to get mix format", res);

	res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, BUFFER_TIME_100NS,
				 0, wfex, nullptr);
	if (FAILED(res))
		throw HRError("Failed to get initialize audio client", res);

	/* Silent loopback fix. Prevents audio stream from stopping and */
	/* messing up timestamps and other weird glitches during silence */
	/* by playing a silent sample all over again. */

	res = client->GetBufferSize(&frames);
	if (FAILED(res))
		throw HRError("Failed to get buffer size", res);

	res = client->GetService(__uuidof(IAudioRenderClient),
				 (void **)render.Assign());
	if (FAILED(res))
		throw HRError("Failed to get render client", res);

	res = render->GetBuffer(frames, &buffer);
	if (FAILED(res))
		throw HRError("Failed to get buffer", res);

	memset(buffer, 0, frames * wfex->nBlockAlign);

	render->ReleaseBuffer(frames, 0);
}

static speaker_layout ConvertSpeakerLayout(DWORD layout, WORD channels)
{
	switch (layout) {
	case KSAUDIO_SPEAKER_2POINT1:
		return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:
		return SPEAKERS_4POINT0;
	case OBS_KSAUDIO_SPEAKER_4POINT1:
		return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1_SURROUND:
		return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND:
		return SPEAKERS_7POINT1;
	}

	return (speaker_layout)channels;
}

void WASAPISource::InitFormat(WAVEFORMATEX *wfex)
{
	DWORD layout = 0;

	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wfex;
		layout = ext->dwChannelMask;
	}

	/* WASAPI is always float */
	sampleRate = wfex->nSamplesPerSec;
	format = AUDIO_FORMAT_FLOAT;
	speakers = ConvertSpeakerLayout(layout, wfex->nChannels);

	//PRISM/WangShaohui/20210806/noissue/save PCM
	channels = wfex->nChannels;
}

void WASAPISource::InitCapture()
{
	HRESULT res = client->GetService(__uuidof(IAudioCaptureClient),
					 (void **)capture.Assign());
	if (FAILED(res))
		throw HRError("Failed to create capture context", res);

	res = client->SetEventHandle(receiveSignal);
	if (FAILED(res))
		throw HRError("Failed to set event handle", res);

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	StopThread(captureThread);

	captureThread = CreateThread(nullptr, 0, WASAPISource::CaptureThread,
				     this, 0, nullptr);
	if (!captureThread.Valid())
		throw "Failed to create capture thread";

	client->Start();
	active = true;

	info("[WASAPISource::InitCapture] Device '%s' initialized. samplerate:%d channels:%d",
	     device_name.c_str(), sampleRate, channels);
}

void WASAPISource::Initialize()
{
	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	//ComPtr<IMMDeviceEnumerator> enumerator;
	HRESULT res;

	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	if (!enumerator) {
		res = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
				       CLSCTX_ALL,
				       __uuidof(IMMDeviceEnumerator),
				       (void **)enumerator.Assign());
		if (FAILED(res))
			throw HRError("Failed to create enumerator", res);

		//PRISM/WangShaohui/20210406/#7526/handle default device changed
		res = enumerator->RegisterEndpointNotificationCallback(
			eventMonitor);
		if (S_OK != res)
			throw HRError("Failed to register event", res);
	}

	if (!InitDevice(enumerator))
		return;

	device_name = GetDeviceName(device);

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_update_module_info(&writer, device_name.c_str(),
			      obs_source_get_name(source),
			      obs_source_get_id(source));

	InitClient();
	if (!isInputDevice)
		InitRender();
	InitCapture();
}

//PRISM/WangShaohui/20200219/#468/for checking source invalid
void WASAPISource::SetInvalidState()
{
	if (isDefaultDevice) {
		obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);
	} else {
		if (device_id.empty()) {
			obs_source_set_capture_valid(source, true,
						     OBS_SOURCE_ERROR_OK);
		} else {
			obs_source_set_capture_valid(
				source, false,
				IsAudioExist() ? OBS_SOURCE_ERROR_UNKNOWN
					       : OBS_SOURCE_ERROR_NOT_FOUND);
		}
	}
}

//PRISM/WangShaohui/20210302/noissue/stop capture thread
void WASAPISource::StopThread(WinHandle &thread)
{
	if (thread.Valid()) {
		WaitForSingleObject(thread, INFINITE);
		thread = nullptr;
	}
}

bool WASAPISource::TryInitialize()
{
	try {
		Initialize();

	} catch (HRError &error) {
		if (previouslyFailed)
			return active;

		warn("[WASAPISource::TryInitialize]:[%s] %s: %lX",
		     device_name.empty() ? device_id.c_str()
					 : device_name.c_str(),
		     error.str, error.hr);

	} catch (const char *error) {
		if (previouslyFailed)
			return active;

		warn("[WASAPISource::TryInitialize]:[%s] %s",
		     device_name.empty() ? device_id.c_str()
					 : device_name.c_str(),
		     error);
	}

	//PRISM/WangShaohui/20200117/#281/for source unavailable
	if (active) {
		obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);
	} else {
		SetInvalidState();
	}

	previouslyFailed = !active;
	return active;
}

void WASAPISource::Reconnect()
{
	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	StopThread(reconnectThread);

	reconnecting = true;
	reconnectThread = CreateThread(
		nullptr, 0, WASAPISource::ReconnectThread, this, 0, nullptr);

	//PRISM/WangShaohui/20210108/noissue/debug log
	if (!reconnectThread.Valid())
		warn("[WASAPISource::Reconnect] "
		     "Failed to initialize reconnect thread: %lu [%s]",
		     GetLastError(), device_name.c_str());
}

static inline bool WaitForSignal(HANDLE handle, DWORD time)
{
	return WaitForSingleObject(handle, time) != WAIT_TIMEOUT;
}

#define RECONNECT_INTERVAL 3000

DWORD WINAPI WASAPISource::ReconnectThread(LPVOID param)
{
	WASAPISource *source = (WASAPISource *)param;

	os_set_thread_name("win-wasapi: reconnect thread");

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_update_thread_id(&source->writer, GetCurrentThreadId(),
			    cw_thread_type::CW_RECONNECT_AUDIO);

	CoInitializeEx(0, COINIT_MULTITHREADED);

	obs_monitoring_type type =
		obs_source_get_monitoring_type(source->source);
	obs_source_set_monitoring_type(source->source,
				       OBS_MONITORING_TYPE_NONE);

	while (!WaitForSignal(source->stopSignal, RECONNECT_INTERVAL)) {
		if (source->TryInitialize())
			break;
	}

	obs_source_set_monitoring_type(source->source, type);

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	//source->reconnectThread = nullptr;
	source->reconnecting = false;

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_remove_thread_id(&source->writer, GetCurrentThreadId());
	return 0;
}

//PRISM/WangShaohui/20210806/noissue/save PCM
void WASAPISource::StartSavePCM(int sec)
{
	if (sec > 0) {
		pcmSaveDuration = sec * 1000;
		pcmToSave = true;
	}
}

//PRISM/WangShaohui/20210806/noissue/save PCM
std::wstring WASAPISource::GetAudioPath()
{
	wchar_t dir[_MAX_PATH];
	SHGetSpecialFolderPathW(0, dir, CSIDL_DESKTOP, 0); // NO \

	wchar_t *unicodeStr = NULL;
	const char *sourceName = obs_source_get_name(source);
	if (sourceName) {
		os_utf8_to_wcs_ptr(sourceName, 0, &unicodeStr);
	}

	wchar_t name[MAX_PATH];
	swprintf(name, L"%s %p-%d-%d.pcm", unicodeStr ? unicodeStr : L"",
		 source, sampleRate, channels);

	if (unicodeStr) {
		bfree(unicodeStr);
	}

	std::wstring path = std::wstring(dir) + L"\\" + std::wstring(name);
	return path;
}

//PRISM/WangShaohui/20210806/noissue/save PCM
bool WASAPISource::CheckCreatePCM()
{
	bool fileOpenned = (pcmWriter && pcmWriter != INVALID_HANDLE_VALUE);
	if (!fileOpenned) {
		pcmWriter = CreateFile(GetAudioPath().c_str(), GENERIC_WRITE, 0,
				       NULL, CREATE_ALWAYS,
				       FILE_ATTRIBUTE_NORMAL, NULL);
		fileOpenned = (pcmWriter && pcmWriter != INVALID_HANDLE_VALUE);
		if (fileOpenned) {
			pcmStartTime = GetTickCount();
		} else {
			pcmToSave = false;
			warn("[%s] Failed to create PCM file. error:%u",
			     device_name.empty() ? device_id.c_str()
						 : device_name.c_str(),
			     GetLastError());
		}
	}

	return fileOpenned;
}

//PRISM/WangShaohui/20211009/#9933/exit thread soon
bool IsEventSigned(const HANDLE &hHandle)
{
	if (!hHandle || hHandle == INVALID_HANDLE_VALUE)
		return true;
	else
		return (WAIT_OBJECT_0 == WaitForSingleObject(hHandle, 0));
}

bool WASAPISource::ProcessCaptureData()
{
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	DWORD flags;
	UINT64 pos, ts;
	UINT captureSize = 0;

	//PRISM/WangShaohui/20211009/#9933/exit thread soon
	while (!IsEventSigned(stopSignal)) {
		res = capture->GetNextPacketSize(&captureSize);

		if (FAILED(res)) {
			//PRISM/WangShaohui/20210108/noissue/debug log
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				warn("[WASAPISource::ProcessCaptureData]"
				     " capture->GetNextPacketSize"
				     " failed: %lX [%s]",
				     res, device_name.c_str());
			return false;
		}

		if (!captureSize)
			break;

		res = capture->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				warn("[WASAPISource::ProcessCaptureData]"
				     " capture->GetBuffer"
				     " failed: %lX",
				     res);
			return false;
		}

		//PRISM/WangShaohui/20210806/noissue/save PCM
		if (pcmToSave) {
			if (CheckCreatePCM()) {
				DWORD temp;
				DWORD len = frames * channels * sizeof(float);
				WriteFile(pcmWriter, buffer, len, &temp, NULL);
				FlushFileBuffers(pcmWriter);

				if (GetTickCount() >=
				    (pcmStartTime + pcmSaveDuration)) {
					CloseHandle(pcmWriter);
					pcmWriter = 0;
					pcmToSave = false;
				}
			}
		} else {
			if (pcmWriter && pcmWriter != INVALID_HANDLE_VALUE) {
				CloseHandle(pcmWriter);
				pcmWriter = 0;
			}
		}

		obs_source_audio data = {};
		data.data[0] = (const uint8_t *)buffer;
		data.frames = (uint32_t)frames;
		data.speakers = speakers;
		data.samples_per_sec = sampleRate;
		data.format = format;
		data.timestamp = useDeviceTiming ? ts * 100 : os_gettime_ns();

		//PRISM/WangShaohui/20201124/noissue/monitor audio delay
		delayMonitor.OnFrameCaptured(this, ts / 10000,
					     os_gettime_ns() / 1000000);

		if (!useDeviceTiming)
			//PRISM/LiuHaibin/20200803/#None/https://github.com/obsproject/obs-studio/pull/2657
			//data.timestamp -= (uint64_t)frames * 1000000000ULL /
			//		  (uint64_t)sampleRate;
			data.timestamp -= util_mul_div64(frames, 1000000000ULL,
							 sampleRate);

		obs_source_output_audio(source, &data);

		capture->ReleaseBuffer(frames);
	}

	return true;
}

static inline bool WaitForCaptureSignal(DWORD numSignals, const HANDLE *signals,
					DWORD duration)
{
	DWORD ret;
	ret = WaitForMultipleObjects(numSignals, signals, false, duration);

	return ret == WAIT_OBJECT_0 || ret == WAIT_TIMEOUT;
}

//PRISM/WangShaohui/20210406/#7526/handle default device changed
bool WASAPISource::DefaultDeviceChanged()
{
	std::wstring usingID = usedDefaultDev;
	std::wstring dftID = eventMonitor->GetDefaultDevice(isInputDevice);
	if (!usingID.empty() && !dftID.empty() && usingID != dftID) {
		return true;
	}

	return false;
}

DWORD WINAPI WASAPISource::CaptureThread(LPVOID param)
{
	WASAPISource *wasSource = (WASAPISource *)param;
	bool reconnect = false;

	//PRISM/WangShaohui/20210406/#7526/modify log
	obs_source_t *source = wasSource->source;
	bool isInputDevice = wasSource->isInputDevice;

	/* Output devices don't signal, so just make it check every 10 ms */
	DWORD dur = wasSource->isInputDevice ? RECONNECT_INTERVAL : 10;

	HANDLE sigs[2] = {wasSource->receiveSignal, wasSource->stopSignal};

	os_set_thread_name("win-wasapi: capture thread");

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_update_thread_id(&wasSource->writer, GetCurrentThreadId(),
			    cw_thread_type::CW_CAPTURE_AUDIO);

	//PRISM/WangShaohui/20201124/noissue/monitor audio delay
	memset(&wasSource->delayMonitor, 0, sizeof(delayMonitor));

	//PRISM/WangShaohui/20210406/#7526/handle default device changed
	wasSource->usedDefaultDev = L"";
	if (wasSource->isDefaultDevice) {
		LPWSTR pwszID = NULL;
		if (SUCCEEDED(wasSource->device->GetId(&pwszID)) && pwszID) {
			wasSource->usedDefaultDev = pwszID;
			CoTaskMemFree(pwszID);
			info("Using default device and device id is : %s",
			     UnicodeToUtf8(wasSource->usedDefaultDev).c_str());
		}
	}

	while (WaitForCaptureSignal(2, sigs, dur)) {
		if (!wasSource->ProcessCaptureData()) {
			//PRISM/WangShaohui/20200117/#281/for source unavailable
			wasSource->SetInvalidState();
			reconnect = true;
			break;
		}

		//PRISM/WangShaohui/20210406/#7526/handle default device changed
		if (wasSource->isDefaultDevice &&
		    wasSource->DefaultDeviceChanged()) {
			info("Stop capture audio from '%s' because default device is changed.",
			     wasSource->device_name.c_str());
			reconnect = true;
			break;
		}
	}

	wasSource->client->Stop();

	//PRISM/WangShaohui/20210302/noissue/stop capture thread
	//source->captureThread = nullptr;
	wasSource->active = false;

	if (reconnect) {
		info("[WASAPISource::CaptureThread] Device '%s' invalidated.  Retrying",
		     wasSource->device_name.c_str());
		wasSource->Reconnect();
	}

	//PRISM/ZengQin/20210226/#none/for save device info to file.
	cw_remove_thread_id(&wasSource->writer, GetCurrentThreadId());
	return 0;
}

/* ------------------------------------------------------------------------- */

static const char *GetWASAPIInputName(void *)
{
	return obs_module_text("AudioInput");
}

static const char *GetWASAPIOutputName(void *)
{
	return obs_module_text("AudioOutput");
}

//PRISM/WangShaohui/20210406/#7526/handle default device changed
static std::string UnicodeToUtf8(const std::wstring &id)
{
	std::string ret = "";

	char *str = NULL;
	os_wcs_to_utf8_ptr(id.c_str(), 0, &str);

	if (str) {
		ret = str;
		bfree(str);
	}

	return ret;
}

static void GetWASAPIDefaultsInput(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "default");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, false);
}

static void GetWASAPIDefaultsOutput(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "default");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, true);
}

static void *CreateWASAPISource(obs_data_t *settings, obs_source_t *source,
				bool input)
{
	try {
		return new WASAPISource(settings, source, input);
	} catch (const char *error) {
		//PRISM/WangShaohui/20210406/#7526/modify log
		bool isInputDevice = input;
		err("[WASAPISource] Error happen in CreateWASAPISource : %s",
		    error);
	}

	return nullptr;
}

static void *CreateWASAPIInput(obs_data_t *settings, obs_source_t *source)
{
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);
	return CreateWASAPISource(settings, source, true);
}

static void *CreateWASAPIOutput(obs_data_t *settings, obs_source_t *source)
{
	//PRISM/WangShaohui/20200117/#281/for source unavailable
	obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);
	return CreateWASAPISource(settings, source, false);
}

static void DestroyWASAPISource(void *obj)
{
	delete static_cast<WASAPISource *>(obj);
}

static void UpdateWASAPISource(void *obj, obs_data_t *settings)
{
	static_cast<WASAPISource *>(obj)->Update(settings);
}

//PRISM/WangShaohui/20200414/#2224/for lost audio device
static bool on_audio_list_changed(obs_properties_t *ppts, obs_property_t *p,
				  obs_data_t *settings)
{
	const char *cur_val = obs_data_get_string(settings, OPT_DEVICE_ID);
	if (!cur_val) {
		return false;
	}

	string current_name;
	string current_id;
	DecodeAudioString(cur_val, current_name, current_id);

	if (current_name.empty()) {
		if (current_id == "default") {
			return true;
		} else {
			return false;
		}
	}

	bool match = false;
	for (size_t i = 0;;) {
		const char *val = obs_property_list_item_string(p, i++);
		if (!val)
			break;

		string temp_name;
		string temp_id;
		DecodeAudioString(val, temp_name, temp_id);

		if (current_id == temp_id) {
			match = true;
			break;
		}
	}

	if (!match) {
		size_t count = obs_property_list_item_count(p);
		int index = (count > 0) ? 1 : 0; // after "default"
		obs_property_list_insert_string(p, index, current_name.c_str(),
						cur_val);
		obs_property_list_item_disable(p, index, true);
		return true;
	}

	UNUSED_PARAMETER(ppts);
	return false;
}

static obs_properties_t *GetWASAPIProperties(void *obj, bool input)
{
	obs_properties_t *props = obs_properties_create();
	vector<AudioDeviceInfo> devices;

	obs_property_t *device_prop = obs_properties_add_list(
		props, OPT_DEVICE_ID, obs_module_text("Device"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	//PRISM/ZengQin/20210226/#none/for crash writer
	WASAPISource *source = reinterpret_cast<WASAPISource *>(obj);
	if (source)
		cw_update_invoke_mark(source->GetCrashWriter(), true);

	GetWASAPIAudioDevices(devices, input);

	//PRISM/ZengQin/20210226/#none/for crash writer
	if (source)
		cw_update_invoke_mark(source->GetCrashWriter(), false);

	//PRISM/WangShaohui/20200414/#2224/for lost audio device
	/*if (devices.size())*/ {
		obs_property_list_add_string(
			device_prop, obs_module_text("Default"), "default");
	}

	for (size_t i = 0; i < devices.size(); i++) {
		AudioDeviceInfo &device = devices[i];
		//PRISM/WangShaohui/20200414/#2224/for lost audio device
		obs_property_list_add_string(
			device_prop, device.name.c_str(),
			EncodeAudioString(device.name.c_str(),
					  device.id.c_str())
				.c_str());
	}

	//PRISM/WangShaohui/20200414/#2224/for lost audio device
	obs_property_set_modified_callback(device_prop, on_audio_list_changed);

	obs_properties_add_bool(props, OPT_USE_DEVICE_TIMING,
				obs_module_text("UseDeviceTiming"));

	return props;
}

static obs_properties_t *GetWASAPIPropertiesInput(void *obj)
{
	return GetWASAPIProperties(obj, true);
}

static obs_properties_t *GetWASAPIPropertiesOutput(void *obj)
{
	return GetWASAPIProperties(obj, false);
}

//PRISM/WangShaohui/20201124/noissue/monitor audio delay
void AudioDelayMonitor::OnFrameCaptured(WASAPISource *plugin, uint64_t dev,
					uint64_t sys)
{
#define FRAME_DELAY_REVISE 5            // ms
#define MAX_FRAME_INTERVAL (2 * 1000)   // ms
#define STATUS_LOG_INTERVAL (60 * 1000) // ms

	++totalFrames;

	bool ignoreCalc = (preDevTs == 0 || preSysTs == 0 || dev <= preDevTs ||
			   sys <= preSysTs ||
			   (sys - preSysTs) >= MAX_FRAME_INTERVAL);

	if (ignoreCalc) {
		preLogSysTs = sys;
	} else {
		uint64_t dataSpace = dev - preDevTs;
		uint64_t sysSpace = sys - preSysTs;
		if (sysSpace > dataSpace &&
		    (sysSpace - dataSpace >= FRAME_DELAY_REVISE)) {
			++delayFrames;
		}
	}

	preDevTs = dev;
	preSysTs = sys;

	if (totalFrames > 0 && (sys - preLogSysTs) > STATUS_LOG_INTERVAL) {
		preLogSysTs = sys;
		SaveDelayStatus(plugin);
	}
}

//PRISM/WangShaohui/20201124/noissue/monitor audio delay
void AudioDelayMonitor::SaveDelayStatus(WASAPISource *plugin)
{
	double delayPercent = (double)delayFrames / (double)totalFrames * 100;

	obs_source_t *source = plugin->source;
	bool isInputDevice = plugin->isInputDevice;

	info("[%s] Statistic information. totalPkt:%llu delayPkt:%llu delayPercent:%.2lf%%",
	     plugin->device_name.c_str(), totalFrames, delayFrames,
	     delayPercent);
}

//PRISM/ZengQin/20210604/#none/Get properties parameters
static obs_data_t *GetPropsParams(void *data)
{
	if (!data)
		return NULL;

	return static_cast<WASAPISource *>(data)->getPropsParams();
}

//PRISM/WangShaohui/20210806/noissue/save PCM
static bool SetPrivateData(void *obj, obs_data_t *data)
{
	if (!obj || !data) {
		return false;
	}

	const char *method = obs_data_get_string(data, "method");
	if (method && 0 == strcmp(method, "save_pcm")) {
		int sec = (int)obs_data_get_int(data, "duration_seconds");
		static_cast<WASAPISource *>(obj)->StartSavePCM(sec);
		return true;
	}

	return false;
}

void RegisterWASAPIInput()
{
	obs_source_info info = {};
	info.id = "wasapi_input_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = GetWASAPIInputName;
	info.create = CreateWASAPIInput;
	info.destroy = DestroyWASAPISource;
	info.update = UpdateWASAPISource;
	info.get_defaults = GetWASAPIDefaultsInput;
	info.get_properties = GetWASAPIPropertiesInput;
	info.icon_type = OBS_ICON_TYPE_AUDIO_INPUT;
	//PRISM/ZengQin/20210604/#none/Get properties parameters
	info.props_params = GetPropsParams;
	//PRISM/WangShaohui/20210806/noissue/save PCM
	info.set_private_data = SetPrivateData;

	obs_register_source(&info);
}

void RegisterWASAPIOutput()
{
	obs_source_info info = {};
	info.id = "wasapi_output_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE |
			    OBS_SOURCE_DO_NOT_SELF_MONITOR;
	info.get_name = GetWASAPIOutputName;
	info.create = CreateWASAPIOutput;
	info.destroy = DestroyWASAPISource;
	info.update = UpdateWASAPISource;
	info.get_defaults = GetWASAPIDefaultsOutput;
	info.get_properties = GetWASAPIPropertiesOutput;
	info.icon_type = OBS_ICON_TYPE_AUDIO_OUTPUT;
	//PRISM/ZengQin/20210604/#none/Get properties parameters
	info.props_params = GetPropsParams;
	//PRISM/WangShaohui/20210806/noissue/save PCM
	info.set_private_data = SetPrivateData;

	obs_register_source(&info);
}
