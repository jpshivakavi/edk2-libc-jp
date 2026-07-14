#pragma once

#define MS_SYNC 4 /* Synchronous memory sync.  */

#define MAP_SHARED  0x01		/* Share changes.  */
#define MAP_PRIVATE 0x02		/* Changes are private.  */

#define PROT_READ   0x1		/* Page can be read.  */
#define PROT_WRITE  0x2		/* Page can be written.  */


int getpagesize(void);    
int munmap (void *__addr, size_t __len);
int msync (void *__addr, size_t __len, int __flags);
void *mmap (void *__addr, size_t __len, int __prot,
            int __flags, int __fd, __off_t __offset);
