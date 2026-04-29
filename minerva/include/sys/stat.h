#ifndef MINERVA_SYS_STAT_H
#define MINERVA_SYS_STAT_H

#include <sys/types.h>
#include <time.h>

struct stat {
    dev_t    st_dev;
    ino_t    st_ino;
    nlink_t  st_nlink;
    mode_t   st_mode;
    uid_t    st_uid;
    gid_t    st_gid;
    dev_t    st_rdev;
    off_t    st_size;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;
    struct timespec st_atim, st_mtim, st_ctim;
};

#define S_IFMT  0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFLNK 0120000
#define S_ISREG(m) (((m)&S_IFMT)==S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#define S_ISLNK(m) (((m)&S_IFMT)==S_IFLNK)

int stat(const char *p, struct stat *s);
int fstat(int fd, struct stat *s);
int lstat(const char *p, struct stat *s);
int mkdir(const char *p, mode_t m);

#endif
