#import <AppKit/AppKit.h>
#include <liblog.h>
#include "PLSMacProductProcessMonitor.h"
#include "PLSEvents.h"
#include "libutils-api-mac.h"

@interface PLSMacProductProcessMonitor : NSObject
@end

@implementation PLSMacProductProcessMonitor {
	BOOL _monitoring;
	NSString *_prismBundleId;
	NSString *_lensBundleId;
	NSMutableDictionary<NSNumber *, NSRunningApplication *> *_prismAppsByPid;
	NSMutableDictionary<NSNumber *, NSRunningApplication *> *_lensAppsByPid;
}

+ (instancetype)sharedInstance
{
	static PLSMacProductProcessMonitor *sharedSingleton = nil;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sharedSingleton = [[self alloc] init];
	});
	return sharedSingleton;
}
+ (bool)isAppMainExe:(NSRunningApplication *)runningApp
{
	///Applications/PRISMLiveStudio.app/Contents/MacOS/PRISMLogger will return false
	NSBundle *bundle = [NSBundle bundleWithURL:runningApp.bundleURL];
	NSString *mainName = [bundle objectForInfoDictionaryKey:@"CFBundleExecutable"];
	NSString *execName = runningApp.executableURL.lastPathComponent;
	return [execName isEqualToString:mainName];
}

- (BOOL)isPRISMLiveStudio:(NSRunningApplication *)app
{
	if (![app.bundleIdentifier isEqualToString:self->_prismBundleId]) {
		return NO;
	}
	return [PLSMacProductProcessMonitor isAppMainExe:app];
}

- (BOOL)isPRISMLens:(NSRunningApplication *)app
{
	if (![app.bundleIdentifier isEqualToString:self->_lensBundleId]) {
		return NO;
	}
	return [PLSMacProductProcessMonitor isAppMainExe:app];
}

- (void)removeTerminatedAppsFrom:(NSMutableDictionary<NSNumber *, NSRunningApplication *> *)dict
{
	NSMutableArray<NSNumber *> *stale = [NSMutableArray array];
	[dict enumerateKeysAndObjectsUsingBlock:^(NSNumber *pid, NSRunningApplication *ra, BOOL *stop) {
		if (!ra || ra.isTerminated) {
			[stale addObject:pid];
		}
	}];
	for (NSNumber *pid in stale) {
		[dict removeObjectForKey:pid];
	}
}

- (instancetype)init
{
	self = [super init];
	if (!self) {
		return nil;
	}
	_prismAppsByPid = [NSMutableDictionary dictionary];
	_lensAppsByPid = [NSMutableDictionary dictionary];
	_prismBundleId = [NSString stringWithUTF8String:pls::KIdentifier_PRISM];
	_lensBundleId = [NSString stringWithUTF8String:pls::KIdentifier_LENS];

	void (^start)(void) = ^{
		if (self->_monitoring) {
			return;
		}
		self->_monitoring = YES;
		NSNotificationCenter *nc = NSWorkspace.sharedWorkspace.notificationCenter;
		[nc addObserver:self selector:@selector(onAppLaunched:) name:NSWorkspaceDidLaunchApplicationNotification object:nil];
		[nc addObserver:self selector:@selector(onAppTerminated:) name:NSWorkspaceDidTerminateApplicationNotification object:nil];
		@synchronized(self) {
			[self->_prismAppsByPid removeAllObjects];
			[self->_lensAppsByPid removeAllObjects];
			for (NSRunningApplication *app in NSWorkspace.sharedWorkspace.runningApplications) {
				if (app.bundleIdentifier.length == 0) {
					continue;
				}
				if ([self isPRISMLiveStudio:app]) {
					self->_prismAppsByPid[@(app.processIdentifier)] = app;
				} else if ([self isPRISMLens:app]) {
					self->_lensAppsByPid[@(app.processIdentifier)] = app;
				}
			}
			PLS_DEBUG("MacProcessMonitor", "[MacProcessMonitor] baseline snapshot prism apps %lu, lens apps %lu", (unsigned long)self->_prismAppsByPid.count,
				  (unsigned long)self->_lensAppsByPid.count);
		}
	};
	if ([NSThread isMainThread]) {
		start();
	} else {
		dispatch_sync(dispatch_get_main_queue(), start);
	}
	return self;
}

