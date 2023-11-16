#include "PLSGraphicsHandler.h"
#include <util/windows/win-version.h>
#include <util/platform.h>
#include <obs.h>
#include <string>

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d2d1.lib")

static const IID dxgiFactory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
const static std::vector<D3D_FEATURE_LEVEL> featureLevels = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
};

static inline uint32_t get_win_version()
{
	static uint32_t ver = 0;

	if (ver == 0) {
		struct win_version_info ver_info;

		get_win_ver(&ver_info);
		ver = (ver_info.major << 8) | ver_info.minor;
	}
	return ver;
}

PLSGraphicsHandler *PLSGraphicsHandler::Instance()
{
	static PLSGraphicsHandler instance;
	return &instance;
}

PLSGraphicsHandler::~PLSGraphicsHandler()
{
	DestroyDevice();
}

bool PLSGraphicsHandler::CreateDevice()
{
	IID factoryIID = (get_win_version() >= 0x602) ? dxgiFactory2 : __uuidof(IDXGIFactory1);
	HRESULT hr = CreateDXGIFactory1(factoryIID, (void **)dxgiFactory.Assign());
	if (FAILED(hr)) {
		return false;
	}

	uint32_t adapterIdx = 0;
	ComPtr<IDXGIAdapter1> adapter_ptr;
	hr = dxgiFactory->EnumAdapters1(adapterIdx, adapter_ptr.Assign());
	if (FAILED(hr))
		return false;

	if (!adapter_ptr) {
		return false;
	}

	hr = D3D11CreateDevice(adapter_ptr.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels.data(), (UINT)featureLevels.size(), D3D11_SDK_VERSION,
			       d3d11Device.Assign(), nullptr, d3d11DeviceContext.Assign());

	if (FAILED(hr)) {
		return false;
	}

	DXGI_ADAPTER_DESC desc;
	adapter_ptr->GetDesc(&desc);

	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d1Factory.Assign());
	if (FAILED(hr)) {
		return false;
	}

	inited = true;
	return true;
}

bool PLSGraphicsHandler::RebuildDevice()
{
	if (inited)
		DestroyDevice();
	return CreateDevice();
}

void PLSGraphicsHandler::DestroyDevice()
{
	if (!inited)
		return;
	inited = false;
	d3d11DeviceContext.Release();
	d3d11Device.Release();
	dxgiFactory.Release();
	d2d1Factory.Release();
	return;
}
