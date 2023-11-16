#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <Windows.h>
#include "win_lock.hpp"
#include <util/windows/ComPtr.hpp>
#include "graphics/graphics.h"
#include <obs.h>

enum class DuplicatorType {
	DUPLICATOR_SHARED_HANDLE = 0,
	DUPLICATOR_MEMORY_COPY,
};

class PLSAutoLockRender {
public:
	PLSAutoLockRender() { obs_enter_graphics(); }
	virtual ~PLSAutoLockRender() { obs_leave_graphics(); }

	PLSAutoLockRender(const PLSAutoLockRender &) = delete;
	PLSAutoLockRender &operator=(const PLSAutoLockRender &) = delete;
	PLSAutoLockRender(PLSAutoLockRender &&) = delete;
	PLSAutoLockRender &operator=(PLSAutoLockRender &&) = delete;
};

struct PLSDuplicatorCore {
public:
	friend class PLSDuplicatorInstance;

	explicit PLSDuplicatorCore(DuplicatorType duplicator_type, int adapter_index, int adapter_output_index);
	virtual ~PLSDuplicatorCore();

	PLSDuplicatorCore(const PLSDuplicatorCore &) = delete;
	PLSDuplicatorCore &operator=(const PLSDuplicatorCore &) = delete;
	PLSDuplicatorCore(PLSDuplicatorCore &&) = delete;
	PLSDuplicatorCore &operator=(PLSDuplicatorCore &&) = delete;

	bool init();
	void uninit();
	bool check_update_texture();

	bool upload_memory_texture(gs_texture *&gs_frame);
	bool upload_shared_texture(gs_texture *&gs_frame, HANDLE &handle_output);

private:
	bool init_factory(unsigned adapterIdx, IDXGIAdapter1 **ppAdapter);
	bool init_device(IDXGIAdapter *pAdapter);
	bool init_output(int monitor_idx, IDXGIOutput **dxgiOutput) const;

	bool update_texture();
	bool check_device_error(HRESULT hr);
	bool copy_texture(ComPtr<IDXGIResource> res, const DXGI_OUTDUPL_FRAME_INFO &info);
	bool create_read_texture(D3D11_TEXTURE2D_DESC &td);
	bool create_shared_texture(const D3D11_TEXTURE2D_DESC &descTemp);

	bool is_size_changed(const gs_texture *gs_frame) const;

	//----------------------------------------------------------------------------------
	DuplicatorType type;
	int adapter;
	int adapter_output;
	bool inited = false;

	CCSection duplicator_lock;
	ComPtr<IDXGIFactory1> dxgi_factory = nullptr;
	ComPtr<ID3D11Device> d3d11_device = nullptr;
	ComPtr<ID3D11DeviceContext> d3d11_device_context = nullptr;
	ComPtr<IDXGIOutputDuplication> output_duplicator = nullptr;
	ComPtr<ID3D11Texture2D> monitor_texture = nullptr;
	D3D11_TEXTURE2D_DESC texture_desc{};
	HANDLE shared_handle = nullptr;
};
