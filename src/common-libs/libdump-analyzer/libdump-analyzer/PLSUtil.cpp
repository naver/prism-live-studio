#include "PLSUtil.h"
#include "libutils-api.h"

#include <string>

#if _WIN32
#include <Windows.h>
#include <VersionHelpers.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#endif

#if __APPLE__
#include "mac/PLSUtilInterface.h"
#endif

static std::vector<std::string> g_third_party_plugins;
static const std::string third_party_plugin_list_path = "PRISMLiveStudio\\crashDump\\third_party_plugins.json";

namespace pls {

#if _WIN32
static QString get_app_data_dir_internal(QString name)
{
	std::array<TCHAR, MAX_PATH> dir{};
	SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir.data());
	auto utf8_dir = pls_unicode_to_utf8(dir.data());
	return QString(utf8_dir.data()) + "\\" + name;
}

static QString get_app_run_dir_internal(QString name)
{
	std::array<TCHAR, MAX_PATH> dir;
	GetModuleFileName(nullptr, dir.data(), dir.size());
	PathRemoveFileSpec(dir.data());
	auto utf8_dir = pls_unicode_to_utf8(dir.data());
	return QString(utf8_dir.data()) + "\\" + name;
}

static bool get_windows_os_info(DWORD &major, DWORD &minor, DWORD &build_number)
{
	using GetOSVersion = void(WINAPI *)(DWORD *, DWORD *, DWORD *);

	HMODULE hModule = LoadLibraryW(L"ntdll.dll");
	if (hModule) {
		auto pAddress = (GetOSVersion)GetProcAddress(hModule, "RtlGetNtVersionNumbers");
		if (pAddress) {
			pAddress(&major, &minor, &build_number);
			build_number &= 0x0ffff;
		}
		FreeLibrary(hModule);
		return true;
	}
	return false;
}

static std::string get_windows_version()
{
	DWORD major = 0;
	DWORD minor = 0;
	DWORD build_version = 0;
	bool res = get_windows_os_info(major, minor, build_version);
	if (!res) {
		return "Windows 10(x64)";
	}

	uint32_t version_info = (major << 8) | minor;

	bool server = IsWindowsServer();
	bool x64bit = true;
	if (sizeof(void *) != 8) {
		x64bit = false;
	}
	std::string bitStr = x64bit ? "(x64)" : "(x32)";

	std::string ver_str("");
	if (version_info >= 0x0A00) {
		if (server) {
			ver_str = "Windows Server 2016 Technical Preview";
		} else {
			if (build_version >= 21664) {
				ver_str = "Windows 11";
			} else {
				ver_str = "Windows 10";
			}
		}
	} else if (version_info >= 0x0603) {
		ver_str = server ? "Windows Server 2012 r2" : "Windows 8.1";
	} else if (version_info >= 0x0602) {
		ver_str = server ? "Windows Server 2012" : "Windows 8";
	} else if (version_info >= 0x0601) {
		ver_str = server ? "Windows Server 2008 r2" : "Windows 7";
	} else if (version_info >= 0x0600) {
		ver_str = server ? "Windows Server 2008" : "Windows Vista";
	} else {
		ver_str = "Windows Before Vista";
	}

	std::string output_ver_info = ver_str + bitStr;
	return output_ver_info;
}

#endif

QString get_app_run_dir(QString name)
{
#if _WIN32
	return get_app_run_dir_internal(name);
#else
	return QString::fromStdString(mac_get_app_run_dir(name.toStdString()));
#endif
}

QString get_app_data_dir(QString name)
{
#if _WIN32
	return get_app_data_dir_internal(name);
#else
	return QString::fromStdString(mac_get_app_data_dir(name.toStdString()));
#endif
}

std::string get_os_version()
{
#if _WIN32
	return get_windows_version();
#else
	return mac_get_os_version();
#endif
}

#if __APPLE__
std::string get_device_model()
{
	return mac_get_device_model();
}

std::string get_device_name()
{
	return mac_get_device_name();
}

std::string generate_dump_file(std::string info, std::string message)
{
	return mac_generate_dump_file(info, message);
}

#endif

bool is_third_party_plugin(std::string &module_path)
{
#if __APPLE__
	return mac_is_third_party_plugin(module_path);
#else
	auto path = pls_get_app_data_dir(QString::fromStdString(third_party_plugin_list_path));
	auto data = pls_read_data(path);
	QJsonObject obj = QJsonDocument::fromJson(data).object();
	QJsonArray plugin_list = obj["plugins"].toArray();

	std::filesystem::path pathObj(std::filesystem::u8path(module_path));
	std::string dllName = pathObj.stem().u8string();

	for (auto plugin : plugin_list) {
		if (plugin.toObject()["name"].toString().toStdString() == dllName) {
			return true;
		}
	}

	return false;
#endif
}

std::string get_plugin_version(std::string &path)
{
#if __APPLE__
	return mac_get_plugin_version(path);
#else
	struct pls_win_ver_t version_info;
	pls_get_win_dll_ver(version_info, QString::fromStdString(path));
	std::stringstream version;
	version << version_info.major << "." << version_info.minor << "." << version_info.build << "." << version_info.revis;
	return version.str();
#endif
}
}