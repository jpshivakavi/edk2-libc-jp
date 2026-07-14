#pragma once

#define WNOHANG 1

#define UNAME_SIZE_MAX 20

struct utsname {
   char sysname[UNAME_SIZE_MAX];    /* Operating system name (e.g., "Linux") */
   char nodename[UNAME_SIZE_MAX];   /* Name within "some implementation-defined
                                       network" */
   char release[UNAME_SIZE_MAX];    /* Operating system release (e.g., "2.6.28") */
   char version[UNAME_SIZE_MAX];    /* Operating system version */
   char machine[UNAME_SIZE_MAX];    /* Hardware identifier */
};

int execve(const char *filename, char *const argv[],
           char *const envp[]);
int execv(const char *path, char *const argv[]);
int uname(struct utsname *buf);
