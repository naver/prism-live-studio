//
//  PLSMacFunction.h
//  4444
//
//  Created by 艾艾广华 on 2023-02-15.
//

#import <Foundation/Foundation.h>


NS_ASSUME_NONNULL_BEGIN

@interface PLSMacProcessHandle : NSObject
@property (nonatomic, strong) NSTask *task;
@property (nonatomic, assign) NSInteger errorCode;
@property (nonatomic, assign) uint32_t processId;
@end

@interface PLSMacFunction : NSObject

+(NSString *)get_app_executable_dir;

+(NSString *)get_app_process_name;

+(NSTask *)create_process:(NSString *)app
                arguments:(NSArray *)arugments
                  workDir:(NSString *)workDir
                    error:(NSError **)error;

+(void)destroy_process:(PLSMacProcessHandle *)handle;

+(void)wait_process:(PLSMacProcessHandle *)handle;

+(uint32_t)get_process_id:(PLSMacProcessHandle *)handle;

+(bool)process_is_running:(PLSMacProcessHandle *)handle;

+(NSTaskTerminationReason)process_terminite_status:(PLSMacProcessHandle *)handle;

@end

NS_ASSUME_NONNULL_END
