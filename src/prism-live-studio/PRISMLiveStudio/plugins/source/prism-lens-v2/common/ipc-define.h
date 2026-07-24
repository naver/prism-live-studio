#pragma once
#include "prism-ipc-buffer.h"

#define DEFAULT_AUDIO_CHANNEL 2
#define DEFAULT_AUDIO_SAMPLERATE 48000
#define DEFAULT_AUDIO_FORMAT AVSampleFormat::AV_SAMPLE_FMT_S16

/*
* IPC defines start
* Use share memory to pass audio and video data from camera studio to virtual camera module
*/
#define IPC_AUDIO_MAX_CHANNEL DEFAULT_AUDIO_CHANNEL
#define WAVE_BUFFER_SIZE 16 * 1024
#define SAMPLE_RATE_PER_SEC DEFAULT_AUDIO_SAMPLERATE
#define AVG_BYTES_PER_SEC 192000
#define AUDIO_DEFAULT_FORMAT DEFAULT_AUDIO_FORMAT

// the name of output av from lens
#define VIDEO_OUTPUT_BUFFER "camera_video_output"
#define AUDIO_OUTPUT_BUFFER "camera_audio_output"

#pragma pack(push, 1)
struct audio_item_header {
	int frames = 0; // count of samples saved in each channel
	int channel = 0;
	uint32_t samples_per_sec = 0;
	enum AVSampleFormat format;
	uint64_t timestamp = 0;
	int sendTime = 0;
};

#define MAX_DURATION_PER_CHANNEL 50.f // in milliseconds
#define MAX_SAMPLES_PER_CHANNEL (SAMPLE_RATE_PER_SEC * MAX_DURATION_PER_CHANNEL / 1000.f)
#define MAX_SIZE_PER_CHN (16 / 8 * (int)MAX_SAMPLES_PER_CHANNEL) //AV_SAMPLE_FMT_S16
#define MAX_AUDIO_SAMPLE_SIZE (IPC_AUDIO_MAX_CHANNEL * MAX_SIZE_PER_CHN)

struct audio_item_sample {
	unsigned char data[MAX_AUDIO_SAMPLE_SIZE];
};

struct shared_handle_header {
	bool flip = false;
	int width = 0;
	int height = 0;
	uint64_t timestamp = 0;
	int sendTime = 0;
	struct gs_luid {
		unsigned long low_part = 0;
		long high_part = 0;
	} luid;
};

struct shared_handle_sample {
	ULONG64 handle = 0;
};
#pragma pack(pop)

//--------------------------------------------------------------
// {alignment}: This value depends on the lens app and needs to be consistent with the lens
// Before lens-1.0.8, alignment is 32 and it is changed to 64 since lens-1.0.8

class SharedHandleBuffer : public CircleBufferIPC {
public:
	SharedHandleBuffer(const char *queueName, int alignment) : CircleBufferIPC(queueName, 0, sizeof(shared_handle_header), 3, sizeof(shared_handle_sample), alignment) {}
};

class AudioCircleBuffer : public CircleBufferIPC {
public:
	AudioCircleBuffer(const char *queueName, int alignment) : CircleBufferIPC(queueName, 0, sizeof(audio_item_header), 50, sizeof(audio_item_sample), alignment) {}
};

/*
* IPC defines end
*/