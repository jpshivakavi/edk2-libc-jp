/** -*- C -*-
 * @file
 *
 * @brief Common stuff for edk2 Python
 *
 * Copyright (c) 2016-2019 Intel Corporation. All rights reserved.
 *
 * @page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Intel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY INTEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL INTEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <Uefi.h>
#include <stdint.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextInEx.h>
#include <Protocol/Cpu.h>
#include <Protocol/Shell.h>
#include <Protocol/Rng.h>

typedef struct _edk2_globals {
   EFI_HANDLE image_handle;
   EFI_SYSTEM_TABLE *system_table;
   void *stack;
   uint64_t stack_size;
   /* Lowest rsp PyOS_CheckStack() will treat as safe, margin already applied.
    * Set on both entry paths, including PY_UEFI_MSVC_368_ENTRY where `stack`
    * stays NULL because no switch happens. Zero means "unknown". */
   uint64_t stack_limit;
   /* rsp sampled at UefiMain entry, i.e. near the top of the firmware stack. */
   uint64_t stack_entry_rsp;
   /* ShellCEntryLib's return value, parked here across the stack switch on the
    * PY_UEFI_MSVC_STACK_SWITCH path because UefiMain's own locals are not
    * addressable while rsp points at the switched stack under MSVC. */
   EFI_STATUS switch_status;
   /* Deepest (lowest) rsp PyOS_CheckStack() ever observed. Sampled, so it
    * under-reports the true peak, but it brackets the firmware stack base:
    * a value from a run that exits cleanly lies above the base, one from a run
    * that corrupts the firmware lies below it. Zero means "never sampled". */
   uint64_t stack_min_rsp;
   
   EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
   EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *console_in;
   EFI_CPU_ARCH_PROTOCOL *cpu;
   EFI_SHELL_PROTOCOL *shell;
   EFI_RNG_PROTOCOL *rng;
} edk2_globals_t;

extern edk2_globals_t g_edk2_globals;
