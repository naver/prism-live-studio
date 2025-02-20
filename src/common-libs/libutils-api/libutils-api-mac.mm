#include "libutils-api-mac.h"

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/errno.h>
#include <sysexits.h>

#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/task_info.h>
#include <mach/task.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include <Foundation/Foundation.h>
#include <Foundation/NSProcessInfo.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#import <AppKit/AppKit.h>
#include "libutils-api.h"
#include "PLSMacFunction.h"
//#include "NMacManager.h"
#include "libutils-api-log.h"
#include "CrossProcessNotification.h"
#include <assert.h>

#define PROCESS_NAME_SIZE 256
#define NSApplicationRelaunchDaemon @"PRISMRelaunch"
#define kPrismLensBundleID "com.prismlive.camstudio"
#define kLensHasRunKey "kLensHasRunKey"

#if TARGET_RT_BIG_ENDIAN
const NSStringEncoding kEncoding_wchar_t = CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingUTF32BE);
#else
const NSStringEncoding kEncoding_wchar_t = CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingUTF32LE);
#endif

static int process_exit_kq;

@implementation NSString (Shared)

/// - Returns: error info
- (NSString *)runCommandAndReturnErrorWithAdminPrivilege:(BOOL)admin
{
	NSString *scriptText = NULL;
	if (admin) {
		scriptText = [NSString stringWithFormat:@"do shell script \"%@\" with administrator privileges", self];
	} else {
		scriptText = [NSString stringWithFormat:@"do shell script \"%@\"", self];
	}

	NSAppleScript *script = [[NSAppleScript alloc] initWithSource:scriptText];
	NSDictionary *errorDict = NULL;

	NSAppleEventDescriptor *result = [script executeAndReturnError:&errorDict];
	if (result) { // success
		return NULL;
	} else {
		return [NSString stringWithFormat:@"Failed to install %@, error is %@", self, errorDict];
	}
}

@end

namespace pls_libutil_api_mac {

QString pls_get_app_executable_dir()
{
	NSString *executablePath = [PLSMacFunction get_app_executable_dir];
	return QString([executablePath UTF8String]);
}

QString pls_get_app_resource_dir()
{
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	return QString([resourcePath UTF8String]);
}

QString pls_get_mac_app_data_dir()
{
	NSArray *path = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	NSString *directory = [path objectAtIndex:0];
	return QString([directory UTF8String]);
}

QString pls_get_app_content_dir()
{
	NSString *contentPath = [[NSBundle mainBundle] bundlePath];
	return QString([contentPath UTF8String]);
}

std::string pls_get_cpu_name()
{
	char *name = NULL;
	size_t size;
	int ret;

	ret = sysctlbyname("machdep.cpu.brand_string", NULL, &size, NULL, 0);
	if (ret != 0)
		return "";

	name = (char *)malloc(size);

	ret = sysctlbyname("machdep.cpu.brand_string", name, &size, NULL, 0);
	std::string name_str(name);
	free(name);
	return name_str;
}

QString pls_get_app_plugin_dir()
{
	NSString *pluginDir = [[NSBundle mainBundle] builtInPlugInsPath];
	return QString([pluginDir UTF8String]);
}

QString pls_get_app_macos_dir()
{
	NSString *executablePath = [[NSBundle mainBundle] executablePath];
	return QString([executablePath UTF8String]);
}

QString pls_get_bundle_dir()
{
	NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
	return QString([bundlePath UTF8String]);
}

bool pls_is_install_app(const QString &identifier, QString &appPath)
{
	NSWorkspace *workSpace = [NSWorkspace sharedWorkspace];
	NSURL *appURL = [workSpace URLForApplicationWithBundleIdentifier:getNSStringFromQString(identifier)];
	appPath = QString([appURL.path UTF8String]);
	if (appURL) {
		return true;
	}
	return false;
}

void pls_get_install_app_list(const QString &identifier, QStringList &appList)
{
	NSWorkspace *workSpace = [NSWorkspace sharedWorkspace];
	NSString *bundleIdentifier = getNSStringFromQString(identifier);
	if (@available(macOS 12.0, *)) {
		NSArray<NSURL *> *appURLs = [workSpace URLsForApplicationsWithBundleIdentifier:bundleIdentifier];
		for (int i = 0; i < appURLs.count; i++) {
			NSURL *appURL = appURLs[i];
			QString appPath = QString([appURL.path UTF8String]);
			appList.append(appPath);
		}
	} else {
		CFArrayRef appURLs = LSCopyApplicationURLsForBundleIdentifier((__bridge CFStringRef)bundleIdentifier, NULL);
		NSArray *appURLs_oc = (__bridge_transfer NSArray *)appURLs;
		for (int i = 0; i < appURLs_oc.count; i++) {
			NSURL *appURL = appURLs_oc[i];
			QString appPath = QString([appURL.path UTF8String]);
			appList.append(appPath);
		}
	}
}

QString pls_get_dll_dir(const QString &pluginName)
{
	QString pluginPath = pls_get_app_plugin_dir() + "/" + pluginName + ".plugin";
	NSBundle *bundle = [NSBundle bundleWithPath:getNSStringFromQString(pluginPath)];
	return QString([bundle resourcePath].UTF8String);
}

QString pls_get_app_pn()
{
	NSString *processName = [PLSMacFunction get_app_process_name];
	return QString([processName UTF8String]);
}

std::wstring pls_utf8_to_unicode(const char *utf8)
{
	NSString *utf8_str = [NSString stringWithCString:utf8 encoding:NSUTF8StringEncoding];
	NSStringEncoding pEncode = CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingUTF32LE);
	NSData *pSData = [utf8_str dataUsingEncoding:pEncode];
	std::wstring w_string = std::wstring((wchar_t *)[pSData bytes], [pSData length] / sizeof(wchar_t));
	return w_string;
}

