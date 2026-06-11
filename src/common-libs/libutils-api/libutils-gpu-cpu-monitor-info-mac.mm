#if __APPLE__
//
//  gpu_info_collector.m
//  pls-gpu-info
//
//  Created by Zhong Ling on 2023/3/2.
//

#include "libutils-gpu-cpu-monitor-info.h"
#include "libutils-api-mac.h"

#include <map>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/errno.h>
#include <sysexits.h>

#include <AppKit/AppKit.h>

// MARK: - NSString (Util)

@interface NSString (Util)
+ (NSString *)hexStringWithData:(NSData *)data;
@end

@implementation NSString (Util)
+ (NSString *)hexStringWithData:(NSData *)data
{
	if (!data || [data length] == 0) {
		return nil;
	}

	NSMutableString *string = [[NSMutableString alloc] initWithCapacity:[data length]];

	[data enumerateByteRangesUsingBlock:^(const void *bytes, NSRange byteRange, BOOL *stop) {
		unsigned char *dataBytes = (unsigned char *)bytes;
		for (NSInteger i = 0; i < byteRange.length; i++) {
			NSString *hexStr = [NSString stringWithFormat:@"%x", (dataBytes[byteRange.length - 1 - i]) & 0xff];

			if ([hexStr length] == 2) {
				[string appendString:hexStr];
			} else {
				[string appendFormat:@"0%@", hexStr];
			}
		}
	}];

	NSString *pre = [string commonPrefixWithString:@"00000000" options:NSCaseInsensitiveSearch];

	return [NSString stringWithFormat:@"0x%@", [string stringByReplacingOccurrencesOfString:pre withString:@""]];
}

@end

// MARK: - Vendor ID

const UInt32 kVendorIDIntel = 0x8086;
const UInt32 kVendorIDNVidia = 0x10de;
const UInt32 kVendorIDAMD = 0x1002;
const UInt32 kVendorIDApple = 0x106B;
const UInt32 kVendorIDQualcomm = 0x17A0;
const UInt32 kVendorIDBroadcom = 0x14E4;

// MARK: - PLSDeviceHelper

@interface PLSDeviceHelper : NSObject

+ (NSString *)convertVendorID:(UInt32)vendorID;
+ (UInt32)getEntryProperty:(io_registry_entry_t)entry propertyName:(CFStringRef)propertyName;
+ (NSString *)getModelIdentifier;
+ (NSArray *)getMatchingServices:(NSString *)match;
+ (NSString *)getIntelGPUInfo;

@end

@implementation PLSDeviceHelper

+ (NSString *)convertVendorID:(UInt32)vendorID
{
	if (vendorID == kVendorIDIntel) {
		return @"Intel";
	} else if (vendorID == kVendorIDNVidia) {
		return @"NVidia";
	} else if (vendorID == kVendorIDAMD) {
		return @"AMD";
	} else if (vendorID == kVendorIDApple) {
		return @"Apple";
	} else if (vendorID == kVendorIDQualcomm) {
		return @"Qualcomm";
	} else if (vendorID == kVendorIDBroadcom) {
		return @"Broadcom";
	} else {
		return @"Unknown";
	}
}

// Return 0 if we couldn't find the property.
// The property values we use should not be 0, so it's OK to use 0 as failure.
+ (UInt32)getEntryProperty:(io_registry_entry_t)entry propertyName:(CFStringRef)propertyName
{
	CFDataRef dataRef =
		static_cast<CFDataRef>(IORegistryEntrySearchCFProperty(entry, kIOServicePlane, propertyName, kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents));
	if (!dataRef)
		return 0;

	UInt32 value = 0;
	const UInt32 *valuePointer = reinterpret_cast<const UInt32 *>(CFDataGetBytePtr(dataRef));
	if (valuePointer != NULL)
		value = *valuePointer;
	if (dataRef)
		CFRelease(dataRef);
	return value;
}

