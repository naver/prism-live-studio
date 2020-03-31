#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include "monitor-duplicator.h"
#include <util/windows/ComPtr.hpp>
#include "obs.h"
#include <thread>
#include <mutex>

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")

const int MONITOR_BUFFER_CACHE_SIZE = 2;

struct monitor_buffer_info {
	int width;
	int height;
	int pitch;
	uint8_t *buffer;
	bool valid;
	mutex buffer_info_mutex;
	monitor_buffer_info()
	{
		width = 0;
		height = 0;
		buffer = NULL;
		valid = false;
		pitch = 0;
	}
	~monitor_buffer_info()
	{
		if (buffer) {
			delete[] buffer;
			buffer = NULL;
		}
	}
};

class PLSD3D11Duplicator : public PLSMonitorDuplicator {
public:
	PLSD3D11Duplicator();
	PLSD3D11Duplicator(int adapter, int dev_id);
	~PLSD3D11Duplicator();

	virtual bool update_frame();
	virtual bool create_duplicator();
	virtual gs_texture *get_texture();
	virtual bool is_obs_gs_duplicator();
	virtual bool init_device();
	virtual bool capture_thread_run();
	virtual bool get_monitor_info(monitor_info &info);

	static void capture_thread_proc(void *param);

private:
	bool init_duplicator();
	void uninit_duplicator();
	bool init_factory(int adapterIdx, IDXGIAdapter1 **ppAdapter);
	bool init_device(IDXGIAdapter *pAdapter);
	bool get_monitor(int monitor_idx, IDXGIOutput **dxgiOutput);
	bool init_monitor_info();

	ID3D11Texture2D *create_dump_texture(ComPtr<ID3D11Device> device, D3D11_TEXTURE2D_DESC &td);
	bool upload_buffer_to_texture(void *buffer, int width, int height);

	void thread_begin();
	void thread_stop();

	void monitor_capture_thread();
	void dump_duplicator_memory();
	void init_buffer_info_array(int width, int height);
	void clear_buffer_info_array();
	void update_mapdata_to_buffer_array(D3D11_MAPPED_SUBRESOURCE &map, int width, int height);

	bool update_texture();
	int get_read_index();
	int get_write_index();

private:
	ComPtr<IDXGIFactory1> dup_factory;
	ComPtr<ID3D11Device> dup_device;
	ComPtr<ID3D11DeviceContext> dup_context;
	ComPtr<IDXGIOutputDuplication> dup_duplicator;
	ComPtr<ID3D11Texture2D> dump_texture;
	int dump_texture_width;
	int dump_texture_height;

	monitor_info m_info;

	monitor_buffer_info *buffer_info_array[MONITOR_BUFFER_CACHE_SIZE];
	thread *capture_thread;
	mutex capture_mutex;
	bool capture_stop;

	bool update_token;
};
