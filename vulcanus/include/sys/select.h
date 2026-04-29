#ifndef VULCANUS_SYS_SELECT_H
#define VULCANUS_SYS_SELECT_H

#include <sys/types.h>
#include <time.h>

#define FD_SETSIZE 1024

typedef struct {
    unsigned int fds_bits[FD_SETSIZE / 32];
} fd_set;

#define FD_ZERO(s)   do { for (int _i = 0; _i < FD_SETSIZE/32; _i++) (s)->fds_bits[_i] = 0; } while (0)
#define FD_SET(n,s)   ((s)->fds_bits[(n)/32] |= (1u << ((n)%32)))
#define FD_CLR(n,s)   ((s)->fds_bits[(n)/32] &= ~(1u << ((n)%32)))
#define FD_ISSET(n,s) (((s)->fds_bits[(n)/32] & (1u << ((n)%32))) != 0)

struct timeval {
    long tv_sec;
    long tv_usec;
};

int select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *to);

#endif