+ (NSString *)getModelIdentifier
{
	NSString *value = @"";
	io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
	if (!platformExpert)
		return value;

	CFDataRef modelData = static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(platformExpert, CFSTR("model"), kCFAllocatorDefault, 0));
	if (!modelData) {
		IOObjectRelease(platformExpert);
		return value;
	}

	const char *valuePointer = reinterpret_cast<const char *>(CFDataGetBytePtr(modelData));
	if (valuePointer)
		value = [NSString stringWithUTF8String:valuePointer];

	CFRelease(modelData);
	IOObjectRelease(platformExpert);

	return value;
}

+ (NSArray *)getMatchingServices:(NSString *)match
{
	NSMutableArray *result = [[NSMutableArray alloc] init];
	CFMutableDictionaryRef matchDict = IOServiceMatching([match UTF8String]);
	io_iterator_t iterator;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchDict, &iterator) != kIOReturnSuccess) {
		return [result copy];
	}

	io_registry_entry_t regEntry;
	while ((regEntry = IOIteratorNext(iterator))) {
		CFMutableDictionaryRef serviceDictionary;
		if (IORegistryEntryCreateCFProperties(regEntry, &serviceDictionary, kCFAllocatorDefault, kNilOptions) != kIOReturnSuccess) {
			IOObjectRelease(regEntry);
			continue;
		}

		NSDictionary *dic = (__bridge NSDictionary *)serviceDictionary;
		[result addObject:dic];

		CFRelease(serviceDictionary);
		IOObjectRelease(regEntry);
	}
	IOObjectRelease(iterator);
	return [result copy];
}

+ (NSString *)getIntelGPUInfo
{
	NSMutableArray *cachedGPUs = [NSMutableArray array];

	// The IOPCIDevice class includes display adapters/GPUs.
	CFMutableDictionaryRef devices = IOServiceMatching("IOPCIDevice");
	io_iterator_t entryIterator;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, devices, &entryIterator) == kIOReturnSuccess) {
		io_registry_entry_t device;

		while ((device = IOIteratorNext(entryIterator))) {
			CFMutableDictionaryRef serviceDictionary;

			if (IORegistryEntryCreateCFProperties(device, &serviceDictionary, kCFAllocatorDefault, kNilOptions) != kIOReturnSuccess) {
				// Couldn't get the properties for this service, so clean up and
				// continue.
				IOObjectRelease(device);
				continue;
			}

			const void *ioName = CFDictionaryGetValue(serviceDictionary, CFSTR("IOName"));
			NSString *str = @"display";
			if (ioName && [str isEqualToString:(__bridge NSString *_Nonnull)(ioName)]) {
				const void *model = CFDictionaryGetValue(serviceDictionary, CFSTR("model"));
				NSString *gpuName = [[NSString alloc] initWithData:(__bridge NSData *)model encoding:NSASCIIStringEncoding];
				[cachedGPUs addObject:gpuName];
			}
			CFRelease(serviceDictionary);
		}
	}

	if (cachedGPUs.count > 0) {
		// I don't why, cachedGPUs[0] is a  @"Intel Iris Plus Graphics 640\0", the tail is '\0'.
		// Maybe this has risk action, so, I replace `cachedGPUs[0]` of `[NSString stringWithUTF8String:[cachedGPUs[0] UTF8String]]`.
		return [NSString stringWithUTF8String:[cachedGPUs[0] UTF8String]];
	} else {
		return @"";
	}
}

@end

bool ParseModelIdentifier(const std::string &ident, std::string *type, int *major, int *minor)
{
	size_t number_loc = ident.find_first_of("0123456789");
	if (number_loc == std::string::npos)
		return false;
	size_t comma_loc = ident.find(',', number_loc);
	if (comma_loc == std::string::npos)
		return false;

	int32_t major_tmp, minor_tmp;

	std::string s1 = ident.substr(number_loc, comma_loc);
	std::string s2 = ident.substr(comma_loc + 1, ident.size());
	*major = atoi(s1.c_str());
	*minor = atoi(s2.c_str());
	*type = ident.substr(0, number_loc);
	return true;
}

