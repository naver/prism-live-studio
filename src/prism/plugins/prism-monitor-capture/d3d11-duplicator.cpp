#include "d3d11-duplicator.h"
#include "window-version.h"
#include <assert.h>
#include <log.h>

static const IID dxgiFactory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};

const static D3D_FEATURE_LEVEL featureLevels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
};

void PLSD3D11Duplicator::capture_thread_proc(void *param)
{
	PLSD3D11Duplicator *duplicator = reinterpret_cast<PLSD3D11Duplicator *>(param);
	duplicator->monitor_capture_thread();
}

PLSD3D11Duplicator::PLSD3D11Duplicator() {}

PLSD3D11Duplicator::PLSD3D11Duplicator(int adapter, int dev_id) : PLSMonitorDuplicator(adapter, dev_id)
{
	dump_texture = NULL;
	dump_texture_width = 0;
	dump_texture_height = 0;
	update_token = true;
	capture_thread = NULL;
	m_info = {};
}

PLSD3D11Duplicator::~PLSD3D11Duplicator()
{
	thread_stop();
	uninit_duplicator();
	clear_buffer_info_array();
}

bool PLSD3D11Duplicator::init_device()
{
	bool res = init_duplicator();
	init_buffer_info_array(m_info.width, m_info.height);
	return res;
}

bool PLSD3D11Duplicator::capture_thread_run()
{
	thread_begin();
	return true;
}

bool PLSD3D11Duplicator::update_frame()
{
	return update_token;
}

bool PLSD3D11Duplicator::create_duplicator()
{
	return true;
}

gs_texture *PLSD3D11Duplicator::get_texture()
{
	bool res = update_texture();
	if (!res)
		return NULL;

	return monitor_texture;
}

bool PLSD3D11Duplicator::is_obs_gs_duplicator()
{
	return false;
}

bool PLSD3D11Duplicator::init_duplicator()
{
	ComPtr<IDXGIAdapter1> adapter;
	if (!init_factory(adapter_id, adapter.Assign()) || !init_device(adapter)) {
		return false;
	}

	ComPtr<IDXGIOutput> output;
	if (!get_monitor(monitor_dev_id, output.Assign())) {
		PLS_PLUGIN_ERROR("get adatper:%d monitor:%d output failed", adapter_id, monitor_dev_id);
		return false;
	}

	ComPtr<IDXGIOutput1> output1;
	HRESULT hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void **)output1.Assign());
	if (FAILED(hr)) {
		return false;
	}

	hr = output1->DuplicateOutput(dup_device, dup_duplicator.Assign());
	if (FAILED(hr)) {
		PLS_PLUGIN_ERROR("get adatper:%d monitor:%d duplicator failed, error code:%ld", adapter_id, monitor_dev_id, hr);
		return false;
	}

	init_monitor_info();
	return true;
}

void PLSD3D11Duplicator::uninit_duplicator()
{
	dup_factory.Release();
	dup_device.Release();
	dup_context.Release();
	dup_duplicator.Release();
	dump_texture.Release();

	gs_texture_destroy(monitor_texture);
	monitor_texture = NULL;
}

bool PLSD3D11Duplicator::init_factory(int adapterIdx, IDXGIAdapter1 **ppAdapter)
{
	IID factoryIID = (PLSWindowVersion::get_win_version() >= 0x602) ? dxgiFactory2 : __uuidof(IDXGIFactory1);

	HRESULT hr = CreateDXGIFactory1(factoryIID, (void **)dup_factory.Assign());
	if (FAILED(hr)) {
		PLS_PLUGIN_ERROR("%s", "init dxgifactory failed");
		return false;
	}

	hr = dup_factory->EnumAdapters1(adapterIdx, ppAdapter);
	if (FAILED(hr)) {
		PLS_PLUGIN_ERROR("%s", "init dxgifactory failed");
		return false;
	}

	return true;
}

bool PLSD3D11Duplicator::init_device(IDXGIAdapter *pAdapter)
{
	D3D_FEATURE_LEVEL levelUsed = D3D_FEATURE_LEVEL_9_3;
	uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	ID3D11Device *pD3D11Device = NULL;
	ID3D11DeviceContext *pD3D11Context = NULL;
	HRESULT hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, createFlags, featureLevels, sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, &dup_device,
				       &levelUsed, &dup_context);

	if (FAILED(hr)) {
		PLS_PLUGIN_ERROR("D3D11CreateDevice failed, error:%ld", hr);
		return false;
	}

	return true;
}

