/**

   @file
   Implement micro pipe() functionality

   Copyright (c) 2023, Intel Corporation
**/

#include "MainData.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/file.h>
#include <sys/poll.h>
#include <sys/EfiSysCall.h>
#include <Efi/SysEfi.h>

#define UPIPE_BUFFER_SIZE 1024

typedef struct _upipe {
  struct _upipe *next;
  uint64_t buffer_size;
  uint64_t size;
  uint8_t *buffer;  
  uint8_t *head;
  uint8_t *tail;  
} upipe_t;

typedef struct _upipe_pair {
  upipe_t left;
  upipe_t right;
  uint64_t refcount;
  uint8_t left_buffer[UPIPE_BUFFER_SIZE];
  uint8_t right_buffer[UPIPE_BUFFER_SIZE];
} upipe_pair_t;

typedef struct _upipe_ptr {
  upipe_pair_t *pipe;
  upipe_t *inst;
} upipe_ptr_t;

upipe_ptr_t *upipe_ref(upipe_pair_t *pipe, int is_right)
{
  upipe_ptr_t *ref;

  ref = (upipe_ptr_t*)malloc(sizeof(upipe_ptr_t));
  if(ref == NULL) 
    return NULL;

  ref->pipe = pipe;
  ref->inst = is_right ? &(pipe->right) : &(pipe->left);
  pipe->refcount++;

  return ref;
}

void upipe_delref(upipe_ptr_t *ref) {
  if(ref->pipe->refcount > 0)
    ref->pipe->refcount--;

  free(ref);
}

int EFIAPI upipe_close(struct __filedes *filp);
ssize_t EFIAPI upipe_read(struct __filedes *filp, off_t *Offset, size_t Len, void *Buf);
ssize_t EFIAPI upipe_write(struct __filedes *filp, off_t *Offset, size_t Len, const void *Buf);
short EFIAPI upipe_poll(struct __filedes *filp, short Events);

const struct fileops upipe_fileops = {
  upipe_close,     //  close
  upipe_read,      //  read
  upipe_write,     //  write

  fnullop_fcntl,      //  fcntl
  upipe_poll,      //  poll
  fnullop_flush,      //  flush

  fbadop_stat,        //  stat
  fbadop_ioctl,       //  ioctl
  fbadop_delete,      //  delete
  fbadop_rmdir,       //  rmdir
  fbadop_mkdir,       //  mkdir
  fbadop_rename,      //  rename
  fbadop_lseek        //  lseek
};


int
upipe_to_fd (upipe_pair_t *pipe, int is_right, int * pErrno)
{
  int fd;
  struct __filedes * descriptor;
  upipe_ptr_t *ref;

  ref = upipe_ref(pipe, is_right);
  if(ref == NULL) {
    if(pErrno)
      *pErrno = ENOMEM;
    return -1;
  }
  
  //
  //  Locate a file descriptor
  //
  fd = FindFreeFD ( VALID_CLOSED );
  if ( fd < 0 ) {
    //
    // All available FDs are in use
    //
    if(pErrno)
      *pErrno = EMFILE;
    return -1;
  }

  descriptor = &gMD->fdarray[fd];
  descriptor->f_offset = 0;
  descriptor->f_flag = 0;
  descriptor->f_iflags = DTYPE_PIPE;
  descriptor->MyFD = (UINT16)fd;
  descriptor->Oflags = O_RDWR;
  descriptor->Omode = S_ACC_READ | S_ACC_WRITE;
  descriptor->RefCount = 1;
  FILE_SET_MATURE(descriptor);

  descriptor->devdata = ref;
  descriptor->f_ops = &upipe_fileops;


  //
  //  Return the socket's file descriptor
  //
  return fd;
}

upipe_ptr_t *upipe_fd_ref(int fd, int *pErrno)
{
  struct __filedes * descriptor;
  upipe_ptr_t *pipe;
  
  if (!ValidateFD (fd, TRUE)) {
    if(pErrno)
      *pErrno = EBADF;
    return NULL;
  }

  descriptor = &gMD->fdarray[fd];

  if((descriptor->f_iflags & DTYPE_MASK) != DTYPE_PIPE) {
    if(pErrno)
      *pErrno = EBADF;
    return NULL;
  }
  pipe = (upipe_ptr_t*)descriptor->devdata;

  return pipe;
}

int EFIAPI upipe_close(struct __filedes *filp) {
  upipe_ptr_t *pipe_ptr;
  upipe_pair_t *pipe;
  
  if((filp->f_iflags & DTYPE_MASK) != DTYPE_PIPE) {
    errno = EBADF;
    return -1;
  }
  
  pipe_ptr = (upipe_ptr_t*)filp->devdata;
  filp->devdata = NULL;
  pipe = pipe_ptr->pipe;
  
  upipe_delref(pipe_ptr);
  if(pipe->refcount == 0)
    free(pipe);

  return 0;
}

