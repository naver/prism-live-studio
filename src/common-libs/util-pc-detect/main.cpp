#include <Windows.h>
#include <string>
#include <thread>
#include <memory>
#include <assert.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <string>
#include <format>
#include <vector>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "Shlwapi.lib")

static const auto WINDOW_CLASS_NAME = L"prism_detect_pc_state_win";

FILE *fpWriter = nullptr;
HANDLE state_handle = nullptr;
std::string timeZone = "";

bool sessionSaved = false;
std::string session = "";
std::string startTime = "";

enum class EXIT_CODE {
	NORMAL = 0,
	INVALID_PARAMS,
	FAILED_OPEN_FILE,
	FAILED_CREATE_WINDOW,
	CRASHED,
};

LONG WINAPI CrashProc(struct _EXCEPTION_POINTERS *pExceptionPointers);
LRESULT __stdcall WndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);

BOOL CustomRegister(LPCWSTR pszClassName);
HWND CustomCreate(LPCWSTR pszClassName);

void InitTimeZone();
std::string GetFormatTime();

std::string UnocodeToUTF8(const wchar_t *str);

HANDLE GetStateEvent(const wchar_t *name);

/*
command list:
[0] full path of exe
[1] the path of txt where to write state information
[2] prism session string
[3] event name for pc state
*/
int wmain(int argc, wchar_t *argv[])
{
	SetUnhandledExceptionFilter(CrashProc);
	InitTimeZone();

	if (argc != 4 || !argv[1] || !argv[2] || !argv[3]) {
		assert(false);
		return (int)EXIT_CODE::INVALID_PARAMS;
	}

	_wfopen_s(&fpWriter, argv[1], L"wb+");
	if (!fpWriter) {
		assert(false);
		return (int)EXIT_CODE::FAILED_OPEN_FILE;
	}

	state_handle = GetStateEvent(argv[3]);

	std::shared_ptr<int> autoClose(nullptr, [](int *) {
		if (fpWriter) {
			fclose(fpWriter);
		}
		if (state_handle) {
			CloseHandle(state_handle);
		}
	});

	session = UnocodeToUTF8(argv[2]);
	startTime = GetFormatTime();

	HWND hWnd = CustomCreate(WINDOW_CLASS_NAME);
	if (!IsWindow(hWnd)) {
		assert(false);
		return (int)EXIT_CODE::FAILED_CREATE_WINDOW;
	}

	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass(WINDOW_CLASS_NAME, GetModuleHandle(NULL));
	return (int)EXIT_CODE::NORMAL;
}

LONG WINAPI CrashProc(struct _EXCEPTION_POINTERS *pExceptionPointers)
{
	assert(false);
	TerminateProcess(GetCurrentProcess(), (UINT)EXIT_CODE::CRASHED);
	return EXCEPTION_EXECUTE_HANDLER;
}

void WriteHeader()
{
	if (fpWriter && !sessionSaved) {
		sessionSaved = true;
		fprintf(fpWriter, "\t %s prism session started: %s \n", startTime.c_str(), session.c_str());
	}
}

LRESULT __stdcall WndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch (nMsg) {
	case WM_DESTROY: {
		PostQuitMessage(0); // exit message loop
		break;
	}

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}

	case WM_QUERYENDSESSION: {
		if (state_handle)
			SetEvent(state_handle);

		if (fpWriter) {
			WriteHeader();

			fprintf(fpWriter, "\t %s WM_QUERYENDSESSION arrived \n", GetFormatTime().c_str());
			fflush(fpWriter);
		}

		break;
	}

	case WM_ENDSESSION: {
		if (state_handle)
			SetEvent(state_handle);

		if (fpWriter) {
			WriteHeader();

			fprintf(fpWriter, "\t %s WM_ENDSESSION arrived, ending:%s \n", GetFormatTime().c_str(), wParam ? "YES" : "NO");
			fflush(fpWriter);
		}

		break;
	}

	default:
		break;
	}

	return DefWindowProc(hWnd, nMsg, wParam, lParam);
}

BOOL CustomRegister(LPCWSTR pszClassName)
{
	WNDCLASS wc = {0};
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = pszClassName;

	ATOM ret = RegisterClass(&wc);
	if (0 == ret && ERROR_CLASS_ALREADY_EXISTS != GetLastError()) {
		assert(false);
		return FALSE;
	}

	return TRUE;
}

HWND CustomCreate(LPCWSTR pszClassName)
{
	if (!CustomRegister(pszClassName))
		return 0;

	HWND hWnd = CreateWindow(pszClassName, pszClassName, WS_POPUPWINDOW | WS_EX_TOOLWINDOW, 0, 0, 32, 32, NULL, NULL, GetModuleHandle(NULL), NULL);
	if (!IsWindow(hWnd)) {
		UnregisterClass(WINDOW_CLASS_NAME, GetModuleHandle(NULL));
		return 0;
	}

	return hWnd;
}

void InitTimeZone()
{
	TIME_ZONE_INFORMATION tmp;
	::GetTimeZoneInformation(&tmp);

	timeZone = std::to_string(tmp.Bias / (-60));
	assert(!timeZone.empty() && "failed to get time zone");
}

std::string GetFormatTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	auto str = std::format("{}-{}-{} {}:{}:{}.{}+{}H", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, timeZone.c_str());
	return str;
}

std::string UnocodeToUTF8(const wchar_t *str)
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

HANDLE GetStateEvent(const wchar_t *name)
{
	auto handle = OpenEventW(EVENT_ALL_ACCESS, TRUE, name);
	if (handle)
		return handle;

	handle = CreateEventW(nullptr, TRUE, FALSE, name);
	return handle;
}
