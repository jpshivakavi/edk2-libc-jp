#pragma once

#include <stdint.h>

#define PY_UEFI_DEFAULT_STACK_SIZE (64*1024*1024)

/* Reserve below the usable stack bound so PyOS_CheckStack() trips while there
 * is still room to raise and unwind the MemoryError. */
#ifndef PY_UEFI_STACK_MARGIN
#define PY_UEFI_STACK_MARGIN (8*1024)
#endif

void edk2_switch_stack(uint64_t stack, uint64_t size);
void edk2_revert_stack(void);
uint64_t edk2_read_rsp(void);
