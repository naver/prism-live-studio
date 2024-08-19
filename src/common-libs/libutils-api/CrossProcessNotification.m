#if __APPLE__
//
//  CrossProcessNotification.m
//  LaunchDaemon
//
//  Created by Zhong Ling on 2023/11/16.
//  Copyright © 2023 Example. All rights reserved.
//

#import "CrossProcessNotification.h"

#import <notify.h>

@interface CrossProcessNotification()

@property (nonatomic, assign) int token;
@property (nonatomic, strong) NSString *name;

@property (nonatomic, copy) ObserveCallback callback;

@end

@implementation CrossProcessNotification

+ (instancetype)defaultInstance {
	static CrossProcessNotification *sharedInstance = nil;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sharedInstance = [[self alloc] initWithName:@kPrismCurrentLensNotify];
	});
	return sharedInstance;
}

- (instancetype)initWithName:(NSString *)name {
	_token = 0;
	_name = [name copy];
	_callback = NULL;
	return self;
}

- (void)start {
	__weak CrossProcessNotification *weakSelf = self;
	notify_register_dispatch([_name UTF8String], &_token, dispatch_get_global_queue(0, 0), ^(int token) {
		__strong CrossProcessNotification *strongSelf = weakSelf;
		if (!strongSelf || !strongSelf.callback) return;
		
		uint64_t state = 3;
		notify_get_state(token, &state);
		strongSelf.callback(state);
	});
	
	assert(notify_is_valid_token(_token));
}

- (bool)isStarted {
	return notify_is_valid_token(_token);
}

- (void)stop {
	if (!notify_is_valid_token(self.token)) {
		return;
	}
	
	notify_cancel(self.token);
	
	self.token = 0;
}

- (bool)setState:(uint64_t)state {
	if (![self isStarted])
		return false;
	
	notify_set_state(self.token, state);
	notify_post([self.name UTF8String]);
	return true;
}

- (void)registerObserveCallback:(ObserveCallback)callback {
	self.callback = callback;
}

@end

#endif
