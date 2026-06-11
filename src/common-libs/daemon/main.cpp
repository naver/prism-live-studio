#include <libutils-api.h>
#include <pls-shared-values.h>
#include <liblog.h>
#include <stdio.h>
#include <QtCore>
#include <QString>
#include <QSharedMemory>
#include <time.h>

#ifdef __APPLE__
#include <sys/event.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/errno.h>

static void notifyExitKQueueCallback(CFFileDescriptorRef f, CFOptionFlags callBackTypes, void *info);
static CFFileDescriptorRef g_noteExitKQueueRef;
#endif

#define DeamandMoudle "PRISMDaemon"

QSharedMemory g_objSharedMemory(shared_values::k_daemon_sm_key);
static std::atomic<int> g_maxRetryCount = 2;
static QString g_prismSession;
static bool g_isMemeoryAttached = true;
static int g_prismPID = -1;
static bool g_isParentIsDebuggered = false;

static bool /*false=not retry, true=retry*/ receiveExitCode(int returnCode, void *info);

static void trySetMemory(const QString &setStr)
{
	QBuffer buffer;
	buffer.open(QBuffer::ReadWrite);
	QDataStream out(&buffer);
	QString fileName;
	out << setStr;

	g_objSharedMemory.lock();
	auto to = g_objSharedMemory.data();
	const char *from = buffer.data().data();
	memcpy(to, from, qMin(static_cast<qint64>(g_objSharedMemory.size()), buffer.size()));
	g_objSharedMemory.unlock();
	buffer.close();
}
static QString tryGetMemory()
{
	QBuffer bufferA;
	QDataStream in(&bufferA);
	QString text;
	g_objSharedMemory.lock();
	bufferA.setData(static_cast<const char *>(g_objSharedMemory.constData()), (int)g_objSharedMemory.size());
	bufferA.open(QBuffer::ReadOnly);
	in >> text;

	g_objSharedMemory.unlock();

	bufferA.close();
	return text;
}

