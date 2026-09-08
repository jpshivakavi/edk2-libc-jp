#pragma once

void edk2_pause(void);

#if defined(_MSC_VER)
/* UEFI/EDK2 MSVC builds do not expose intrin.h (see pycore_atomic.h). */
# define EDK2_PAUSE() edk2_pause()
# define EDK2_RW_BARRIER() do { volatile int _edk2_barrier = 0; (void)_edk2_barrier; } while (0)
#else
# define EDK2_PAUSE() __asm__ __volatile__("pause")
# define EDK2_RW_BARRIER() __asm__ __volatile__("" ::: "memory")
#endif
