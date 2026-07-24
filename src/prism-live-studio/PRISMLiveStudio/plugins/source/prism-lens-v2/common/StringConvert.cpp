#include <Windows.h>
#include <vector>
#include "StringConvert.h"

#define U2T(s) str::u2T(s.c_str()).c_str()

namespace str {
//---------------------------------------------------------------

std::_tstring a2T(const char *str)
{
#ifdef UNICODE
	return a2w(str);
#else
	return str;
#endif
}

std::_tstring w2T(const wchar_t *str)
{
#ifdef UNICODE
	return str;
#else
	return w2a(str);
#endif
}

std::string T2a(const TCHAR *str)
{
#ifdef UNICODE
	return w2a(str);
#else
	return str;
#endif
}

std::wstring T2w(const TCHAR *str)
{
#ifdef UNICODE
	return str;
#else
	return a2w(str);
#endif
}

std::wstring a2w(const char *str)
{
	if (!str)
		return L"";
	int n = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);
	std::vector<wchar_t> vecBuffer(n + 1);
	wchar_t *pBuffer = vecBuffer.data();
	n = MultiByteToWideChar(CP_ACP, 0, str, -1, pBuffer, n);
	pBuffer[n] = 0;
	std::wstring ret(pBuffer);
	return ret;
}

std::string w2a(const wchar_t *str)
{
	if (!str)
		return "";
	int n = WideCharToMultiByte(CP_ACP, 0, str, -1, nullptr, 0, nullptr, nullptr);
	std::vector<char> vecBuffer(n + 1);
	auto pBuffer = vecBuffer.data();
	n = WideCharToMultiByte(CP_ACP, 0, str, -1, pBuffer, n, nullptr, nullptr);
	pBuffer[n] = 0;
	std::string ret(pBuffer);
	return ret;
}

std::string w2u(const wchar_t *str)
{
	if (!str)
		return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
	std::vector<char> vecBuffer(n + 1);
	auto pBuffer = vecBuffer.data();
	n = WideCharToMultiByte(CP_UTF8, 0, str, -1, pBuffer, n, nullptr, nullptr);
	pBuffer[n] = 0;
	std::string ret(pBuffer);
	return ret;
}

std::wstring u2w(const char *str)
{
	if (!str)
		return L"";
	int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
	std::vector<wchar_t> vecBuffer(n + 1);
	auto pBuffer = vecBuffer.data();
	n = MultiByteToWideChar(CP_UTF8, 0, str, -1, pBuffer, n);
	pBuffer[n] = 0;
	std::wstring ret(pBuffer);
	return ret;
}

std::string a2u(const char *str)
{
	return w2u(a2w(str).c_str());
}

std::string u2a(const char *str)
{
	return w2a(u2w(str).c_str());
}

std::string T2u(const TCHAR *str)
{
#ifdef UNICODE
	return w2u(str);
#else
	return w2u(a2w(str).c_str());
#endif
}

std::_tstring u2T(const char *str)
{
#ifdef UNICODE
	return u2w(str);
#else
	return w2a(u2w(str).c_str());
#endif
}

std::wstring SplitFileName(std::wstring path)
{
	size_t suffixPos = path.find_last_of('\\');
	std::wstring subStr = path.substr(suffixPos + 1);
	return subStr;
}

std::string GenerateGuid()
{
	GUID guid;
	(void)CoCreateGuid(&guid);

	char buf[64] = {0};
	sprintf_s(buf, sizeof(buf) / sizeof(buf[0]), "{%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		  guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return std::string(buf);
}
} // namespace -------------------------------------------
