#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <setjmp.h>

#include  <Uefi.h>
#include  <Library/UefiLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/Cpu.h>
#include <Protocol/Shell.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/Rng.h>

#include "efi/edk2main.h"
#include "efi/edk2console_api.h"
#include "efi/edk2stack.h"
#include "efi/edk2asm.h"
#include "efi/environ.h"
#include "efi/py312boot.h"
#include "efi/edk2excep.h"

#ifdef PY_UEFI_BOOT_TRACE
#define PY312_BOOT_PRINT(Step) Print(L"Python312 boot: " Step L"\n")
#else
#define PY312_BOOT_PRINT(Step) do { } while (0)
#endif

#ifdef PY_UEFI_BOOT_TRACE
void
py312_boot_print_ascii(const char *msg)
{
    if (msg != NULL) {
        Print(L"Python312 boot: %a\n", msg);
    }
}
#endif

edk2_globals_t g_edk2_globals;

extern EFI_STATUS EFIAPI ShellCEntryLib (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
);

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        image,
  IN EFI_SYSTEM_TABLE  *systab
  )
{
   EFI_LOADED_IMAGE_PROTOCOL *loaded_image_protocol;
   EFI_STATUS status;

   memset(&g_edk2_globals, 0, sizeof(g_edk2_globals));

   Print(L"Python312: UefiMain\r\n");
   PY312_BOOT_PRINT(L"UefiMain enter");

   //
   // turn off the watchdog timer
   //
   systab->BootServices->SetWatchdogTimer (0, 0, 0, NULL);
   
   status = systab->BootServices->OpenProtocol(
      image,
      &gEfiLoadedImageProtocolGuid,
      (VOID **)&loaded_image_protocol,
      image,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
   );
   
   if (EFI_ERROR(status)) {
      Print(L"Failed to access image info: %r\n", status);      
      return status;
   }

   EFI_HANDLE cpu_arch_handle[10] = {0};
   UINTN buffer_size = sizeof(cpu_arch_handle);
   
   status = systab->BootServices->LocateHandle(
      ByProtocol,
      &gEfiCpuArchProtocolGuid,
      NULL,
      &buffer_size,
      cpu_arch_handle
   );

   if (EFI_ERROR(status)) {
      Print(L"Failed to find CPU protocol: %r\n", status);      
      return status;
   }
   
   EFI_CPU_ARCH_PROTOCOL *cpu_protocol;
   
   status = systab->BootServices->OpenProtocol(
      cpu_arch_handle[0],
      &gEfiCpuArchProtocolGuid,
      (VOID **)&cpu_protocol,
      image,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
   );
   
   if (EFI_ERROR(status)) {
      Print(L"Failed to access CPU protocol: %r\n", status);      
      return status;
   }
   
   EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *console_in = NULL;     

#ifndef PY_UEFI_PYREADLINE
   /* Default UEFI build: no pyreadline; do not open ConInEx from the app. */
#else
   status = systab->BootServices->OpenProtocol(
      systab->ConsoleInHandle,
      &gEfiSimpleTextInputExProtocolGuid,
      (VOID**)&console_in,
      image,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
   );

   if (EFI_ERROR(status)) {
      Print(L"Failed to open input console: %r\n", status);      
   }
#endif
   
   EFI_SHELL_PARAMETERS_PROTOCOL *args_protocol = NULL;

   status = systab->BootServices->OpenProtocol(
      image,
      &gEfiShellParametersProtocolGuid,
      (VOID **)&args_protocol,
      image,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
   );
   
   if (!EFI_ERROR(status)) {
      if(args_protocol->Argc > 1 &&
         memcmp(args_protocol->Argv[args_protocol->Argc-1],
                "-\000-\000s\000t\000a\000r\000t\000d\000e\000l\000a\000y\000",
                24) == 0) {
         volatile int wait = 1;
        while(wait)
         {
            EDK2_PAUSE();
         }

         args_protocol->Argc -= 1;
      }
   }

   EFI_SHELL_PROTOCOL *shell_protocol = NULL;
   status = systab->BootServices->OpenProtocol(
      image,
      &gEfiShellProtocolGuid,
      (VOID **)&shell_protocol,
      image,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
   );
   
   if (EFI_ERROR(status)) {
      status = systab->BootServices->LocateProtocol(
         &gEfiShellProtocolGuid,
         NULL,
         (VOID **)&shell_protocol
      );
   }
   if (!EFI_ERROR(status)) {
      g_edk2_globals.shell = shell_protocol;
   }

   EFI_RNG_PROTOCOL *rng_protocol = NULL;
   status = systab->BootServices->LocateProtocol(
       &gEfiRngProtocolGuid,
       NULL,
       (VOID **)&rng_protocol);
   
   g_edk2_globals.image_handle = image;
   g_edk2_globals.system_table = systab;
   g_edk2_globals.loaded_image = loaded_image_protocol;
   g_edk2_globals.console_in = console_in;
   g_edk2_globals.cpu = cpu_protocol;
   g_edk2_globals.rng = rng_protocol;

#ifdef PY_UEFI_PYREADLINE
   PY312_BOOT_PRINT(L"console prepare for launch");
   edk2_console_prepare_for_launch();
#endif

   py312_uefi_reentry_cleanup();

   edk2_alloc_environ();

#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_368_ENTRY) && \
    !defined(PY_UEFI_MSVC_STACK_SWITCH)
   /* Python 3.6.8 AppPkg: ENTRY_POINT = ShellCEntryLib on the firmware stack
    * (no edk2_switch_stack / py_install_idt). VS2022 3.12 hang reproduces
    * inside ShellCEntryLib only after stack switch — try 368-style path. */
   /* That hang is now explained: edk2stack.nasm read its arguments from rdi/rsi
    * (System V) while MSVC passes them in rcx/rdx, so edk2_switch_stack set rsp
    * from garbage. Fixed via PY_UEFI_MS_ABI in MSFT:*_*_*_NASM_FLAGS. Define
    * PY_UEFI_MSVC_STACK_SWITCH to take the 64 MB stack path on MSVC instead of
    * this one; it stays opt-in until validated on hardware. */
   /* No stack switch on this path, so g_edk2_globals.stack stays NULL and
    * PyOS_CheckStack() has no bound to compare against. Derive one from the
    * current rsp instead, or deep recursion silently runs off the firmware
    * stack and corrupts memory the Shell and BDS still need. */
   {
      uint64_t entry_rsp = edk2_read_rsp();

      g_edk2_globals.stack_entry_rsp = entry_rsp;
      if (entry_rsp > PY_UEFI_FIRMWARE_STACK_BUDGET) {
         g_edk2_globals.stack_limit = entry_rsp - PY_UEFI_FIRMWARE_STACK_BUDGET;
      }
#ifdef PY_UEFI_BOOT_TRACE
      Print(L"Python312 boot: firmware stack rsp=%lx limit=%lx budget=%lx\n",
            (UINT64)entry_rsp,
            (UINT64)g_edk2_globals.stack_limit,
            (UINT64)PY_UEFI_FIRMWARE_STACK_BUDGET);
#endif
   }
   PY312_BOOT_PRINT(L"ShellCEntryLib 368-style (no custom stack/IDT)");
   status = ShellCEntryLib(image, systab);
   PY312_BOOT_PRINT(L"after ShellCEntryLib");
#ifdef PY_UEFI_BOOT_TRACE
   /* How deep execution actually got, so the budget can be sized from
    * measurement instead of guesswork. `used` past `limit` is the excursion the
    * sampled guard failed to stop; the firmware stack base sits between the
    * min_rsp of a run that hangs and that of a run that exits cleanly. */
   Print(L"Python312 boot: stack high-water min_rsp=%lx limit=%lx used=%lx\n",
         (UINT64)g_edk2_globals.stack_min_rsp,
         (UINT64)g_edk2_globals.stack_limit,
         (UINT64)(g_edk2_globals.stack_min_rsp == 0
                     ? 0
                     : g_edk2_globals.stack_entry_rsp -
                          g_edk2_globals.stack_min_rsp));
#endif
   edk2_free_environ();
   PY312_BOOT_PRINT(L"after edk2_free_environ");
   PY312_BOOT_PRINT(L"before return from UefiMain");
   return status;
#endif

   g_edk2_globals.stack_size = PY_UEFI_DEFAULT_STACK_SIZE; 
   g_edk2_globals.stack = malloc(g_edk2_globals.stack_size + 1024);
   if(g_edk2_globals.stack == NULL) {
      Print(L"Failed to allocate stack memory\n");
      return EFI_OUT_OF_RESOURCES;
   }

   edk2_alloc_environ();

   PY312_BOOT_PRINT(L"before switch_stack");

#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_STACK_SWITCH)
   /* edk2_switch_stack() leaves rsp at base+size-0x200, so base+size must be
    * 16-byte aligned or MSVC's SSE spills (movaps) fault. Round the base *up*
    * to 512; the malloc above over-allocates by 1024, so this cannot overrun.
    *
    * The GCC expression below adds (base % 512) instead, which does not align
    * anything — it just offsets by an arbitrary amount. It is left untouched
    * because GCC is the signed-off toolchain and changing it would invalidate
    * that sign-off; it should be corrected separately, with a GCC re-test. */
   uint64_t aligned_stack = ((uint64_t)g_edk2_globals.stack + 511) & ~(uint64_t)511;
#else
   uint64_t aligned_stack = (uint64_t)g_edk2_globals.stack +
                            (((uint64_t)g_edk2_globals.stack) % 512);
#endif
   edk2_switch_stack(aligned_stack, g_edk2_globals.stack_size);

   g_edk2_globals.stack_limit = (uint64_t)g_edk2_globals.stack +
                                PY_UEFI_STACK_MARGIN;

   /* The IDT is independent of the stack switch. Skip it on the MSVC opt-in
    * path so the two can be brought up one at a time: the stack is what fixes
    * the depth problem, the IDT only adds fault reporting. */
#if !(defined(_MSC_VER) && defined(PY_UEFI_MSVC_STACK_SWITCH) && \
      !defined(PY_UEFI_MSVC_IDT))
   PY312_BOOT_PRINT(L"before py_install_idt");
   py_install_idt();
