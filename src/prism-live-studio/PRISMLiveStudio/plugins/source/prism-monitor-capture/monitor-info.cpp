#include "monitor-info.h"
#include <util/windows/ComPtr.hpp>
#include <assert.h>
#include <liblog.h>

using namespace std;

PLSMonitorManager::PLSMonitorManager()
{
	reload_monitor(true);
}

PLSMonitorManager *PLSMonitorManager::get_instance()
{
	static PLSMonitorManager monitor_manager;
	return &monitor_manager;
}

std::string wchar_to_string(const wchar_t *str)
{
	if (!str)
		return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
	std::vector<char> vecBuffer(n + 1);
	vecBuffer.assign(n + 1, 0);
	char *pBuffer = vecBuffer.data();
	n = WideCharToMultiByte(CP_UTF8, 0, str, -1, pBuffer, n, nullptr, nullptr);
	if (n <= 0) {
		return "";
	}
	std::string ret(pBuffer);
	return ret;
}

int get_rotation_degree(DISPLAYCONFIG_ROTATION config_rotation)
{
	switch (config_rotation) {
	case DISPLAYCONFIG_ROTATION_IDENTITY:
		return 0;
	case DISPLAYCONFIG_ROTATION_ROTATE90:
		return 90;
	case DISPLAYCONFIG_ROTATION_ROTATE180:
		return 180;
	case DISPLAYCONFIG_ROTATION_ROTATE270:
		return 270;
	default:
		return 0;
	}
}

void enum_monitor_detail(const WCHAR *destDevice, monitor_info *info)
{
	UINT32 requiredPaths;
	UINT32 requiredModes;
	if (ERROR_SUCCESS != GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes))
		return;

	std::vector<DISPLAYCONFIG_PATH_INFO> paths(requiredPaths);
	std::vector<DISPLAYCONFIG_MODE_INFO> modes(requiredModes);
	if (ERROR_SUCCESS != QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), nullptr))
		return;

	for (const auto &pathTemp : paths) {
		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = pathTemp.sourceInfo.adapterId;
		sourceName.header.id = pathTemp.sourceInfo.id;
		if (ERROR_SUCCESS != DisplayConfigGetDeviceInfo(&sourceName.header)) {
			continue;
		}

		if (0 == wcscmp(destDevice, sourceName.viewGdiDeviceName)) {
			DISPLAYCONFIG_TARGET_DEVICE_NAME name;
			name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			name.header.size = sizeof(name);
			name.header.adapterId = pathTemp.sourceInfo.adapterId;
			name.header.id = pathTemp.targetInfo.id;
			if (ERROR_SUCCESS == DisplayConfigGetDeviceInfo(&name.header)) {
				info->friendly_name = wchar_to_string(name.monitorFriendlyDeviceName);
				info->display_id = pathTemp.targetInfo.id;
				info->rotation = get_rotation_degree(pathTemp.targetInfo.rotation);
			}

			return;
		}
	}
}

BOOL CALLBACK enum_monitor_proc(HMONITOR handle, HDC, LPRECT, LPARAM lParam)
{
	auto info_array = (std::vector<monitor_info> *)(lParam);

	MONITORINFOEX mi = {};
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfo(handle, &mi)) {
		return TRUE;
	}

	monitor_info info;
	info.handle = handle;
	info.offset_x = mi.rcMonitor.left;
	info.offset_y = mi.rcMonitor.top;
	info.width = (mi.rcMonitor.right - mi.rcMonitor.left);
	info.height = (mi.rcMonitor.bottom - mi.rcMonitor.top);

	enum_monitor_detail(mi.szDevice, &info);

	if (mi.dwFlags == MONITORINFOF_PRIMARY) {
		info.is_primary = true;
		info_array->insert(info_array->begin(), info);
	} else {
		info.is_primary = false;
		info_array->push_back(info);
	}

	return TRUE;
}

void fill_monitor_id(std::vector<monitor_info> &list_info_out, const monitor_info &match_info)
{
	for (unsigned i = 0; i < static_cast<int>(list_info_out.size()); ++i) {
		if (list_info_out.at(i).width == match_info.width && list_info_out.at(i).height == match_info.height && list_info_out.at(i).offset_x == match_info.offset_x &&
		    list_info_out.at(i).offset_y == match_info.offset_y) {
			list_info_out.at(i).adapter_index = match_info.adapter_index;
			list_info_out.at(i).adapter_output_index = match_info.adapter_output_index;
		}
	}
}

void PLSMonitorManager::reload_monitor(bool save_log)
{
	lock_guard locker(list_lock);

	monitor_info_array.clear();
	EnumDisplayMonitors(nullptr, nullptr, enum_monitor_proc, (LPARAM)&monitor_info_array);

	std::vector<monitor_info> d3dList;
	if (enum_duplicator_array(d3dList)) {
		for (const auto &info : d3dList) {
			fill_monitor_id(monitor_info_array, info);
		}
	}

	if (save_log) {
		PLS_INFO("monitor capture", "------------------- monitor list count : %d --------------------", monitor_info_array.size());
		for (auto item : monitor_info_array) {
			std::string name = item.friendly_name.empty() ? "no name" : item.friendly_name;
			PLS_INFO("monitor capture", "Monitor detail. [%s] %dx%d is_primary:%d hardware_id:%d adapter:%d output:%d", name.c_str(), item.width, item.height, item.is_primary,
				 item.display_id, item.adapter_index, item.adapter_output_index);
		}
	}
}

std::vector<monitor_info> PLSMonitorManager::get_monitor()
{
	lock_guard locker(list_lock);

	return monitor_info_array;
}

bool PLSMonitorManager::enum_duplicator_array(std::vector<monitor_info> &outputList) const
{
	ComPtr<IDXGIFactory1> m_pFactory;
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIOutput> output;

	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)m_pFactory.Assign());
	if (FAILED(hr)) {
		PLS_ERROR("monitor capture", "create dxgi factory failed, error code:%ld", hr);
		return false;
	}

	int adapter_index = 0;
	while (m_pFactory->EnumAdapters1(adapter_index, adapter.Assign()) == S_OK) {
		int output_index = 0;
		while (adapter->EnumOutputs(output_index, &output) == S_OK) {
			DXGI_OUTPUT_DESC desc;
			if (SUCCEEDED(output->GetDesc(&desc))) {
				const RECT &rect = desc.DesktopCoordinates;
				monitor_info info;
				info.adapter_index = adapter_index;
				info.adapter_output_index = output_index;
				info.offset_x = rect.left;
				info.offset_y = rect.top;
				info.width = rect.right - rect.left;
				info.height = rect.bottom - rect.top;
				outputList.push_back(info);
			}
			++output_index;
		}
		++adapter_index;
	}

	return !outputList.empty();
}

bool PLSMonitorManager::find_monitor(int index, int display_id, monitor_info &info)
{
	lock_guard locker(list_lock);

	info = monitor_info();

	if (display_id > 0) {
		for (const auto &temp : monitor_info_array) {
			if (temp.display_id == display_id) {
				info = temp;
				return true;
			}
		}
	}

	if (index < 0 || index >= (int)monitor_info_array.size())
		return false;

	info = monitor_info_array.at(index);
	return true;
}
