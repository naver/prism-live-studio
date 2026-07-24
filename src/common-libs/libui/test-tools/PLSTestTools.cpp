#include "PLSTestTools.hpp"
#include <QMenu>
#include "PLSNetworkHookTool.hpp"
#include "PLSNetworkMonitorTool.hpp"
#include "PLSPerformanceTool.hpp"

LIBUI_API void pls_init_test_tools(QMenu *menu, const std::function<void(const std::function<void(const char *name, const std::function<void()> &show)> &add_tool)> &extern_tools)
{
	if (pls_get_qsetting_value("TestTools").toBool()) {
		auto toolsMenu = menu->addMenu("Test Tools");
		auto addTool = [toolsMenu](const char *name, const std::function<void()> &show) { toolsMenu->addAction(name, toolsMenu, show); };
		pls_init_test_tools(addTool);
		if (extern_tools)
			extern_tools(addTool);
	}
}
LIBUI_API void pls_init_test_tools(const std::function<void(const char *name, const std::function<void()> &show)> &add_tool)
{
	if (!pls_get_qsetting_value("TestTools").toBool())
		return;

	add_tool("Network Hook Tool", []() {
		if (auto hook = PLSNetworkHookTool::instance(); hook)
			hook->show();
	});
	add_tool("Network Monitor Tool", []() {
		if (auto hook = PLSNetworkMonitorTool::instance(); hook)
			hook->show();
	});
	add_tool("Performance Tool", []() {
		if (auto view = PLSPerformanceTool::instance(); view)
			view->show();
	});
	add_tool("Action Stats",
		 []() { //
			 pls_add_global_field("showActionStats", "", PLS_SET_TAG_CN);
		 });
}
