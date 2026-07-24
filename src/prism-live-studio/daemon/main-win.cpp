#include <libutils-api.h>
#include <pls-shared-values.h>
#include <liblog.h>
#include <stdio.h>
#include <QtCore>
#include <QString>
#include <QSharedMemory>
#include <time.h>
#include <Windows.h>
#include "prism-version.h"
#include "libui.h"

#define DaemonModule "PRISMDaemon"

#define SharedMemoryLength 100
pls_shm_base_t *g_shm = nullptr;
static std::atomic<int> g_maxRetryCount = 2;
static QString g_prismSession;
static bool g_isMemeoryAttached = true;
static int g_prismPID = -1;
static bool g_isParentIsDebuggered = false;
static pls_process_t *g_loadingAppProcess = nullptr;
static bool /*false=not retry, true=retry*/ receiveExitCode(int returnCode, void *info);

static void createAppLoading(QStringList &args, const QString &session, const QString &subSession)
{
	g_loadingAppProcess = pls_create_loading_app(session, subSession);
	auto pid = pls_process_id(g_loadingAppProcess);

	args.append(shared_values::k_launcher_command_loading_app_pid + QString::number(pid));
}
static bool isIgnoreDebugCase()
{
	//ignore debugger force kill prismlivestudio
	if (g_isParentIsDebuggered) {
		bool isSettingIgnoreRepull = pls_get_qsetting_value("IgnoreRepull", false).toBool();
		PLS_INFO(DaemonModule, "Daemon module. QSettings IgnoreRepull:%s", pls_bool_2_string(isSettingIgnoreRepull));
		if (isSettingIgnoreRepull) {
			return true;
		}
	}
	return false;
}

static bool /*false=not retry, true=retried*/ receiveExitCode(int returnCode, void *info = nullptr)
{
	// Record exit time to streaming_state.json for crash recovery
	pls_update_exit_time();

	bool willRepullPrism = pls_is_repull_exit_code(returnCode);
	auto codeReason = pls_get_init_exit_code_str(static_cast<PLSCode::init_exception_code>(returnCode));
	QString hexCodeStr = "0x" + QString::number(returnCode, 16);
	PLS_INFO(DaemonModule, "Daemon module. obs64 current retry count is:%d, returnCode:%d, returnCode_hex:%s codeReason:%s, is will repull:%s", g_maxRetryCount.load(), returnCode,
		 hexCodeStr.toUtf8().constData(), codeReason.toUtf8().constData(), pls_bool_2_string(willRepullPrism));
	if (!willRepullPrism) {
		PLS_INFO(DaemonModule, "Daemon module. ignore repull: obs64 is not re-pull code: codeReason:%s", codeReason.toUtf8().constData());
		return false;
	}
	char buffer[SharedMemoryLength] = {};
	int len = sizeof(buffer);
	pls_shm_base_read(g_shm, buffer, len);
	if (buffer[0] != '\0') { //crah in prism exiting
		PLS_INFO(DaemonModule, "Daemon module. ignore repull: obs64 crash in exiting");
		return false;
	}
	if (!g_isMemeoryAttached) {
		PLS_INFO(DaemonModule, "Daemon module. the sharememory is not attached, So unable to determine whether obs64 is in the exit stage");
	}

	if (g_maxRetryCount <= 0) {
		PLS_INFO(DaemonModule, "Daemon module. ignore repull: obs64 retry count is used up.");
		return false;
	}

	if (isIgnoreDebugCase()) {
		PLS_INFO(DaemonModule, "Daemon module. ignore repull: in debugger case.");
		return false;
	}

	--g_maxRetryCount;
	PLS_INFO(DaemonModule, "Daemon module. retry to run obs64 with limit retry count:%d, currentCodeReason:%s", g_maxRetryCount.load(), codeReason.toUtf8().constData());
	static QString startedSubSessionID;
	startedSubSessionID = QUuid::createUuid().toString();

	QStringList argus;
	argus.append(shared_values::k_launcher_command_type + shared_values::k_launcher_command_daemon);
	argus.append(shared_values::k_launcher_command_log_prism_session + g_prismSession);
	argus.append(shared_values::k_launcher_command_log_sub_prism_session + startedSubSessionID);
	argus.append(shared_values::k_daemon_limit_retry_count + QString::number(g_maxRetryCount).toUtf8().constData()); //first is 1, second is 0;
	//app loading app
	createAppLoading(argus, g_prismSession, startedSubSessionID);
	QString appDir = pls_get_app_dir() + "/" + "obs64.exe";
	auto prismPro = pls_process_create(appDir, argus, "", true);
	PLS_INFO(DaemonModule, "Daemon module. start obs64 succeed:%d, reason:%d subSessionID:%s", !!prismPro, pls_last_error(), startedSubSessionID.toUtf8().constData());
	if (!prismPro) {
		return false;
	}
	g_prismPID = pls_process_id(prismPro);
	pls_process_destroy(prismPro);
	return true;
}

