#pragma once
#include <string>
#include <tchar.h>

//------------------------------------------------------------------------------
#define _CRT_NON_CONFORMING_SWPRINTFS

#ifdef UNICODE
#define _tstring wstring
#define _ttoupper towupper
#define _ttolower tolower
#else
#define _tstring string
#define _ttoupper toupper
#define _ttolower towlower
#endif

#define U2T(s) str::u2T(s.c_str()).c_str()
#define U2W(s) str::u2w(s.c_str()).c_str()
#define A2W(s) str::a2w(s.c_str()).c_str()
#define W2U(s) str::w2u(s.c_str()).c_str()

namespace str {
std::_tstring a2T(const char *str);
std::wstring a2w(const char *str);
std::string a2u(const char *str);

std::_tstring w2T(const wchar_t *str);
std::string w2a(const wchar_t *str);
std::string w2u(const wchar_t *str);

std::wstring u2w(const char *str);
std::_tstring u2T(const char *str);
std::string u2a(const char *str);

std::string T2a(const TCHAR *str);
std::string T2u(const TCHAR *str);
std::wstring T2w(const TCHAR *str);
}
