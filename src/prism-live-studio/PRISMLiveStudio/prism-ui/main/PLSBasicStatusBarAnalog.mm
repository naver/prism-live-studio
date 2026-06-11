#ifdef __APPLE__

#include "PLSBasicStatusBar.hpp"

#include <sstream>

#include <Foundation/Foundation.h>

#include <dns_sd.h>
#include <qwebsocket.h>

#include <libutils-api.h>

// MARK: - DNSServiceResolver

class DNSServiceResolver final {
public:
	static void Resolve(std::function<void(uint16_t)> closure);

private:
	static void callback__(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullName, const char *hostName, uint16_t port,
			       uint16_t txtLen, const unsigned char *txtRecord, void *context);

private:
	static uint16_t port_;
	static std::function<void(uint16_t)> closure_;
	static DNSServiceRef sd_ref_;
	static dispatch_queue_t sd_queue_;
};

uint16_t DNSServiceResolver::port_ = 0;
std::function<void(uint16_t)> DNSServiceResolver::closure_ = nullptr;
DNSServiceRef DNSServiceResolver::sd_ref_ = nullptr;
dispatch_queue_t DNSServiceResolver::sd_queue_ = dispatch_queue_create("prism-extension-client-info-resolver-Serial", nullptr);

@implementation NSHost (safeHostName)

+ (NSString *)getSafeHostName
{
	@try {
		static dispatch_once_t onceToken;
		static NSString *cachedHostName = NULL;

		dispatch_once(&onceToken, ^{ // Access ONLY once
			dispatch_semaphore_t sema = dispatch_semaphore_create(0);
			dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
				NSLog(@"getSafeHostName");
				@try {
					NSHost *host = [NSHost currentHost];
					cachedHostName = host ? [host name] : NULL;
				} @catch (NSException *exception) {
					NSLog(@"Failed to get host name: %@", exception);
				}
				dispatch_semaphore_signal(sema);
			});
			dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
		});

		return cachedHostName;
	} @catch (NSException *exception) {
		NSLog(@"Failed to get host name: %@", exception);
		return NULL;
	}
}

@end

void DNSServiceResolver::Resolve(std::function<void(uint16_t)> closure)
{
	if (port_) {
		closure(port_);
		return;
	}

	port_ = 0;
	closure_ = closure;
	if (sd_ref_) {
		DNSServiceRefDeallocate(sd_ref_);
		sd_ref_ = nullptr;
	}

	NSString *name = [NSHost getSafeHostName];
	if (!name) {
		if (sd_ref_) {
			DNSServiceRefDeallocate(sd_ref_);
			sd_ref_ = nullptr;
		}
		if (closure_)
			closure_(0);
		closure_ = nullptr;
		return;
	}
	name = (name.length != 0) ? [name stringByAppendingString:@".prism-vcam-info"] : @"prism-vcam-info";
	NSString *type = @"_prismvcam._tcp";
	NSString *domain = @"local";

	DNSServiceErrorType dnsError = DNSServiceResolve(&sd_ref_, 0, kDNSServiceInterfaceIndexAny, [name UTF8String], [type UTF8String], [domain UTF8String], DNSServiceResolver::callback__, nullptr);
	if (dnsError != kDNSServiceErr_NoError) {
		if (sd_ref_) {
			DNSServiceRefDeallocate(sd_ref_);
			sd_ref_ = nullptr;
		}
		if (closure_)
			closure_(0);
		closure_ = nullptr;
		return;
	}

	dnsError = DNSServiceSetDispatchQueue(sd_ref_, sd_queue_);
	if (dnsError != kDNSServiceErr_NoError) {
		if (sd_ref_) {
			DNSServiceRefDeallocate(sd_ref_);
			sd_ref_ = nullptr;
		}
		if (closure_)
			closure_(0);
		closure_ = nullptr;
		return;
	}
}

void DNSServiceResolver::callback__(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullName, const char *hostName, uint16_t port,
				    uint16_t txtLen, const unsigned char *txtRecord, void *context)
{
	port_ = (errorCode == kDNSServiceErr_NoError) ? CFSwapInt16HostToBig(port) : 0;
	if (closure_)
		closure_(port_);
	closure_ = nullptr;
}

// MARK: - DNSServiceResolverSocket

using ExtensionClientCallback = std::function<void(int pid, const std::string &signingId, const std::string &clientId, const std::string &time)>;

class DNSServiceResolverSocket final : public QObject {
public:
	DNSServiceResolverSocket(QObject *parent = Q_NULLPTR);
	virtual ~DNSServiceResolverSocket();

	void Init(quint16 port);
	bool IsConnected() const { return connected_; }
	static DNSServiceResolverSocket *GetInstance();

	void RequestExtensionClientInfo(ExtensionClientCallback callback);

private:
	void deinit();
	void messageDidReceive(NSData *data);

private Q_SLOTS:
	void onConnected();
	void onDisconnected();
	void onTextMessageReceived(QString message);
	void onBinaryMessageReceived(const QByteArray &message);

private:
	QWebSocket *ws_ = Q_NULLPTR;
	bool connected_ = false;
	quint16 service_port_ = 0;
	static DNSServiceResolverSocket *sr_socket_;
	ExtensionClientCallback cb_ = nullptr;
};

