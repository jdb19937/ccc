#include "capitum.h"

time_t time(time_t *);
int clock_gettime(int, void *);

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};

struct tm *localtime(const time_t *);
struct tm *gmtime(const time_t *);
time_t mktime(struct tm *);
unsigned long strftime(char *, unsigned long, const char *, const struct tm *);

typedef long clock_t;
clock_t clock(void);
#define CLOCKS_PER_SEC 1000000
