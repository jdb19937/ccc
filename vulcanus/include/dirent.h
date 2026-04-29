#ifndef VULCANUS_DIRENT_H
#define VULCANUS_DIRENT_H

#include <sys/types.h>

struct dirent {
    ino_t    d_ino;
    off_t    d_seekoff;
    unsigned short d_reclen;
    unsigned short d_namlen;
    unsigned char  d_type;
    char     d_name[1024];
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK    10
#define DT_SOCK   12

typedef struct _vdir DIR;

DIR            *opendir(const char *p);
struct dirent  *readdir(DIR *d);
int             closedir(DIR *d);

#endif
