
#import <Foundation/Foundation.h>
#import "PLSAnalysisStackInterface.h"
#import <KSCrashC.h>
#import <KSCrash.h>
#import <KSCrashInstallation.h>
#import <KSCrashReportFilterAppleFmt.h>
#import <KSJSONCodecObjC.h>

// MARK: Private

static void get_module_names(NSDictionary *report, std::set<std::string> &module_names);
static void get_dump_data(NSDictionary *report, std::string &dump_data);
static NSArray<NSDictionary *> * crash_thread_backtrace(NSDictionary *report);
static void get_report_location(NSDictionary *report, std::string &location);

static NSString *getBundleName() {
    NSString *bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
    if(bundleName == nil) {
        bundleName = @"Unknown";
    }
    return bundleName;
}

static NSString *getBasePath(std::string process_name) {
    NSArray *directories = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                               NSUserDomainMask,
                                                               YES);
    if([directories count] == 0) {
        NSLog(@"Could not locate cache directory path.");
        return nil;
    }
    NSString *cachePath = [directories objectAtIndex:0];
    if([cachePath length] == 0) {
        NSLog(@"Could not locate cache directory path.");
        return nil;
    }
    NSString *pathEnd = process_name.empty() ? getBundleName() : [NSString stringWithUTF8String:process_name.c_str()];
    return [cachePath stringByAppendingPathComponent:pathEnd];
}

static void get_latest_report(ProcessInfo const *info, NSDictionary **report) {
    const ProcessInfo &localInfo = *info;

    NSString *reportsPath = [getBasePath(localInfo.process_name) stringByAppendingPathComponent:@"Reports"];
    
    NSError *error = nil;
    NSFileManager *fileManager = [NSFileManager defaultManager];
        
    NSArray<NSString *> *logPaths = [fileManager contentsOfDirectoryAtPath:reportsPath error:&error];
    
    if (error == nil) {
        NSArray *sortedFiles = [logPaths sortedArrayUsingComparator:^NSComparisonResult(id obj1, id obj2) {
            NSString *file1 = [reportsPath stringByAppendingPathComponent:obj1];
            NSString *file2 = [reportsPath stringByAppendingPathComponent:obj2];
            NSDictionary *file1Attributes = [fileManager attributesOfItemAtPath:file1 error:nil];
            NSDictionary *file2Attributes = [fileManager attributesOfItemAtPath:file2 error:nil];
            return [[file2Attributes objectForKey:NSFileCreationDate] compare:[file1Attributes objectForKey:NSFileCreationDate]];
        }];
        if (sortedFiles.count > 0) {
            NSString *newestFilePath = [reportsPath stringByAppendingPathComponent:[sortedFiles objectAtIndex:0]];
            
            NSData *jsonData = [fileManager contentsAtPath:newestFilePath];
            NSError *decodeError = nil;
            int decodeOption = KSJSONDecodeOptionIgnoreAllNulls | KSJSONDecodeOptionKeepPartialObject;
            NSDictionary *decodedReport = [KSJSONCodec decode:jsonData
                                               options:(KSJSONDecodeOption)decodeOption
                                                 error:&decodeError];
            
            if (decodeError == nil) {
                *report = decodedReport;
            }
        }
    }
}

// MARK: Public

void mac_install_crash_reporter(const std::string &process_name) {
    KSCrash *reporter = [KSCrash sharedInstance];
    reporter.basePath = getBasePath(process_name);
    [reporter install];
}

void mac_get_latest_dump_data_location(ProcessInfo const &info, std::string &dump_data, std::string &location) {
    if (info.dump_file.empty() == false) {
        NSString *path = [NSString stringWithUTF8String:info.dump_file.c_str()];
        NSString *dumpData = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
        dump_data = dumpData.UTF8String;
        return;
    }
    NSDictionary *report = nil;
    get_latest_report(&info, &report);
    
    if (!report) {
        return;
    }
    
    get_report_location(report, location);
    get_dump_data(report, dump_data);
}

