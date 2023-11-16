//
//  mach_backtrace.h
//  WatchDog
//
//  Created by Keven on 2020/10/13.
//
//

#ifndef mach_backtrace_h
#define mach_backtrace_h

#include <mach/mach.h>

/**
 *  fill a backtrace call stack array of given thread
 *
 *  @param thread   mach thread for tracing
 *  @param stack    caller space for saving stack trace info
 *  @param maxSymbols max stack array count
 *
 *  @return call stack address array
 */

int mach_backtrace(thread_t thread, uintptr_t *stack, int maxSymbols);

#endif /* mach_backtrace_h */