bool PLSD3D11Duplicator::get_monitor(int monitor_idx, IDXGIOutput **dxgiOutput)
{
	ComPtr<IDXGIAdapter> dxgiAdapter;
	ComPtr<IDXGIDevice> dxgiDevice;
	HRESULT hr;

	assert(dup_device);
	hr = dup_device->QueryInterface(__uuidof(IDXGIDevice), (void **)dxgiDevice.Assign());
	if (FAILED(hr))
		return false;

	hr = dxgiDevice->GetAdapter(dxgiAdapter.Assign());
	if (FAILED(hr))
		return false;

	hr = dxgiAdapter->EnumOutputs(monitor_idx, dxgiOutput);
	if (FAILED(hr))
		return false;

	return true;
}

int get_rotation_degree(DXGI_MODE_ROTATION config_rotation)
{
	switch (config_rotation) {
	case DXGI_MODE_ROTATION_IDENTITY:
		return 0;
	case DXGI_MODE_ROTATION_ROTATE90:
		return 90;
	case DXGI_MODE_ROTATION_ROTATE180:
		return 180;
	case DXGI_MODE_ROTATION_ROTATE270:
		return 270;
	default:
		return 0;
	}
	return 0;
}

bool PLSD3D11Duplicator::init_monitor_info()
{
	ComPtr<IDXGIOutput> output;
	if (!get_monitor(monitor_dev_id, output.Assign())) {
		PLS_PLUGIN_ERROR("get monitor:%d failed", monitor_dev_id);
		return false;
	}

	DXGI_OUTPUT_DESC desc;
	HRESULT hr = output->GetDesc(&desc);
	if (FAILED(hr))
		return false;

	m_info.monitor_dev_id = monitor_dev_id;
	m_info.offset_x = desc.DesktopCoordinates.left;
	m_info.offset_y = desc.DesktopCoordinates.top;
	m_info.width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
	m_info.height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
	m_info.rotation = get_rotation_degree(desc.Rotation);
	if (desc.Rotation == DXGI_MODE_ROTATION_ROTATE90 || desc.Rotation == DXGI_MODE_ROTATION_ROTATE270) {
		m_info.width = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
		m_info.height = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
	}
	PLS_PLUGIN_INFO("monitor info: width:%d, height:%d, offsetX:%d, offsetY:%d", m_info.width, m_info.height, m_info.offset_x, m_info.offset_y);

	return true;
}

ID3D11Texture2D *PLSD3D11Duplicator::create_dump_texture(ComPtr<ID3D11Device> device, D3D11_TEXTURE2D_DESC &td)
{
	td.BindFlags = 0;
	td.Usage = D3D11_USAGE_STAGING;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	td.MiscFlags = 0;
	ID3D11Texture2D *texture = NULL;
	HRESULT hr = device->CreateTexture2D(&td, NULL, &texture);
	if (FAILED(hr)) {
		PLS_PLUGIN_ERROR("create dump texture failed, error code:%ld", hr);
		return NULL;
	}

	return texture;
}

bool PLSD3D11Duplicator::upload_buffer_to_texture(void *buffer, int width, int height)
{
	obs_enter_graphics();
	if (!monitor_texture || texture_width != width || texture_height != height) {
		gs_texture_destroy(monitor_texture);
		monitor_texture = gs_texture_create(width, height, GS_BGRA, 1, NULL, GS_DYNAMIC);
		texture_width = width;
		texture_height = height;
	}

	uint32_t line_size = width * 4;
	gs_texture_set_image(monitor_texture, (uint8_t *)buffer, line_size, false);
	obs_leave_graphics();
	return true;
}

void PLSD3D11Duplicator::thread_begin()
{
	capture_stop = false;
	capture_thread = new thread(&PLSD3D11Duplicator::capture_thread_proc, this);
}

void PLSD3D11Duplicator::thread_stop()
{
	capture_stop = true;
	if (capture_thread) {
		if (capture_thread->joinable()) {
			capture_thread->join();
		}

		delete capture_thread;
		capture_thread = NULL;
	}
}

void PLSD3D11Duplicator::monitor_capture_thread()
{
	while (!capture_stop) {
		dump_duplicator_memory();
	}
}

