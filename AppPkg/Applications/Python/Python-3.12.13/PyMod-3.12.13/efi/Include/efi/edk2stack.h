#pragma once

#define PY_UEFI_DEFAULT_STACK_SIZE (64*1024*1024)

void edk2_switch_stack(uint64_t stack, uint64_t size);
void edk2_revert_stack();
