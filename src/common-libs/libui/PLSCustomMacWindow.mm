#include <AppKit/AppKit.h>
#include "PLSCustomMacWindow.h"
#include <libutils-api.h>
#include <QTimer>

#define INVALID_WIN_ID 0

static NSRect m_closeButtonOriginRect;
static NSRect m_minButtonOriginRect;
static NSRect m_maxButtonOriginRect;

@interface WAYView : NSView

@end

@implementation WAYView

+ (float)defaultTitleBarHeight
{
	NSRect frame = NSMakeRect(0, 0, 800, 600);
	NSRect contentRect = [NSWindow contentRectForFrameRect:frame styleMask:NSWindowStyleMaskTitled];
	return NSHeight(frame) - NSHeight(contentRect);
}

@end

@interface PLSNotification : NSObject

@end

@implementation PLSNotification

- (id)init
{
	self = [super init];
	if (self) {
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];

		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
	}
	return self;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	NSWindow *keyWindow = notification.object;
	NSLog(@"Window did become key: %@, userinfo is %@", keyWindow, notification.userInfo);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
	NSWindow *resignedWindow = notification.object;
	NSLog(@"Window did resign key: %@, userinfo is %@", resignedWindow, notification.userInfo);
}

@end

namespace Cocoa {

NSWindow *getMacWindow(QWidget *widget)
{
	if (!widget) {
		return nullptr;
	}
	if (widget->winId() == INVALID_WIN_ID) {
		return nullptr;
	}
	NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
	NSWindow *window = [view window];
	return window;
}

NSButton *getWindowButton(QWidget *widget, NSWindowButton windowButton)
{
	NSWindow *window = getMacWindow(widget);
	if (!window) {
		return nullptr;
	}
	NSButton *button = [window standardWindowButton:windowButton];
	return button;
}

void setWindowButtonHidden(QWidget *widget, bool hidden, NSWindowButton windowButton)
{
	NSButton *button = getWindowButton(widget, windowButton);
	if (!button) {
		return;
	}
	button.hidden = hidden;
}

void setWindowButtonEnabled(QWidget *widget, bool enabled, NSWindowButton windowButton)
{
	NSButton *button = getWindowButton(widget, windowButton);
	if (!button) {
		return;
	}
	button.enabled = enabled;
}

void setTitleAccessoryViewHidden(QWidget *widget, bool hidden)
{
	NSButton *button = getWindowButton(widget, NSWindowCloseButton);
	if (!button) {
		return;
	}
	NSWindow *window = getMacWindow(widget);
	if (!window) {
		return;
	}
	NSView *titlebarView = button.superview;
	titlebarView.hidden = hidden;
	if ([window.titlebarAccessoryViewControllers count] > 0) {
		NSTitlebarAccessoryViewController *vc = [window.titlebarAccessoryViewControllers objectAtIndex:0];
		vc.hidden = hidden;
	}
}

NSLayoutConstraint *layoutConstraintSimilarTo(NSView *installedView, NSLayoutConstraint *layoutConstraint)
{
	for (NSLayoutConstraint *existingConstraint in installedView.constraints.reverseObjectEnumerator) {
		if (![existingConstraint isKindOfClass:NSLayoutConstraint.class])
			continue;
		if (existingConstraint.firstItem != layoutConstraint.firstItem)
			continue;
		if (existingConstraint.secondItem != layoutConstraint.secondItem)
			continue;
		if (existingConstraint.firstAttribute != layoutConstraint.firstAttribute)
			continue;
		if (existingConstraint.secondAttribute != layoutConstraint.secondAttribute)
			continue;
		if (existingConstraint.relation != layoutConstraint.relation)
			continue;
		if (existingConstraint.multiplier != layoutConstraint.multiplier)
			continue;
		if (existingConstraint.priority != layoutConstraint.priority)
			continue;
		return existingConstraint;
	}
	return nil;
}

}

