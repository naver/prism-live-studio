#pragma once
#include "PLSDrawPenStroke.h"
#include "graphics/graphics.h"

class PLSGraphicsHandler {
public:
	static PLSGraphicsHandler *Instance();
	~PLSGraphicsHandler();

	bool CreateDevice();
	bool RebuildDevice();
	void DestroyDevice();
	bool InitedDevice() const { return inited; }
	ComPtr<ID3D11Device> GetD3D11Device() { return d3d11Device; }
	ComPtr<ID2D1Factory> GetD2DFactory() { return d2d1Factory; };
	ComPtr<ID3D11DeviceContext> GetD3DContext() { return d3d11DeviceContext; }

private:
	PLSGraphicsHandler() = default;

	// Direct3D
	ComPtr<IDXGIFactory1> dxgiFactory{};
	ComPtr<ID3D11Device> d3d11Device{};
	ComPtr<ID3D11DeviceContext> d3d11DeviceContext{};

	// Direct2D
	ComPtr<ID2D1Factory> d2d1Factory{};

	bool inited = false;
};
