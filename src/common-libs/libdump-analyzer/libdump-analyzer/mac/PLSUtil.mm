//
//  PLSUtil.mm
//  libdump-analyzer
//
//  Created by Keven on 3/27/23.
//
//

#import <Foundation/Foundation.h>
#import <sys/types.h>
#import <sys/sysctl.h>
#import "PLSUtilInterface.h"
#import <execinfo.h>
#import <KSCrash.h>
#import <KSCrashC.h>
#import <CommonCrypto/CommonDigest.h>

NSString *callstackOfThread(thread_t thread);

std::string mac_get_app_run_dir(std::string name)
{
	NSString *bundlePath = [NSBundle mainBundle].bundlePath;
	NSString *pathName = [NSString stringWithUTF8String:name.c_str()];
	return [bundlePath stringByAppendingPathComponent:pathName].UTF8String;
}

std::string mac_get_app_data_dir(std::string name)
{
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSArray *appSupportPaths = [fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
	NSURL *appSupportURL = [appSupportPaths lastObject];
	NSString *namePath = [NSString stringWithUTF8String:name.c_str()];
	return [appSupportURL URLByAppendingPathComponent:namePath].path.UTF8String;
}

std::string mac_get_os_version()
{
	NSProcessInfo *processInfo = [NSProcessInfo processInfo];
	NSOperatingSystemVersion version = [processInfo operatingSystemVersion];

	NSString *versionString = [NSString stringWithFormat:@"macOS %ld.%ld.%ld", version.majorVersion, version.minorVersion, version.patchVersion];

	return versionString.UTF8String;
}

std::string mac_get_device_model()
{
	size_t size;
	sysctlbyname("hw.model", NULL, &size, NULL, 0);
	char *model = (char *)malloc(size);
	sysctlbyname("hw.model", model, &size, NULL, 0);

	std::string deviceModel(model);
	free(model);

	return deviceModel;
}

std::string mac_get_device_name()
{
	std::string model = mac_get_device_model();
	NSString *modelKey = [NSString stringWithUTF8String:model.c_str()];

	NSDictionary *contentMap = nil;

	NSString *localConfigPath = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"data/prism-studio/user/mac/DeviceNames.json"];

	NSString *remoteConfigPath = [NSString stringWithUTF8String:mac_get_app_data_dir("/library/Library_Policy_PC/DeviceNames.json").c_str()];

	NSData *localConfigData = [NSData dataWithContentsOfFile:localConfigPath];
	NSData *remoteConfigData = [NSData dataWithContentsOfFile:remoteConfigPath];

	NSError *error = nil;
	NSDictionary *localConfigMap = [NSJSONSerialization JSONObjectWithData:localConfigData options:NSJSONReadingAllowFragments error:&error];

	if (error) {
		return "Unknown";
	}

	contentMap = localConfigMap;

	if (remoteConfigData) {
		NSDictionary *remoteConfigMap = [NSJSONSerialization JSONObjectWithData:remoteConfigData options:NSJSONReadingAllowFragments error:&error];

		if (error == nil) {
			// check version
			NSInteger localVersion = [localConfigMap[@"version"] integerValue];
			NSInteger remoteVersion = [remoteConfigMap[@"version"] integerValue];

			contentMap = remoteVersion >= localVersion ? remoteConfigMap : localConfigMap;
		}
	}

	if (!contentMap) {
		return "Unknown";
	}

	NSDictionary *deviceNames = contentMap[@"deviceNames"];

	NSString *productName = deviceNames[modelKey] ?: @"Unknown";
	std::string deviceName(productName.UTF8String);

	return deviceName;
}

static std::string latest_report_path;

static void crash_report_written_callback(int64_t reportID)
{
	KSCrash *reporter = [KSCrash sharedInstance];
	NSString *blockDumpPath = [reporter reportPathWithID:@(reportID)];

	NSMutableArray *pathComponents = [blockDumpPath.pathComponents mutableCopy];
	NSInteger pathCount = pathComponents.count;
	if (pathCount >= 2) {
		pathComponents[pathCount - 2] = @"BlockDump";
	}

	NSString *targetPath = [pathComponents componentsJoinedByString:@"/"];

	[[NSFileManager defaultManager] moveItemAtPath:blockDumpPath toPath:targetPath error:nil];

	latest_report_path = targetPath.UTF8String;
}

std::string mac_generate_dump_file(std::string info, std::string message)
{
	KSCrash *reporter = [KSCrash sharedInstance];
	kscrash_setReportWrittenCallback(crash_report_written_callback);

	[reporter reportUserException:@"UNKNOWN" // exclude from nelo crash statistics
			       reason:[NSString stringWithUTF8String:message.c_str()]
			     language:@"objc"
			   lineOfCode:nil
			   stackTrace:nil
			logAllThreads:YES
		     terminateProgram:NO];

	return latest_report_path;
}

NSString *mac_get_md5(NSString *input)
{
	// Convert to C string using UTF8 encoding
	const char *cstr = [input UTF8String];

	// Create a buffer to store the hash value (16 bytes for MD5)
	unsigned char result[CC_MD5_DIGEST_LENGTH];

	// Call the CC_MD5 function with the C string, its length, and the buffer
	CC_MD5(cstr, (CC_LONG)strlen(cstr), result);

	// Convert the buffer into a hexadecimal string
	NSMutableString *hexString = [NSMutableString string];
	for (int i = 0; i < CC_MD5_DIGEST_LENGTH; i++) {
		[hexString appendFormat:@"%02x", result[i]];
	}

	return [hexString copy];
}

bool mac_is_third_party_plugin(std::string &module_path)
{
	auto data_path = mac_get_app_data_dir("obs-studio");
	NSString *dataPathStr = [[NSString stringWithUTF8String:data_path.c_str()] stringByAppendingPathComponent:@"plugins"];
	NSString *modulePathStr = [NSString stringWithUTF8String:module_path.c_str()];

	return [modulePathStr hasPrefix:dataPathStr];
}

std::string mac_get_plugin_version(std::string &path)
{
	NSString *executablePath = [NSString stringWithUTF8String:path.c_str()];
	NSArray *pathComponents = executablePath.pathComponents;
	if (pathComponents.count < 3) {
		return "";
	}

	NSString *bundlePath = [NSString pathWithComponents:[pathComponents subarrayWithRange:NSMakeRange(0, executablePath.pathComponents.count - 3)]];
	if (!bundlePath) {
		return "";
	}

	NSBundle *bundle = [NSBundle bundleWithURL:[NSURL fileURLWithPath:bundlePath]];
	if (!bundle) {
		return "";
	}
	NSString *versionString = bundle.infoDictionary[@"CFBundleVersion"];
	if (!versionString) {
		return "";
	}

	std::string version(versionString.UTF8String);

	return version;
}
