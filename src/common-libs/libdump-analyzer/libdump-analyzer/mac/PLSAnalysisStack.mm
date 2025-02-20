
#import <Foundation/Foundation.h>
#import "PLSAnalysisStackInterface.h"
#import <KSCrashC.h>
#import <KSCrash.h>
#import <KSCrashInstallation.h>
#import <KSCrashReportFilterAppleFmt.h>
#import <KSJSONCodecObjC.h>

// MARK: Private
static NSArray<NSDictionary *> * crash_thread_backtrace(NSDictionary *report) {
	NSDictionary *crash = report[@"crash"];
	if (!crash) {
		return @[];
	}
	
	NSArray<NSDictionary *> *threads = crash[@"threads"];
	if (!threads || threads.count == 0) {
		return @[];
	}
	
	__block NSDictionary *crashedThread = threads.firstObject;
	
	NSString *type = crash[@"error"][@"type"];
	if (![type isEqualToString:@"user"]) {
		[threads enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
			id crashed = obj[@"crashed"];
			if (crashed && [crashed boolValue]) {
				crashedThread = obj;
				*stop = YES;
			}
		}];
	}
	
	if (!crashedThread) {
		return @[];
	}
	
	NSArray<NSDictionary *> *backtrace = crashedThread[@"backtrace"][@"contents"];
	return backtrace;
}

static NSString *getBundleName() {
    NSString *bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
    if(bundleName == nil) {
        bundleName = @"Unknown";
    }
    return bundleName;
}

static NSString *getBasePath(ProcessInfo const &info) {
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
    NSString *processPath = info.process_name.empty() ? getBundleName() : [NSString stringWithUTF8String:info.process_name.c_str()];
	NSString *subsessionPath = [processPath stringByAppendingPathComponent:[NSString stringWithUTF8String:info.prism_sub_session.c_str()]];
	NSString *finalPath = [subsessionPath stringByAppendingPathComponent:[NSString stringWithUTF8String:info.pid.c_str()]];
	
	return [cachePath stringByAppendingPathComponent:finalPath];
}

static void get_latest_report(ProcessInfo const *info, NSDictionary **report) {
    const ProcessInfo &localInfo = *info;

    NSString *reportsPath = [getBasePath(localInfo) stringByAppendingPathComponent:@"Reports"];
    
    NSError *error = nil;
    NSFileManager *fileManager = [NSFileManager defaultManager];
        
    NSArray<NSString *> *logPaths = [fileManager contentsOfDirectoryAtPath:reportsPath error:&error];
	
	__block NSDictionary *selectedReport = nil;
    
    if (error == nil) {
        NSArray<NSString *> *sortedFiles = [logPaths sortedArrayUsingComparator:^NSComparisonResult(id obj1, id obj2) {
            NSString *file1 = [reportsPath stringByAppendingPathComponent:obj1];
            NSString *file2 = [reportsPath stringByAppendingPathComponent:obj2];
            NSDictionary *file1Attributes = [fileManager attributesOfItemAtPath:file1 error:nil];
            NSDictionary *file2Attributes = [fileManager attributesOfItemAtPath:file2 error:nil];
            return [[file2Attributes objectForKey:NSFileCreationDate] compare:[file1Attributes objectForKey:NSFileCreationDate]];
        }];
		
		[sortedFiles enumerateObjectsUsingBlock:^(NSString * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
			NSString *newestFilePath = [reportsPath stringByAppendingPathComponent:obj];
			
			NSData *jsonData = [fileManager contentsAtPath:newestFilePath];
			NSError *decodeError = nil;
			int decodeOption = KSJSONDecodeOptionIgnoreAllNulls | KSJSONDecodeOptionKeepPartialObject;
			NSDictionary *decodedReport = [KSJSONCodec decode:jsonData
													  options:(KSJSONDecodeOption)decodeOption
														error:&decodeError];
			
			if (decodeError == nil) {
				NSDictionary *crash = decodedReport[@"crash"];
				if (!crash) {
					return;
				}
				
				NSString *type = crash[@"error"][@"type"];
				// type == user means it's a block dump, so we skip and check next one.
				if ([type isEqualToString:@"user"]) {
					return;
				}
				selectedReport = decodedReport;
				*stop = YES;
			}
		}];
    }
	
	*report = selectedReport;
}

static NSString * find_binary_image_path(NSDictionary *report, NSString *objectName) {
	NSArray<NSDictionary *> *images = report[@"binary_images"] ?: @[];
	__block NSString *image_path;
	[images enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
		NSString *path = obj[@"name"] ?: @"";
		if ([path containsString:objectName]) {
			image_path = path;
			*stop = YES;
		}
	}];
	return image_path;
}

static void get_crash_stack_hash(std::string processName, NSDictionary *report, std::string &stack_hash) {
	NSArray<NSDictionary *> *backtrace = crash_thread_backtrace(report);
	
	NSMutableString *modules = [NSMutableString new];
	
	[backtrace enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
		NSString *objectName = obj[@"object_name"];
		if (objectName) {
			NSString *objectPath = find_binary_image_path(report, objectName);
			if (objectPath && [objectPath containsString:[NSString stringWithUTF8String:processName.c_str()]]) {
				NSString *symbolName = obj[@"symbol_name"];
				[modules appendString:[NSString stringWithFormat:@"%@ %@\n", objectName, symbolName]];
			}
		}
	}];
	
	stack_hash = [NSString stringWithString:modules].UTF8String;
}