std::string pls_unicode_to_utf8(const wchar_t *unicode)
{
	NSString *unicode_str = [[NSString alloc] initWithBytes:unicode length:wcslen(unicode) * sizeof(*unicode) encoding:NSUTF32LittleEndianStringEncoding];
	return std::string([unicode_str UTF8String]);
}

NSString *getNSStringFromQString(const QString &qString)
{
	NSString *string = [NSString stringWithCString:qString.toUtf8().data() encoding:NSUTF8StringEncoding];
	return string;
}

NSString *getNSStringFromWChar_t(const wchar_t *charText)
{
	return [[NSString alloc] initWithBytes:charText length:wcslen(charText) * sizeof(*charText) encoding:NSUTF32LittleEndianStringEncoding];
	;
}

NSString *gettNSStringFromChar(const char *charText)
{
	return [NSString stringWithUTF8String:charText];
}

std::wstring getWStringFromNSString(NSString *str)
{
	NSData *asData = [str dataUsingEncoding:kEncoding_wchar_t];
	return std::wstring((wchar_t *)[asData bytes], [asData length] / sizeof(wchar_t));
}

NSRunningApplication *pls_current_user_proccess(uint32_t pid_t)
{
	NSRunningApplication *runningApp = [NSRunningApplication runningApplicationWithProcessIdentifier:pid_t];
	return runningApp;
}

bool pls_is_process_running(int process_id, char *process_name, int process_name_length)
{
	size_t proc_buf_size = 0;
	const int NAME_LEN = 4;
	int name[NAME_LEN] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	int result = sysctl(name, NAME_LEN, NULL, &proc_buf_size, NULL, 0);
	if (result != 0) {
		return false;
	}

	struct kinfo_proc *process = (struct kinfo_proc *)malloc(proc_buf_size);
	result = sysctl(name, NAME_LEN, process, &proc_buf_size, NULL, 0);
	if (result != 0) {
		free(process);
		return false;
	}

	int proc_count = (int)(proc_buf_size / sizeof(struct kinfo_proc));
	for (int i = 0; i < proc_count; i++) {
		if (process_id == process[i].kp_proc.p_pid) {
			strncpy(process_name, (const char *)process[i].kp_proc.p_comm, process_name_length);
			process_name[process_name_length - 1] = '\0';
			free(process);
			return true;
		}
	}
	free(process);
	return false;
}

bool pls_is_process_running(const char *process_name, int &process_id)
{
	size_t proc_buf_size = 0;
	const int NAME_LEN = 4;
	int name[NAME_LEN] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	int result = sysctl(name, NAME_LEN, NULL, &proc_buf_size, NULL, 0);
	if (result != 0) {
		return false;
	}

	struct kinfo_proc *process = (struct kinfo_proc *)malloc(proc_buf_size);
	result = sysctl(name, NAME_LEN, process, &proc_buf_size, NULL, 0);
	if (result != 0) {
		free(process);
		return false;
	}

	int proc_count = (int)(proc_buf_size / sizeof(struct kinfo_proc));
	for (int i = 0; i < proc_count; i++) {
		if (pls_is_equal(process[i].kp_proc.p_comm, process_name)) {
			process_id = process[i].kp_proc.p_pid;
			free(process);
			return true;
		}
	}
	free(process);
	return false;
}

bool pls_is_process_running(unsigned int pid)
{
	char process_name[PROCESS_NAME_SIZE] = {0};
	bool isRunning = pls_is_process_running(pid, process_name, PROCESS_NAME_SIZE);
	return isRunning;
}

bool pls_is_dev_environment()
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	bool value = [defaults boolForKey:@"DevServer"];
	return value;
}

bool pls_save_local_log()
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	bool value = [defaults boolForKey:@"LocalLog"];
	return value;
}

uint32_t pls_current_process_id()
{
	return (uint32_t)[NSProcessInfo processInfo].processIdentifier;
}

QStringList pls_cmdlines()
{
	NSProcessInfo *processInfo = [NSProcessInfo processInfo];
	QStringList cmdList;
	for (int i = 0; i < processInfo.arguments.count; i++) {
		NSString *cmd = processInfo.arguments[i];
		const char *cms_s = [cmd UTF8String];
		cmdList.append(cms_s);
	}
	return cmdList;
}

bool unZip(const QString &dstDirPath, const QString &srcFilePath)
{
	NSString *zipPath = srcFilePath.toNSString();
	NSString *targetFolder = dstDirPath.toNSString();
	if (![[NSFileManager defaultManager] fileExistsAtPath:zipPath]) {
		PLS_INFO("Process", "mac unzip failed, with not invalid zipPath: %s", "see kr to look detail path");
		PLS_INFO_KR("Process", "mac unzip failed, with not invalid zipPath: %s", zipPath.UTF8String);
		assert(false && "must contain srcFilePath");
		return false;
	}

	if (![[NSFileManager defaultManager] fileExistsAtPath:targetFolder]) {
		PLS_INFO("Process", "mac unzip failed, with Target folder does not exist: %s", "see kr to look detail path");
		PLS_INFO_KR("Process", "mac unzip failed, with Target folder does not exist: %s", targetFolder.UTF8String);
		assert(false && "must contain dstDirPath");
		return false;
	}
	@try {
		NSTask *unzipTask = [[NSTask alloc] init];
		[unzipTask setLaunchPath:@"/usr/bin/unzip"];
		[unzipTask setArguments:@[@"-o", zipPath, @"-d", targetFolder]];

		[unzipTask launch];
		[unzipTask waitUntilExit];

		return [unzipTask terminationStatus] == 0;
	} @catch (NSException *exception) {
		PLS_INFO("Process", "mac unzip failed, with catched exception. name:%s desc: %s", exception.name.UTF8String, exception.description.UTF8String);
		return false;
	}
}

