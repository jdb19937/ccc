#ifndef VULCANUS_UNISTD_H
#define VULCANUS_UNISTD_H

#include <stddef.h>

typedef long ssize_t;
typedef long off_t;
typedef int  pid_t;
typedef unsigned uid_t;
typedef unsigned gid_t;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int     close(int fd);
off_t   lseek(int fd, off_t off, int w);
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
uid_t   geteuid(void);
gid_t   getgid(void);
gid_t   getegid(void);
void    _exit(int c);
int     isatty(int fd);
unsigned sleep(unsigned s);
int     usleep(unsigned us);
int     unlink(const char *p);
int     rmdir(const char *p);
char   *getcwd(char *b, size_t n);
pid_t   fork(void);
int     pipe(int fildes[2]);
int     dup(int fd);
int     dup2(int o, int n);
int     execve(const char *p, char *const a[], char *const e[]);
int     execvp(const char *p, char *const a[]);
int     execv(const char *p, char *const a[]);
int     ftruncate(int fd, off_t len);
unsigned alarm(unsigned s);
int     pause(void);

extern char **environ;

#endif
