/**

   @file
   Implement  fdstat() functionality

   Copyright (c) 2023, Intel Corporation
**/

#include "MainData.h"

#include <string.h>
#include <stdlib.h>
    
void fdstat(uint32_t *fdopen, uint32_t *fdclosed)
{
  uint32_t opn = 0, cls = 0;
  
  for(int i = 0; i < OPEN_MAX; ++i) {
    if(gMD->fdarray[i].f_iflags == 0) 
      cls++;
    else
      opn++;
  }

  if(fdopen) *fdopen = opn;
  if(fdclosed) *fdclosed = cls;
}
