#include <stdlib.h>
#include <stdint.h>

#include <sys/ansi.h>
#include <sys/stat.h>

#include <unistd.h>

#include <efi/mmap.h>

int getpagesize(void)
{
  return 4096;
}

int munmap (void *__addr, size_t __len)
{
  if(__addr != NULL)
    free(__addr);
  return 0;
}
    
int msync (void *__addr, size_t __len, int __flags)
{
  return -1;  
}

void *mmap (void *__addr, size_t __len, int __prot,
            int __flags, int __fd, __off_t __offset)
{
  uint8_t *res = NULL;

  if(__len == 0 && __fd < 0) 
    return NULL;


  if(__len == 0 && __fd >= 0) {
    struct stat st = {0};
    int rc = fstat(__fd, &st);
    if(rc < 0)
      return NULL;

    __len = st.st_size;
    if(__len <= 0)
      return NULL;
  }
  
  res = (uint8_t*)malloc(__len);
  
  if(__fd >= 0) {
    size_t bytes_read = read(__fd, res, __len);
    if(bytes_read < 0 || bytes_read != __len) {
      free(res);
      res = NULL;
    }
  }
  
  return res;
}
