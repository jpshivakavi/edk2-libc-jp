/*
 * Phase V2 exit check: pyconfig.h SIZEOF_* and PLATFORM under the UEFI toolchains.
 *
 * MSVC: compile with cl /DUEFI_MSVC_64 (matches Python312.inf [BuildOptions.X64]).
 * GCC:  compile without UEFI_MSVC_* (reference AppPkg X64 port).
 */
#include "pyconfig.h"

#if defined(_MSC_VER)

#ifndef UEFI_MSVC_64
#error "MSVC V2 verify: define /DUEFI_MSVC_64 (Python312.inf [BuildOptions.X64])"
#endif

#if SIZEOF_LONG != 4
#error MSVC LLP64: SIZEOF_LONG must be 4
#endif
#if ALIGNOF_LONG != 4
#error MSVC LLP64: ALIGNOF_LONG must be 4
#endif
#if SIZEOF_OFF_T != 8
#error UEFI_MSVC_64: SIZEOF_OFF_T must be 8
#endif
#if SIZEOF_SIZE_T != 8
#error UEFI_MSVC_64: SIZEOF_SIZE_T must be 8
#endif
#if SIZEOF_UINTPTR_T != 8
#error UEFI_MSVC_64: SIZEOF_UINTPTR_T must be 8
#endif
#if SIZEOF_VOID_P != 8
#error UEFI_MSVC_64: SIZEOF_VOID_P must be 8
#endif

#else /* GCC reference port */

#ifdef UEFI_MSVC_64
#error "GCC V2 verify: do not define UEFI_MSVC_64"
#endif
#ifdef UEFI_MSVC_32
#error "GCC V2 verify: do not define UEFI_MSVC_32"
#endif

#if SIZEOF_LONG != 8
#error GCC LP64: SIZEOF_LONG must be 8
#endif
#if ALIGNOF_LONG != 8
#error GCC LP64: ALIGNOF_LONG must be 8
#endif
#if SIZEOF_OFF_T != 8
#error GCC UEFI X64: SIZEOF_OFF_T must be 8
#endif
#if SIZEOF_SIZE_T != 8
#error GCC UEFI X64: SIZEOF_SIZE_T must be 8
#endif
#if SIZEOF_UINTPTR_T != 8
#error GCC UEFI X64: SIZEOF_UINTPTR_T must be 8
#endif
#if SIZEOF_VOID_P != 8
#error GCC UEFI X64: SIZEOF_VOID_P must be 8
#endif

#endif

#if !defined(PLATFORM)
#error PLATFORM must be defined
#endif

typedef char platform_is_uefi[(sizeof(PLATFORM) == sizeof("uefi")) ? 1 : -1];

int verify_pyconfig_sizes(void)
{
    return (int)sizeof(platform_is_uefi);
}
