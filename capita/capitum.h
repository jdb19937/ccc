/*
 * capita_interna.h — capita systematis interna pro CCC
 *
 * Declarationes typorum, functionum, macrarum systematis.
 * CCC hanc plicam legit et quasi caput systematis praebet
 * cum programma #include <...> invenit.
 */

#ifndef _CCC_INTERNA_H
#define _CCC_INTERNA_H

typedef unsigned long __ccc_size_t;
typedef long __ccc_ssize_t;
typedef unsigned long size_t;
typedef long ssize_t;
#define NULL ((void *)0)
typedef int _Bool;
typedef int pid_t;
typedef int mode_t;
typedef int off_t;
typedef long time_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long int64_t;
typedef unsigned int useconds_t;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* stddef — offsetof */
#define offsetof(type, member) ((size_t)&((type *)0)->member)

/* stdarg — va_copy */
#define va_copy(dest, src) ((dest) = (src))

/* setjmp/longjmp */
typedef int jmp_buf[48];
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);

/* inttypes — formae impressionis */
#define PRId64 "ld"
#define PRIi64 "ld"
#define PRIu64 "lu"
#define PRIx64 "lx"
#define PRIX64 "lX"
#define PRId32 "d"
#define PRIu32 "u"
#define PRIx32 "x"
#define INT64_C(c) c##L
#define UINT64_C(c) c##UL
#define INT32_C(c) c
#define UINT32_C(c) c##U

/* stdio */
typedef struct __sFILE FILE;
FILE *fopen(const char *, const char *);
FILE *fdopen(int fildes, const char *mode);
FILE *popen(const char *command, const char *mode);
int fclose(FILE *);
int pclose(FILE *stream);
unsigned long fwrite(const void *, unsigned long, unsigned long, FILE *);
int snprintf(char *, unsigned long, const char *, ...);
int sprintf(char *, const char *, ...);
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
void perror(const char *);
long getline(char **, unsigned long *, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);

/* stdlib */
void *malloc(unsigned long);
void *realloc(void *, unsigned long);
void *calloc(unsigned long, unsigned long);
void free(void *);
void exit(int);
long strtol(const char *, char **, int);
char *strtok(char *str, const char *sep);
char *strtok_r(char *str, const char *sep, char **lasts);

char *strdup(const char *);
int atoi(const char *);
void abort(void);
int atexit(void (*)(void));

/* string */
void *memcpy(void *, const void *, unsigned long);
void *memmove(void *, const void *, unsigned long);
void *memset(void *, int, unsigned long);
void *memchr(const void *s, int c, size_t n);
int memcmp(const void *, const void *, unsigned long);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, unsigned long);
unsigned long strlen(const char *);
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strstr(const char *, const char *);
char *strncpy(char *, const char *, unsigned long);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);
double strtod(const char *nptr, char **endptr);

/* ctype */
int isspace(int);
int isdigit(int);
int isalpha(int);
int isalnum(int);
int toupper(int);
int tolower(int);
int isxdigit(int c);
int ishexnumber(int c);

/* unistd */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
long read(int, void *, unsigned long);
long write(int, const void *, unsigned long);
int close(int);
unsigned int sleep(unsigned int);
void _exit(int);
char *strerror(int);

/* errno */
int *__error(void);
#define errno (*__error())
#define EAGAIN 35
#define EWOULDBLOCK EAGAIN
#define EINTR 4

/* termios */
#define NCCS 24
#define BRKINT  2
#define ICRNL   256
#define INPCK   16
#define ISTRIP  32
#define IXON    512
#define OPOST   1
#define CS8     768
#define ECHO    8
#define ICANON  256
#define IEXTEN  1024
#define ISIG    128
#define VMIN    16
#define VTIME   17
#define TCSAFLUSH 2
#define TCSANOW 0
struct termios {
    unsigned long c_iflag;
    unsigned long c_oflag;
    unsigned long c_cflag;
    unsigned long c_lflag;
    unsigned char c_cc[24];
    unsigned long c_ispeed;
    unsigned long c_ospeed;
};
int tcgetattr(int, struct termios *);
int tcsetattr(int, int, const struct termios *);

/* sys/ioctl */
#define TIOCGWINSZ 0x40087468
#define TIOCSWINSZ 0x80087467
struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};
int ioctl(int, unsigned long, ...);

/* stdarg */
typedef char *va_list;
int vfprintf(void *, const char *, va_list);
int vsnprintf(char *, unsigned long, const char *, va_list);
int vsprintf(char *, const char *, va_list);

/* signal */
typedef int sig_atomic_t;
typedef void (*__sighandler_t)(int);
struct sigaction {
    __sighandler_t sa_handler;
    int sa_mask;
    int sa_flags;
};
#define SIG_DFL ((__sighandler_t)0)
#define SIG_IGN ((__sighandler_t)1)
#define SIGINT 2
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 20
#define SIGWINCH 28
#define SIGUSR1 30
#define SIGUSR2 31
#define SA_RESTART 0x0002
#define SA_NOCLDSTOP 0x0008
#define sigemptyset(s) (*(s) = 0)
int sigaction(int, const struct sigaction *, struct sigaction *);
__sighandler_t signal(int, __sighandler_t);
int kill(int, int);
unsigned int alarm(unsigned int);
int pause(void);

