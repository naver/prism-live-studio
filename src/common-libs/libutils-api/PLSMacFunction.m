//
//  PLSMacFunction.m
//  4444
//
//  Created by 艾艾广华 on 2023-02-15.
//

#import "PLSMacFunction.h"
#import <AppKit/AppKit.h>

@implementation PLSMacProcessHandle

@end

@implementation PLSMacFunction

+(NSString *)get_app_executable_dir {
    NSString *executablePath = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    return executablePath;
}

+(NSString *)get_app_process_name {
    NSString *processName = [NSProcessInfo processInfo].processName;
    return processName;
}

+ (NSTask *)create_process:(NSString *)app arguments:(NSArray *)arugments workDir:(NSString *)workDir error:(NSError **)error
{
	NSTask *task = [[NSTask alloc] init];
	[task setLaunchPath:app];
	[task setArguments:arugments];
	[task setCurrentDirectoryPath:workDir];
	[task launchAndReturnError:error];
	return task;
}

+(PLSMacProcessHandle *)create_process:(uint32_t)process_id {
    PLSMacProcessHandle *handle = [[PLSMacProcessHandle alloc] init];
    handle.errorCode = 0;
    handle.processId = process_id;
    return handle;
}

+(void)destroy_process:(PLSMacProcessHandle *)handle {
    [handle.task terminate];
    
}

+(void)wait_process:(PLSMacProcessHandle *)handle {
    [handle.task waitUntilExit];
}

+(uint32_t)get_process_id:(PLSMacProcessHandle *)handle {
    if (handle.processId > 0) {
        return handle.processId;
    }
    return handle.task.processIdentifier;
}

+(bool)process_is_running:(PLSMacProcessHandle *)handle {
    return handle.task.isRunning;
}

+(NSTaskTerminationReason)process_terminite_status:(PLSMacProcessHandle *)handle {
    return handle.task.terminationStatus;
}

@end
