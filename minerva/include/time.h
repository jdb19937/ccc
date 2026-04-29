#ifndef MINERVA_TIME_H
#define MINERVA_TIME_H

#include <stddef.h>

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000000L
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

typedef int clockid_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};

time_t time(time_t *t);
clock_t clock(void);
int clock_gettime(clockid_t id, struct timespec *ts);
struct tm *localtime(const time_t *t);
struct tm *gmtime(const time_t *t);
double difftime(time_t a, time_t b);

#endif
