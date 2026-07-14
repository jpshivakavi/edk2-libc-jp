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
   
   Print(L"Image base: 0x%lx\n", loaded_image_protocol->ImageBase);
   Print(L"UefiMain: 0x%lx\n", UefiMain);
   
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
            __asm__ __volatile__("pause");
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

   EFI_RNG_PROTOCOL *rng_protocol = NULL;
   status = systab->BootServices->LocateProtocol(
       &gEfiRngProtocolGuid,
       NULL,
       (VOID **)&rng_protocol);
         
   return status;
}
