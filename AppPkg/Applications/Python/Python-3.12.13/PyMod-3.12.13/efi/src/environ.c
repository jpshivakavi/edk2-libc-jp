#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <setjmp.h>
#include <wchar.h>
#include <errno.h>

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/Cpu.h>
#include <Protocol/Shell.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/Rng.h>

#include "efi/edk2main.h"
#include "efi/environ.h"

wchar_t **environ = NULL;

void edk2_alloc_environ()
{
   if(g_edk2_globals.shell == NULL)
      return;
   
   CONST CHAR16 *var_names = g_edk2_globals.shell->GetEnv(NULL);
   int num_vars = 0;
   size_t environ_size = 0, environ_values_size = 0;
   CONST CHAR16 *cur, *var;

   if(var_names == NULL)
      return;

   cur = var_names;
   var = var_names;
   while(*cur) {
      while(*cur++);
      CONST CHAR16 *value = g_edk2_globals.shell->GetEnv(var);
      if(value) {
         environ_values_size += sizeof(CHAR16) * (cur-var+2+StrLen(value));
      }
      num_vars++;      
      var = cur;
   }

   num_vars++;
   
   environ_size = sizeof(wchar_t*) * num_vars;
   char *env = (char*)malloc(environ_size + environ_values_size);
   memset(env, 0, environ_size + environ_values_size);
   
   environ = (wchar_t**)env;
   wchar_t *values = (wchar_t*)(env + environ_size);


   wchar_t **env_cur = environ;
   wchar_t *val_cur = values;
   
   cur = var_names;
   var = var_names;
   while(*cur) {
      while(*cur++);
      CONST CHAR16 *value = g_edk2_globals.shell->GetEnv(var);
      if(value) {
         int written = swprintf(
            val_cur,
            values + environ_values_size / sizeof(wchar_t) - val_cur,
            L"%ls=%ls",
            var,
            value
         );
         *env_cur++ = val_cur;         
         val_cur += written+1;
      }
      var = cur;
   }
}

void edk2_free_environ()
{
   if(environ) {
      free(environ);
      environ = NULL;
   }
}

int unsetenv(const char *name)
{
  int err = EINVAL;
  CHAR16 * uname;

  if(name == NULL || !*name)
    return err;

  size_t uname_size = (strlen(name)+1) * sizeof(CHAR16);
  uname = (CHAR16*)malloc(uname_size);
  if(uname == NULL)
    return ENOMEM;
  
  AsciiStrToUnicodeStrS( name, uname, uname_size / sizeof(CHAR16));

  ShellSetEnvironmentVariable ( uname, L"", FALSE );  
  ShellSetEnvironmentVariable ( uname, L"", TRUE );

  free(uname);
  
  return 0;
}
