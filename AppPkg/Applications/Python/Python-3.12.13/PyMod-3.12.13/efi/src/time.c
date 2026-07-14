#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include <efi/time.h>

int clock_getres(clockid_t clk_id, struct timespec *res)
{
   if(res) {
      res->tv_sec = 0;
      res->tv_nsec = 1000;
   }

   return 0;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
   struct timeval mtp = {0};
   
   gettimeofday (&mtp, NULL);

   if(tp) {
      tp->tv_sec = mtp.tv_sec;
      tp->tv_nsec = mtp.tv_usec * 1000;
   }

   return 0;   
}

int clock_settime(clockid_t clk_id, const struct timespec *tp)
{
   return EINVAL;
}


struct tm*
localtime_r(const time_t *timep, struct tm *result)
{
   struct tm *res = localtime(timep);

   if(!res)
      return NULL;
   
   if(result) {
      memcpy(result, res, sizeof(struct tm));
      return result;
   }

   return res;
}

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
   struct tm *res = gmtime(timep);

   if(!res)
      return NULL;
   
   if(result) {
      memcpy(result, res, sizeof(struct tm));
      return result;
   }

   return res;
}

