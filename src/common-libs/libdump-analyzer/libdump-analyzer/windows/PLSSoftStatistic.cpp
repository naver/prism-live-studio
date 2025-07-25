#include "PLSSoftStatistic.h"
#include "libutils-api.h"

#include <array>
#include <codecvt>
#if _WIN32
#include <tchar.h>
#endif

const static std::vector<std::string> blacklist = {"Fasoo"};
std::vector<SoftInfo> PLSSoftStatistic::balcklistSoftInfos{0};

#if _WIN32
std::vector<SoftInfo> PLSSoftStatistic::GetInstalledSoftware()
{
	if (!balcklistSoftInfos.empty())
		return balcklistSoftInfos;

	auto rootKey = HKEY_LOCAL_MACHINE;
	LPCTSTR lpSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
	bool ret = regEnumKey(rootKey, lpSubKey);

	if (!ret || balcklistSoftInfos.empty()) {
		rootKey = HKEY_CURRENT_USER;
		lpSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
		ret = regEnumKey(rootKey, lpSubKey);
	}

	if (!ret || balcklistSoftInfos.empty()) {
		rootKey = HKEY_LOCAL_MACHINE;
		lpSubKey = _T("SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
		regEnumKey(rootKey, lpSubKey);
	}

	return balcklistSoftInfos;
}

bool PLSSoftStatistic::regEnumKey(HKEY rootKey, LPCTSTR lpSubKey)
{
	HKEY hkResult;
	std::array<TCHAR, MAX_PATH> buffer{0};
	std::wstring strMidReg{};
	LONG lReturn = RegOpenKeyEx(rootKey, lpSubKey, 0, KEY_READ | KEY_WOW64_64KEY, &hkResult);
	if (lReturn != ERROR_SUCCESS) {
		return false;
	}
	for (unsigned long index = 0;; index++) {
		DWORD len = buffer.size();
		if (RegEnumKeyEx(hkResult, index, buffer.data(), &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS && !buffer.empty()) {
			strMidReg = (std::wstring)lpSubKey + _T("\\") + buffer.data();
			if (regQueryValue(strMidReg))
				break;
			else
				continue;
		} else {
			break;
		}
	}
	RegCloseKey(hkResult);

	return true;
}

bool PLSSoftStatistic::regQueryValue(const std::wstring &strMidReg)
{
	SoftInfo softinfo;
	HKEY hkRKey;
	std::array<TCHAR, 255> szBuffer{};
	DWORD dwType = REG_BINARY | REG_DWORD | REG_EXPAND_SZ | REG_MULTI_SZ | REG_NONE | REG_SZ;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, strMidReg.data(), 0, KEY_READ | KEY_WOW64_64KEY, &hkRKey) == ERROR_SUCCESS) {
		DWORD dwNameLen = szBuffer.size() * sizeof(TCHAR);
		LRESULT lr = RegQueryValueEx(hkRKey, _T("SystemComponent"), nullptr, &dwType, (LPBYTE)szBuffer.data(), &dwNameLen);
		if (lr == ERROR_SUCCESS && !dwNameLen) {
			RegCloseKey(hkRKey);
			return false;
		}
		lr = RegQueryValueEx(hkRKey, _T("ParentKeyName"), nullptr, &dwType, (LPBYTE)szBuffer.data(), &dwNameLen);
		if (lr == ERROR_SUCCESS && !dwNameLen) {
			RegCloseKey(hkRKey);
			return false;
		}

		lr = RegQueryValueEx(hkRKey, _T("DisplayName"), nullptr, &dwType, (LPBYTE)szBuffer.data(), &dwNameLen);
		if (lr != ERROR_SUCCESS) {
			RegCloseKey(hkRKey);
			return false;
		}
		softinfo.softName = pls_unicode_to_utf8(szBuffer.data());
		dwNameLen = 255;
		memset(szBuffer.data(), 0, 255);

		std::string value = softinfo.softName;
		auto finder = [value, &softinfo](const std::string &item) {
			bool find = value.find(item) != std::wstring::npos;
			if (find)
				softinfo.key = item;
			return find;
		};
		auto ret = std::find_if(blacklist.begin(), blacklist.end(), finder);
		if (ret == blacklist.end()) {
			RegCloseKey(hkRKey);
			return false;
		}

		RegQueryValueEx(hkRKey, _T("Publisher"), nullptr, nullptr, (LPBYTE)szBuffer.data(), &dwNameLen);
		softinfo.softPublisher = pls_unicode_to_utf8(szBuffer.data());
		dwNameLen = 255;
		memset(szBuffer.data(), 0, 255);

		RegQueryValueEx(hkRKey, _T("DisplayVersion"), nullptr, &dwType, (LPBYTE)szBuffer.data(), &dwNameLen);
		softinfo.softVersion = pls_unicode_to_utf8(szBuffer.data());
		dwNameLen = 255;
		memset(szBuffer.data(), 0, 255);

		balcklistSoftInfos.push_back(softinfo);
		RegCloseKey(hkRKey);
	}
	return true;
}
#endif

std::vector<std::string> PLSSoftStatistic::GetBalcklist()
{
	return blacklist;
}
