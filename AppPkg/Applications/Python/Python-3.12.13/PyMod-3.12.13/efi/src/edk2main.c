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
#include "efi/edk2stack.h"
#include "efi/edk2asm.h"
#include "efi/environ.h"
#include "efi/edk2excep.h"

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
      if (!EFI_ERROR(status)) {
         g_edk2_globals.shell = shell_protocol;         
      }      
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
   
   g_edk2_globals.stack_size = PY_UEFI_DEFAULT_STACK_SIZE; 
   g_edk2_globals.stack = malloc(g_edk2_globals.stack_size + 1024);
   if(g_edk2_globals.stack == NULL) {
      Print(L"Failed to allocate stack memory\n");
      return EFI_OUT_OF_RESOURCES;
   }

   edk2_alloc_environ();
   
   uint64_t aligned_stack = (uint64_t)g_edk2_globals.stack +
                            (((uint64_t)g_edk2_globals.stack) % 512);
   edk2_switch_stack(aligned_stack, g_edk2_globals.stack_size);

   py_install_idt();
   status = ShellCEntryLib(image, systab);
   py_restore_idt();
   
   edk2_revert_stack();

   edk2_free_environ();
   
   free(g_edk2_globals.stack);
   g_edk2_globals.stack = NULL;
      
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