static void startRunWork()
{
	while (true) {
		PLS_INFO(DaemonModule, "Daemon module. start in loop for pid:%d", g_prismPID);
		pls_process_t *ppt = pls_process_create(g_prismPID);
		if (!ppt) {
			PLS_INFO(DaemonModule, "Daemon module. quit with pls_process_t crate failed by pid:%d", g_prismPID);
			return;
		}
		PLS_INFO(DaemonModule, "Daemon module. process ptr get succeed, pid:%d. wait prism exit...", g_prismPID);

		pls_process_wait(ppt);
		pls_destory_loading_app(g_loadingAppProcess);
		uint32_t exitCode = pls_process_exit_code(ppt);
		pls_process_destroy(ppt);
		if (!receiveExitCode(exitCode)) { //not retry or retry failed
			return;
		}
	}
}

int main(int argc, char **argv)
{
	//::ShowWindow(::GetConsoleWindow(), SW_HIDE);
	pls_set_cmdline_args(argc, argv);
	auto cmdline_args = pls_cmdline_args();
	auto startTime = (double)clock();
	QString prismVersion = PLS_VERSION;
	QString prismSubSession;
	uint restartType = -1;
	g_isParentIsDebuggered = pls_is_debugger_present();
	// Get command line arguments
	for (int i = 0; i < cmdline_args.size(); ++i) {
		QString ar = cmdline_args[i];
		PLS_DEBUG(DaemonModule, "Daemon module. get argv[%d]: %s", i, ar.toUtf8().constData());
		if (ar.startsWith(shared_values::k_launcher_command_log_prism_session)) {
			g_prismSession = ar.remove(0, QString(shared_values::k_launcher_command_log_prism_session).size());
		} else if (ar.startsWith(shared_values::k_launcher_command_log_sub_prism_session)) {
			prismSubSession = ar.remove(0, shared_values::k_launcher_command_log_sub_prism_session.size());
		} else if (ar.startsWith(shared_values::k_launcher_prism_version)) {
			prismVersion = ar.remove(0, shared_values::k_launcher_prism_version.size());
		} else if (ar.startsWith(shared_values::k_launcher_command_type)) {
			restartType = ar.remove(0, shared_values::k_launcher_command_type.size()).toUInt();
		}
	}
	if (g_prismSession.isEmpty()) {
		g_prismSession = QUuid::createUuid().toString();
		cmdline_args.append(shared_values::k_launcher_command_log_prism_session + g_prismSession);
	}
	if (prismSubSession.isEmpty()) {
		prismSubSession = QUuid::createUuid().toString();
		cmdline_args.append(shared_values::k_launcher_command_log_sub_prism_session + prismSubSession);
	}
	cmdline_args.append(shared_values::k_launcher_command_type + shared_values::k_launcher_command_daemon);

	const wchar_t *eventName = L"PRISMLiveStudio";
	HANDLE hEvent = OpenEventW(EVENT_ALL_ACCESS, false, eventName);
	if (!hEvent && restartType != static_cast<uint>(RestartAppType::Update)) {
		createAppLoading(cmdline_args, g_prismSession, prismSubSession);
	} else {
		CloseHandle(hEvent);
	}
	// init the log
	pls_prism_log_init(prismVersion.toUtf8().constData(), "prism-daemon", g_prismSession.toUtf8().constData(), prismSubSession.toUtf8().constData());
	pls_add_global_field("prismSession", g_prismSession.toUtf8().constData());
	pls_add_global_field("prismSubSession", prismSubSession.toUtf8().constData());
	pls_add_global_field("OSType", pls_is_os_sys_macos() ? "MAC" : "Windows");

	auto logInitTime = (double)clock();
	PLS_INFO(DaemonModule, "Daemon module. enter main method. start argc count: %d, logInitTime:%.3fs", argc, (logInitTime - startTime) / 1000);
	PLS_INFO(DaemonModule, "Daemon module. get argv. prismsession is:%s. version:%s", g_prismSession.toUtf8().constData(), prismVersion.toUtf8().constData());

	QString shmName = QStringLiteral("PRISMDaemonShm_%1").arg(prismSubSession);
	g_shm = pls_shm_base_create(shmName, SharedMemoryLength);

	if (!g_shm) {
		PLS_INFO(DaemonModule, "Daemon module. Failed to create shared memory");
		g_isMemeoryAttached = false;
	}
	QString appDir = pls_get_app_dir() + "/" + "obs64.exe";
	auto prismPro = pls_process_create(appDir, cmdline_args, "", true);
	PLS_INFO(DaemonModule, "Daemon module. start obs64 succeed:%d, reason:%d subSessionID:%s", !!prismPro, pls_last_error(), prismSubSession.toUtf8().constData());
	if (!prismPro) {
		goto exit;
	}
	g_prismPID = pls_process_id(prismPro);
	pls_process_destroy(prismPro);

	startRunWork();

exit:
	PLS_INFO(DaemonModule, "Daemon module. byebye!");
	if (g_shm) {
		pls_delete(g_shm, pls_shm_base_destroy, nullptr);
	}
	pls_log_cleanup();
	return 0;
}