bool zip(const QString &destZipPath, const QString &sourceDirName, const QString &sourceFolderPath)
{
	NSString *zipPath = destZipPath.toNSString();
	NSString *copyFolderName = sourceDirName.toNSString();
	NSString *directoryPath = sourceFolderPath.toNSString();

	if (![[NSFileManager defaultManager] fileExistsAtPath:directoryPath]) {
		PLS_INFO("Process", "mac zip failed, with Source folder does not exist: %s", "see kr to look detail path");
		PLS_INFO_KR("Process", "mac zip failed, with Source folder does not exist: %s", directoryPath.UTF8String);
		assert(false && "must contain sourceFolderPath");
		return false;
	}
	@try {
		NSTask *zipTask = [[NSTask alloc] init];
		[zipTask setLaunchPath:@"/usr/bin/zip"];
		[zipTask setCurrentDirectoryPath:directoryPath];
		[zipTask setArguments:@[@"-r", zipPath, copyFolderName]];
		[zipTask launch];
		[zipTask waitUntilExit];
		return [zipTask terminationStatus] == 0;
	} @catch (NSException *exception) {
		PLS_INFO("Process", "mac zip failed, with catched exception. name:%s desc: %s", exception.name.UTF8String, exception.description.UTF8String);
		return false;
	}
}

pls_mac_ver_t pls_get_mac_systerm_ver()
{

	pls_mac_ver_t mac_version;
	NSOperatingSystemVersion systemVersion = [NSProcessInfo processInfo].operatingSystemVersion;
	mac_version.major = (int)systemVersion.majorVersion;
	mac_version.minor = (int)systemVersion.minorVersion;
	mac_version.patch = (int)systemVersion.patchVersion;

	NSString *ctlKey = @"kern.osversion";
	BOOL buildValueFound;
	NSString *buildValue;
	size_t size = 0;
	if (sysctlbyname([ctlKey UTF8String], NULL, &size, NULL, 0) == -1) {
		return mac_version;
	}
	char *machine = (char *)calloc(1, size);
	sysctlbyname([ctlKey UTF8String], machine, &size, NULL, 0);
	mac_version.buildNum = machine;
	free(machine);

	return mac_version;
}

bool pls_copy_file(const QString &fileName, const QString &newName, bool overwrite, int &errorCode)
{
	NSString *sourceFilePath = [NSString stringWithCString:fileName.toStdString().c_str() encoding:NSUTF8StringEncoding];
	NSString *destFilePath = [NSString stringWithCString:newName.toStdString().c_str() encoding:NSUTF8StringEncoding];
	NSError *error = NULL;
	bool result = true;
	if (overwrite) {
		result = [[NSFileManager defaultManager] removeItemAtPath:destFilePath error:&error];
	}
	if (!result) {
		errorCode = (int32_t)error.code;
		return result;
	}
	result = [[NSFileManager defaultManager] copyItemAtPath:sourceFilePath toPath:destFilePath error:&error];
	if (!result) {
		errorCode = (int)error.code;
		return result;
	}

	return true;
}

QString pls_get_system_identifier()
{
	NSLocale *locale = [NSLocale systemLocale];
	NSString *localLanguage = [locale objectForKey:NSLocaleIdentifier];
	return QString([localLanguage UTF8String]);
}

bool pls_file_is_existed(const QString &filePath)
{
	NSString *mac_file_path = [NSString stringWithCString:filePath.toUtf8().data() encoding:NSUTF8StringEncoding];
	bool ret = [[NSFileManager defaultManager] fileExistsAtPath:mac_file_path];
	return ret;
}

