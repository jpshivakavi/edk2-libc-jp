#pragma once

#define RTLD_NOW 0

typedef struct {
   const char *dli_fname;  /* Pathname of shared object that
                              contains address */
   void       *dli_fbase;  /* Base address at which shared
                              object is loaded */
   const char *dli_sname;  /* Name of symbol whose definition
                              overlaps addr */
   void       *dli_saddr;  /* Exact address of symbol named
                              in dli_sname */
} Dl_info;

void* dlsym(void *handle, const char *symbol);
char *dlerror(void);
void* dlopen(const char *filename, int flags);
int dlclose(void *handle);
int dladdr(void *addr, Dl_info *info);
