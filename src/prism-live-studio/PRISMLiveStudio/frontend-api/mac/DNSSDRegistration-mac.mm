#if __APPLE__

#import "DNSSDRegistration-mac.h"

#include "DNSSDRegistrationObject-C-Interface.hpp"

typedef void (^RetainSelfBlock)(void);
@interface DNSSDRegistration ()
- (void)breakRetainCycly;
- (void)stopWithError:(NSError *)error notify:(BOOL)notify;
@end

@implementation DNSSDRegistration {
	RetainSelfBlock _retainBlock;
}

DNSSDRegistrationImpl::DNSSDRegistrationImpl() : self(nullptr) {}

DNSSDRegistrationImpl::~DNSSDRegistrationImpl(void)
{
	[(__bridge id)self breakRetainCycly];
}

void DNSSDRegistrationImpl::init(const std::string &domain, const std::string &type, const std::string &name, int port, const std::map<std::string, std::string> &txtRecords)
{
	NSMutableDictionary *txtDicts = [NSMutableDictionary dictionary];
	for (const auto &it : txtRecords) {
		NSString *key = [NSString stringWithCString:it.first.c_str() encoding:[NSString defaultCStringEncoding]];
		NSString *value = [NSString stringWithCString:it.second.c_str() encoding:[NSString defaultCStringEncoding]];
		txtDicts[key] = value;
	}

	DNSSDRegistration *object = [[DNSSDRegistration alloc] initWithDomain:[NSString stringWithUTF8String:domain.c_str()]
									 type:[NSString stringWithUTF8String:type.c_str()]
									 name:[NSString stringWithUTF8String:name.c_str()]
									 port:(NSUInteger)port
								   txtRecords:txtDicts];
	registerCB = nullptr;

	object->_registerCompletion = [^(NSError *e) {
		if (registerCB) {
			registerCB(e == nullptr);
		}
	} copy];
	object->_retainBlock = [^{ //
		[object class];
	} copy];

	self = (__bridge void *)object;

	NSLog(@"%p", self);
}

void DNSSDRegistrationImpl::registerService(CallbackFunction cb)
{
	NSLog(@"%p", self);
	registerCB = cb;

	[(__bridge id)self registerService];
}

void DNSSDRegistrationImpl::unregisterService()
{
	NSLog(@"%p", self);
	[(__bridge id)self unregisterService];
}

#pragma mark * DNSSDRegistration

@synthesize domain = domain_;
@synthesize type = type_;
@synthesize name = name_;
@synthesize port = port_;
@synthesize txtRecords = txtRecords_;
@synthesize registeredDomain = registeredDomain_;
@synthesize registeredName = registeredName_;
@synthesize sdRef = sdRef_;

- (id)initWithDomain:(NSString *)domain type:(NSString *)type name:(NSString *)name port:(NSUInteger)port txtRecords:(NSDictionary *)txtRecords
{
	// domain may be nil or empty
	assert(![domain hasPrefix:@"."]);
	assert(type != nil);
	assert([type length] != 0);
	assert(![type hasPrefix:@"."]);
	assert(port > 0);
	assert(port < 65536);

	self = [super init];
	if (self != nil) {
		if (([domain length] != 0) && ![domain hasSuffix:@"."]) {
			domain = [domain stringByAppendingString:@"."];
		}
		if (![type hasSuffix:@"."]) {
			type = [type stringByAppendingString:@"."];
		}
		self->domain_ = [domain copy];
		self->type_ = [type copy];
		self->name_ = [name copy];
		self->port_ = port;
		self->txtRecords_ = txtRecords;
	}
	return self;
}

- (void)dealloc
{
	if (self->sdRef_ != NULL) {
		DNSServiceRefDeallocate(self->sdRef_);
	}

#if !__has_feature(objc_arc)
	[domain_ release];
	[type_ release];
	[name_ release];
	[txtRecords_ release];
	[registeredDomain_ release];
	[registeredName_ release];

	[super dealloc];
#endif
}

- (void)didRegisterWithDomain:(NSString *)domain name:(NSString *)name
// Called when DNS-SD tells us that a registration has been added.
{
	// On the Mac this routine can get called multiple times, once for the "local." domain and again
	// for wide-area domains.  As a matter of policy we ignore everything except the "local."
	// domain.  The "local." domain is really the flagship domain here; that's what our clients
	// care about.  If it works but a wide-area registration fails, we don't want to error out.
	// Conversely, if a wide-area registration succeeds but the "local." domain fails, that
	// is a good reason to fail totally.

	assert([domain caseInsensitiveCompare:@"local"] != NSOrderedSame); // DNS-SD always gives us the trailing dot; complain otherwise.

	if ([domain caseInsensitiveCompare:@"local."] == NSOrderedSame) {
		self.registeredDomain = domain;
		self.registeredName = name;
		NSLog(@"domain %@ name %@ registered.", domain, name);
	}
}

