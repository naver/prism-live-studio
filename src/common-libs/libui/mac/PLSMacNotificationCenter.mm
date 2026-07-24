#include <Foundation/Foundation.h>
#include "PLSMacNotificationCenter.h"
#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <QDebug>

static NSDictionary *QtJsonToNSDictionary(const QByteArray &json);
static QByteArray NSDictionaryToQtJson(NSDictionary *dict);

@interface PLSMacSignalCenter : NSObject
@end

@implementation PLSMacSignalCenter {
	QHash<QString, QVector<pls::mac::signalCallback>> _callbacks;
}

+ (instancetype)sharedInstance
{
	static PLSMacSignalCenter *sharedSingleton = nil;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^(void) {
		sharedSingleton = [[self alloc] init];
	});
	return sharedSingleton;
}

- (void)addListener:(const QString &)signalName callback:(const pls::mac::signalCallback &)callback
{
	if (signalName.isEmpty()) {
		return;
	}
	auto it = _callbacks.find(signalName);
	if (it == _callbacks.end()) {
		[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(receiveSignalNotify:) name:signalName.toNSString() object:nil];
	}
	_callbacks[signalName].push_back(callback);
}

- (void)removeListener:(const QString &)signalName
{
	if (signalName.isEmpty()) {
		return;
	}
	auto it = _callbacks.find(signalName);
	if (it == _callbacks.end()) {
		return;
	}
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:signalName.toNSString() object:nil];
	_callbacks.erase(it);
}

- (void)removeAllListeners
{
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
	_callbacks.clear();
}

- (void)receiveSignalNotify:(NSNotification *)noti
{
	NSString *name = noti.name;
	if (name.length == 0) {
		return;
	}
	QString signalName = QString::fromNSString(name);
	auto it = _callbacks.find(signalName);
	if (it == _callbacks.end() || it.value().isEmpty()) {
		return;
	}
	QByteArray jsonData = NSDictionaryToQtJson(noti.userInfo);
	QJsonObject payload = QJsonDocument::fromJson(jsonData).object();
	auto callbacks = it.value();
	dispatch_async(dispatch_get_main_queue(), ^{
		for (const auto &cb : callbacks) {
			if (cb) {
				cb(signalName, payload);
			}
		}
	});
}

- (void)sendSignal:(const QString &)signalName jsonData:(const QByteArray &)jsonData
{
	if (signalName.isEmpty()) {
		return;
	}
	NSDictionary *userInfo = jsonData.isEmpty() ? nil : QtJsonToNSDictionary(jsonData);
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:signalName.toNSString() object:nil userInfo:userInfo deliverImmediately:YES];
}

- (void)dealloc
{
	_callbacks.clear();
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
}
@end

static NSDictionary *QtJsonToNSDictionary(const QByteArray &json)
{
	if (json.isEmpty()) {
		return nil;
	}
	NSData *data = [NSData dataWithBytes:json.constData() length:(NSUInteger)json.size()];
	NSError *error = nil;
	id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
	if (error || ![obj isKindOfClass:[NSDictionary class]]) {
		return nil;
	}
	return (NSDictionary *)obj;
}

static QByteArray NSDictionaryToQtJson(NSDictionary *dict)
{
	if (!dict || dict.count == 0) {
		return QByteArray();
	}
	NSError *error = nil;
	NSData *data = [NSJSONSerialization dataWithJSONObject:dict options:0 error:&error];
	if (error || !data) {
		return QByteArray();
	}
	return QByteArray((const char *)data.bytes, (int)data.length);
}

namespace pls {
namespace mac {
void listenSignal(const QString &signalName, const signalCallback &callback)
{
	[[PLSMacSignalCenter sharedInstance] addListener:signalName callback:callback];
}

void sendSignal(const QString &signalName, const QJsonObject &payload)
{
	QByteArray jsonData;
	if (!payload.isEmpty()) {
		QJsonDocument doc(payload);
		jsonData = doc.toJson(QJsonDocument::Compact);
	}
	[[PLSMacSignalCenter sharedInstance] sendSignal:signalName jsonData:jsonData];
}

void removeSignalListener(const QString &signalName)
{
	[[PLSMacSignalCenter sharedInstance] removeListener:signalName];
}

void removeAllSignalListeners()
{
	[[PLSMacSignalCenter sharedInstance] removeAllListeners];
}
}
}
