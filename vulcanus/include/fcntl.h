#ifndef VULCANUS_FCNTL_H
#define VULCANUS_FCNTL_H

#include <sys/types.h>

/* Flag valores Darwin-consentanei. */
#define O_RDONLY   0x0000
#define O_WRONLY   0x0001
#define O_RDWR     0x0002
#define O_NONBLOCK 0x0004
#define O_APPEND   0x0008
#define O_CREAT    0x0200
#define O_TRUNC    0x0400
#define O_EXCL     0x0800

#define F_DUPFD   0
#define F_GETFD   1
#define F_SETFD   2
#define F_GETFL   3
#define F_SETFL   4
#define F_GETLK   7
#define F_SETLK   8
#define F_SETLKW  9

#define F_RDLCK   1
#define F_UNLCK   2
#define F_WRLCK   3

#define FD_CLOEXEC 1

struct flock {
    off_t  l_start;
    off_t  l_len;
    pid_t  l_pid;
    short  l_type;
    short  l_whence;
};

int open(const char *p, int flags, ...);
int fcntl(int fd, int cmd, ...);

#endif
