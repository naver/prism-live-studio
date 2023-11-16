//
//  mach_backtrace.c
//  WatchDog
//
//  Created by Keven on 2020/10/13.
//
//

#include "mach_backtrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <machine/_mcontext.h>

// macro `MACHINE_THREAD_STATE` shipped with system header is wrong..
#if defined __i386__
#define THREAD_STATE_FLAVOR x86_THREAD_STATE
#define THREAD_STATE_COUNT x86_THREAD_STATE_COUNT
#define __framePointer __ebp

#elif defined __x86_64__
#define THREAD_STATE_FLAVOR x86_THREAD_STATE64
#define THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#define __framePointer __rbp

#elif defined __arm__
#define THREAD_STATE_FLAVOR ARM_THREAD_STATE
#define THREAD_STATE_COUNT ARM_THREAD_STATE_COUNT
#define __framePointer __r[7]

#elif defined __arm64__
#define THREAD_STATE_FLAVOR ARM_THREAD_STATE64
#define THREAD_STATE_COUNT ARM_THREAD_STATE64_COUNT
#define __framePointer __fp

#else
#error "Current CPU Architecture is not supported"
#endif

#pragma - mark DEFINE MACRO FOR DIFFERENT CPU ARCHITECTURE
#if defined(__arm64__)
#define DETAG_INSTRUCTION_ADDRESS(A) ((A) & ~(3UL))
#define THREAD_STATE_COUNT ARM_THREAD_STATE64_COUNT
#define THREAD_STATE ARM_THREAD_STATE64
#define FRAME_POINTER __fp
#define STACK_POINTER __sp
#define INSTRUCTION_ADDRESS __pc

#elif defined(__arm__)
#define DETAG_INSTRUCTION_ADDRESS(A) ((A) & ~(1UL))
#define THREAD_STATE_COUNT ARM_THREAD_STATE_COUNT
#define THREAD_STATE ARM_THREAD_STATE
#define FRAME_POINTER __r[7]
#define STACK_POINTER __sp
#define INSTRUCTION_ADDRESS __pc

#elif defined(__x86_64__)
#define DETAG_INSTRUCTION_ADDRESS(A) (A)
#define THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE x86_THREAD_STATE64
#define FRAME_POINTER __rbp
#define STACK_POINTER __rsp
#define INSTRUCTION_ADDRESS __rip

#elif defined(__i386__)
#define DETAG_INSTRUCTION_ADDRESS(A) (A)
#define THREAD_STATE_COUNT x86_THREAD_STATE32_COUNT
#define THREAD_STATE x86_THREAD_STATE32
#define FRAME_POINTER __ebp
#define STACK_POINTER __esp
#define INSTRUCTION_ADDRESS __eip

#endif

#define CALL_INSTRUCTION_FROM_RETURN_ADDRESS(A) (DETAG_INSTRUCTION_ADDRESS((A)) - 1)

#if defined(__LP64__)
#define TRACE_FMT "%-4d%-31s 0x%016lx %s + %lu"
#define POINTER_FMT "0x%016lx"
#define POINTER_SHORT_FMT "0x%lx"
#define NLIST struct nlist_64
#else
#define TRACE_FMT "%-4d%-31s 0x%08lx %s + %lu"
#define POINTER_FMT "0x%08lx"
#define POINTER_SHORT_FMT "0x%lx"
#define NLIST struct nlist
#endif

typedef struct StackFrameEntry {
	const struct StackFrameEntry *const previous;
	const uintptr_t return_address;
} StackFrameEntry;

kern_return_t mach_copyMem(const void *const src, void *const dst, const size_t numBytes)
{
	vm_size_t bytesCopied = 0;
	return vm_read_overwrite(mach_task_self(), (vm_address_t)src, (vm_size_t)numBytes, (vm_address_t)dst, &bytesCopied);
}

/**
 *  fill a backtrace call stack array of given thread
 *
 *  Stack frame structure for x86/x86_64:
 *
 *    | ...                   |
 *    +-----------------------+ hi-addr     ------------------------
 *    | func0 ip              |
 *    +-----------------------+
 *    | func0 bp              |--------|     stack frame of func1
 *    +-----------------------+        v
 *    | saved registers       |  bp <- sp
 *    +-----------------------+   |
 *    | local variables...    |   |
 *    +-----------------------+   |
 *    | func2 args            |   |
 *    +-----------------------+   |         ------------------------
 *    | func1 ip              |   |
 *    +-----------------------+   |
 *    | func1 bp              |<--+          stack frame of func2
 *    +-----------------------+
 *    | ...                   |
 *    +-----------------------+ lo-addr     ------------------------
 *
 *  list we need to get is `ip` from bottom to top
 *
 *
 *  Stack frame structure for arm/arm64:
 *
 *    | ...                   |
 *    +-----------------------+ hi-addr     ------------------------
 *    | func0 lr              |
 *    +-----------------------+
 *    | func0 fp              |--------|     stack frame of func1
 *    +-----------------------+        v
 *    | saved registers       |  fp <- sp
 *    +-----------------------+   |
 *    | local variables...    |   |
 *    +-----------------------+   |
 *    | func2 args            |   |
 *    +-----------------------+   |         ------------------------
 *    | func1 lr              |   |
 *    +-----------------------+   |
 *    | func1 fp              |<--+          stack frame of func2
 *    +-----------------------+
 *    | ...                   |
 *    +-----------------------+ lo-addr     ------------------------
 *
 *  when function return, first jump to lr, then restore lr
 *  (namely first address in list is current lr)
 *
 *  fp (frame pointer) is r7 register under ARM and fp register in ARM64
 *  reference: iOS ABI Function Call Guide https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARMv7FunctionCallingConventions.html#//apple_ref/doc/uid/TP40009022-SW1
 *
 *  @param thread   mach thread for tracing
 *  @param stack    caller space for saving stack trace info
 *  @param maxSymbols max stack array count
 *
 *  @return call stack address array
 */
int mach_backtrace(thread_t thread, uintptr_t *stack, int maxSymbols)
{
	_STRUCT_MCONTEXT machineContext;
	mach_msg_type_number_t stateCount = THREAD_STATE_COUNT;

	kern_return_t kret = thread_get_state(thread, THREAD_STATE_FLAVOR, (thread_state_t) & (machineContext.__ss), &stateCount);
	if (kret != KERN_SUCCESS) {
		return 0;
	}

	int i = 0;

	const uintptr_t instructionAddress = machineContext.__ss.INSTRUCTION_ADDRESS;
	stack[i] = instructionAddress;

	++i;

#if defined(__arm__) || defined(__arm64__)
	stack[i] = (void *)machineContext.__ss.__lr;
	++i;
#endif

	StackFrameEntry frame = {0};
	const uintptr_t framePointer = machineContext.__ss.__framePointer;
	if (framePointer == 0 || mach_copyMem((void *)framePointer, &frame, sizeof(frame)) != KERN_SUCCESS) {
		return 0;
	}

	for (; i < maxSymbols; i++) {
		stack[i] = frame.return_address;
		if (stack[i] == 0 || frame.previous == 0 || mach_copyMem(frame.previous, &frame, sizeof(frame)) != KERN_SUCCESS) {
			break;
		}
	}

	return i;
}