// MARK: Public

static void get_dump_data(NSDictionary *report, std::string &dump_data) {
	KSCrashReportFilterAppleFmt *filter = [KSCrashReportFilterAppleFmt filterWithReportStyle:KSAppleReportStyleSymbolicated];
	[filter filterReports:@[report] onCompletion:^(NSArray *filteredReports, BOOL completed, NSError *error) {
		if (filteredReports.count > 0) {
			NSString *dumpData = filteredReports.firstObject;
			dump_data = dumpData.UTF8String;
		}
	}];
}

static void get_module_names(NSDictionary *report, std::set<std::map<std::string, std::string>> &module_names) {
	NSArray<NSDictionary *> *binaryImages = report[@"binary_images"];
	NSArray<NSDictionary *> *backtrace = crash_thread_backtrace(report);
	[backtrace enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull stack, NSUInteger idx, BOOL * _Nonnull stop) {
		NSString *symbolName = stack[@"symbol_name"];
		if (!symbolName) {
			return;
		}
		NSNumber *symbolAddress = stack[@"symbol_addr"];
		NSString *objectName = stack[@"object_name"];
		NSNumber *objectAddress = stack[@"object_addr"];
		std::map<std::string, std::string> module_info;
		module_info["symbol_name"] = symbolName.UTF8String;
		module_info["symbol_addr"] = [NSString stringWithFormat:@"%@", symbolAddress].UTF8String;
		module_info["object_name"] = objectName.UTF8String;
		module_info["object_addr"] = [NSString stringWithFormat:@"%@", objectAddress].UTF8String;
		
		__block NSString *objectPath = nil;
		[binaryImages enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
			if ([obj[@"image_addr"] isEqualToNumber:objectAddress]) {
				objectPath = obj[@"name"];
				*stop = YES;
			}
		}];
		
		if (objectPath) {
			module_info["object_path"] = objectPath.UTF8String;
		}
		
		module_names.insert(module_info);
	}];
}

static void get_report_location(std::string processName, NSDictionary *report, std::string &location) {
	NSArray<NSDictionary *> *backtrace = crash_thread_backtrace(report);
	if (backtrace && backtrace.count > 0) {
		__block NSDictionary *stack = nil;
		
		[backtrace enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
			NSString *objectName = obj[@"object_name"];
			if (!objectName) {
				return;
			}
			NSString *objectPath = find_binary_image_path(report, objectName);
			if (objectPath && [objectPath containsString:[NSString stringWithUTF8String:processName.c_str()]]) {
				stack = obj;
				*stop = YES;
			}
		}];
		if (!stack) {
			stack = backtrace.firstObject;
		}
		
		NSString *symbolName = stack[@"symbol_name"];
		NSString *symbolAddress = stack[@"symbol_addr"];
		NSString *objectName = stack[@"object_name"];
		NSString *objectAddress = stack[@"object_addr"];
		NSString *locationName = [NSString stringWithFormat:@"%@ +%@ %@ +%@", objectName, objectAddress, symbolName, symbolAddress];
		
		location = locationName.UTF8String;
	}
}

void mac_install_crash_reporter(ProcessInfo const &info) {
    KSCrash *reporter = [KSCrash sharedInstance];
	reporter.basePath = getBasePath(info);
	[reporter enableSwapOfCxaThrow];
    [reporter install];
}

void mac_get_latest_dump_data(ProcessInfo const &info, std::string &dump_data, std::string &location, std::string &stack_hash, std::set<std::map<std::string, std::string>> &module_names) {
    if (info.dump_file.empty() == false) {
        NSString *path = [NSString stringWithUTF8String:info.dump_file.c_str()];
        NSString *dumpData = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
		
		NSError *decodeError = nil;
		int decodeOption = KSJSONDecodeOptionIgnoreAllNulls | KSJSONDecodeOptionKeepPartialObject;
		NSDictionary *decodedReport = [KSJSONCodec decode:[dumpData dataUsingEncoding:NSUTF8StringEncoding]
												  options:(KSJSONDecodeOption)decodeOption
													error:&decodeError];
		if (decodedReport) {
			get_report_location(info.process_name, decodedReport, location);
			get_dump_data(decodedReport, dump_data);
			
		}
        return;
    }
    NSDictionary *report = nil;
	get_latest_report(&info, &report);
    
    if (!report) {
        return;
    }
    
	get_report_location(info.process_name, report, location);
	get_crash_stack_hash(info.process_name, report, stack_hash);
    get_dump_data(report, dump_data);
	get_module_names(report, module_names);
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
    
    NSString *path = [getBasePath(info) stringByAppendingPathComponent:@"Reports"];
	
    [[NSFileManager defaultManager] removeItemAtPath:path error:&error];
    
    removed = error == nil;

    return removed;
}