void PLSD3D11Duplicator::dump_duplicator_memory()
{
	DXGI_OUTDUPL_FRAME_INFO info;
	ComPtr<IDXGIResource> res;
	update_token = true;
	if (!dup_duplicator) {
		update_token = false;
		return;
	}
	HRESULT hr = dup_duplicator->AcquireNextFrame(0, &info, res.Assign());
	if (hr == DXGI_ERROR_ACCESS_LOST) {
		update_token = false;
		return;
	} else if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
		return;
	} else if (FAILED(hr)) {
		update_token = false;
		return;
	}

	if (0 == info.TotalMetadataBufferSize) {
		dup_duplicator->ReleaseFrame();
		return;
	}

	ComPtr<ID3D11Texture2D> new_texture;
	hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void **)new_texture.Assign());
	if (SUCCEEDED(hr) && new_texture.Get()) {
		D3D11_TEXTURE2D_DESC td;
		new_texture->GetDesc(&td);
		if (dump_texture == NULL || dump_texture_width != td.Width || dump_texture_height != td.Height) {
			if (dump_texture != NULL) {
				dump_texture->Release();
				dump_texture = NULL;
			}
			dump_texture = create_dump_texture(dup_device, td);
			dump_texture_width = td.Width;
			dump_texture_height = td.Height;
		}
		dup_context->CopyResource(dump_texture, new_texture.Get());

		D3D11_MAPPED_SUBRESOURCE map;
		hr = dup_context->Map(dump_texture, 0, D3D11_MAP_READ, 0, &map);
		if (FAILED(hr)) {
			PLS_PLUGIN_ERROR("map texture failed, error:%ld", hr);
			return;
		}
		update_mapdata_to_buffer_array(map, td.Width, td.Height);
		dup_context->Unmap(dump_texture, 0);
	}
	dup_duplicator->ReleaseFrame();
}

void PLSD3D11Duplicator::update_mapdata_to_buffer_array(D3D11_MAPPED_SUBRESOURCE &map, int width, int height)
{
	int write_index = get_write_index();
	buffer_info_array[write_index]->buffer_info_mutex.lock();
	if (width * 4 == map.RowPitch) {
		memmove(buffer_info_array[write_index]->buffer, map.pData, width * height * 4);
	} else {
		for (int row = 0; row < height; ++row) {
			uint8_t *src = (uint8_t *)(map.pData) + row * map.RowPitch;
			uint8_t *des = buffer_info_array[write_index]->buffer + row * buffer_info_array[write_index]->width * 4;
			memmove(des, src, buffer_info_array[write_index]->width * 4);
		}
	}
	buffer_info_array[write_index]->valid = true;
	buffer_info_array[1 - write_index]->valid = false;

	buffer_info_array[write_index]->buffer_info_mutex.unlock();
}

void PLSD3D11Duplicator::init_buffer_info_array(int width, int height)
{
	std::unique_lock<std::mutex> lck(capture_mutex);
	for (int i = 0; i < MONITOR_BUFFER_CACHE_SIZE; i++) {
		buffer_info_array[i] = new monitor_buffer_info;
		buffer_info_array[i]->valid = false;
		if (width == 0 || height == 0) {
			buffer_info_array[i]->width = 0;
			buffer_info_array[i]->height = 0;
			buffer_info_array[i]->buffer = NULL;
		} else {
			buffer_info_array[i]->width = width;
			buffer_info_array[i]->height = height;
			buffer_info_array[i]->buffer = new uint8_t[width * height * 4];
		}
	}
}

void PLSD3D11Duplicator::clear_buffer_info_array()
{
	std::unique_lock<std::mutex> lck(capture_mutex);
	for (int i = 0; i < MONITOR_BUFFER_CACHE_SIZE; i++) {
		if (buffer_info_array[i]) {
			delete buffer_info_array[i];
			buffer_info_array[i] = NULL;
		}
	}
}

int PLSD3D11Duplicator::get_read_index()
{
	std::unique_lock<std::mutex> lck(capture_mutex);
	for (int i = 0; i < MONITOR_BUFFER_CACHE_SIZE; i++) {
		if (buffer_info_array[i]->valid)
			return i;
	}
	return 0;
}

int PLSD3D11Duplicator::get_write_index()
{
	std::unique_lock<std::mutex> lck(capture_mutex);
	for (int i = 0; i < MONITOR_BUFFER_CACHE_SIZE; i++) {
		if (!buffer_info_array[i]->valid)
			return i;
	}
	return 0;
}

bool PLSD3D11Duplicator::update_texture()
{
	int index = get_read_index();
	if (index < 0)
		return false;

	std::unique_lock<std::mutex> lck(buffer_info_array[index]->buffer_info_mutex);

	monitor_buffer_info *buffer_info = buffer_info_array[index];
	if (!buffer_info) {
		return false;
	}
	upload_buffer_to_texture(buffer_info->buffer, buffer_info_array[index]->width, buffer_info_array[index]->height);
	return true;
}

bool PLSD3D11Duplicator::get_monitor_info(monitor_info &info)
{
	info.width = m_info.width;
	info.height = m_info.height;
	info.offset_x = m_info.offset_x;
	info.offset_y = m_info.offset_y;
	info.rotation = m_info.rotation;
	return true;
}
