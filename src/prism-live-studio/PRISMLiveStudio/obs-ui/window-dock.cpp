#include "window-dock.hpp"
#include "obs-app.hpp"

#include "PLSAlertView.h"

void OBSDock::closeEvent(QCloseEvent *event)
{
	auto msgBox = []() {
		pls_check_app_exiting();
		auto result = PLSAlertView::information(
			pls_get_main_view(), QTStr("DockCloseWarning.Title"),
			QTStr("DockCloseWarning.Text"), QTStr("DoNotShowAgain"),
			PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
		if (result.isChecked) {
			config_set_bool(App()->GlobalConfig(), "General",
					"WarnedAboutClosingDocks", true);
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GlobalConfig(), "General",
				      "WarnedAboutClosingDocks");
	if (!warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection,
					  Q_ARG(VoidFunc, msgBox));
	}

	QDockWidget::closeEvent(event);
}
