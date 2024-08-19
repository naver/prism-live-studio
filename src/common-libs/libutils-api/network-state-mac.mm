#include <AppKit/AppKit.h>
#include <Network/Network.h>
#include "network-state-mac.h"

NSString *const kReachabilityChangedNotification = @"kReachabilityChangedNotification";

@interface NetWorkMonitor : NSObject
+ (instancetype)shared;
-(void)startNetwrkMonitoring;
-(void)stopNetwrkMonitoring;
@property(nonatomic,strong) nw_path_monitor_t monitor;
@property(nonatomic,strong) dispatch_queue_t monitorQueue;
@property(nonatomic,assign) bool isWiFi;
@property(nonatomic,assign) bool isCellular;
@property(nonatomic,assign) bool isEthernet;
@property(nonatomic,assign) bool hasIPv4;
@property(nonatomic,assign) bool hasIPv6;
@property(nonatomic,assign) bool isConnected;
@property(nonatomic,assign) bool isFristGot;
@end

@implementation NetWorkMonitor

+ (instancetype)shared {
    static NetWorkMonitor *session;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        session = [[NetWorkMonitor alloc] init];
    });
    return session;
}

- (void)startNetwrkMonitoring {
    dispatch_queue_attr_t attrs = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_USER_INITIATED,DISPATCH_QUEUE_PRIORITY_DEFAULT);
    self.monitorQueue = dispatch_queue_create("com.example.network.monitor",attrs);
    self.monitor = nw_path_monitor_create();
    nw_path_monitor_set_queue(self.monitor,self.monitorQueue);
    __weak NetWorkMonitor *weak_self = self;
    nw_path_monitor_set_update_handler(self.monitor,^(nw_path_t _Nonnull path){
        bool oldIsConnected = weak_self.isConnected;
        nw_path_status_t status = nw_path_get_status(path);
        weak_self.isConnected = status == nw_path_status_satisfied ? true : false;
        weak_self.isWiFi = nw_path_uses_interface_type(path,nw_interface_type_wifi);
        weak_self.isCellular = nw_path_uses_interface_type(path,nw_interface_type_cellular);
        weak_self.isEthernet = nw_path_uses_interface_type(path,nw_interface_type_wired);
        weak_self.hasIPv4 = nw_path_has_ipv4(path);
        weak_self.hasIPv6 = nw_path_has_ipv6(path);
        if (!weak_self.isFristGot) {
            if (oldIsConnected == weak_self.isConnected) {
                return;
            }
        }
        weak_self.isFristGot = false;
        [[NSNotificationCenter defaultCenter] postNotificationName:@"kReachabilityChangedNotification" object:nil];
    });
    nw_path_monitor_start(self.monitor);
}

-(void)stopNetwrkMonitoring {
    nw_path_monitor_cancel(self.monitor);
}

-(void)dealloc {
    [self stopNetwrkMonitoring];
}

@end

namespace pls {

    NetworkState *NetworkState::instance() {
        static NetworkState ns;
        return &ns;
    }

    NetworkState::NetworkState(QObject *parent) : QObject(parent) {
        [[NSNotificationCenter defaultCenter] addObserverForName:kReachabilityChangedNotification
                                                             object:nil
                                                              queue:[NSOperationQueue mainQueue]
                                                         usingBlock:^(NSNotification *note) {
                                                                emit NetworkState::instance()->stateChanged([NetWorkMonitor shared].isConnected);
                                                         }];
    }

    bool NetworkState::isAvailable() const
    {
        return [NetWorkMonitor shared].isConnected;
    }

    bool network_state_start()
    {
        [[NetWorkMonitor shared] startNetwrkMonitoring];
        return true;
    }

    void network_state_stop()
    {
       [[NetWorkMonitor shared] stopNetwrkMonitoring];
    }

}


