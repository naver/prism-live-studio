#ifndef PLSTESTTOOLS_HPP
#define PLSTESTTOOLS_HPP

#include <functional>

#include "libui-globals.h"

class QMenu;
LIBUI_API void pls_init_test_tools(QMenu *menu, const std::function<void(const std::function<void(const char *name, const std::function<void()> &show)> &add_tool)> &extern_tools = nullptr);
LIBUI_API void pls_init_test_tools(const std::function<void(const char *name, const std::function<void()> &show)> &add_tool);

#endif // PLSTESTTOOLS_HPP
