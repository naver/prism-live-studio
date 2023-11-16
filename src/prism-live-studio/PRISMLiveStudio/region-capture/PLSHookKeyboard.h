#pragma once

enum class KeyMask {
	Key_escape,
};

using pls_hook_keyboard_callback = void (*)(KeyMask mask, void *param);

struct pls_hook_callback_data {
	pls_hook_keyboard_callback callback;
	void *param;
};

bool pls_init_keyboard_hook();
bool pls_stop_keyboard_hook();

void pls_register_keydown_hook_callback(pls_hook_callback_data call_data);
void pls_unregister_keydown_hook_callback(pls_hook_keyboard_callback callback, const void *param);