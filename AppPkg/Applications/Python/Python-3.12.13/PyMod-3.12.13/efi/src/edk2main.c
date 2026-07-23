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
#include "efi/edk2excep.h"
#include "efi/py312boot.h"

#ifdef PY_UEFI_BOOT_TRACE
#define PY312_BOOT_PRINT(Step) Print(L"Python312 boot: " Step L"\n")
#else
#define PY312_BOOT_PRINT(Step) do { } while (0)
#endif

void
py312_boot_print_ascii(const char *msg)
{
#ifdef PY_UEFI_BOOT_TRACE
    if (msg != NULL) {
        Print(L"Python312 boot: %a\n", msg);
    }
#endif
}

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

#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_368_ENTRY)
   /* Python 3.6.8 AppPkg: ENTRY_POINT = ShellCEntryLib on the firmware stack
    * (no edk2_switch_stack / py_install_idt). VS2022 3.12 hang reproduces
    * inside ShellCEntryLib only after stack switch — try 368-style path. */
   PY312_BOOT_PRINT(L"ShellCEntryLib 368-style (no custom stack/IDT)");
   status = ShellCEntryLib(image, systab);
   PY312_BOOT_PRINT(L"after ShellCEntryLib");
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

   uint64_t aligned_stack = (uint64_t)g_edk2_globals.stack +
                            (((uint64_t)g_edk2_globals.stack) % 512);
   edk2_switch_stack(aligned_stack, g_edk2_globals.stack_size);

   PY312_BOOT_PRINT(L"before py_install_idt");
   py_install_idt();
   PY312_BOOT_PRINT(L"before ShellCEntryLib");
   status = ShellCEntryLib(image, systab);
   PY312_BOOT_PRINT(L"after ShellCEntryLib");
   py_restore_idt();
   
   edk2_revert_stack();

   edk2_free_environ();
   PY312_BOOT_PRINT(L"after edk2_free_environ");
   
   free(g_edk2_globals.stack);
   g_edk2_globals.stack = NULL;

   PY312_BOOT_PRINT(L"before return from UefiMain");
   return status;
}

int
PyOS_CheckStack(void)
{
   uint64_t rsp = edk2_read_rsp();
   if(rsp > (uint64_t)g_edk2_globals.stack)
      return 0;
   return 1;
}
