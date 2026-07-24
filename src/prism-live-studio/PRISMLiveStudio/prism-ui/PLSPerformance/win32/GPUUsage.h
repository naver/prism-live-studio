#pragma once
#include <Windows.h>

#define UPDATE_GPU_INTERVAL 2000 // in ms

#ifdef _DEBUG
//#define SHOW_CONSOULE
#endif

class GPUUsage {
	GPUUsage();
	virtual ~GPUUsage();

public:
	static GPUUsage *Instance();

	void Start(DWORD topProcessId);
	void Stop();

	void GetUsage(double &systemGPU, double &processGPU);

protected:
	class GPUImpl *self = nullptr;
};
