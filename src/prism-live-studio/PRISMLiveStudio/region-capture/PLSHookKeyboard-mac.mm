#include "PLSHookKeyboard.h"
#include <AppKit/AppKit.h>
#include <vector>

std::vector<pls_hook_callback_data> callbacks;

bool pls_init_keyboard_hook(){
//    [NSEvent addGlobalMonitorForEventsMatchingMask:NSRightMouseDownMask handler:^(NSEvent * mouseEvent) {
//        [self keyboardDown:mouseEvent];
//    }];
}

bool pls_stop_keyboard_hook(){
    
}

void pls_register_keydown_hook_callback(pls_hook_callback_data call_data){
    callbacks.emplace_back(call_data);
}
void pls_unregister_keydown_hook_callback(pls_hook_keyboard_callback callback, void *param){
    for (auto iter = callbacks.begin(); iter < callbacks.end(); iter++) {
        if ((*iter).callback == callback || (*iter).param == param) {
            callbacks.erase(iter);
            break;
        }
    }
}