void PLSCustomMacWindow::initWinId(QWidget *widget)
{
	m_widget = widget;
	NSApplication *application = [NSApplication sharedApplication];
	NSAppearance *darkAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
	[application setAppearance:darkAppearance];
	if (m_hasCornerRadius) {
		setWindowCornerRadius(widget);
	}
	if (NSEqualSizes(m_closeButtonOriginRect.size, NSZeroSize)) {
		NSButton *button = Cocoa::getWindowButton(widget, NSWindowCloseButton);
		if (!button) {
			m_closeButtonOriginRect = NSMakeRect(7, 6, 14, 16);
			return;
		}
		m_closeButtonOriginRect = button.frame;
	}
	if (NSEqualSizes(m_minButtonOriginRect.size, NSZeroSize)) {
		NSButton *button = Cocoa::getWindowButton(widget, NSWindowMiniaturizeButton);
		if (!button) {
			m_minButtonOriginRect = NSMakeRect(27, 6, 14, 16);
			return;
		}
		m_minButtonOriginRect = button.frame;
	}
	if (NSEqualSizes(m_maxButtonOriginRect.size, NSZeroSize)) {
		NSButton *button = Cocoa::getWindowButton(widget, NSWindowZoomButton);
		if (!button) {
			m_maxButtonOriginRect = NSMakeRect(47, 6, 14, 16);
			return;
		}
		m_maxButtonOriginRect = button.frame;
	}
	updateTrafficButtonUI();
}

void PLSCustomMacWindow::setWidthResizeEnabled(QWidget *widget, bool widthResizeEnabled)
{
	if (!widthResizeEnabled) {
		QSize size = widget->size();
		widget->setMinimumWidth(size.width());
		widget->setMaximumWidth(size.width());
	}
}

void PLSCustomMacWindow::setHeightResizeEnabled(QWidget *widget, bool heightResizeEnabled)
{
	if (!heightResizeEnabled) {
		QSize size = widget->size();
		widget->setMinimumHeight(size.height());
		widget->setMaximumHeight(size.height());
	}
}

bool PLSCustomMacWindow::hasTitleBar(QWidget *widget)
{
	NSWindow *window = Cocoa::getMacWindow(widget);
	if (!window) {
		return false;
	}
	return window.hasTitleBar;
}

void PLSCustomMacWindow::setWindowCornerRadius(QWidget *widget)
{
	NSWindow *window = Cocoa::getMacWindow(widget);
	if (!window) {
		return;
	}
	NSView *contentView = window.contentView;
	contentView.wantsLayer = YES;
	contentView.layer.cornerRadius = 10.0;
	contentView.layer.masksToBounds = YES;
}

float PLSCustomMacWindow::getTitlebarHeight(QWidget *widget)
{
	return [WAYView defaultTitleBarHeight];
}

