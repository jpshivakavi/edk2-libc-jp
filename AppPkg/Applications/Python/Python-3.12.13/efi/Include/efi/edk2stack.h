#pragma once

#include <stdint.h>

#define PY_UEFI_DEFAULT_STACK_SIZE (64*1024*1024)

/* Reserve below the usable stack bound so PyOS_CheckStack() trips while there
 * is still room to raise and unwind the MemoryError. */
#ifndef PY_UEFI_STACK_MARGIN
#define PY_UEFI_STACK_MARGIN (8*1024)
#endif

/* Usable depth assumed when Python runs on the *firmware* stack, i.e. the
 * PY_UEFI_MSVC_368_ENTRY path, which never calls edk2_switch_stack(). UEFI does
 * not expose the stack size to an application, so this is a deliberately
 * conservative budget measured down from rsp at UefiMain entry; a Shell app
 * typically gets on the order of 128 KB.
 *
 * Tuning: if Shell `exit` still hangs, this is too large and the overflow is
 * not being caught. If a legitimate script raises MemoryError("Stack overflow")
 * too eagerly, this is too small. Override with -DPY_UEFI_FIRMWARE_STACK_BUDGET. */
#ifndef PY_UEFI_FIRMWARE_STACK_BUDGET
#define PY_UEFI_FIRMWARE_STACK_BUDGET (96*1024)
#endif

void edk2_switch_stack(uint64_t stack, uint64_t size);
void edk2_revert_stack(void);
uint64_t edk2_read_rsp(void);
