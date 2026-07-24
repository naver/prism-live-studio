//
//  PLSMacFunction.m
//  4444
//
//  Created by 艾艾广华 on 2023-02-15.
//

#import "PLSMacFunction.h"
#import <AppKit/AppKit.h>
#include <CoreGraphics/CGWindow.h>

@implementation PLSMacWindowInfo
@end

@implementation PLSMacWindowDetectionResult
@end

@implementation PLSMacProcessHandle

@end

@interface PLSMacProcessObserver ()
@property (nonatomic, strong) NSRunningApplication *app;
@property (nonatomic, strong) NSTimer *timer;
@property (nonatomic, copy) void (^handler)(BOOL launched);
@end

static NSMutableArray<PLSMacProcessObserver *> *observers;

@implementation PLSMacProcessObserver

- (instancetype)initWithApp:(NSRunningApplication *)app completion:(void (^)(BOOL launched))completion
{
	self = [super init];
	if (self) {
		self.app = app;
		self.handler = completion;

		if ([NSThread isMainThread]) {
			self.timer = [NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(checkTerminated:) userInfo:nil repeats:YES];
			[app addObserver:self forKeyPath:@"finishedLaunching" options:NSKeyValueObservingOptionNew context:nil];
			[app addObserver:self forKeyPath:@"terminated" options:NSKeyValueObservingOptionNew context:nil];

			if (observers == nil) {
				observers = [NSMutableArray array];
			}
			[observers addObject:self];
		} else {
			dispatch_sync(dispatch_get_main_queue(), ^{
				self.timer = [NSTimer scheduledTimerWithTimeInterval:0.2 target:self selector:@selector(checkTerminated:) userInfo:nil repeats:YES];
				[app addObserver:self forKeyPath:@"finishedLaunching" options:NSKeyValueObservingOptionNew context:nil];
				[app addObserver:self forKeyPath:@"terminated" options:NSKeyValueObservingOptionNew context:nil];

				if (observers == nil) {
					observers = [NSMutableArray array];
				}
				[observers addObject:self];
			});
		}
	}
	return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
	if ([object isKindOfClass:[NSRunningApplication class]]) {
		NSRunningApplication *app = object;
		if (app.finishedLaunching || app.terminated) {
			if ([NSThread isMainThread]) {
				[self complete];
			} else {
				dispatch_sync(dispatch_get_main_queue(), ^{
					[self complete];
				});
			}
		}
	}
}

- (void)checkTerminated:(NSTimer *)timer
{
	if (self.app.terminated) {
		if ([NSThread isMainThread]) {
			[self complete];
		} else {
			dispatch_sync(dispatch_get_main_queue(), ^{
				[self complete];
			});
		}
	}
}

- (void)complete
{
	if (self.handler) {
		[self.timer invalidate];
		self.timer = nil;

		[self.app removeObserver:self forKeyPath:@"finishedLaunching"];
		[self.app removeObserver:self forKeyPath:@"terminated"];
		[observers removeObject:self];

		self.handler(self.app.finishedLaunching);
		self.handler = nil;
	}
}

@end

@implementation PLSMacFunction

+ (NSString *)get_app_executable_dir
{
	NSString *executablePath = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
	return executablePath ?: @"";
}

+ (NSString *)get_app_process_name
{
	NSString *processName = [NSProcessInfo processInfo].processName;
	return processName ?: @"";
}

NSString *findFirstAppPathRobust(NSString *executablePath)
{
	if (!executablePath)
		return @"";
	NSString *normalizedPath = [executablePath stringByStandardizingPath];
	NSArray *components = [normalizedPath pathComponents];
	for (NSInteger i = 0; i < components.count; i++) {
		NSString *component = components[i];

		if ([component.pathExtension caseInsensitiveCompare:@"app"] == NSOrderedSame) {
			NSArray *appComponents = [components subarrayWithRange:NSMakeRange(0, i + 1)];
			return [NSString pathWithComponents:appComponents];
		}
	}
	return @"";
}

+ (NSTask *)create_process:(NSString *)app arguments:(NSArray *)arugments workDir:(NSString *)workDir error:(NSError **)error
{
	BOOL isInherited = false;
	NSString *currentPath = findFirstAppPathRobust([NSBundle mainBundle].executablePath);
	NSString *appNormalizedPath = [app stringByStandardizingPath];
	if (currentPath.length == 0 || appNormalizedPath.length == 0) {
		isInherited = false;
	} else if ([appNormalizedPath hasPrefix:currentPath]) {
		isInherited = true;
	}

	NSTask *task = [[NSTask alloc] init];

	if (!isInherited) {
		NSMutableDictionary *environment = [NSMutableDictionary dictionary];
		[environment setObject:@"/usr/bin:/bin" forKey:@"PATH"];
		[environment setObject:@"" forKey:@"DYLD_LIBRARY_PATH"];
		[task setEnvironment:environment];
	}
	[task setLaunchPath:app];
	[task setArguments:arugments];
	[task setCurrentDirectoryPath:workDir];
	[task launchAndReturnError:error];
	return task;
}

+ (PLSMacProcessHandle *)create_process:(uint32_t)process_id
{
	PLSMacProcessHandle *handle = [[PLSMacProcessHandle alloc] init];
	handle.errorCode = 0;
	handle.processId = process_id;
	return handle;
}