#ifdef __APPLE__
static bool startNotifyPrismExit(pid_t targetPID)
{
	int kq;
	struct kevent changes;
	CFFileDescriptorContext context = {0, nullptr, NULL, NULL, NULL};
	CFRunLoopSourceRef rls;

	kq = kqueue();
	if (kq == -1) {
		PLS_WARN(DeamandMoudle, "Daemon moudle. startNotifyPrismExit kqueue crate failed! error: %s(errno: %d)", strerror(errno), errno);
		return false;
	}
	EV_SET(&changes, targetPID, EVFILT_PROC, EV_ADD | EV_RECEIPT, NOTE_EXITSTATUS, 0, NULL);
	/* Attach event to the kqueue. */
	int ret = kevent(kq, &changes, 1, &changes, 1, NULL);
	if (ret == -1) {
		PLS_WARN(DeamandMoudle, "Daemon moudle. startNotifyPrismExit kevent crate failed! error: %s(errno: %d)", strerror(errno), errno);
		return false;
	}
	PLS_INFO(DeamandMoudle, "Daemon moudle. startNotifyPrismExit notify pid:%d. wait prism exit...", targetPID);
	if (g_noteExitKQueueRef) {
		CFFileDescriptorIsValid(g_noteExitKQueueRef);
		CFRelease(g_noteExitKQueueRef);
		g_noteExitKQueueRef = nullptr;
	}

	g_noteExitKQueueRef = CFFileDescriptorCreate(NULL, kq, true, notifyExitKQueueCallback, &context);
	rls = CFFileDescriptorCreateRunLoopSource(NULL, g_noteExitKQueueRef, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRelease(rls);

	CFFileDescriptorEnableCallBacks(g_noteExitKQueueRef, kCFFileDescriptorReadCallBack);

	return true;
}
static void stopRunloop()
{
	PLS_INFO(DeamandMoudle, "Daemon moudle. stop runloop");
	CFRunLoopStop(CFRunLoopGetCurrent());
}
static void notifyExitKQueueCallback(CFFileDescriptorRef f, CFOptionFlags callBackTypes, void *info)
{
	struct kevent event;

	(void)kevent(CFFileDescriptorGetNativeDescriptor(f), NULL, 0, &event, 1, NULL);
	int returnCode = (pid_t)event.data;
	PLS_INFO(DeamandMoudle, "Daemon moudle. kqueue callbacked. notify pid:%i, return code: %i", (int)(pid_t)event.ident, (int)(pid_t)event.data);
	if (receiveExitCode(returnCode, info)) {
		return;
	}
	stopRunloop();
}
#endif //#ifdef __APPLE__

static bool isIgnoreDebugCase()
{
	//ignore debugger force kill prismlivestudio
	if (g_isParentIsDebuggered) {
		bool isSettingIgnoreRepull = pls_prism_get_qsetting_value("IgnoreRepull", false).toBool();
		PLS_INFO(DeamandMoudle, "Daemon moudle. QSettings IgnoreRepull:%s", pls_bool_2_string(isSettingIgnoreRepull));
		if (isSettingIgnoreRepull) {
			return true;
		}
	}
	return false;
}

static bool /*false=not retry, true=retried*/ receiveExitCode(int returnCode, void *info = nullptr)
{
	bool willRepullPrism = pls_is_repull_exit_code(returnCode);
	auto codeReason = pls_get_init_exit_code_str(static_cast<PLSCode::init_exception_code>(returnCode));
	QString hexCodeStr = "0x" + QString::number(returnCode, 16);
	PLS_INFO(DeamandMoudle, "Daemon moudle. prism current retry count is:%d, returnCode:%d, returnCode_hex:%s codeReason:%s, is will repull:%s", g_maxRetryCount.load(), returnCode,
		 hexCodeStr.toUtf8().constData(), codeReason.toUtf8().constData(), pls_bool_2_string(willRepullPrism));
	if (!willRepullPrism) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. ignore repull: prism is not re-pull code: codeReason:%s", codeReason.toUtf8().constData());
		return false;
	}

	if (!tryGetMemory().isEmpty()) { //crah in prism exiting
		trySetMemory("");
		PLS_INFO(DeamandMoudle, "Daemon moudle. ignore repull: prism crash in exiting");
		return false;
	}
	if (!g_isMemeoryAttached) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. the sharememory is not attached, So unable to determine whether prism is in the exit stage");
	}

	if (g_maxRetryCount <= 0) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. ignore repull: prism retry count is used up.");
		return false;
	}

	if (isIgnoreDebugCase()) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. ignore repull: in debugger case.");
		return false;
	}

	--g_maxRetryCount;
	PLS_INFO(DeamandMoudle, "Daemon moudle. retry to run prism with limit retry count:%d, currentCodeReason:%s", g_maxRetryCount.load(), codeReason.toUtf8().constData());
	static QString startedSubSessionID;
	startedSubSessionID = QUuid::createUuid().toString();

	QStringList argus;
	argus.append(shared_values::k_launcher_command_type + shared_values::k_launcher_command_daemon);
	argus.append(shared_values::k_launcher_command_log_prism_session + g_prismSession);
	argus.append(shared_values::k_launcher_command_log_sub_prism_session + startedSubSessionID);
	argus.append(shared_values::k_daemon_limit_retry_count + QString::number(g_maxRetryCount).toUtf8().constData()); //first is 1, second is 0;

#ifdef __APPLE__
	pls_libutil_api_mac::pls_mac_create_process_with_not_inherit(pls_get_app_dir() + "/../../", argus, info, [](void *, bool isSucceed, int pid) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. start prism succeed:%d pid:%d subSessionID:%s", isSucceed, pid, startedSubSessionID.toUtf8().constData());
		g_prismPID = pid;
		if (!isSucceed || !startNotifyPrismExit(pid)) {
			stopRunloop();
		}
	});
#else
	QString appDir = pls_get_app_dir() + "/" + "PRISMLiveStudio.exe";
	auto prismPro = pls_process_create(appDir, argus, "", true);
	PLS_INFO(DeamandMoudle, "Daemon moudle. start prism succeed:%d, reason:%d subSessionID:%s", !!prismPro, pls_last_error(), startedSubSessionID.toUtf8().constData());
	if (!prismPro) {
		return false;
	}
	g_prismPID = pls_process_id(prismPro);
#endif
	return true;
}

