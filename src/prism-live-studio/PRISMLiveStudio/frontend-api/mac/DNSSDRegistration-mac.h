#if __APPLE__

#import <Foundation/Foundation.h>

#include <dns_sd.h>

#pragma mark * DNSSDRegistration

typedef void (^ServiceCompletion)(NSError *error);

@interface DNSSDRegistration : NSObject

///
/// Parameters
/// @domain: can be nil or empty
/// @name: can be nil or empty
/// @type: must be of the form "_foo._tcp." or "_foo._udp."
/// @port: must be in the range 1..65535.
///
/// WARNING:
/// @domain and @type should include the trailing dot; if they don't, one is added
/// and that change is reflected in the domain and type properties.
///
/// @domain and @type must not include a leading dot.
///
- (id)initWithDomain:(NSString *)domain type:(NSString *)type name:(NSString *)name port:(NSUInteger)port txtRecords:(NSDictionary *)txtRecords;

- (void)registerService;
- (void)unregisterService;

// MARK: -- init
@property (copy, readonly) NSString *domain;
@property (copy, readonly) NSString *type;
@property (copy, readonly) NSString *name;
@property (assign, readonly) NSUInteger port;
@property (copy, readonly) NSDictionary *txtRecords;

// MARK: -- changable
@property (copy, readwrite) NSString *registeredDomain;
@property (copy, readwrite) NSString *registeredName;
@property (assign, readwrite) DNSServiceRef sdRef;
@property (nonatomic) ServiceCompletion registerCompletion;

@end

#endif