bool CollectPCIVideoCardInfo(pls_gpu_basic_info &gpu_info)
{
	// Collect all GPUs' info.
	// match_dictionary will be consumed by IOServiceGetMatchingServices, no need
	// to release it.
	CFMutableDictionaryRef match_dictionary = IOServiceMatching("IOPCIDevice");
	io_iterator_t entry_iterator;
	std::vector<pls_gpu_basic_info::pls_gpu_device> gpu_list;
	if (IOServiceGetMatchingServices(kIOMasterPortDefault, match_dictionary, &entry_iterator) == kIOReturnSuccess) {
		io_registry_entry_t entry;
		while (static_cast<void>(entry = IOIteratorNext(entry_iterator)), entry) {
			if ([PLSDeviceHelper getEntryProperty:entry propertyName:CFSTR("class-code")] != 0x30000) { // 0x30000 : DISPLAY_VGA
			}
			pls_gpu_basic_info::pls_gpu_device gpu;
			gpu.vendor_id = [PLSDeviceHelper getEntryProperty:entry propertyName:CFSTR("vendor-id")];
			gpu.device_id = [PLSDeviceHelper getEntryProperty:entry propertyName:CFSTR("device-id")];
			if (gpu.vendor_id && gpu.device_id) {
				gpu_list.push_back(gpu);
			}
		}
		IOObjectRelease(entry_iterator);
	}

	gpu_info.gpus = gpu_list;

	return !gpu_list.empty();
}

// MARK: - Public functions

std::string pls_get_cpu_info()
{
	char buff[512] = {0};
	size_t buff_len = sizeof(buff);
	sysctlbyname("machdep.cpu.brand_string", &buff, &buff_len, NULL, 0);
	NSString *cpuType = [NSString stringWithFormat:@"%s", buff];

	NSProcessInfo *procInfo = [NSProcessInfo processInfo];
	unsigned long cpuLogicCount = (unsigned long)procInfo.processorCount;

	NSString *strResult = [NSString stringWithFormat:@"%@ %lu-Core Processor", cpuType, cpuLogicCount];

	std::string result = strResult.UTF8String;
	return result;
}

std::string pls_get_gpu_info()
{
	NSDictionary *accelerator = [PLSDeviceHelper getMatchingServices:@kIOAcceleratorClassName].firstObject;
	if (accelerator == NULL)
		return "";

	NSMutableString *result = [NSMutableString stringWithString:@""];
	NSString *gpuName = [accelerator objectForKey:@"model"];
	if (gpuName.length > 0) {
		NSString *tmpStr = [NSString stringWithUTF8String:gpuName.UTF8String];
		[result appendString:tmpStr];
	}

	if (result.length <= 0) {
		[result appendString:[PLSDeviceHelper getIntelGPUInfo]];
	}

	return [result UTF8String];
}

LIBUTILSAPI_API double pls_get_gpu_device_usage()
{
	NSDictionary *accelerator = [PLSDeviceHelper getMatchingServices:@kIOAcceleratorClassName].firstObject;
	if (accelerator == NULL)
		return 0.0;
	NSDictionary *statistics = [accelerator objectForKey:@"PerformanceStatistics"];
	NSString *utilization = [statistics objectForKey:@"Device Utilization %"];
	return fmin(fmax(0.0, [utilization doubleValue]), 100.0);
}

bool pls_get_basic_graphics_info(pls_gpu_basic_info &gpu_info)
{
	std::string model_id = [PLSDeviceHelper getModelIdentifier].UTF8String;
	std::string model_name;
	int model_major = 0, model_minor = 0;
	ParseModelIdentifier(model_id, &model_name, &model_major, &model_minor);
	gpu_info.machine_model_name = model_name;
	gpu_info.machine_model_version = [[NSString stringWithFormat:@"%d.%d", model_major, model_minor] UTF8String];
	return CollectPCIVideoCardInfo(gpu_info);
}