+ (void)destroy_process:(PLSMacProcessHandle *)handle
{
	[handle.task terminate];
}

+ (void)wait_process:(PLSMacProcessHandle *)handle
{
	[handle.task waitUntilExit];
}

+ (uint32_t)get_process_id:(PLSMacProcessHandle *)handle
{
	if (handle.processId > 0) {
		return handle.processId;
	}
	return handle.task.processIdentifier;
}

+ (bool)process_is_running:(PLSMacProcessHandle *)handle
{
	return handle.task.isRunning;
}

+ (NSTaskTerminationReason)process_terminite_status:(PLSMacProcessHandle *)handle
{
	return handle.task.terminationStatus;
}

+ (PLSMacWindowDetectionResult *)detectWindowStateForBundleID:(NSString *)bundleID
{
	PLSMacWindowDetectionResult *result = [[PLSMacWindowDetectionResult alloc] init];
	result.bundleID = bundleID;
	result.visibleWindows = @[];
	result.nonVisibleWindows = @[];
	result.pids = @[];

	if (!bundleID || bundleID.length == 0) {
		NSLog(@"Bundle ID is nil or empty");
		result.state = PLSMacAppWindowStateNotRunning;
		return result;
	}

	// Get all running applications with the specified bundle ID
	NSArray<NSRunningApplication *> *runningApps = [NSRunningApplication runningApplicationsWithBundleIdentifier:bundleID];
	if (runningApps.count == 0) {
		NSLog(@"No running applications found for bundle ID: %@", bundleID);
		result.state = PLSMacAppWindowStateNotRunning;
		return result;
	}

	// Collect PIDs of all running instances
	NSMutableSet<NSNumber *> *targetPIDs = [[NSMutableSet alloc] init];
	NSMutableArray<NSNumber *> *pidsArray = [[NSMutableArray alloc] init];
	for (NSRunningApplication *app in runningApps) {
		NSNumber *pid = @(app.processIdentifier);
		[targetPIDs addObject:pid];
		[pidsArray addObject:pid];
	}
	result.pids = [pidsArray copy];

	NSLog(@"Found %lu running instances of %@", (unsigned long)runningApps.count, bundleID);

	// Get ALL windows (including hidden/minimized) using CGWindowListCopyWindowInfo
	CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
	if (!windowList) {
		NSLog(@"Failed to get window list");
		result.state = PLSMacAppWindowStateRunningNoWindows;
		return result;
	}

	NSArray *windows = (__bridge NSArray *)windowList;
	NSMutableArray<PLSMacWindowInfo *> *visibleWindowInfos = [[NSMutableArray alloc] init];
	NSMutableArray<PLSMacWindowInfo *> *nonVisibleWindowInfos = [[NSMutableArray alloc] init];

	for (NSDictionary *window in windows) {
		// Get window information
		NSNumber *windowPID = [window objectForKey:(NSString *)kCGWindowOwnerPID];

		// Only include windows from the target application(s)
		if (![targetPIDs containsObject:windowPID]) {
			continue;
		}

		NSString *windowName = [window objectForKey:(NSString *)kCGWindowName];
		NSString *ownerName = [window objectForKey:(NSString *)kCGWindowOwnerName];
		NSNumber *windowLayer = [window objectForKey:(NSString *)kCGWindowLayer];
		NSNumber *windowNumber = [window objectForKey:(NSString *)kCGWindowNumber];
		NSNumber *isOnScreen = [window objectForKey:(NSString *)kCGWindowIsOnscreen];

		PLSMacWindowInfo *windowInfo = [[PLSMacWindowInfo alloc] init];
		windowInfo.windowName = windowName;
		windowInfo.ownerName = ownerName;
		windowInfo.windowPID = [windowPID integerValue];
		windowInfo.windowLayer = [windowLayer integerValue];
		windowInfo.windowNumber = [windowNumber integerValue];
		windowInfo.onscreen = [isOnScreen boolValue];

		if (windowInfo.onscreen) {
			[visibleWindowInfos addObject:windowInfo];
		} else {
			[nonVisibleWindowInfos addObject:windowInfo];
		}
	}

	CFRelease(windowList);

	result.visibleWindows = [visibleWindowInfos copy];
	result.nonVisibleWindows = [nonVisibleWindowInfos copy];

	// Determine the final state
	NSInteger totalWindows = visibleWindowInfos.count + nonVisibleWindowInfos.count;
	if (visibleWindowInfos.count > 0) {
		result.state = PLSMacAppWindowStateRunningWindowsVisible;
	} else if (totalWindows > 0) {
		result.state = PLSMacAppWindowStateRunningWindowsHidden;
	} else {
		result.state = PLSMacAppWindowStateRunningNoWindows;
	}

	NSLog(@"Found %lu visible and %lu non-visible windows for bundle ID %@ (state: %ld)", (unsigned long)visibleWindowInfos.count, (unsigned long)nonVisibleWindowInfos.count, bundleID,
	      (long)result.state);

	return result;
}

+ (NSArray<PLSMacWindowInfo *> *)detectWindowsForBundleID:(NSString *)bundleID
{
	PLSMacWindowDetectionResult *result = [PLSMacFunction detectWindowStateForBundleID:bundleID];
	return result.visibleWindows;
}

@end