- (void)didUnregisterWithDomain:(NSString *)domain name:(NSString *)name
// Called when DNS-SD tells us that a registration has been removed.
{
#pragma unused(name)

	// The registration can be removed in the following cases:
	//
	// A. When you register with the default name (that is, passing the empty string
	//    to the "name" parameter of DNSServiceRegister) /and/ you specify the 'no rename'
	//    flags (kDNSServiceFlagsNoAutoRename) /and/ the computer name changes.
	//
	// B. When you successfully register in a domain and then the domain becomes unavailable
	//    (for example, if you turn off Back to My Mac after registering).
	//
	// Case B we ignore based on the same policy outlined in -didRegisterWithDomain:name:.
	//
	// Case A is interesting and we handle it here.

	assert([domain caseInsensitiveCompare:@"local"] != NSOrderedSame); // DNS-SD always gives us the trailing dot; complain otherwise.

	if ([domain caseInsensitiveCompare:@"local."] == NSOrderedSame) {
		[self stopWithError:[NSError errorWithDomain:NSNetServicesErrorDomain code:kDNSServiceErr_NameConflict userInfo:nil] notify:YES];
	}
}

static void RegisterReplyCallback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, void *context)
{
	assert([NSThread isMainThread]);

	if ((flags & kDNSServiceFlagsAdd) == 0) {
		return;
	}

	DNSSDRegistration *obj = (__bridge DNSSDRegistration *)context;
	assert([obj isKindOfClass:[DNSSDRegistration class]]);
	assert(sdRef == obj->sdRef_);
	assert([[NSString stringWithUTF8String:regtype] caseInsensitiveCompare:obj.type] == NSOrderedSame);

	if (obj->_registerCompletion == NULL) {
		return;
	}

	if (errorCode == kDNSServiceErr_NoError) {
		obj.registerCompletion(nil);
		[obj didRegisterWithDomain:[NSString stringWithUTF8String:domain] name:[NSString stringWithUTF8String:name]];
	} else {
		auto error = [NSError errorWithDomain:NSNetServicesErrorDomain code:errorCode userInfo:nil];
		[obj stopWithError:error notify:YES];
		obj.registerCompletion(error);
	}
}

- (void)registerService
{
	if (self.sdRef != nullptr) {
		self.registerCompletion([[NSError alloc] initWithDomain:@"mdns" code:-1 userInfo:NULL]);
		return;
	}

	NSString *name;
	NSString *domain;
	DNSServiceErrorType errorCode;

	domain = self.domain;
	if (domain == nil) {
		domain = @"";
	}
	name = self.name;
	if (name == nil) {
		name = @"";
	}

	NSData *txtData = [NSNetService dataFromTXTRecordDictionary:txtRecords_];

	errorCode = DNSServiceRegister(&self->sdRef_, 0, kDNSServiceInterfaceIndexAny, [name UTF8String], [self.type UTF8String], [domain UTF8String], NULL, htons(self.port), txtData.length,
				       txtData.bytes, RegisterReplyCallback, (__bridge void *)(self));
	if (errorCode == kDNSServiceErr_NoError) {
		errorCode = DNSServiceSetDispatchQueue(self.sdRef, dispatch_get_main_queue());
	}
	if (errorCode == kDNSServiceErr_NoError) {
		NSLog(@"Will register");
	} else {
		[self stopWithError:[NSError errorWithDomain:NSNetServicesErrorDomain code:errorCode userInfo:nil] notify:YES];
	}
}

- (void)stopWithError:(NSError *)error notify:(BOOL)notify
{
	if (notify) {
		if (error != nil) {
			NSLog(@"failed to register, error is %@", error);
			self.registerCompletion(error);
		}
	}
	self.registeredDomain = nil;
	self.registeredName = nil;
	if (self.sdRef != NULL) {
		DNSServiceRefDeallocate(self.sdRef);
		self.sdRef = NULL;
	}
	if (notify) {
		NSLog(@"unregistered");
	}
}

- (void)unregisterService
{
	[self stopWithError:nil notify:NO];
}

//打破循环引用，释放对象
- (void)breakRetainCycly
{
	_retainBlock = nil;
}

@end

#endif