void PLSCustomMacWindow::updateTrafficButtonUI()
{
	if (!m_widget) {
		return;
	}

	NSButton *closeButton = Cocoa::getWindowButton(m_widget, NSWindowCloseButton);
	if (closeButton) {
		closeButton.hidden = m_closeButtonHidden;
	}

	NSButton *minButton = Cocoa::getWindowButton(m_widget, NSWindowMiniaturizeButton);
	if (minButton) {
		minButton.hidden = m_minButtonHidden;
	}

	NSButton *zoomButton = Cocoa::getWindowButton(m_widget, NSWindowZoomButton);
	if (zoomButton) {
		zoomButton.hidden = m_maxButtonHidden;
	}

	bool relayoutTrafficLight = false;
	if (m_closeButtonHidden && (!m_minButtonHidden || !m_maxButtonHidden)) {
		relayoutTrafficLight = true;
	}
	if (m_minButtonHidden && !m_maxButtonHidden) {
		relayoutTrafficLight = true;
	}

	if (!relayoutTrafficLight || (!closeButton && !minButton && !zoomButton)) {
		return;
	}

	NSMutableArray *buttons = [[NSMutableArray alloc] init];
	if (closeButton) {
		[buttons addObject:closeButton];
	}
	if (minButton) {
		[buttons addObject:minButton];
	}
	if (zoomButton) {
		[buttons addObject:zoomButton];
	}

	NSArray *positions = [NSArray arrayWithObjects:@(m_closeButtonOriginRect.origin.x), @(m_minButtonOriginRect.origin.x), @(m_maxButtonOriginRect.origin.x), nil];
	int positionIndex = 0;
	for (int i = 0; i < buttons.count; i++) {
		NSButton *button = buttons[i];
		if (button.hidden) {
			continue;
		}
		CGFloat start_x = [positions[positionIndex] floatValue];
		button.translatesAutoresizingMaskIntoConstraints = NO;
		NSView *titlebarView = button.superview;
		NSLayoutConstraint *layoutConstraint = [NSLayoutConstraint constraintWithItem:button
										    attribute:NSLayoutAttributeLeft
										    relatedBy:NSLayoutRelationEqual
										       toItem:titlebarView
										    attribute:NSLayoutAttributeLeft
										   multiplier:1
										     constant:start_x];
		NSLayoutConstraint *updateConstraint = Cocoa::layoutConstraintSimilarTo(titlebarView, layoutConstraint);
		if (updateConstraint) {
			updateConstraint.constant = start_x;
		} else {
			updateConstraint = layoutConstraint;
		}
		[NSLayoutConstraint activateConstraints:@[updateConstraint]];
		positionIndex++;
	}
}

PLSCustomMacWindow &PLSCustomMacWindow::setCornerRadius(bool hasCornerRadius)
{
	m_hasCornerRadius = hasCornerRadius;
	return *this;
}

PLSCustomMacWindow &PLSCustomMacWindow::setCloseButtonHidden(bool hidden)
{
	m_closeButtonHidden = hidden;
	if (m_widget) {
		Cocoa::setWindowButtonHidden(m_widget, hidden, NSWindowCloseButton);
	}
	return *this;
}

PLSCustomMacWindow &PLSCustomMacWindow::setMinButtonHidden(bool hidden)
{
	m_minButtonHidden = hidden;
	if (m_widget) {
		Cocoa::setWindowButtonHidden(m_widget, hidden, NSWindowMiniaturizeButton);
	}
	return *this;
}

PLSCustomMacWindow &PLSCustomMacWindow::setMaxButtonHidden(bool hidden)
{
	m_maxButtonHidden = hidden;
	if (m_widget) {
		Cocoa::setWindowButtonHidden(m_widget, hidden, NSWindowZoomButton);
	}
	return *this;
}

bool PLSCustomMacWindow::getCloseButtonHidden() const
{
	return m_closeButtonHidden;
}

bool PLSCustomMacWindow::getMinButtonHidden() const
{
	return m_minButtonHidden;
}

bool PLSCustomMacWindow::getMaxButtonHidden() const
{
	return m_maxButtonHidden;
}

void PLSCustomMacWindow::removeMacTitleBar(QWidget *widget)
{
	NSWindow *window = Cocoa::getMacWindow(widget);
	if (!window) {
		return;
	}

	NSView *titleBarView = [[window standardWindowButton:NSWindowMiniaturizeButton] superview];
	if (titleBarView.superview && !titleBarView.superview.hidden) {
		titleBarView.superview.hidden = YES;
		window.styleMask |= NSWindowStyleMaskFullSizeContentView;
	}
}
void PLSCustomMacWindow::moveToFullScreen(QWidget *widget, int screenIdx)
{
	NSWindow *window = Cocoa::getMacWindow(widget);
	if (!window) {
		return;
	}
	NSArray *screens = [NSScreen screens];
	if (screens.count <= screenIdx) {
		return;
	}

	if ((([window styleMask] & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen)) {
		[window toggleFullScreen:nil];
	}
	[window setFrame:[screens[screenIdx] visibleFrame] display:YES];
	[window toggleFullScreen:nil];
}
