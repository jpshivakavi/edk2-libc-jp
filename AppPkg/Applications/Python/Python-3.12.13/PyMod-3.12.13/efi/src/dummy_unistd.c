#include <errno.h>
#include <stdio.h>

#include <efi/unistd.h>

int execve(const char *filename, char *const argv[],
           char *const envp[])
{
   errno = ENOEXEC;
   return -1;
}   

int execv(const char *path, char *const argv[])
{
   errno = ENOEXEC;
   return -1;
}   

int
uname(struct utsname *buf)
{
   if(buf == NULL) {
      errno = EFAULT;
      return -1;
   }
   
   snprintf(buf->sysname, sizeof(buf->sysname), "uefi");
   snprintf(buf->nodename, sizeof(buf->nodename), "unknown");
   snprintf(buf->release, sizeof(buf->release), "unknown");
   snprintf(buf->version, sizeof(buf->version), "unknown");
   snprintf(buf->machine, sizeof(buf->machine), "X86_64");
   
   return 0;
}


