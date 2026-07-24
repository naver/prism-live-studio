#include "moc_window-dock.cpp"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "PLSAlertView.h"
#include "PLSErrorHandler.h"

void OBSDock::closeEvent(QCloseEvent *event)
{
	auto msgBox = []() {
		pls_check_app_exiting();
		auto result = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_DOCK_CLOSE_WARNING,
								    PLSErrKeyAllAlert, {},
								    PLSErrorHandler::ExtraData("OBSDock::closeEvent"),
								    pls_get_main_view());
		if (result.isCheckBoxClick) {
			config_set_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks", true);
			config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks");
	if (!OBSBasic::Get()->Closing() && !pls_get_app_exiting() && !warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}

	PLSDock::closeEvent(event);

	if (widget() && event->isAccepted()) {
		QEvent widgetEvent(QEvent::Type(QEvent::User + QEvent::Close));
		qApp->sendEvent(widget(), &widgetEvent);
	}
}

void OBSDock::showEvent(QShowEvent *event)
{
	setProperty("vis", true);
	PLSDock::showEvent(event);
}

void OBSDockOri::closeEvent(QCloseEvent *event)
{
	auto msgBox = []() {
		pls_check_app_exiting();
		auto result = PLSErrorHandler::showAlertByPrismCode(
			PLSErrorHandler::ALERT_DOCK_CLOSE_WARNING, PLSErrKeyAllAlert, {},
			PLSErrorHandler::ExtraData("OBSDockOri::closeEvent"), pls_get_main_view());
		if (result.isCheckBoxClick) {
			config_set_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks", true);
			config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks");
	if (!OBSBasic::Get()->Closing() && !warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}

	QDockWidget::closeEvent(event);

	if (widget() && event->isAccepted()) {
		QEvent widgetEvent(QEvent::Type(QEvent::User + QEvent::Close));
		qApp->sendEvent(widget(), &widgetEvent);
	}
}

void OBSDockOri::showEvent(QShowEvent *event)
{
	setProperty("vis", true);
	QDockWidget::showEvent(event);
}
