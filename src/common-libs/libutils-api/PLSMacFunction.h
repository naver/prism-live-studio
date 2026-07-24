//
//  PLSMacFunction.h
//  4444
//
//  Created by 艾艾广华 on 2023-02-15.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PLSMacAppWindowState) {
	PLSMacAppWindowStateNotRunning,
	PLSMacAppWindowStateRunningNoWindows,
	PLSMacAppWindowStateRunningWindowsHidden, // running, windows exist, none visible
	PLSMacAppWindowStateRunningWindowsVisible // running, at least one visible
};

@interface PLSMacWindowInfo : NSObject
@property (nonatomic, strong) NSString *windowName;
@property (nonatomic, strong) NSString *ownerName;
@property (nonatomic, assign) NSInteger windowPID;
@property (nonatomic, assign) NSInteger windowLayer;
@property (nonatomic, assign) NSInteger windowNumber;
@property (nonatomic, assign) BOOL onscreen;
@end

@interface PLSMacWindowDetectionResult : NSObject
@property (nonatomic, strong) NSString *bundleID;
@property (nonatomic, assign) PLSMacAppWindowState state;
@property (nonatomic, strong) NSArray<NSNumber *> *pids;
@property (nonatomic, strong) NSArray<PLSMacWindowInfo *> *visibleWindows;
@property (nonatomic, strong) NSArray<PLSMacWindowInfo *> *nonVisibleWindows;
@end

@interface PLSMacProcessHandle : NSObject
@property (nonatomic, strong) NSTask *task;
@property (nonatomic, assign) NSInteger errorCode;
@property (nonatomic, assign) uint32_t processId;
@end

@interface PLSMacProcessObserver : NSObject
- (instancetype)initWithApp:(NSRunningApplication *)app completion:(void (^)(BOOL launched))completion;
@end

@interface PLSMacFunction : NSObject

+ (NSString *)get_app_executable_dir;

+ (NSString *)get_app_process_name;

+ (NSTask *)create_process:(NSString *)app arguments:(NSArray *)arugments workDir:(NSString *)workDir error:(NSError **)error;

+ (void)destroy_process:(PLSMacProcessHandle *)handle;

+ (void)wait_process:(PLSMacProcessHandle *)handle;

+ (uint32_t)get_process_id:(PLSMacProcessHandle *)handle;

+ (bool)process_is_running:(PLSMacProcessHandle *)handle;

+ (NSTaskTerminationReason)process_terminite_status:(PLSMacProcessHandle *)handle;

+ (PLSMacWindowDetectionResult *)detectWindowStateForBundleID:(NSString *)bundleID;

@end

NS_ASSUME_NONNULL_END
