#include <libutils-api.h>
#include <pls-shared-values.h>
#include <liblog.h>
#include <stdio.h>
#include <QtCore>
#include <QString>
#include <QSharedMemory>
#include <time.h>
#include "PLSAppLoadingView.h"
#include <QApplication>
#include "pls-common-define.hpp"
#include "PLSDumpAnalyzer.h"

#define SharedMemoryLength 100
#define LOG_MODULE "apploading"
static QString g_parent_pid;
static pls_process_t *g_parent_process = nullptr;
static QString g_prismSession;
static QString g_prismSubSession;
static QString g_prismVersion;

void processWaitingForExit()
{
	g_parent_process = pls_process_create(g_parent_pid.toUInt());
	static QTimer t;
	QObject::connect(&t, &QTimer::timeout, []() {
		if (g_parent_process && (pls_process_wait(g_parent_process, 0) > 0)) {
			PLS_INFO("loading-view", "RPSIMLiveStudio or PRISMLens exits abnormally.");
			t.stop();
			PLSAppLoadingView::instance()->close();
			return;
		}
	});
	t.start(10);
}
int main(int argc, char **argv)
{
	if (argc < 2)
		return 0;
	pls_set_cmdline_args(argc, argv);
	auto cmdline_args = pls_cmdline_args();
	QString locale("en-US");
	for (auto cmdline : cmdline_args) {
		if (cmdline.startsWith(shared_values::k_launcher_command_locale)) {
			locale = cmdline.remove(0, shared_values::k_launcher_command_locale.size());
		} else if (cmdline.startsWith(shared_values::k_launcher_command_prism_pid)) {
			g_parent_pid = cmdline.remove(0, shared_values::k_launcher_command_prism_pid.size());
		} else if (cmdline.startsWith(shared_values::k_launcher_command_log_prism_session)) {
			g_prismSession = cmdline.remove(0, shared_values::k_launcher_command_log_prism_session.size());
		} else if (cmdline.startsWith(shared_values::k_launcher_command_log_sub_prism_session)) {
			g_prismSubSession = cmdline.remove(0, shared_values::k_launcher_command_log_sub_prism_session.size());
		} else if (cmdline.startsWith(shared_values::k_launcher_prism_version)) {
			g_prismVersion = cmdline.remove(0, shared_values::k_launcher_prism_version.size());
		}
	}
	auto prismSession = g_prismSession.toUtf8();
	auto prismSubSession = g_prismSubSession.toUtf8();
	pls_prism_log_init(g_prismVersion.toUtf8().constData(), "prism-app-loading", prismSession.constData(), prismSubSession.constData());
	pls_add_global_field("prismSession", prismSession.constData());
	pls_add_global_field("prismSubSession", prismSubSession.constData());
	pls_add_global_field("OSType", pls_is_os_sys_macos() ? "MAC" : "Windows");
	PLS_INFO("loading-view", "init log session = %s,subSession = %s, os = %s", prismSession.constData(), prismSubSession.constData(), pls_is_os_sys_macos() ? "MAC" : "Windows");
	auto processName = pls_get_app_pn().toStdString();
	pls_catch_unhandled_exceptions(processName);
	auto translator = pls_load_language_translator(":/data/locale", locale);
	QApplication a(argc, argv);
	a.installTranslator(translator);
	QSettings settings(pls_get_app_user_data_file_path_pn("/global.ini"), QSettings::IniFormat);
	auto parentGeometry = settings.value("BasicWindow/geometry").toString();
	PLSAppLoadingView::instance()->setPosition(parentGeometry);
	PLSAppLoadingView::instance()->show();
	QApplication::setQuitOnLastWindowClosed(true);
#if defined(Q_OS_MACOS)
	processWaitingForExit();
#endif
	auto result = a.exec();
	pls_log_cleanup();
	return result;
}