#else
   PY312_BOOT_PRINT(L"skipping py_install_idt (MSVC stack-switch opt-in)");
#endif
   PY312_BOOT_PRINT(L"before ShellCEntryLib");
#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_STACK_SWITCH)
   /* edk2_switch_stack() moves rsp out from under a *running* function. GCC at
    * -O0 keeps a frame pointer, so UefiMain's locals stay reachable through rbp
    * into the old stack. MSVC x64 addresses locals and spilled parameters
    * rsp-relative with no frame pointer, so after the switch `image`, `systab`
    * and `status` all resolve into the new stack at the old offsets — garbage.
    * Passing garbage handles to ShellCEntryLib is what hangs it.
    *
    * Read the handles from globals instead (already populated above) and park
    * the result in one too; both are RIP-relative and immune to the switch.
    * UefiMain's own frame becomes valid again after edk2_revert_stack(). */
   g_edk2_globals.switch_status = ShellCEntryLib(g_edk2_globals.image_handle,
                                                g_edk2_globals.system_table);
#else
   status = ShellCEntryLib(image, systab);
#endif
   PY312_BOOT_PRINT(L"after ShellCEntryLib");
#if !(defined(_MSC_VER) && defined(PY_UEFI_MSVC_STACK_SWITCH) && \
      !defined(PY_UEFI_MSVC_IDT))
   py_restore_idt();
#endif
   
   edk2_revert_stack();
   g_edk2_globals.stack_limit = 0;
#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_STACK_SWITCH)
   /* Frame is addressable again now that rsp is restored. */
   status = g_edk2_globals.switch_status;
#endif

   edk2_free_environ();
   PY312_BOOT_PRINT(L"after edk2_free_environ");
   
   free(g_edk2_globals.stack);
   g_edk2_globals.stack = NULL;

   PY312_BOOT_PRINT(L"before return from UefiMain");
   return status;
}

/* Nonzero means "overflow imminent". Callers turn that into a MemoryError:
 * Objects/object.c raises "stack overflow" (PyObject_Repr/Str paths) and
 * ceval.c:263 _Py_CheckRecursiveCall() raises "Stack overflow". */
int
PyOS_CheckStack(void)
{
   uint64_t limit = g_edk2_globals.stack_limit;
   uint64_t rsp = edk2_read_rsp();

   if(g_edk2_globals.stack_min_rsp == 0 || rsp < g_edk2_globals.stack_min_rsp)
      g_edk2_globals.stack_min_rsp = rsp;

   if(limit == 0)
      return 0;

   return rsp <= limit;
}
