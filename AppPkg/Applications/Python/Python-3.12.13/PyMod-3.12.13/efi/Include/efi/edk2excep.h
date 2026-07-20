#pragma once

#include <Uefi.h>
#include <setjmp.h>
#include <stdint.h>

#define EDK2_SEH_MAGIC 0x0053454820435458
#define EDK2_SEH_CONTEXT_SIZE 10

struct _edk2_seh_context;

typedef struct _edk2_seh_context {
  uint64_t magic;  
  jmp_buf ret_context;
  EFI_EXCEPTION_TYPE exc_kind;
  EFI_SYSTEM_CONTEXT_X64 exc_context;
} edk2_seh_context_t;
#if defined(_MSC_VER)
# pragma pack(push, 1)
typedef struct _excep_idt_entry {
  UINT16 offset_0_15;
  UINT16 segment;
  UINT8 ist_and_reserved;
  UINT8 type_dpl_p;
  UINT16 offset_16_31;
  UINT32 offset_32_63;
  UINT32 reserved2;
} edk2_idt_entry_t;
# pragma pack(pop)
#else
typedef struct _excep_idt_entry {
  uint64_t offset_0_15 : 16;
  uint64_t segment : 16;

  uint64_t ist : 3;
  uint64_t reserved1 : 5;
  uint64_t id : 3;
  uint64_t size : 2;
  uint64_t dpl : 2;
  uint64_t p : 1;
  uint64_t offset_16_31 : 16;

  uint64_t offset_32_63 : 32;
  uint64_t reserved2 : 32;
} edk2_idt_entry_t;
#endif

void
edk2_get_idtr(uint8_t *);
void
edk2_set_idtr(uint8_t *);
void *
edk2_seh_try();
void
edk2_seh_catch(INTN should_raise, uint64_t *exc_kind_out, uint8_t *buf, size_t bufsize);
EFI_STATUS
py_install_idt();
void
py_restore_idt();
