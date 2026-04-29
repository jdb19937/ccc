#ifndef MINERVA_FCNTL_H
#define MINERVA_FCNTL_H

#define O_RDONLY   0
#define O_WRONLY   1
#define O_RDWR     2
#define O_CREAT    0100
#define O_EXCL     0200
#define O_TRUNC    01000
#define O_APPEND   02000
#define O_NONBLOCK 04000

int open(const char *p, int flags, ...);
int fcntl(int fd, int cmd, ...);

#endif