void mac_get_latest_dump_data_module_names(ProcessInfo const &info, std::string &dump_data, std::set<std::string> &module_names) {
    NSDictionary *report = nil;
    get_latest_report(&info, &report);
    
    if (!report) {
        return;
    }
    
    get_dump_data(report, dump_data);
    get_module_names(report, module_names);
}

static void get_dump_data(NSDictionary *report, std::string &dump_data) {
    KSCrashReportFilterAppleFmt *filter = [KSCrashReportFilterAppleFmt filterWithReportStyle:KSAppleReportStyleSymbolicated];
    [filter filterReports:@[report] onCompletion:^(NSArray *filteredReports, BOOL completed, NSError *error) {
        if (filteredReports.count > 0) {
            NSString *dumpData = filteredReports.firstObject;
            dump_data = dumpData.UTF8String;
        }
    }];
}

static void get_module_names(NSDictionary *report, std::set<std::string> &module_names) {
    NSArray<NSDictionary *> *backtrace = crash_thread_backtrace(report);
    [backtrace enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        NSString *objectName = obj[@"object_name"];
        if (objectName) {
            module_names.insert(objectName.UTF8String);
        }
    }];
}

static void get_report_location(NSDictionary *report, std::string &location) {
    NSArray<NSDictionary *> *backtrace = crash_thread_backtrace(report);
    if (backtrace && backtrace.count > 0) {
        NSDictionary *stack = backtrace.firstObject;
        NSString *symbolName = stack[@"symbol_name"];
        if (symbolName) {
            location = symbolName.UTF8String;
        }
    }
}

static NSArray<NSDictionary *> * crash_thread_backtrace(NSDictionary *report) {
    NSDictionary *crash = report[@"crash"];
    if (!crash) {
        return @[];
    }
    NSArray<NSDictionary *> *threads = crash[@"threads"];
    if (!threads) {
        return @[];
    }
    __block NSDictionary *crashedThread = nil;
    [threads enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        id crashed = obj[@"crashed"];
        if (crashed && [crashed boolValue]) {
            crashedThread = obj;
            *stop = YES;
        }
    }];
    
    if (!crashedThread) {
        return @[];
    }
    
    NSArray<NSDictionary *> *backtrace = crashedThread[@"backtrace"][@"contents"];
    return backtrace;
}

bool mac_send_data(std::string post_body) {
    NSURLSessionConfiguration *config = [NSURLSessionConfiguration defaultSessionConfiguration];
    NSURLSession *session = [NSURLSession sessionWithConfiguration:config];
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://nelo2-col.navercorp.com/_store"]];
    request.HTTPMethod = @"POST";
    [request setValue:@"application/x-www-form-urlencoded" forHTTPHeaderField:@"Content-Type"];
    
    NSString *bodyString = [NSString stringWithUTF8String:post_body.c_str()];
    NSData *postData = [bodyString dataUsingEncoding:NSUTF8StringEncoding];
    request.HTTPBody = postData;
    
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    
    __block bool res = false;
    
    NSURLSessionTask *task = [session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
        
        if (data) {
            NSString *result = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            if ([result containsString:@"Success"]) {
                res = true;
            }
        }
        
        dispatch_semaphore_signal(semaphore);
    }];
    
    [task resume];
    
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    
    return res;
}

bool mac_remove_crash_logs(ProcessInfo const &info) {
    NSError *error = nil;
    BOOL removed = NO;
    if (info.dump_file.empty() == false) {
        NSString *path = [[NSString stringWithUTF8String:info.dump_file.c_str()] stringByDeletingLastPathComponent];
        [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    }
    removed = error == nil;
    
    NSString *path = [getBasePath(info.process_name) stringByAppendingPathComponent:@"Reports"];
    [[NSFileManager defaultManager] removeItemAtPath:path error:&error];
    
    removed = error == nil;

    return removed;
}