/* sys/wait */
#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WTERMSIG(s) ((s) & 0x7f)
int waitpid(int, int *, int);
int fork(void);
int execvp(const char *, char *const *);
int pipe(int *);
int dup2(int, int);
int setpgid(int, int);
int tcsetpgrp(int, int);
int getpid(void);
int getppid(void);
unsigned int getuid(void);
unsigned int getgid(void);
int getpgrp(void);
long lseek(int, long, int);
int ftruncate(int, long);
int unlink(const char *);
int mkdir(const char *, unsigned short);
int rmdir(const char *);
int mkfifo(const char *, unsigned short);

/* fcntl */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_NONBLOCK 4
#define O_CREAT  512
#define O_TRUNC  1024
#define O_APPEND 8
#define O_NOCTTY 0x00020000
#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  7
#define F_SETLK  8
#define F_SETLKW 9
#define F_RDLCK  1
#define F_WRLCK  3
#define F_UNLCK  2
struct flock {
    long l_start;
    long l_len;
    int l_pid;
    short l_type;
    short l_whence;
};
int open(const char *, int, ...);
int fcntl(int, int, ...);

/* sys/stat */
struct stat {
    int st_dev;
    unsigned short st_mode;
    unsigned short st_nlink;
    unsigned long st_ino;
    unsigned int st_uid;
    unsigned int st_gid;
    int st_rdev;
    char __pad1[68];
    long st_size;
    char __pad2[40];
};
int stat(const char *, struct stat *);
int fstat(int, struct stat *);
int lstat(const char *, struct stat *);
int chmod(const char *, unsigned short);
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#define S_ISREG(m) (((m) & 0170000) == 0100000)

/* dirent */
struct dirent {
    unsigned long d_ino;
    unsigned long d_seekoff;
    unsigned short d_reclen;
    unsigned short d_namlen;
    unsigned char d_type;
    char d_name[1024];
};
typedef struct __DIR DIR;
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);

/* pwd */
struct passwd {
    char *pw_name;
    char *pw_dir;
};
struct passwd *getpwuid(unsigned int);

/* misc */
int chdir(const char *);
char *getcwd(char *, unsigned long);
char *getenv(const char *);
int setenv(const char *, const char *, int);
int unsetenv(const char *);
int access(const char *, int);
#define X_OK 1
#define R_OK 4
int isatty(int);
int putchar(int);
int fputc(int c, FILE *stream);
int fputs(const char *, void *);
int puts(const char *s);
int fflush(void *);
int fileno(void *);
extern void *__stdinp;
extern void *__stdoutp;
extern void *__stderrp;
#define stdin __stdinp
#define stdout __stdoutp
#define stderr __stderrp
int fgetc(void *);
int feof(void *);
char *fgets(char *, int, void *);
unsigned long fread(void *, unsigned long, unsigned long, void *);

/* sys/mman */
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED    1
#define MAP_PRIVATE   2
#define MAP_ANONYMOUS 4096
#define MAP_FAILED ((void *)-1)
#define MS_SYNC 16
void *mmap(void *, unsigned long, int, int, int, long);
int munmap(void *, unsigned long);
int msync(void *, unsigned long, int);

/* sys/socket */
#define AF_UNIX 1
#define SOCK_STREAM 1
int socketpair(int, int, int, int *);
long send(int, const void *, unsigned long, int);
long recv(int, void *, unsigned long, int);

/* sys/utsname */
struct utsname {
    char sysname[256];
    char nodename[256];
    char release[256];
    char version[256];
    char machine[256];
};
int uname(struct utsname *);

/* pthread */
typedef unsigned long pthread_t;
typedef struct {
    long __sig;
    char __opaque[56];
} pthread_mutex_t;
typedef struct {
    long __sig;
    char __opaque[56];
} pthread_attr_t;
typedef struct {
    long __sig;
    char __opaque[56];
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER {0x32aaaba7}
int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int pthread_join(pthread_t, void **);
int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_mutex_destroy(pthread_mutex_t *);

/* sys/select */
struct timeval {
    long tv_sec;
    long tv_usec;
};
typedef struct {
    int fds_bits[32];
} fd_set;
#define FD_ZERO(s) memset((s), 0, sizeof(fd_set))
#define FD_SET(fd, s) ((s)->fds_bits[(fd)/32] |= (1 << ((fd) % 32)))
#define FD_CLR(fd, s) ((s)->fds_bits[(fd)/32] &= ~(1 << ((fd) % 32)))
#define FD_ISSET(fd, s) ((s)->fds_bits[(fd)/32] & (1 << ((fd) % 32)))
int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

int system(const char *command);
int remove(const char *path);
pid_t wait(int *stat_loc);
int mkstemp(char *template);
useconds_t ualarm(useconds_t useconds, useconds_t interval);
pid_t setsid(void);

int posix_openpt(int oflag);
int grantpt(int fildes);
int unlockpt(int fildes);
char *ptsname(int fildes);

int execlp(const char *file, const char *arg0, ...);

struct pollfd {                                                                                         
    int   fd;       /* descriptio plicae */
    short events;   /* eventus quaesiti */                                                              
    short revents;  /* eventus qui acciderunt */                                                        
};                  
typedef unsigned int nfds_t;
#define POLLIN  0x0001                                                                                  
#define POLLOUT 0x0004                                                                                  
#define POLLERR 0x0008   
int poll(struct pollfd fds[], nfds_t nfds, int timeout);

#endif
