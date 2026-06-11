#include <Windows.h>
#include <memory>
#include <vector>
#include "string-convert.h"

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
	std::wstring ret = pBuffer;
	return ret;
}

std::string w2a(const wchar_t *str)
{
	if (!str)
		return "";

	int n = WideCharToMultiByte(CP_ACP, 0, str, -1, nullptr, 0, nullptr, nullptr);

	std::vector<char> vecBuffer(n + 1);
	char *pBuffer = vecBuffer.data();

	n = WideCharToMultiByte(CP_ACP, 0, str, -1, pBuffer, n, nullptr, nullptr);
	pBuffer[n] = 0;
	std::string ret = pBuffer;
	return ret;
}

std::string w2u(const wchar_t *str)
{
	if (!str)
		return "";

	int n = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);

	std::vector<char> vecBuffer(n + 1);
	char *pBuffer = vecBuffer.data();

	n = WideCharToMultiByte(CP_UTF8, 0, str, -1, pBuffer, n, nullptr, nullptr);
	pBuffer[n] = 0;
	std::string ret = pBuffer;
	return ret;
}

std::wstring u2w(const char *str)
{
	if (!str)
		return L"";

	int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);

	std::vector<wchar_t> vecBuffer(n + 1);
	wchar_t *pBuffer = vecBuffer.data();

	n = MultiByteToWideChar(CP_UTF8, 0, str, -1, pBuffer, n);
	pBuffer[n] = 0;
	std::wstring ret = pBuffer;
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
} // namespace -------------------------------------------
