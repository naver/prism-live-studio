#pragma once

#include <vector>
#include <string>
#if _WIN32
#include <Windows.h>
#endif

struct SoftInfo {
	std::string key;
	std::string softName;
	std::string softVersion;
	std::string softPublisher;
};

class PLSSoftStatistic {
public:
	PLSSoftStatistic() = default;
	~PLSSoftStatistic() = default;

#if _WIN32
	static std::vector<SoftInfo> GetInstalledSoftware();
	static bool regEnumKey(HKEY rootKey, LPCTSTR lpSubKey);
	static bool regQueryValue(const std::wstring &strMidReg);
#endif
	static std::vector<std::string> GetBalcklist();

private:
	static std::vector<SoftInfo> balcklistSoftInfos;
};
