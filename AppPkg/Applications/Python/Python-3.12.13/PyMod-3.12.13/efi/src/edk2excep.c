#include <setjmp.h>
#include <signal.h>

#include <Uefi.h>
#include <Library/UefiLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/Cpu.h>
#include <Protocol/Shell.h>
#include <Protocol/Rng.h>

#include "Python.h"

#include "efi/edk2main.h"
#include "efi/edk2excep.h"

void
py_common_interrupt_entry();

static edk2_seh_context_t g_context[EDK2_SEH_CONTEXT_SIZE];
static int g_context_index = -1;

void *
edk2_seh_try()
{  
  edk2_seh_context_t *entry = NULL;

  if(++g_context_index >= EDK2_SEH_CONTEXT_SIZE)
    return NULL;

  entry = g_context + g_context_index;
  
  memset((char *)entry, '\0', sizeof(edk2_seh_context_t));

  entry->magic = EDK2_SEH_MAGIC;

  return &entry->ret_context;
}

void
edk2_seh_catch(INTN raise, uint64_t *exc_type, uint8_t *buf, size_t bufsize)
{
  edk2_seh_context_t *entry = NULL;

  if(g_context_index < 0) 
    return;
  
  entry = g_context + g_context_index--;

  if(entry && entry->magic != EDK2_SEH_MAGIC)
    return;

  if (raise) {
    *exc_type = entry->exc_type;
    if (buf != NULL && bufsize > 0) {
      size_t src_size = sizeof(EFI_SYSTEM_CONTEXT_X64);
      size_t dst_size = (src_size < bufsize) ? src_size : bufsize;
      memcpy(buf, (uint8_t*)&entry->exc_context, dst_size);          
    }
  }

  memset((char *)entry, '\0', sizeof(edk2_seh_context_t));
}



EFIAPI VOID
py_handle_exception(IN CONST EFI_EXCEPTION_TYPE InterruptType,
                    IN CONST EFI_SYSTEM_CONTEXT SystemContext)
{
  if(g_context_index < 0) {  
    volatile int exc_trap = 1;
    void *p = g_edk2_globals.loaded_image->ImageBase;
    int signum = SIGABRT;
    
    switch(InterruptType) {
      case EXCEPT_X64_PAGE_FAULT:
        signum = SIGSEGV;
        break;
      case EXCEPT_X64_INVALID_OPCODE:
        signum = SIGILL;
        break;
      case EXCEPT_X64_FP_ERROR:
        signum = SIGFPE;
        break;          
    }

    raise(signum);
    
    while (exc_trap) {
      asm("mov %0, %%rax; "
          :
          : "r"(p) /* input */
          : "%rax" /* clobbered register */
      );
      __asm__ __volatile__("pause");
    }

    return;
  }

  edk2_seh_context_t *entry = g_context + g_context_index;

  entry->exc_type = InterruptType;
  entry->exc_context = *SystemContext.SystemContextX64;
  
  longjmp(entry->ret_context, 1);
}

//
// Error code flag indicating whether or not an error code will be
// pushed on the stack if an exception occurs.
//
// 1 means an error code will be pushed, otherwise 0
//
CONST UINT32 py_error_code_flag = 0x00027d00;

static uint64_t old_idtr[2] = {0};
static edk2_idt_entry_t *new_idt = NULL;
static uint8_t *new_idt_trampoline = NULL;
static UINTN trampoline_size_pages = 0;

EFI_STATUS
py_install_idt()
{
  EFI_STATUS status;
  uint8_t *old_idt;
  uint64_t old_idt_size = 0;
  int max_seh_exception = 0;
  // clang-format off
  uint8_t trampoline_code[] = {
      0x6a, 0, // push vector_num
      0x50, // push rax
      0x48, 0xb8, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, // movabs rax, jmp_addr
      0xff, 0xe0 // jmp rax
  };
  // clang-format on
  EFI_SYSTEM_TABLE *systab = g_edk2_globals.system_table;
  EFI_BOOT_SERVICES *bootsvc = systab->BootServices;

  const int x64_seh_exceptions[] = {EXCEPT_X64_DIVIDE_ERROR,
                                    EXCEPT_X64_OVERFLOW,
                                    EXCEPT_X64_BOUND,
                                    EXCEPT_X64_INVALID_OPCODE,
                                    EXCEPT_X64_INVALID_TSS,
                                    EXCEPT_X64_SEG_NOT_PRESENT,
                                    EXCEPT_X64_STACK_FAULT,
                                    EXCEPT_X64_GP_FAULT,
                                    EXCEPT_X64_PAGE_FAULT,
                                    EXCEPT_X64_FP_ERROR,
                                    EXCEPT_X64_ALIGNMENT_CHECK,
                                    EXCEPT_X64_MACHINE_CHECK,
                                    EXCEPT_X64_SIMD};
  const int x64_seh_exceptions_size = sizeof(x64_seh_exceptions) / sizeof(int);

  for (int i = 0; i < x64_seh_exceptions_size; i++) {
    max_seh_exception = MAX(max_seh_exception, x64_seh_exceptions[i]);
  }

  status = bootsvc->AllocatePages(AllocateAnyPages,
                                  EfiBootServicesData,
                                  1,
                                  (VOID *)&new_idt);

  if (EFI_ERROR(status)) return status;

  trampoline_size_pages =
     (max_seh_exception * sizeof(trampoline_code) + 4095) / 4096;

  status = bootsvc->AllocatePages(AllocateAnyPages,
                                  EfiBootServicesCode,
                                  trampoline_size_pages,
                                  (VOID *)&new_idt_trampoline);

  if (EFI_ERROR(status)) {
    bootsvc->FreePages((uint64_t)new_idt, 1);
    return status;
  }

  edk2_get_idtr((uint8_t *)old_idtr);

  old_idt = (uint8_t *)old_idtr[1];
  old_idt_size = old_idtr[0] >> 48;

  memcpy((uint8_t *)new_idt, old_idt, MIN(old_idt_size, 4096));

  for (int i = 0; i < x64_seh_exceptions_size; i++) {
    int id = x64_seh_exceptions[i];
    uint8_t *trampoline = new_idt_trampoline + i * sizeof(trampoline_code);
    memcpy(trampoline, trampoline_code, sizeof(trampoline_code));

    trampoline[1] = id;

    *(uint64_t *)(trampoline + 5) = (uint64_t)py_common_interrupt_entry;

    new_idt[id].offset_32_63 = ((uint64_t)trampoline >> 32) & 0xffffffff;
    new_idt[id].offset_16_31 = ((uint64_t)trampoline >> 16) & 0xffff;
    new_idt[id].offset_0_15 = (uint64_t)trampoline & 0xffff;
  }

  uint64_t new_idtr[2] = {old_idtr[0], (uint64_t)new_idt};

  edk2_set_idtr((uint8_t *)new_idtr);

  return status;
}

void
py_restore_idt()
{
  EFI_SYSTEM_TABLE *systab = g_edk2_globals.system_table;
  EFI_BOOT_SERVICES *bootsvc = systab->BootServices;
   
  if(old_idtr[1] != 0)
     edk2_set_idtr((uint8_t *)old_idtr);
  
  if(new_idt)
     bootsvc->FreePages((uint64_t)new_idt, 1);
      
  if(new_idt_trampoline)
     bootsvc->FreePages((uint64_t)new_idt_trampoline, trampoline_size_pages);
  
}