DNSServiceResolverSocket *DNSServiceResolverSocket::sr_socket_ = nullptr;
DNSServiceResolverSocket *DNSServiceResolverSocket::GetInstance()
{
	if (!sr_socket_) {
		sr_socket_ = new DNSServiceResolverSocket();
	}
	return sr_socket_;
}
void DNSServiceResolverSocket::RequestExtensionClientInfo(ExtensionClientCallback callback)
{
	cb_ = callback;
	if (connected_) {
		ws_->sendTextMessage("POP_EXTENSION_CLIENT_INFO_REQ");
	}
}
DNSServiceResolverSocket::DNSServiceResolverSocket(QObject *parent) : QObject(parent) {}

DNSServiceResolverSocket::~DNSServiceResolverSocket()
{
	deinit();
}
void DNSServiceResolverSocket::Init(quint16 port)
{
	if (service_port_ == port) {
		return;
	}

	service_port_ = port;
	ws_ = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);
	std::stringstream url;
	url << "ws://localhost:" << service_port_;
	connect(ws_, &QWebSocket::connected, this, &DNSServiceResolverSocket::onConnected);
	connect(ws_, &QWebSocket::disconnected, this, &DNSServiceResolverSocket::onDisconnected);
	connect(ws_, &QWebSocket::textMessageReceived, this, &DNSServiceResolverSocket::onTextMessageReceived);
	connect(ws_, &QWebSocket::binaryMessageReceived, this, &DNSServiceResolverSocket::onBinaryMessageReceived);
	ws_->open(QUrl(url.str().c_str()));
}
void DNSServiceResolverSocket::deinit()
{
	if (!connected_)
		return;
	connected_ = false;
}
void DNSServiceResolverSocket::onConnected()
{
	connected_ = true;
}
void DNSServiceResolverSocket::onDisconnected()
{
	deinit();
}
void DNSServiceResolverSocket::onTextMessageReceived(QString message)
{
	NSString *tmpMsg = [NSString stringWithCString:message.toStdString().c_str() encoding:NSUTF8StringEncoding];
	messageDidReceive([tmpMsg dataUsingEncoding:NSUTF8StringEncoding]);
}
void DNSServiceResolverSocket::onBinaryMessageReceived(const QByteArray &message)
{
	NSData *data = [NSData dataWithBytes:message.data() length:message.length()];
	messageDidReceive(data);
}

void DNSServiceResolverSocket::messageDidReceive(NSData *data)
{
	if (!data || !connected_ || !cb_)
		return;

	NSError *error = nil;
	NSDictionary *dict = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingAllowFragments error:&error];
	if (!dict)
		return;

	if ([[dict valueForKey:@"commandType"] isEqualTo:@"POP_EXTENSION_CLIENT_INFO_RSP"]) {
		if ([[dict valueForKey:@"data"] isKindOfClass:NSArray.class]) {
			NSArray *datas = [dict valueForKey:@"data"];
			for (int index = 0; index < datas.count; index++) {
				if ([datas[index] isKindOfClass:NSDictionary.class]) {
					NSNumber *pid = NULL;
					if ([[datas[index] valueForKey:@"pid"] isKindOfClass:NSNumber.class]) {
						pid = [datas[index] valueForKey:@"pid"];
					} else {
						assert(false && "pid value type");
						break;
					}

					NSString *signingId = NULL;
					if ([[datas[index] valueForKey:@"signingId"] isKindOfClass:NSString.class]) {
						signingId = [datas[index] valueForKey:@"signingId"];
					} else {
						assert(false && "signingId value type");
						break;
					}

					NSString *clientId = NULL;
					if ([[datas[index] valueForKey:@"clientId"] isKindOfClass:NSString.class]) {
						clientId = [datas[index] valueForKey:@"clientId"];
					} else {
						assert(false && "clientId value type");
						break;
					}

					NSString *date = NULL;
					if ([[datas[index] valueForKey:@"date"] isKindOfClass:NSString.class]) {
						date = [datas[index] valueForKey:@"date"];
					} else {
						assert(false && "date value type");
						break;
					}

					if (cb_) {
						cb_([pid intValue], [signingId cStringUsingEncoding:NSUTF8StringEncoding], [clientId cStringUsingEncoding:NSUTF8StringEncoding],
						    [date cStringUsingEncoding:NSUTF8StringEncoding]);
					}
				} else {
					assert(false && "data item is not dict");
				}
			}
		} else {
			assert(false && "data is not array");
		}
	} else {
		assert(false && "POP_EXTENSION_CLIENT_INFO_RSP");
	}
}
// MARK: - PLSBasicStatusBar

void PLSBasicStatusBar::RequestExtensionClientInfo(std::function<void(int pid, const std::string &signingId, const std::string &clientId, const std::string &time)> closure)
{
	DNSServiceResolver::Resolve([closure](uint16_t port) {
		if (!port) {
			closure(0, "", "", "");
		} else {
			pls_async_call_mt([port, closure]() {
				DNSServiceResolverSocket::GetInstance()->Init(port);
				DNSServiceResolverSocket::GetInstance()->RequestExtensionClientInfo(closure);
			});
		}
	});
}

#endif
