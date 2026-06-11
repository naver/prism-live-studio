#if __APPLE__
//
//  CrossProcessNotification.h
//  LaunchDaemon
//
//  Created by Zhong Ling on 2023/11/16.
//  Copyright © 2023 Example. All rights reserved.
//

#import <Foundation/Foundation.h>

#define kPrismCurrentLensNotify "com.prism.currentlens.notify"

NS_ASSUME_NONNULL_BEGIN

typedef void (^ObserveCallback)(uint64_t state);

@interface CrossProcessNotification : NSObject

+ (instancetype)defaultInstance;

- (instancetype)initWithName:(NSString *)name;

- (void)start;

- (bool)isStarted;

- (void)stop;

- (bool)setState:(uint64_t)state;

- (void)registerObserveCallback:(ObserveCallback)callback;

@end

NS_ASSUME_NONNULL_END

#endif
