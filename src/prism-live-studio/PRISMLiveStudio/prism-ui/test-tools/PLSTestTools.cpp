#include "PLSTestTools.hpp"
#include "PLSNetworkHookTool.hpp"
#include "PLSNetworkMonitorTool.hpp"
#include "PLSErrorCodeTransformTool.h"

#include <frontend-api.h>

void pls_init_test_tools()
{
	if (pls_prism_get_qsetting_value("TestTools").toBool()) {
		obs_frontend_add_tools_menu_item(
			"Network Hook Tool",
			[](void *) {
				if (auto hook = PLSNetworkHookTool::instance(); hook)
					hook->show();
			},
			nullptr);
		obs_frontend_add_tools_menu_item(
			"Network Monitor Tool",
			[](void *) {
				if (auto hook = PLSNetworkMonitorTool::instance(); hook)
					hook->show();
			},
			nullptr);
		obs_frontend_add_tools_menu_item(
			"Error Code Transform Tool",
			[](void *) {
				if (auto view = PLSErrorCodeTransformTool::instance(); view)
					view->show();
			},
			nullptr);
	}
}
