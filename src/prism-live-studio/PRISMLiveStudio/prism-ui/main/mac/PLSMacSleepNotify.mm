
#include "PLSMacSleepNotify.h"
#include <AppKit/AppKit.h>
#include <liblog.h>
#include <log/module_names.h>
#include <QMetaObject>

@interface PLSMacSleepNotifyImpl : NSObject
@property (nonatomic, assign) PLSMacSleepNotify *qtObject;
@end

@implementation PLSMacSleepNotifyImpl

- (instancetype)init {
	self = [super init];
	if (self) {
		[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
															   selector:@selector(systemWillSleep:)
																   name:NSWorkspaceWillSleepNotification
																 object:nil];
		
		[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
															   selector:@selector(systemDidWake:)
																   name:NSWorkspaceDidWakeNotification
																 object:nil];
	}
	return self;
}

- (void)dealloc {
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
	[super dealloc];
}

- (void)systemWillSleep:(NSNotification *)notification {
	Q_UNUSED(notification);
	PLS_INFO(MAINFRAME_MODULE, "[PC STATE] macOS enter sleep");
	if (self.qtObject) {
		// NSWorkspace notifications are sent on the main thread, can emit signal directly
		Q_EMIT self.qtObject->systemWillSleep();
	}
}

- (void)systemDidWake:(NSNotification *)notification {
	Q_UNUSED(notification);
	PLS_INFO(MAINFRAME_MODULE, "[PC STATE] macOS wake up");
	if (self.qtObject) {
		// NSWorkspace notifications are sent on the main thread, can emit signal directly
		Q_EMIT self.qtObject->systemDidWake();
	}
}

@end

PLSMacSleepNotify::PLSMacSleepNotify(QObject *parent)
	: QObject(parent)
{
	PLSMacSleepNotifyImpl *impl = [[PLSMacSleepNotifyImpl alloc] init];
	impl.qtObject = this;
	// alloc/init returns object with retain count = 1, we own it
	// No need to retain, just store the pointer
	m_impl = (void *)impl;
}

PLSMacSleepNotify::~PLSMacSleepNotify()
{
	if (m_impl) {
		PLSMacSleepNotifyImpl *impl = (PLSMacSleepNotifyImpl *)m_impl;
		[impl release];
		m_impl = nullptr;
	}
}