bool install_mac_package(const QString &sourceBundlePath, const QString &destBundlePath, const std::string &prismSession, const std::string &prismGcc, const char *version)
{

	// start the replacement bundle process downloaded from the user directory
	NSString *unzipBundlePath = getNSStringFromQString(sourceBundlePath);
	NSString *runningBundlePath = getNSStringFromQString(destBundlePath);
	NSString *dittoPid = [NSString stringWithFormat:@"%d", [NSRunningApplication currentApplication].processIdentifier];
	NSString *dittoSession = [NSString stringWithCString:prismSession.c_str() encoding:NSUTF8StringEncoding];
	NSString *dittoGcc = [NSString stringWithCString:prismGcc.c_str() encoding:NSUTF8StringEncoding];
	NSString *dittoVersion = [NSString stringWithCString:version encoding:NSUTF8StringEncoding];

	// get the replacement program path in the installation package
	NSString *appPath = [unzipBundlePath stringByAppendingPathComponent:@"Contents/MacOS/PRISMUpdater"];

	// print process log
	PLS_INFO("Process", "mac update status: start launch ditto process, prism session is %s, prism pid is %s", dittoSession.UTF8String, dittoPid.UTF8String);
	PLS_INFO_KR("Process", "mac update status: start launch ditto process, source bundle path is %s , dest bundle path is %s , app path is %s", unzipBundlePath.UTF8String,
		    runningBundlePath.UTF8String, appPath.UTF8String);

	// add the command line argument list for the replacement process
	NSMutableArray *tempArguments = [[NSMutableArray alloc] init];
	[tempArguments addObject:unzipBundlePath];
	[tempArguments addObject:runningBundlePath];
	[tempArguments addObject:dittoPid];
	[tempArguments addObject:dittoSession];
	[tempArguments addObject:dittoGcc];
	[tempArguments addObject:dittoVersion];

	// start the replacement installation package process
	NSTask *privilegedTask = [[NSTask alloc] init];
	[privilegedTask setLaunchPath:appPath];
	[privilegedTask setArguments:tempArguments.copy];
	[privilegedTask setEnvironment:@{}];
	[privilegedTask setCurrentDirectoryPath:[[NSBundle mainBundle] resourcePath]];
	NSError *error = nil;
	bool result = [privilegedTask launchAndReturnError:&error];
	PLS_INFO("Process", "mac update status: launch PRISMLauncher Process,result is %s,%s", result ? "true" : "false",
		 [NSString stringWithFormat:@" error code:%zi - msg:%@", error.code, error.localizedDescription].UTF8String);
	if (!result) {
		return NO;
	}
	return true;
}

bool pls_restart_mac_app(const QStringList &arguments)
{
	NSString *daemonPath = [[[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent] stringByAppendingFormat:@"/%@", NSApplicationRelaunchDaemon];
	NSString *executablePath = [[NSBundle mainBundle] executablePath];
	NSString *pid = [NSString stringWithFormat:@"%d", [[NSProcessInfo processInfo] processIdentifier]];
	NSMutableArray *cmdlineArgument = [[NSMutableArray alloc] init];
	[cmdlineArgument addObject:executablePath];
	[cmdlineArgument addObject:pid];
	for (int i = 0; i < arguments.count(); i++) {
		[cmdlineArgument addObject:getNSStringFromQString(arguments.at(i))];
	}
	NSString *startType; //gettNSStringFromChar(restartType);
	NSTask *task = [[NSTask alloc] init];
	[task setLaunchPath:daemonPath];
	[task setArguments:cmdlineArgument.copy];
	NSError *error = NULL;
	[task launchAndReturnError:&error];
	NSLog(@"pls_restart_mac_app start daemonPath is %@, bundlePath is %@,pid is %@,error is %@", daemonPath, executablePath, pid, error);

	return true;
}

NSMutableArray *getBundleList(NSString *downloadedBundleDir, NSString *extension)
{
	NSArray *dirs = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:downloadedBundleDir error:NULL];
	NSMutableArray *bundleList = [[NSMutableArray alloc] init];
	[dirs enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
		NSString *filename = (NSString *)obj;
		NSString *myextension = [[filename pathExtension] lowercaseString];
		if ([myextension isEqualToString:extension]) {
			[bundleList addObject:[downloadedBundleDir stringByAppendingPathComponent:filename]];
		}
	}];
	return bundleList;
}

