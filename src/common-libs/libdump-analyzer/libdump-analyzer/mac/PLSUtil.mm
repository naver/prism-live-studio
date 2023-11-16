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

NSString *callstackOfThread(thread_t thread);

std::string mac_get_app_run_dir(std::string name) {
    NSString *bundlePath = [NSBundle mainBundle].bundlePath;
    NSString *pathName = [NSString stringWithUTF8String:name.c_str()];
    return [bundlePath stringByAppendingPathComponent:pathName].UTF8String;
}

std::string mac_get_app_data_dir(std::string name) {
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSArray *appSupportPaths = [fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
	NSURL *appSupportURL = [appSupportPaths lastObject];
    NSString *namePath = [NSString stringWithUTF8String:name.c_str()];
    return [appSupportURL URLByAppendingPathComponent:namePath].path.UTF8String;
}

std::string mac_get_os_version() {
	NSProcessInfo *processInfo = [NSProcessInfo processInfo];
	NSOperatingSystemVersion version = [processInfo operatingSystemVersion];

	NSString *versionString = [NSString stringWithFormat:@"macOS %ld.%ld.%ld", version.majorVersion, version.minorVersion, version.patchVersion];

	return versionString.UTF8String;
}

std::string mac_get_device_model() {
    size_t size;
    sysctlbyname("hw.model", NULL, &size, NULL, 0);
    char *model = (char *)malloc(size);
    sysctlbyname("hw.model", model, &size, NULL, 0);
    
    std::string deviceModel(model);
    free(model);
    
    return deviceModel;
}

std::string mac_get_device_name() {
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

std::string mac_get_threads() {
	kern_return_t kr;
	task_info_data_t tinfo;
	mach_msg_type_number_t task_info_count;

	task_info_count = TASK_INFO_MAX;
	kr = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count);
	if (kr != KERN_SUCCESS) {
		return "";
	}

	thread_array_t thread_list;
	mach_msg_type_number_t thread_count;

	thread_info_data_t thinfo;
	mach_msg_type_number_t thread_info_count;

	thread_basic_info_t basic_info_th;

	// get threads in the task
	kr = task_threads(mach_task_self(), &thread_list, &thread_count);
	if (kr != KERN_SUCCESS) {
		return "";
	}

	long total_time = 0;
	long total_userTime = 0;
	CGFloat total_cpu = 0;
	int j;

	NSMutableArray *threads = [NSMutableArray new];

	// for each thread
	for (j = 0; j < (int)thread_count; j++) {

		thread_info_count = THREAD_INFO_MAX;

		kr = thread_info(thread_list[j], THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count);
		if (kr != KERN_SUCCESS) {
			return "";
		}

		NSString *threadCallstack = callstackOfThread(thread_list[j]);
		[threads addObject:threadCallstack];
	}

	kr = vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
	assert(kr == KERN_SUCCESS);
    
    return [threads componentsJoinedByString:@"\n"].UTF8String;
}

NSString *callstackOfThread(thread_t thread) {
	const int stack_size = 128;
    uintptr_t *stack = (uintptr_t *)calloc(stack_size, sizeof(uintptr_t *));
	int frame_count = mach_backtrace(thread, stack, stack_size);
	char **stack_symbols = backtrace_symbols((void * const *)stack, frame_count);
	free(stack);

	NSMutableString *backtrace = [NSMutableString new];
	for (int i = 0; i < frame_count; i++) {
		char *stack_info = stack_symbols[i];

		[backtrace appendString:[NSString stringWithUTF8String:stack_info]];
		[backtrace appendString:@"\n"];
	}

	free(stack_symbols);

	return [backtrace copy];
}