// [wiki](https://github.com/giampaolo/psutil/issues/1892)
void pls_get_cpu_frequency(int64_t &hz, int64_t &tick)
{
	// also availble as "CTL_HW, HW_CPU_FREQ", "hw.cpufrequency", "hw.cpufrequency_min", "hw.cpufrequency_max" but it's deprecated
	int mib[2] = {0};
	size_t len = 0;
	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	struct clockinfo clockinfo;
	len = sizeof(clockinfo);
	sysctl(mib, 2, &clockinfo, &len, NULL, 0);
	printf("clockinfo.hz: %d\n", clockinfo.hz);
	printf("clockinfo.tick: %d\n", clockinfo.tick);

	hz = clockinfo.hz;
	tick = clockinfo.tick;
}

void pls_get_memory_info(float &free, float &total)
{
	// These values are in bytes
	int64_t total_mem;
	int64_t used_mem;
	// blah
	vm_size_t page_size;
	mach_port_t mach_port;
	mach_msg_type_number_t count;
	vm_statistics_data_t vm_stats;

	// Get total physical memory
	int mib[2];
	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;
	size_t length = sizeof(int64_t);
	sysctl(mib, 2, &total_mem, &length, NULL, 0);

	mach_port = mach_host_self();
	count = sizeof(vm_stats) / sizeof(natural_t);
	if (KERN_SUCCESS == host_page_size(mach_port, &page_size) && KERN_SUCCESS == host_statistics(mach_port, HOST_VM_INFO, (host_info_t)&vm_stats, &count)) {
		used_mem = ((int64_t)vm_stats.active_count + (int64_t)vm_stats.inactive_count + (int64_t)vm_stats.wire_count) * (int64_t)page_size;
	}

	total = total_mem / 1024 / 1024;
	free = (total_mem - used_mem) / 1024 / 1024;
}

bool pls_get_cpu_info(pls_cpu_info &info)
{
	info.name = pls_libutil_api_mac::pls_get_cpu_name();
	pls_get_cpu_frequency(info.hz, info.tick);

	pls_get_memory_info(info.free_mem, info.total_mem);

	NSProcessInfo *processInfo = [NSProcessInfo processInfo];
	info.physical_cores = processInfo.processorCount;
	info.logical_cores = processInfo.activeProcessorCount;

	return true;
}

bool pls_get_monitors_info(std::vector<pls_monitor_info> &infos)
{
	NSArray *screens = [NSScreen screens];

	for (NSUInteger i = 0; i < [screens count]; ++i) {
		pls_monitor_info info;
		NSScreen *screen = screens[i];
		NSDictionary *device_description = [screen deviceDescription];
		info.monitor_id = static_cast<uint32_t>([[device_description objectForKey:@"NSScreenNumber"] intValue]);

		NSRect ns_bounds = [screen frame];
		info.bounds.x = ns_bounds.origin.x;
		info.bounds.y = ns_bounds.origin.y;
		info.bounds.w = ns_bounds.size.width;
		info.bounds.h = ns_bounds.size.height;

		// If the host is running Mac OS X 10.7+ or later, query the scaling factor
		// between logical and physical (aka "backing") pixels, otherwise assume 1:1.
		if ([screen respondsToSelector:@selector(backingScaleFactor)] && [screen respondsToSelector:@selector(convertRectToBacking:)]) {
			info.dip_to_pixel_scale = [screen backingScaleFactor];
		} else {
			info.dip_to_pixel_scale = 1.0;
		}

		// Determine if the display is built-in or external.
		info.is_builtin = CGDisplayIsBuiltin(info.monitor_id);

		infos.push_back(info);
	}

	return true;
}
#endif
