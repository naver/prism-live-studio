//
//  gpu_info_collector.h
//  pls-gpu-info
//
//  Created by Zhong Ling on 2023/3/2.
//

#ifndef pls_gpu_basic_info_h
#define pls_gpu_basic_info_h

#include <string>
#include <vector>

#include "libutils-export.h"

struct pls_gpu_basic_info {
    struct pls_gpu_device {
        // The DWORD (uint32_t) representing the graphics card vendor id.
        unsigned int vendor_id;
        
        // The DWORD (uint32_t) representing the graphics card device id.
        // Device ids are unique to vendor, not to one another.
        unsigned int device_id;
    };
    
    // GPUs
    std::vector<pls_gpu_device> gpus;
    
    // The machine model identifier. They can contain any character, including
    // whitespaces.  Currently it is supported on MacOSX and Android.
    // Android examples: "Naxus 5", "XT1032".
    // On MacOSX, the version is stripped out of the model identifier, for
    // example, the original identifier is "MacBookPro7,2", and we put
    // "MacBookPro" as machine_model_name, and "7.2" as machine_model_version.
    std::string machine_model_name;

    // The version of the machine model. Currently it is supported on MacOSX.
    // See machine_model_name's comment.
    std::string machine_model_version;
};

struct pls_cpu_info {
    std::string                 name;
    int64_t                     hz; // clock frequency
    int64_t                     tick; // ms per Hz tick
    float                       free_mem;
    float                       total_mem;
    unsigned long               logical_cores;
    unsigned long               physical_cores;
};

struct pls_monitor_info {
    enum pls_origin { pls_origin_bottom_left, pls_origin_top_left };
    struct pls_monitor_rect {
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;
    };
    
    // Scale factor from DIPs to physical pixels.
    float dip_to_pixel_scale = 1.0f;
    
    // Bounds of the desktop excluding monitors with DPI settings different from
    // the main monitor. In Density-Independent Pixels (DIPs).
    pls_monitor_rect bounds;
    
    // Cocoa identifier for this display.
    uint32_t monitor_id = 0;
    
    // Display type, built-in or external.
    bool is_builtin;
};

LIBUTILSAPI_API std::string pls_get_cpu_info();

/// If you have a Mac with Apple Silicon (such as the “M1” chip), you might only see the “Chip” listing,
/// with no special line for “Graphics.” That’s because the GPU and CPU come integrated on the M1 chip.
/// So in this case, “Apple M1” is technically the designation for both the CPU and GPU on our example Mac.
LIBUTILSAPI_API std::string pls_get_gpu_info();

LIBUTILSAPI_API double pls_get_gpu_device_usage();

LIBUTILSAPI_API bool pls_get_basic_graphics_info(pls_gpu_basic_info &gpu_info);
LIBUTILSAPI_API std::string pls_print_basic_graphics_info(const pls_gpu_basic_info &gpu_info);

LIBUTILSAPI_API bool pls_get_cpu_info(pls_cpu_info &cpu_info);
LIBUTILSAPI_API std::string pls_print_cpu_info(const pls_cpu_info &info);

LIBUTILSAPI_API bool pls_get_monitors_info(std::vector<pls_monitor_info> &infos);
LIBUTILSAPI_API std::string pls_print_monitors_info(const std::vector<pls_monitor_info> &infos);

#endif /* pls_gpu_basic_info_h */
