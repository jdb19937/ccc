#ifndef VULCANUS_SYS_MMAN_H
#define VULCANUS_SYS_MMAN_H

#include <sys/types.h>

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x0010
#define MAP_ANON      0x1000   /* Darwin-compatibilis */
#define MAP_ANONYMOUS MAP_ANON
#define MAP_FAILED    ((void*)-1)

#define MS_ASYNC  0x1
#define MS_SYNC   0x10
#define MS_INVALIDATE 0x2

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int   munmap(void *addr, size_t len);
int   mprotect(void *addr, size_t len, int prot);
int   msync(void *addr, size_t len, int flags);

#endif
