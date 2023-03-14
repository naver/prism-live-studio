/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

gs_stage_surface::gs_stage_surface(gs_device_t *device, uint32_t width,
				   uint32_t height, gs_color_format colorFormat)
	: gs_obj(device, gs_type::gs_stage_surface),
	  width(width),
	  height(height),
	  format(colorFormat),
	  dxgiFormat(ConvertGSTextureFormat(colorFormat))
{
	HRESULT hr;

	memset(&td, 0, sizeof(td));
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = dxgiFormat;
	td.SampleDesc.Count = 1;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	td.Usage = D3D11_USAGE_STAGING;

	//PRISM/WangChuanjing/20211013/#9974/device valid check
	if (!device->device_valid)
		throw "Device invalid";

	hr = device->device->CreateTexture2D(&td, NULL, texture.Assign());
	if (FAILED(hr)) {
		//PRISM/WangChuanjing/20210311/#6941/notify engine status
		if (device->engine_notify_cb) {
			int code = gs_engine_notify_code(hr);
			device->engine_notify_cb(GS_ENGINE_NOTIFY_EXCEPTION,
						 code, nullptr);
		}
		throw HRError("Failed to create staging surface", hr);
	}
}

gs_stage_surface::gs_stage_surface(gs_device_t *device, uint32_t width,
				   uint32_t height)
	: gs_obj(device, gs_type::gs_stage_surface),
	  width(width),
	  height(height),
	  format(GS_UNKNOWN),
	  dxgiFormat(DXGI_FORMAT_NV12)
{
	HRESULT hr;

	memset(&td, 0, sizeof(td));
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = dxgiFormat;
	td.SampleDesc.Count = 1;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	td.Usage = D3D11_USAGE_STAGING;

	//PRISM/WangChuanjing/20211013/#9974/device valid check
	if (!device->device_valid)
		throw "Device invalid";

	hr = device->device->CreateTexture2D(&td, NULL, texture.Assign());
	if (FAILED(hr)) {
		//PRISM/WangChuanjing/20210311/#6941/notify engine status
		if (device->engine_notify_cb) {
			int code = gs_engine_notify_code(hr);
			device->engine_notify_cb(GS_ENGINE_NOTIFY_EXCEPTION,
						 code, nullptr);
		}
		throw HRError("Failed to create staging surface", hr);
	}
}