ssize_t EFIAPI upipe_read(struct __filedes *filp, off_t *Offset, size_t Len, void *Buf)
{
  upipe_ptr_t *pipe_ptr;
  ssize_t to_copy_bytes, res = 0;  
  upipe_t *p;
  
  if((filp->f_iflags & DTYPE_MASK) != DTYPE_PIPE) {
    errno = EBADF;
    return -1;
  }
  
  pipe_ptr = (upipe_ptr_t*)filp->devdata;
  p = pipe_ptr->inst;

  while(Len > 0 && p->size > 0) {
    to_copy_bytes = 0;
    if(p->head < p->tail) {
      to_copy_bytes = (ssize_t)MIN(Len, (size_t)(p->tail - p->head));
    } else if(p->head >= p->tail) {
      to_copy_bytes = (ssize_t)MIN(Len, (size_t)(p->buffer + p->buffer_size - p->head));
    } 
    memcpy((uint8_t*)Buf, p->head, to_copy_bytes);
    p->size -= to_copy_bytes;
    p->head += to_copy_bytes;
    if(p->head >= p->buffer + p->buffer_size)
      p->head = p->buffer;
    Len -= to_copy_bytes;
    Buf = (uint8_t*)Buf + to_copy_bytes;
    res += to_copy_bytes;
  }

  return res;
}

ssize_t EFIAPI upipe_write(struct __filedes *filp, off_t *Offset, size_t Len, const void *Buf)
{
  upipe_ptr_t *pipe_ptr;
  ssize_t to_copy_bytes, res = 0;  
  upipe_t *p;
  
  if((filp->f_iflags & DTYPE_MASK) != DTYPE_PIPE) {
    errno = EBADF;
    return -1;
  }
  
  pipe_ptr = (upipe_ptr_t*)filp->devdata;
  p = pipe_ptr->inst->next;

  while(Len > 0 && p->size < p->buffer_size) {
    to_copy_bytes = 0;
    if(p->head <= p->tail) {
      to_copy_bytes = (ssize_t)MIN(Len, (size_t)(p->buffer + p->buffer_size - p->tail));
    } else if(p->head > p->tail) {
      to_copy_bytes = (ssize_t)MIN(Len, (size_t)(p->tail - p->head));
    } 
    memcpy(p->tail, (uint8_t*)Buf, to_copy_bytes);
    p->size += to_copy_bytes;
    p->tail += to_copy_bytes;
    if(p->tail >= p->buffer + p->buffer_size)
      p->tail = p->buffer;
    Len -= to_copy_bytes;
    Buf = (uint8_t*)Buf + to_copy_bytes;
    res += to_copy_bytes;
  }

  return res;
}

short EFIAPI upipe_poll(struct __filedes *filp, short Events)
{
  upipe_ptr_t *pipe_ptr;
  upipe_t *p;
  short res = 0;
  
  if((filp->f_iflags & DTYPE_MASK) != DTYPE_PIPE) {
    errno = EBADF;
    return -1;
  }
  
  pipe_ptr = (upipe_ptr_t*)filp->devdata;
  p = pipe_ptr->inst;

  if((Events & (POLLIN|POLLRDNORM|POLLRDBAND)) != 0 && p->size > 0) {
    res |= POLLIN;
  }

  if((Events & (POLLOUT|POLLWRNORM|POLLWRBAND)) != 0 &&
     p->size < p->buffer_size)
  {
    res |= POLLOUT;
  }

  return res;
}

INT32 upipe(INT32 pipefd[2])
{
  upipe_pair_t *p;

  p = (upipe_pair_t*)malloc(sizeof(upipe_pair_t));
  p->left.next = &(p->right);
  p->right.next = &(p->left);
  p->left.size = 0;
  p->right.size = 0;
  p->left.buffer_size = sizeof(p->left_buffer);
  p->right.buffer_size = sizeof(p->right_buffer);
  p->left.buffer = p->left_buffer;
  p->right.buffer = p->right_buffer;
  p->left.head = p->left_buffer;
  p->left.tail = p->left_buffer;
  p->right.head = p->right_buffer;
  p->right.tail = p->right_buffer;
  p->refcount = 0;

  pipefd[0] = upipe_to_fd(p, 0, &errno);
  if(pipefd[0] < 0) {
    free(p);
    return -1;
  }
  
  pipefd[1] = upipe_to_fd(p, 1, &errno);
  if(pipefd[1] < 0) {
    close(pipefd[0]);
    return -1;
  }

  return 0;
}