QString pls_get_existed_downloaded_mac_app(const QString &downloadedBundleDir, const QString &downloadedVersion, bool deleteBundleExceptUpdatedBundle)
{

	NSArray *appFiles = getBundleList(getNSStringFromQString(downloadedBundleDir), @"app");
	NSString *currentBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
	NSString *downloadedBundleVersion = getNSStringFromQString(downloadedVersion);
	QString findPath;
	for (NSString *filePath in appFiles) {
		NSBundle *bundle = [NSBundle bundleWithPath:filePath];
		NSString *bundleId = bundle.bundleIdentifier;
		if (![bundleId isEqualToString:currentBundleIdentifier]) {
			continue;
		}
		NSString *version = [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
		if ([version isEqualToString:downloadedBundleVersion]) {
			findPath = QString([filePath UTF8String]);
			continue;
		}
		if (deleteBundleExceptUpdatedBundle) {
			[[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
		}
	}
	return findPath;
}

bool isLargeVersion(const QString &v1, const QString &v2)
{
	bool isLarge = false;
	QStringList v1List = v1.split(".");
	QStringList v2List = v2.split(".");
	auto len1 = v1List.count();
	auto len2 = v2List.count();
	for (int i = 0; i < qMin(len1, len2); i++) {
		if (v1List.at(i).toUInt() > v2List.at(i).toUInt()) {
			isLarge = true;
			break;
		}
		if (v1List.at(i).toUInt() < v2List.at(i).toUInt()) {
			break;
		}
	}
	return isLarge;
}

bool pls_remove_all_downloaded_mac_app_small_equal_version(const QString &downloadedBundleDir, const QString &softwareVersion)
{
	NSArray *appFiles = getBundleList(getNSStringFromQString(downloadedBundleDir), @"app");
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSString *currentBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
	for (NSString *filePath in appFiles) {
		NSBundle *bundle = [NSBundle bundleWithPath:filePath];
		NSString *bundleId = bundle.bundleIdentifier;
		if (![bundleId isEqualToString:currentBundleIdentifier]) {
			continue;
		}
		NSString *bundleVersion = [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
		if (isLargeVersion(QString([bundleVersion UTF8String]), softwareVersion)) {
			continue;
		}
		[fileManager removeItemAtPath:filePath error:nil];
	}

	return true;
}

void bring_mac_window_to_front(WId winId)
{
	if (winId == 0)
		return;
	NSView *view = (__bridge NSView *)reinterpret_cast<void *>(winId);
	NSWindow *window = [view window];
	[window makeKeyAndOrderFront:nil];
	[window setOrderedIndex:0];
}

PLSMacProcessHandle *getMacProcessHandle(MacHandle macHandle)
{
	//void *void_handle = macHandle.handle;
	void *void_handle;
	return (__bridge PLSMacProcessHandle *)void_handle;
}

MacHandle pls_process_create(const QString &program, const QStringList &arguments, const QString &work_dir)
{
	NSString *app = getNSStringFromQString(program);
	NSMutableArray *tempArguments = [[NSMutableArray alloc] init];
	for (const QString &argument : arguments) {
		[tempArguments addObject:getNSStringFromQString(argument)];
	}
	NSError *error;
	NSTask *task = [PLSMacFunction create_process:app arguments:[tempArguments copy] workDir:getNSStringFromQString(work_dir) error:&error];
	char process_name[PROCESS_NAME_SIZE] = {0};
	bool isRunning = pls_is_process_running(task.processIdentifier, process_name, PROCESS_NAME_SIZE);
	PLS_INFO("Process", "mac process status: create process with program, pid is %d , program is %s, processName is %s , isRunning is %d error:%s", task.processIdentifier,
		 program.toUtf8().constData(), process_name, isRunning, error.localizedDescription.UTF8String);
	MacHandle process = new MacProcessInfo;
	process->pid = task.processIdentifier;
	process->task = (__bridge void *)task;
	return process;
}

void pls_mac_create_process_with_not_inherit(const QString &program, const QStringList &arguments, void *receiver, PLSMacProcessCallback callback)
{
	NSString *appPath = getNSStringFromQString(program);
	NSMutableArray *tempArguments = [[NSMutableArray alloc] init];
	for (const QString &argument : arguments) {
		[tempArguments addObject:getNSStringFromQString(argument)];
	}

	NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
	NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
	config.arguments = tempArguments.copy;
	config.createsNewApplicationInstance = YES;

	[workspace openApplicationAtURL:[NSURL fileURLWithPath:appPath]
			  configuration:config
		      completionHandler:^(NSRunningApplication *_Nullable app, NSError *_Nullable error) {
			      bool isSucceed = !error;
			      if (error) {
				      pls_set_last_error((uint32_t)error.code);
			      }
			      PLS_INFO("Process", "mac process status: create process with program, pid is %d , program is %s, processName is %s , isRunning is %d%s", app.processIdentifier,
				       appPath.UTF8String, app.localizedName.UTF8String, isSucceed,
				       isSucceed ? "" : [NSString stringWithFormat:@" error code:%zi - msg:%@", error.code, error.localizedDescription].UTF8String);
			      pid_t targetPID = app.processIdentifier;
			      if (callback) {
				      dispatch_async(dispatch_get_main_queue(), ^{
					      callback(receiver, isSucceed, targetPID);
				      });
			      }
		      }];
}

MacHandle pls_process_create(uint32_t process_id)
{
	MacHandle process = new MacProcessInfo;
	process->pid = process_id;
	char process_name[PROCESS_NAME_SIZE] = {0};
	bool isRunning = pls_is_process_running(process_id, process_name, PROCESS_NAME_SIZE);
	PLS_INFO("Process", "mac process status: create process with pid , pid is %d , processName is %s , isRunning is %d", process_id, process_name, isRunning);
	return process;
}

bool pls_process_destroy(MacHandle handle)
{
	if (!pls_is_process_running(handle->pid)) {
		PLS_INFO("Process", "mac process status: destory process failed because process is exited, pid is %d", handle->pid);
		return false;
	}
	NSRunningApplication *runningApp = pls_current_user_proccess(handle->pid);
	if (runningApp) {
		PLS_INFO("Process", "mac process status: destory process running app sucess, pid is %d", handle->pid);
		return [runningApp terminate];
	}
	if (handle->task) {
		NSTask *task = (__bridge NSTask *)handle->task;
		[task terminate];
		PLS_INFO("Process", "mac process status: destory process running task sucess, pid is %d", handle->pid);
		return [task terminationStatus] == 0;
	}
	PLS_INFO("Process", "mac process status: destory process failed because not process, pid is %d", handle->pid);
	return false;
}

bool pls_process_force_terminte(uint32_t process_id, int &exit_code)
{
	if (!pls_is_process_running(process_id)) {
		return false;
	}
	int result = kill(process_id, SIGKILL);
	PLS_INFO("Process", "mac process status: force kill proces, result is %d", result);
	if (result == -1) {
		return false;
	}
	int status;
	wait(&status);
	if (WIFEXITED(status)) {
		exit_code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		exit_code = WTERMSIG(status);
	} else if (WIFSTOPPED(status)) {
		exit_code = WIFSTOPPED(status);
	}
	PLS_INFO("Process", "mac process status: force kill proces, exit code is %d", exit_code);

	//    NSString *killProcessScript = [NSString stringWithFormat:@"kill %d",process_id];
	//    NSString *processId = [NSString stringWithFormat:@"%d", process_id];
	//
	//
	//    NSTask *task = [[NSTask alloc] init];
	//    [task setLaunchPath: @"/bin/sh"];
	////    NSArray *arguments = [NSArray arrayWithObjects: @"-c", [NSString stringWithFormat:@"kill -9 %d",process_id], nil];
	//    NSArray *arguments = [NSArray arrayWithObjects:@"-c",killProcessScript,nil];
	//    [task setArguments:arguments];
	//
	//    NSPipe *stdoutPipe = [NSPipe pipe];
	//    [task setStandardOutput:stdoutPipe];
	//
	//    NSFileHandle *stdoutHandle = [stdoutPipe fileHandleForReading];
	//    [stdoutHandle waitForDataInBackgroundAndNotify];
	//    id observer = [[NSNotificationCenter defaultCenter] addObserverForName:NSFileHandleDataAvailableNotification
	//                                                                    object:stdoutHandle queue:nil
	//                                                                usingBlock:^(NSNotification *note)
	//    {
	//        NSData *dataRead = [stdoutHandle availableData];
	//        NSString *stringRead = [[NSString alloc] initWithData:dataRead encoding:NSUTF8StringEncoding];
	//        NSLog(@"output: %@", stringRead);
	//        [stdoutHandle waitForDataInBackgroundAndNotify];
	//    }];
	//
	//    [task launch];
	//    [task waitUntilExit];
	//    [[NSNotificationCenter defaultCenter] removeObserver:observer];
	return true;
}

bool pls_process_force_terminte(int &exit_code)
{
	return pls_process_force_terminte([NSProcessInfo processInfo].processIdentifier, exit_code);
}

static void noteProcDeath(CFFileDescriptorRef fdref, CFOptionFlags callBackType, void *info)
{
	struct kevent event;
	int fd = CFFileDescriptorGetNativeDescriptor(fdref);
	kevent(fd, NULL, 0, &event, 1, NULL);
	int pid = (int)event.ident;

	int status = (int)event.data;
	int exitCode = 0;
	if (WIFEXITED(status)) {
		exitCode = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		exitCode = WTERMSIG(status);
	} else if (WIFSTOPPED(status)) {
		exitCode = WIFSTOPPED(status);
	}
	PLS_INFO("Process", "mac process status: process died, pid is %d, exit code is %d", (unsigned int)event.ident, exitCode);
	if (event.udata) {
		MacHandle handle = (MacHandle)(event.udata);
		handle->exitCode = exitCode;
	}
	CFFileDescriptorEnableCallBacks(fdref, kCFFileDescriptorReadCallBack);
}

static int createOnProcessExitQueue()
{
	int kq = kqueue();
	if (kq < 0)
		return -1;

	CFFileDescriptorContext context = {.version = 0, .info = NULL, .retain = NULL, .release = NULL, .copyDescription = NULL};
	CFFileDescriptorRef kqFileDescriptor = CFFileDescriptorCreate(NULL, kq, true, noteProcDeath, &context);
	if (kqFileDescriptor == NULL) {
		close(kq);
		kq = -1;
		return -1;
	}
	CFRunLoopSourceRef runLoopSource = CFFileDescriptorCreateRunLoopSource(NULL, kqFileDescriptor, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
	CFRelease(runLoopSource);
	CFFileDescriptorEnableCallBacks(kqFileDescriptor, kCFFileDescriptorReadCallBack);
	CFRelease(kqFileDescriptor);
	return kq;
}

void addObserverProcessExit(MacHandle handle)
{
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		process_exit_kq = createOnProcessExitQueue();
	});
	struct kevent event = {.ident = handle->pid, .filter = EVFILT_PROC, .flags = EV_ADD | EV_ONESHOT, .fflags = NOTE_EXIT, .data = 0, .udata = ((void *)handle)};
	kevent(process_exit_kq, &event, 1, NULL, 0, NULL);
	PLS_INFO("Process", "mac process status: add observer process exit event, pid is %d", handle->pid);
}

void removeObserverProcessExit(MacHandle handle)
{
	PLS_INFO("Process", "mac process status: start remove observer process exit event , pid is %d", handle->pid);
	struct kevent event = {.ident = handle->pid, .filter = EVFILT_PROC, .flags = EV_DELETE, .fflags = NOTE_EXIT, .data = 0, .udata = ((void *)handle)};
	kevent(process_exit_kq, &event, 1, NULL, 0, NULL);
	PLS_INFO("Process", "mac process status: end remove observer process exit event , pid is %d", handle->pid);
}

int pls_process_wait(MacHandle handle, int timeout)
{
	uint32_t pid = pls_process_id(handle);
	if (timeout == 0) {
		return pls_is_process_running(pid) ? 0 : 1;
	} else if (timeout < 0) {
		while (pls_is_process_running(pid)) {
			QThread::msleep(1000);
		}
		return 1;
	}
	for (auto start = std::chrono::steady_clock::now();
	     pls_is_process_running(pid) && (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeout;) {
		QThread::msleep(1000);
	}
	return pls_is_process_running(pid) ? 0 : 1;
}

uint32_t pls_process_id(MacHandle handle)
{
	return handle->pid;
}

bool pls_process_exit_code(MacHandle handle, uint32_t *exit_code)
{
	if (pls_is_process_running(handle->pid)) {
		return false;
	}
	*exit_code = handle->exitCode;
	return true;
}

QUrl build_mac_hmac_url(const QUrl &url, const QByteArray &hmacKey)
{
	return url;
}

bool pls_check_mac_app_is_existed(const wchar_t *executableName)
{
	NSMutableArray *processList = [[NSMutableArray alloc] init];
	NSString *sourceExecutableName = getNSStringFromWChar_t(executableName);
	for (int i = 0; i < [NSRunningApplication runningApplicationsWithBundleIdentifier:[NSBundle mainBundle].bundleIdentifier].count; i++) {
		NSRunningApplication *runningApp = [NSRunningApplication runningApplicationsWithBundleIdentifier:[NSBundle mainBundle].bundleIdentifier][i];
		NSString *fileName = [runningApp.executableURL lastPathComponent];
		if ([fileName isEqualToString:sourceExecutableName]) {
			[processList addObject:[NSString stringWithFormat:@"%d", runningApp.processIdentifier]];
		}
		NSLog(@"mac launcher status : existed running app fileName is %@", fileName);
		NSLog(@"mac launcher status : existed running app localizedName is %@", runningApp.localizedName);
		NSLog(@"mac launcher status : existed running app executableURL is %@", runningApp.executableURL);
		NSLog(@"mac launcher status : existed running app processIdentifier is %d", runningApp.processIdentifier);
		NSLog(@"mac launcher status : existed running app bundleURL is %@", runningApp.bundleURL);
		NSLog(@"mac launcher status : existed running app isHidden is %d", runningApp.isHidden);
		NSLog(@"mac launcher status : existed running app isActive is %d", runningApp.isActive);
		NSLog(@"mac launcher status : existed running app isFinishedLaunching is %d", runningApp.isFinishedLaunching);
	}
	if (processList.count > 1) {
		return true;
	}
	return false;
}

bool pls_activiate_app()
{
	int pid = [NSProcessInfo processInfo].processIdentifier;
	return pls_activiate_app_pid(pid);
}

bool pls_activiate_mac_app_except_self()
{
	int pid = [NSProcessInfo processInfo].processIdentifier;
	NSString *processName = [NSProcessInfo processInfo].processName;
	NSArray<NSRunningApplication *> *runningApps = [NSRunningApplication runningApplicationsWithBundleIdentifier:[NSBundle mainBundle].bundleIdentifier];
	bool result = false;
	for (int i = 0; i < runningApps.count; i++) {
		NSRunningApplication *runningApp = runningApps[i];
		NSString *lastObjectPath = [runningApp.executableURL.absoluteString lastPathComponent];
		if (runningApp.processIdentifier != pid && [lastObjectPath isEqualToString:processName]) {
			result = [runningApp activateWithOptions:NSApplicationActivateIgnoringOtherApps];
		}
	}
	return result;
}

bool pls_activiate_app_bundle_id(const char *identifier)
{
	NSArray<NSRunningApplication *> *runningApps = [NSRunningApplication runningApplicationsWithBundleIdentifier:[NSString stringWithUTF8String:identifier]];
	for (int i = 0; i < runningApps.count; i++) {
		NSRunningApplication *runningApp = runningApps[i];
		return [runningApp activateWithOptions:NSApplicationActivateIgnoringOtherApps];
	}
	return false;
}

bool pls_activiate_app_pid(int pid)
{
	NSRunningApplication *runningApp = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
	NSString *fileName = [runningApp.executableURL lastPathComponent];
	NSLog(@"mac launcher status : activiate running app fileName is %@", fileName);
	NSLog(@"mac launcher status : activiate running app localizedName is %@", runningApp.localizedName);
	NSLog(@"mac launcher status : activiate running app executableURL is %@", runningApp.executableURL);
	NSLog(@"mac launcher status : activiate running app processIdentifier is %d", runningApp.processIdentifier);
	NSLog(@"mac launcher status : activiate running app bundleURL is %@", runningApp.bundleURL);
	NSLog(@"mac launcher status : activiate running app isHidden is %d", runningApp.isHidden);
	NSLog(@"mac launcher status : activiate running app isActive is %d", runningApp.isActive);
	NSLog(@"mac launcher status : activiate running app isFinishedLaunching is %d", runningApp.isFinishedLaunching);
	return [runningApp activateWithOptions:NSApplicationActivateIgnoringOtherApps];
}

bool pls_is_app_running(const char *bundle_id)
{
	if (!bundle_id)
		return false;

	NSString *bundleID = [NSString stringWithUTF8String:bundle_id];
	NSArray *runningApps = [NSRunningApplication runningApplicationsWithBundleIdentifier:bundleID];
	return [runningApps count] > 0;
}

bool pls_launch_app(const char *bundle_id, const char *app_name)
{
	if (!bundle_id || !app_name)
		return false;

	NSString *bundleID = [NSString stringWithUTF8String:bundle_id];
	NSString *appName = [NSString stringWithUTF8String:app_name];
	NSURL *appURL = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundleID];
	NSString *appPath = NULL;
	if ([appURL.path containsString:appName]) {
		appPath = appURL.path;
	} else {
		appPath = [NSString stringWithFormat:@"/Applications/%@.app", appName];
	}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	return [[NSWorkspace sharedWorkspace] launchApplication:appPath];
#pragma GCC diagnostic pop
}

QString pls_get_current_system_language_id()
{
	NSLocale *local = [NSLocale currentLocale];
	return QString([[local objectForKey:NSLocaleCountryCode] UTF8String]);
}

void pls_get_current_datetime(pls_mac_datetime_t &datetime)
{
	NSDate *nowDate = [NSDate date];
	NSCalendar *calender = [NSCalendar currentCalendar];
	NSUInteger unitFlags = NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay | NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond | NSCalendarUnitNanosecond |
			       NSCalendarUnitTimeZone;
	NSDateComponents *dateComponents = [calender components:unitFlags fromDate:nowDate];
	datetime.year = (int)dateComponents.year;
	datetime.month = (int)dateComponents.month;
	datetime.day = (int)dateComponents.day;
	datetime.hour = (int)dateComponents.hour;
	datetime.minute = (int)dateComponents.minute;
	datetime.second = (int)dateComponents.second;
	datetime.milliseconds = (int)dateComponents.nanosecond;
	NSInteger sourceGMTOffset = [dateComponents.timeZone secondsFromGMTForDate:[NSDate date]];
	datetime.timezone = (int)sourceGMTOffset;
}

std::string pls_get_system_version()
{
	char version[500] = {0};

	NSProcessInfo *processInfo = [[NSProcessInfo alloc] init];
	if ([processInfo respondsToSelector:@selector(operatingSystemVersion)]) {
		NSOperatingSystemVersion versionObj = [processInfo operatingSystemVersion];

		snprintf(version, sizeof(version), "OSX %ld.%ld.%ld", (long)versionObj.majorVersion, (long)versionObj.minorVersion, (long)versionObj.patchVersion);
	}

	return std::string(version);
}

std::string pls_get_machine_hardware_name()
{
	struct utsname sysinfo;
	int retVal = uname(&sysinfo);
	if (EXIT_SUCCESS != retVal)
		return "";

	return sysinfo.machine;
}

void pls_application_show_dock_icon(bool isShow)
{
	if (isShow) {
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	} else {
		[NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
	}
}

bool pls_is_mouse_pressed_by_mac(Qt::MouseButton button)
{
	switch (button) {
	case Qt::LeftButton:
		return ((1 << 0) & [NSEvent pressedMouseButtons]) == (1 << 0);
	case Qt::RightButton:
		return ((1 << 1) & [NSEvent pressedMouseButtons]) == (1 << 1);
	default:
		PLS_INFO("Mouse", "mac not support query other mouse button is pressed %i", static_cast<int>(button));
		assert(false);
		break;
	}
	return false;
}

bool pls_is_lens_has_run()
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
	NSString *path = [paths firstObject];
	if (!path) {
		return false;
	}
	path = [NSString stringWithFormat:@"%@/%@/%@.plist", path, @"Preferences", @kPrismLensBundleID];
	NSDictionary *infoDict = [NSDictionary dictionaryWithContentsOfFile:path];

	if (!infoDict) {
		return false;
	}

	return [[infoDict valueForKey:@kLensHasRunKey] boolValue];
}

QString pls_get_app_version_by_identifier(const char *bundleID)
{
	NSString *installApp;
	QStringList appList;
	pls_libutil_api_mac::pls_get_install_app_list(bundleID, appList);
	for (const auto &appPath : appList) {
		if (appPath.startsWith("/Applications")) {
			installApp = pls_libutil_api_mac::getNSStringFromQString(appPath);
			break;
		}
	}
	if (installApp.length == 0) {
		return "";
	}

	NSDictionary *infoDict = [[NSBundle bundleWithPath:installApp] infoDictionary];
	NSString *version = [infoDict objectForKey:@"CFBundleShortVersionString"];
	return QString(version.UTF8String);
}

void pls_set_current_lens(int index)
{
	if (![CrossProcessNotification.defaultInstance isStarted]) {
		[CrossProcessNotification.defaultInstance start];
	}

	if (![CrossProcessNotification.defaultInstance isStarted]) {
		assert(false);
		return;
	}

	if (index < 0 || index >= 3) { // lens
		assert(false);
		return;
	}

	[CrossProcessNotification.defaultInstance setState:index];
}

bool pls_get_is_app_quitting_by_dock()
{
	NSAppleEventDescriptor *appleEvent = [[NSAppleEventManager sharedAppleEventManager] currentAppleEvent];

	if (!appleEvent) {
		return false;
	}

	if ([appleEvent eventClass] != kCoreEventClass || [appleEvent eventID] != kAEQuitApplication) {
		return false;
	}

	NSAppleEventDescriptor *reason = [appleEvent attributeDescriptorForKeyword:kAEQuitReason];
	if (reason) {
		return false;
	}

	pid_t senderPID = [[appleEvent attributeDescriptorForKeyword:keySenderPIDAttr] int32Value];
	if (senderPID == 0) {
		return false;
	}

	NSRunningApplication *sender = [NSRunningApplication runningApplicationWithProcessIdentifier:senderPID];
	if (!sender) {
		return false;
	}

	return [@"com.apple.dock" isEqualToString:[sender bundleIdentifier]];
}

bool pls_open_url_mac(const QString &url)
{
	return [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:getNSStringFromQString(url)]];
}

bool pls_lens_needs_reboot()
{
	NSString *searchActiveLensCmd = @"systemextensionsctl list | grep com.prismlive.camstudio.extension1 | grep activated";
	NSString *searchRebootLensCmd = @"systemextensionsctl list | grep com.prismlive.camstudio.extension1 | grep reboot";
	BOOL isActiveExist = [searchActiveLensCmd runCommandAndReturnErrorWithAdminPrivilege:NO] == NULL;
	BOOL isRebootExist = [searchRebootLensCmd runCommandAndReturnErrorWithAdminPrivilege:NO] == NULL;
	return isActiveExist && isRebootExist;
}

}