- (void)applyRunningApplication:(NSRunningApplication *)app delta:(int)delta
{
	if (!app) {
		return;
	}
	NSNumber *pidKey = @(app.processIdentifier);
	bool isSendPrism = false;
	bool isSendLens = false;
	@synchronized(self) {
		if (delta > 0) {
			if ([self isPRISMLiveStudio:app]) {
				self->_prismAppsByPid[pidKey] = app;
			} else if ([self isPRISMLens:app]) {
				self->_lensAppsByPid[pidKey] = app;
			}
		} else if (delta < 0) {
			auto oldPrismCount = (unsigned long)self->_prismAppsByPid.count;
			auto oldLensCount = (unsigned long)self->_lensAppsByPid.count;
			[self->_prismAppsByPid removeObjectForKey:pidKey];
			[self->_lensAppsByPid removeObjectForKey:pidKey];
			if (oldPrismCount > 0 && (unsigned long)self->_prismAppsByPid.count == 0) {
				isSendPrism = true;
			} else if (oldLensCount > 0 && (unsigned long)self->_lensAppsByPid.count == 0) {
				isSendLens = true;
			}
		}
		const char *bid = app.bundleIdentifier.length ? [app.bundleIdentifier UTF8String] : "";
		PLS_DEBUG("MacProcessMonitor", "[MacProcessMonitor] apply app bundleId %s, pid %d, delta %d prism apps %lu, lens apps %lu", bid, app.processIdentifier, delta,
			  (unsigned long)self->_prismAppsByPid.count, (unsigned long)self->_lensAppsByPid.count);
	}
	if (isSendPrism) {
		emit PLS_EVENTS->processAppExit((int)pls_product_type_t::Prism);
	} else if (isSendLens) {
		emit PLS_EVENTS->processAppExit((int)pls_product_type_t::Lens);
	}
}

- (void)onAppLaunched:(NSNotification *)notification
{
	NSRunningApplication *app = notification.userInfo[NSWorkspaceApplicationKey];
	[self applyRunningApplication:app delta:1];
}

- (void)onAppTerminated:(NSNotification *)notification
{
	NSRunningApplication *app = notification.userInfo[NSWorkspaceApplicationKey];
	[self applyRunningApplication:app delta:-1];
}

- (BOOL)isRunningForProduct:(pls_product_type_t)product
{
	BOOL running = NO;
	@synchronized(self) {
		[self removeTerminatedAppsFrom:_prismAppsByPid];
		[self removeTerminatedAppsFrom:_lensAppsByPid];
		switch (product) {
		case pls_product_type_t::Prism:
			running = _prismAppsByPid.count > 0;
			break;
		case pls_product_type_t::Lens:
			running = _lensAppsByPid.count > 0;
			break;
		default:
			break;
		}
	}
	PLS_DEBUG("MacProcessMonitor", "[MacProcessMonitor] isRunningForProduct product %d running %d", static_cast<int>(product), (int)running);
	return running;
}

- (BOOL)isRunningForPid:(uint32_t)pid
{
	if (pid == 0) {
		return NO;
	}
	NSNumber *pidKey = @(pid);
	BOOL running = NO;
	@synchronized(self) {
		[self removeTerminatedAppsFrom:_prismAppsByPid];
		[self removeTerminatedAppsFrom:_lensAppsByPid];
		NSRunningApplication *prismApp = _prismAppsByPid[pidKey];
		NSRunningApplication *lensApp = _lensAppsByPid[pidKey];
		running = (prismApp != nil && !prismApp.isTerminated) || (lensApp != nil && !lensApp.isTerminated);
	}
	return running;
}

- (void)dealloc
{
	if (_monitoring) {
		[NSWorkspace.sharedWorkspace.notificationCenter removeObserver:self];
	}
}

@end

namespace pls {
namespace mac {

bool is_product_process_running(pls_product_type_t product)
{
	return [[PLSMacProductProcessMonitor sharedInstance] isRunningForProduct:product];
}

bool is_product_process_running_for_pid(std::uint32_t pid)
{
	return [[PLSMacProductProcessMonitor sharedInstance] isRunningForPid:pid];
}
bool pls_is_app_exited(pls_process_t *process)
{
	return ![[PLSMacProductProcessMonitor sharedInstance] isRunningForPid:pls_process_id(process)];
}
}
}
