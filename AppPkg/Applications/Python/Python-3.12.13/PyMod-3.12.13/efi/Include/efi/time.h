#pragma once

#include <LibC/Time/tzfile.h>
#include <LibC/Time/TimeVals.h>

#include <sys/time.h>

int clock_getres(clockid_t clk_id, struct timespec *res);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_settime(clockid_t clk_id, const struct timespec *tp);

struct tm *localtime_r(const time_t *timep, struct tm *result);
struct tm *gmtime_r(const time_t *timep, struct tm *result);

extern void tzset();
