/******************************************************************************
    Copyright (C) 2016 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "d3d11-subsystem.hpp"
#include <util/util.hpp>
#include <util/platform.h>

static const int MAX_REBUILD_FAILED_COUNT = 5;

void gs_vertex_buffer::Rebuild()
{
	uvBuffers.clear();
	uvSizes.clear();
	BuildBuffers();
}

void gs_index_buffer::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateBuffer(&bd, &srd, &indexBuffer);
	if (FAILED(hr))
		throw HRError("Failed to create buffer", hr);
}

void gs_texture_2d::RebuildSharedTextureFallback()
{
	td = {};
	td.Width = 2;
	td.Height = 2;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	td.ArraySize = 1;
	td.SampleDesc.Count = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	width = td.Width;
	height = td.Height;
	dxgiFormat = td.Format;
	levels = 1;

	resourceDesc = {};
	resourceDesc.Format = td.Format;
	resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	resourceDesc.Texture2D.MipLevels = 1;

	isShared = false;
}

//PRISM/LiuHaibin/20200629/#3174/camera effect
void gs_texture_2d::RebuildSharedTexture(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateTexture2D(
		&td, data.size() ? srd.data() : nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create shared 2D texture", hr);

	ComPtr<IDXGIResource> dxgi_res;

	texture->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

	hr = texture->QueryInterface(__uuidof(IDXGIResource),
				     (void **)&dxgi_res);
	if (FAILED(hr)) {
		plog(LOG_WARNING,
		     "InitTexture: Failed to query "
		     "interface: %08lX",
		     hr);
	} else {
		uint32_t old_shared_handle = sharedHandle;
		GetSharedHandle(dxgi_res);

		plog(LOG_INFO,
		     "Shared texture rebuilt: new handle %u, old handle %u",
		     sharedHandle, old_shared_handle);

		if (flags & GS_SHARED_KM_TEX) {
			ComPtr<IDXGIKeyedMutex> km;
			hr = texture->QueryInterface(__uuidof(IDXGIKeyedMutex),
						     (void **)&km);
			if (FAILED(hr)) {
				throw HRError("Failed to query "
					      "IDXGIKeyedMutex",
					      hr);
			}

			km->AcquireSync(0, INFINITE);
			acquired = true;
		}
	}
}

void gs_texture_2d::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	if (isShared) {
		hr = dev->OpenSharedResource((HANDLE)(uintptr_t)sharedHandle,
					     __uuidof(ID3D11Texture2D),
					     (void **)&texture);
		if (FAILED(hr)) {
			plog(LOG_WARNING,
			     "Failed to open original shared texture: ",
			     "0x%08lX", hr);

			//PRISM/LiuHaibin/20200629/#3174/camera effect
			//RebuildSharedTextureFallback();
			RebuildSharedTexture(dev);
		}
	}

	if (!isShared) {
		hr = dev->CreateTexture2D(
			&td, data.size() ? srd.data() : nullptr, &texture);
		if (FAILED(hr))
			throw HRError("Failed to create 2D texture", hr);
	}

	hr = dev->CreateShaderResourceView(texture, &resourceDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create resource view", hr);

	if (isRenderTarget)
		InitRenderTargets();

	if (isGDICompatible) {
		hr = texture->QueryInterface(__uuidof(IDXGISurface1),
					     (void **)&gdiSurface);
		if (FAILED(hr))
			throw HRError("Failed to create GDI surface", hr);
	}

	acquired = false;

	if ((td.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0) {
		ComQIPtr<IDXGIResource> dxgi_res(texture);
		if (dxgi_res)
			GetSharedHandle(dxgi_res);
		device_texture_acquire_sync(this, 0, INFINITE);
	}
}

void gs_texture_2d::RebuildNV12_Y(ID3D11Device *dev)
{
	gs_texture_2d *tex_uv = pairedNV12texture;
	HRESULT hr;

	hr = dev->CreateTexture2D(&td, nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create 2D texture", hr);

	hr = dev->CreateShaderResourceView(texture, &resourceDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create resource view", hr);

	if (isRenderTarget)
		InitRenderTargets();

	tex_uv->RebuildNV12_UV(dev);

	acquired = false;

	if ((td.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0) {
		ComQIPtr<IDXGIResource> dxgi_res(texture);
		if (dxgi_res)
			GetSharedHandle(dxgi_res);
		device_texture_acquire_sync(this, 0, INFINITE);
	}
}

void gs_texture_2d::RebuildNV12_UV(ID3D11Device *dev)
{
	gs_texture_2d *tex_y = pairedNV12texture;
	HRESULT hr;

	texture = tex_y->texture;

	hr = dev->CreateShaderResourceView(texture, &resourceDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create resource view", hr);

	if (isRenderTarget)
		InitRenderTargets();
}

void gs_zstencil_buffer::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	hr = dev->CreateTexture2D(&td, nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil texture", hr);

	hr = dev->CreateDepthStencilView(texture, &dsvd, &view);
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil view", hr);
}

void gs_stage_surface::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateTexture2D(&td, nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create staging surface", hr);
}

void gs_sampler_state::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateSamplerState(&sd, state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create sampler state", hr);
}

void gs_vertex_shader::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	hr = dev->CreateVertexShader(data.data(), data.size(), nullptr,
				     &shader);
	if (FAILED(hr))
		throw HRError("Failed to create vertex shader", hr);

	//PRISM/Wang.Chuanjing/20200402/#2322/for device reset crash
	const UINT layoutSize = (UINT)layoutData.size();
	if (layoutSize > 0) {
		hr = dev->CreateInputLayout(layoutData.data(),
					    (UINT)layoutData.size(),
					    data.data(), data.size(), &layout);
		if (FAILED(hr))
			throw HRError("Failed to create input layout", hr);
	}

	if (constantSize) {
		hr = dev->CreateBuffer(&bd, NULL, &constants);
		if (FAILED(hr))
			throw HRError("Failed to create constant buffer", hr);
	}

	for (gs_shader_param &param : params) {
		param.nextSampler = nullptr;
		param.curValue.clear();
		gs_shader_set_default(&param);
	}
}

void gs_pixel_shader::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;

	hr = dev->CreatePixelShader(data.data(), data.size(), nullptr, &shader);
	if (FAILED(hr))
		throw HRError("Failed to create pixel shader", hr);

	if (constantSize) {
		hr = dev->CreateBuffer(&bd, NULL, &constants);
		if (FAILED(hr))
			throw HRError("Failed to create constant buffer", hr);
	}

	for (gs_shader_param &param : params) {
		param.nextSampler = nullptr;
		param.curValue.clear();
		gs_shader_set_default(&param);
	}
}

void gs_swap_chain::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = device->factory->CreateSwapChain(dev, &swapDesc, &swap);
	if (FAILED(hr))
		throw HRError("Failed to create swap chain", hr);

	Init();
}

void gs_timer::Rebuild(ID3D11Device *dev)
{
	//PRISM/WangChuanjing/20211013/#9974/device valid check
	if (!device->device_valid) {
		throw "Device invalid";
	}

	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_TIMESTAMP;
	desc.MiscFlags = 0;
	HRESULT hr = dev->CreateQuery(&desc, &query_begin);
	if (FAILED(hr))
		throw HRError("Failed to create timer", hr);
	hr = dev->CreateQuery(&desc, &query_end);
	if (FAILED(hr))
		throw HRError("Failed to create timer", hr);
}

void gs_timer_range::Rebuild(ID3D11Device *dev)
{
	//PRISM/WangChuanjing/20211013/#9974/device valid check
	if (!device->device_valid) {
		throw "Device invalid";
	}

	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	desc.MiscFlags = 0;
	HRESULT hr = dev->CreateQuery(&desc, &query_disjoint);
	if (FAILED(hr))
		throw HRError("Failed to create timer", hr);
}

void SavedBlendState::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateBlendState(&bd, &state);
	if (FAILED(hr))
		throw HRError("Failed to create blend state", hr);
}

void SavedZStencilState::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateDepthStencilState(&dsd, &state);
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil state", hr);
}

void SavedRasterState::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateRasterizerState(&rd, &state);
	if (FAILED(hr))
		throw HRError("Failed to create rasterizer state", hr);
}

const static D3D_FEATURE_LEVEL featureLevels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
};

void gs_device::RebuildDevice()
try {
	//PRISM/WangChuanjing/20211013/#9974/device valid check
	device_valid = false;

	ID3D11Device *dev = nullptr;
	HRESULT hr;

	//PRISM/Wang.Chuanjing/20210607/#NoIssue for device rebuild notification
	if (rebuild_fail_count > MAX_REBUILD_FAILED_COUNT) {
		return;
	}

	plog(LOG_WARNING, "Device Remove/Reset!  Rebuilding all assets...");

	/* ----------------------------------------------------------------- */

	for (gs_device_loss &callback : loss_callbacks)
		callback.device_loss_release(callback.data);

	gs_obj *obj = first_obj;

	//PRISM/WangChuanjing/20210915/#None/rebuild test mode
	if (!device_rebuild_normal) {
		throw "device rebuild failed for test";
	}

	while (obj) {
		switch (obj->obj_type) {
		case gs_type::gs_vertex_buffer:
			((gs_vertex_buffer *)obj)->Release();
			break;
		case gs_type::gs_index_buffer:
			((gs_index_buffer *)obj)->Release();
			break;
		case gs_type::gs_texture_2d:
			((gs_texture_2d *)obj)->Release();
			break;
		case gs_type::gs_zstencil_buffer:
			((gs_zstencil_buffer *)obj)->Release();
			break;
		case gs_type::gs_stage_surface:
			((gs_stage_surface *)obj)->Release();
			break;
		case gs_type::gs_sampler_state:
			((gs_sampler_state *)obj)->Release();
			break;
		case gs_type::gs_vertex_shader:
			((gs_vertex_shader *)obj)->Release();
			break;
		case gs_type::gs_pixel_shader:
			((gs_pixel_shader *)obj)->Release();
			break;
		case gs_type::gs_duplicator:
			((gs_duplicator *)obj)->Release();
			break;
		case gs_type::gs_swap_chain:
			((gs_swap_chain *)obj)->Release();
			break;
		case gs_type::gs_timer:
			((gs_timer *)obj)->Release();
			break;
		case gs_type::gs_timer_range:
			((gs_timer_range *)obj)->Release();
			break;
		}

		obj = obj->next;
	}

	for (auto &state : zstencilStates)
		state.Release();
	for (auto &state : rasterStates)
		state.Release();
	for (auto &state : blendStates)
		state.Release();

	//PRISM/Wang.Chuanjing/20200608/if create device failed, context will be nullptr
	if (context) {
		context->ClearState();
		context->Flush();
	}

	context.Release();
	device.Release();
	adapter.Release();
	factory.Release();

	/* ----------------------------------------------------------------- */

	InitFactory(adpIdx);

	uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
			       createFlags, featureLevels,
			       sizeof(featureLevels) /
				       sizeof(D3D_FEATURE_LEVEL),
			       D3D11_SDK_VERSION, &device, nullptr, &context);

	//PRISM/WangChuanjing/20211013/#9974/device valid check
	device_valid = true;

	//PRISM/Wang.Chuanjing/20210607/#NoIssue for device rebuild notification
	if (device_rebuild_fail_test) {
		hr = E_INVALIDARG;
	}

	if (FAILED(hr))
		throw HRError("Failed to create device", hr);

	//PRISM/ZengQin/20210114/#6574/update adapter luid when rebuild device
	wstring adapterName = L"";
	if (adapter) {
		DXGI_ADAPTER_DESC desc;
		if (adapter->GetDesc(&desc) == S_OK) {
			adapterLuid.low_part = desc.AdapterLuid.LowPart;
			adapterLuid.high_part = desc.AdapterLuid.HighPart;
			adapterName = desc.Description;
			plog(LOG_INFO,
			     "Rebuild adapter: %d, luid low part: %lu, luid high part: %ld",
			     adpIdx, adapterLuid.low_part,
			     adapterLuid.high_part);
		} else {
			plog(LOG_WARNING,
			     "Failed to get adapter (index %d) LUID", adpIdx);
		}
	}

	dev = device;

	obj = first_obj;
	while (obj) {
		switch (obj->obj_type) {
		case gs_type::gs_vertex_buffer:
			((gs_vertex_buffer *)obj)->Rebuild();
			break;
		case gs_type::gs_index_buffer:
			((gs_index_buffer *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_texture_2d: {
			gs_texture_2d *tex = (gs_texture_2d *)obj;
			if (!tex->nv12) {
				tex->Rebuild(dev);
			} else if (!tex->chroma) {
				tex->RebuildNV12_Y(dev);
			}
		} break;
		case gs_type::gs_zstencil_buffer:
			((gs_zstencil_buffer *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_stage_surface:
			((gs_stage_surface *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_sampler_state:
			((gs_sampler_state *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_vertex_shader:
			((gs_vertex_shader *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_pixel_shader:
			((gs_pixel_shader *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_duplicator:
			try {
				((gs_duplicator *)obj)->Start();
			} catch (...) {
				((gs_duplicator *)obj)->Release();
			}
			break;
		case gs_type::gs_swap_chain:
			((gs_swap_chain *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_timer:
			((gs_timer *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_timer_range:
			((gs_timer_range *)obj)->Rebuild(dev);
			break;
		}

		obj = obj->next;
	}

	//PRISM/Wang.Chuanjing/20200403/#2322/for device reset crash
	obj = first_obj;
	while (obj) {
		if (obj->obj_type == gs_type::gs_swap_chain)
			((gs_swap_chain *)obj)->ResizeBuffer();
		obj = obj->next;
	}

	curRenderTarget = nullptr;
	curZStencilBuffer = nullptr;
	curRenderSide = 0;
	memset(&curTextures, 0, sizeof(curTextures));
	memset(&curSamplers, 0, sizeof(curSamplers));
	curVertexBuffer = nullptr;
	curIndexBuffer = nullptr;
	curVertexShader = nullptr;
	curPixelShader = nullptr;
	curSwapChain = nullptr;
	zstencilStateChanged = true;
	rasterStateChanged = true;
	blendStateChanged = true;
	curDepthStencilState = nullptr;
	curRasterState = nullptr;
	curBlendState = nullptr;
	curToplogy = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	for (auto &state : zstencilStates)
		state.Rebuild(dev);
	for (auto &state : rasterStates)
		state.Rebuild(dev);
	for (auto &state : blendStates)
		state.Rebuild(dev);

	for (gs_device_loss &callback : loss_callbacks)
		callback.device_loss_rebuild(device.Get(), callback.data);

	//PRISM/Wang.Chuanjing/20200408/#2321 for device rebuild
	if (engine_notify_cb) {
		BPtr<char> adapter_name_utf8;
		os_wcs_to_utf8_ptr(adapterName.c_str(), 0, &adapter_name_utf8);
		char *adapter_name = bstrdup(adapter_name_utf8.Get());
		engine_notify_cb(GS_ENGINE_NOTIFY_STATUS,
				 GS_ENGINE_STATUS_NORMAL, adapter_name);
		if (adapter_name) {
			bfree(adapter_name);
		}
	}
	rebuild_fail_count = 0;
} catch (const char *error) {
	//PRISM/WangChuanjing/20211013/#9974/device valid check
	device_valid = false;
	//PRISM/Wang.Chuanjing/20200408/#2321 for device rebuild
	if (engine_notify_cb)
		engine_notify_cb(GS_ENGINE_NOTIFY_STATUS,
				 GS_ENGINE_STATUS_EXCEPTIONAL, nullptr);
	plog(LOG_WARNING, "Device Rebuild failed: %s", error);

	//PRISM/Wang.Chuanjing/20210607/#NoIssue for device rebuild notification
	rebuild_fail_count++;
	if (rebuild_fail_count >= MAX_REBUILD_FAILED_COUNT &&
	    engine_notify_cb) {
		engine_notify_cb(GS_ENGINE_NOTIFY_EXCEPTION,
				 GS_E_REBUILD_FAILED, nullptr);
	}
	//bcrash("Failed to recreate D3D11: %s", error);

} catch (const HRError &error) {
	//PRISM/WangChuanjing/20211013/#9974/device valid check
	device_valid = false;
	//PRISM/Wang.Chuanjing/20200408/#2321 for device rebuild
	if (engine_notify_cb)
		engine_notify_cb(GS_ENGINE_NOTIFY_STATUS,
				 GS_ENGINE_STATUS_EXCEPTIONAL, nullptr);
	plog(LOG_WARNING, "Failed to recreate D3D11: %s (%08lX)", error.str,
	     error.hr);

	//PRISM/Wang.Chuanjing/20210607/#NoIssue for device rebuild notification
	rebuild_fail_count++;
	if (rebuild_fail_count >= MAX_REBUILD_FAILED_COUNT &&
	    engine_notify_cb) {
		engine_notify_cb(GS_ENGINE_NOTIFY_EXCEPTION,
				 GS_E_REBUILD_FAILED, nullptr);
	}
	//bcrash("Failed to recreate D3D11: %s (%08lX)", error.str, error.hr);
}
