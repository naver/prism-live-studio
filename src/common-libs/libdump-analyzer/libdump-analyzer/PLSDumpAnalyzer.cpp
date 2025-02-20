#include "PLSDumpAnalyzer.h"
#include "libutils-api.h"
#include "PLSUtil.h"

#include <fstream>
#include <filesystem>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <ShlObj.h>
#endif

#include "PLSAnalysisStack.h"
#include "libutils-api.h"

#if defined(Q_OS_WIN)

static bool find_dump_file(std::string& dump_file_path, uint64_t& file_size, const std::string& dump_folder, const std::string& process_name, std::string process_pid)
{
	auto prefix = pls_utf8_to_unicode(process_name.c_str()) + L"." + pls_utf8_to_unicode(process_pid.c_str());
	std::filesystem::path dump_folder_path(std::filesystem::u8path(dump_folder));
	std::error_code ec;
	if (std::filesystem::exists(dump_folder_path, ec) && std::filesystem::is_directory(dump_folder_path, ec)) {
		std::filesystem::directory_iterator it(dump_folder_path);
		for (const auto& entry : it) {
			if (entry.is_regular_file() && entry.path().wstring().find(L".dmp") != std::wstring::npos && entry.path().wstring().find(prefix) != std::wstring::npos) {
				dump_file_path = entry.path().u8string();
				file_size = std::filesystem::file_size(entry.path(), ec);
				return true;
			}
		}
	}

	return false;
}

static bool wait_for_dump_file_generated(std::string& dump_file_path, uint64_t& file_size, const std::string& dump_folder, const std::string& process_name, std::string process_pid)
{
	using namespace std::chrono;
	for (auto start = steady_clock::now(); duration_cast<seconds>(steady_clock::now() - start) < 60s;) { // wait 60s
		if (find_dump_file(dump_file_path, file_size, dump_folder, process_name, process_pid)) {
			return true;
		}

		Sleep(1000);
	}
	return false;
}

static bool check_disappear_dump(std::string& dump_file_path, uint64_t& file_size, const std::string& process_name, const std::string& process_pid) {
	HKEY key;
	std::wstring subkey = L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\" + pls_utf8_to_unicode(process_name.c_str());

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return false;
	}

	wchar_t buffer[MAX_PATH];
	DWORD buffer_size = MAX_PATH;

	if (RegQueryValueExW(key, L"DumpFolder", NULL, NULL, (LPBYTE)buffer, &buffer_size) != ERROR_SUCCESS) {
		RegCloseKey(key);
		return false;
	}

	RegCloseKey(key);
	std::string strBuffer = pls_unicode_to_utf8(buffer);
	if (!wait_for_dump_file_generated(dump_file_path, file_size, strBuffer, process_name, process_pid))
		return false;
	return true;
}

static bool check_crash_dump(std::string& dump_file_path, uint64_t& file_size, const std::string& process_name, std::string process_pid)
{
	std::array<wchar_t, MAX_PATH> dir{};
	SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir.data());
	std::string dump_folder = pls_unicode_to_utf8(dir.data()) + "\\PRISMLiveStudio\\crashDump";


	if (find_dump_file(dump_file_path, file_size, dump_folder, process_name, process_pid)) {
		return true;
	}
	return false;
}

static bool check_dump_file_win(ProcessInfo &info)
{
	DumpType dump_type = DumpType::DT_NONE;
	std::string dump_file_path;
	uint64_t file_size = 0;
	if (check_crash_dump(dump_file_path, file_size, info.process_name, info.pid)) {
		dump_type = DumpType::DT_CRASH;
		if (info.found_dumo_func) {
			if (file_size == 0)
				info.found_dumo_func("CrashedWithoutDump", info.process_name);
			else
				info.found_dumo_func("Crashed", info.process_name);
		}
	} else if (check_disappear_dump(dump_file_path, file_size, info.process_name, info.pid)) {
		dump_type = DumpType::DT_DISAPPEAR;
		if (info.found_dumo_func)
			info.found_dumo_func("DisappearWithDump", info.process_name);
	} else {
		if (info.found_dumo_func)
			info.found_dumo_func("DisappearWithoutDump", info.process_name);
		return false;
	}
	info.dump_file = dump_file_path;
	info.dump_type = dump_type;
	return true;
}

#elif defined(Q_OS_MACOS)
// TODO: - mac next

#endif


LIBDUMPANALUZER_API void pls_catch_unhandled_exceptions(const std::string &process_name, const std::string &dump_path)
{
	pls::catch_unhandled_exceptions(process_name, dump_path);
}

LIBDUMPANALUZER_API void pls_catch_unhandled_exceptions_and_send_dump(const ProcessInfo &info)
{
	pls::catch_unhandled_exceptions_and_send_dump(info);
}

LIBDUMPANALUZER_API void pls_set_prism_user_id(const std::string &user_id)
{
	pls::set_prism_user_id(user_id);
}

LIBDUMPANALUZER_API void pls_set_prism_session(const std::string &prism_session)
{
	pls::set_prism_session(prism_session);
}

LIBDUMPANALUZER_API void pls_set_prism_video_adapter(const std::string &video_adapter)
{
	pls::set_prism_video_adapter(video_adapter);
}

LIBDUMPANALUZER_API void pls_set_setup_session(const std::string &session)
{
	pls::set_setup_session(session);
}

LIBDUMPANALUZER_API void pls_set_prism_sub_session(const std::string &session)
{
	pls::set_prism_sub_session(session);
}

LIBDUMPANALUZER_API void pls_set_prism_pid(const std::string &pid) {
	pls::set_prism_pid(pid);
}


LIBDUMPANALUZER_API bool pls_wait_send_dump(ProcessInfo info)
{
#if _WIN32
	bool ret = check_dump_file_win(info);
	return ret ? pls::analysis_stack_and_send_dump(info) : false;
#else
	return pls::analysis_stack_and_send_dump(info);
#endif
}

#if defined(Q_OS_WIN)
LIBDUMPANALUZER_API bool pls_send_block_dump(ProcessInfo info)
{
	return pls::analysis_stack_and_send_dump(info);
}

LIBDUMPANALUZER_API std::vector<SoftInfo> pls_installed_software()
{
	return PLSSoftStatistic::GetInstalledSoftware();
}

#elif defined(Q_OS_MACOS)

// TODO: - mac next

#endif
