#include "PLSHookKeyboard.h"
#include <Windows.h>
#include <strsafe.h>
#include <vector>
#include <liblog.h>
#include <QMutex>

struct LocalGlobalVars {
	static QMutex mute;
	static std::vector<pls_hook_callback_data> callbacks;
	static HHOOK g_hook_handler;
};

QMutex LocalGlobalVars::mute;
std::vector<pls_hook_callback_data> LocalGlobalVars::callbacks;
HHOOK LocalGlobalVars::g_hook_handler = nullptr;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0) // do not process message
		return CallNextHookEx(LocalGlobalVars::g_hook_handler, nCode, wParam, lParam);
	if (wParam == WM_KEYDOWN) {
		auto hookStruct = *((KBDLLHOOKSTRUCT *)lParam);
		if (hookStruct.vkCode == VK_ESCAPE) {
			QMutexLocker locker(&LocalGlobalVars::mute);
			for (const auto call_data : LocalGlobalVars::callbacks) {
				if (call_data.callback)
					call_data.callback(KeyMask::Key_escape, call_data.param);
			}
		}
	}

	return CallNextHookEx(LocalGlobalVars::g_hook_handler, nCode, wParam, lParam);
}

bool pls_init_keyboard_hook()
{
	if (nullptr == LocalGlobalVars::g_hook_handler) {
		LocalGlobalVars::g_hook_handler = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, (HINSTANCE) nullptr, 0);
		if (!LocalGlobalVars::g_hook_handler) {
			auto error_code = GetLastError();
			PLS_WARN("Hook keyborad", "Failed when SetWindowsHookEx, error code: %u", error_code);
		}
	}

	return (LocalGlobalVars::g_hook_handler != nullptr);
}

bool pls_stop_keyboard_hook()
{
	if (nullptr != LocalGlobalVars::g_hook_handler)
		return !!UnhookWindowsHookEx(LocalGlobalVars::g_hook_handler);
	return true;
}

void pls_register_keydown_hook_callback(pls_hook_callback_data call_data)
{
	QMutexLocker locker(&LocalGlobalVars::mute);
	LocalGlobalVars::callbacks.emplace_back(call_data);
}

void pls_unregister_keydown_hook_callback(pls_hook_keyboard_callback callback, const void *param)
{
	QMutexLocker locker(&LocalGlobalVars::mute);
	auto iter = std::find_if(LocalGlobalVars::callbacks.begin(), LocalGlobalVars::callbacks.end(),
				 [callback, param](pls_hook_callback_data item) { return (item.callback == callback || item.param == param); });

	if (iter != LocalGlobalVars::callbacks.end())
		LocalGlobalVars::callbacks.erase(iter);
}