static void startRunWork()
{
#ifdef __APPLE__
	if (startNotifyPrismExit(g_prismPID)) {
		CFRunLoopRun();
	}
#else
	while (true) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. start in loop for pid:%d", g_prismPID);
		pls_process_t *ppt = pls_process_create(g_prismPID);
		if (!ppt) {
			PLS_INFO(DeamandMoudle, "Daemon moudle. quit with pls_process_t crate failed by pid:%d", g_prismPID);
			return;
		}
		PLS_INFO(DeamandMoudle, "Daemon moudle. process ptr get succeed, pid:%d. wait prism exit...", g_prismPID);

		pls_process_wait(ppt);
		uint32_t exitCode = pls_process_exit_code(ppt);
		pls_process_destroy(ppt);
		if (!receiveExitCode(exitCode)) { //not retry or retry failed
			return;
		}
	}
#endif
}

int main(int argc, char **argv)
{
	auto startTime = (double)clock();
	QString prismGcc;
	QString prismVersion;
	QString prismSubSession;
	for (int i = 1; i < argc; i++) {
		QString ar = argv[i];
		if (ar.startsWith(shared_values::k_launcher_command_log_prism_session)) {
			g_prismSession = ar.remove(0, QString(shared_values::k_launcher_command_log_prism_session).size());
		} else if (ar.startsWith(shared_values::k_launcher_command_log_sub_prism_session)) {
			prismSubSession = ar.remove(0, shared_values::k_launcher_command_log_sub_prism_session.size());
		} else if (ar.startsWith(shared_values::k_launcher_command_update_gcc)) {
			prismGcc = ar.remove(0, shared_values::k_launcher_command_update_gcc.size());
		} else if (ar.startsWith(shared_values::k_launcher_command_prism_pid)) {
			g_prismPID = ar.remove(0, shared_values::k_launcher_command_prism_pid.size()).toInt();
		} else if (ar.startsWith(shared_values::k_launcher_prism_version)) {
			prismVersion = ar.remove(0, shared_values::k_launcher_prism_version.size());
		} else if (ar == shared_values::k_daemon_parent_is_debugger) {
			g_isParentIsDebuggered = true;
		}
	}
	// init the log
	pls_prism_log_init(prismVersion.toUtf8().constData(), "prism-daemon", g_prismSession.toUtf8().constData());
	pls_add_global_field("prismSession", g_prismSession.toUtf8().constData());
	pls_add_global_field("prismSubSession", prismSubSession.toUtf8().constData());
	pls_add_global_field("OSType", pls_is_os_sys_macos() ? "MAC" : "Windows");
	pls_add_global_field("gcc", prismGcc.toUtf8().constData());

	auto logInitTime = (double)clock();
	PLS_INFO(DeamandMoudle, "Daemon moudle. enter main method. start argc count: %d, logInitTime:%.3fs", argc, (logInitTime - startTime) / 1000);
	PLS_INFO(DeamandMoudle, "Daemon moudle. get argv. prismsession is:%s. version:%s gcc:%s prismPID:%d", g_prismSession.toUtf8().constData(), prismVersion.toUtf8().constData(),
		 prismGcc.toUtf8().constData(), g_prismPID);

	if (!pls_is_valid_process_id(g_prismPID)) {
		PLS_ERROR(DeamandMoudle, "Daemon moudle. the pid is invalid, exit daemon process. pid:%d", g_prismPID);
		goto exit;
	}

	if (!g_objSharedMemory.create(42)) {
		PLS_INFO(DeamandMoudle, "Daemon moudle. Failed to create shared memory. reason:%s", g_objSharedMemory.errorString().toUtf8().constData());
	}
	if (!g_objSharedMemory.isAttached()) {
		if (!g_objSharedMemory.attach()) {
			g_isMemeoryAttached = false;
			PLS_WARN(DeamandMoudle, "Daemon moudle. Shared memory attatch failed. reason:%s", g_objSharedMemory.errorString().toUtf8().constData());
		}
	}
	startRunWork();

exit:
	PLS_INFO(DeamandMoudle, "Daemon moudle. byebye!");
	if (g_isMemeoryAttached) {
		g_objSharedMemory.detach();
	}
#ifdef __APPLE__
	if (g_noteExitKQueueRef) {
		CFFileDescriptorIsValid(g_noteExitKQueueRef);
		CFRelease(g_noteExitKQueueRef);
		g_noteExitKQueueRef = nullptr;
	}
#endif
	pls_log_cleanup();
	return 0;
}
