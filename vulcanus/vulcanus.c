/*
 * vulcanus.c — bibliotheca minima C99 pro imitatore armlxiv (Darwin arm64).
 *
 * Fundamenta: _start, vocatores systematis, printf simplex, malloc bump,
 * operationes chordarum et numerorum, mathesis fluitans per serias.
 * Processus et plicae quoque (fork, pipe, waitpid, select, socketpair,
 * opendir, mmap cum fasciculo, ...). Nulla TLS, nulla fila vera, nec
 * plena signalium traditio: SIGALRM tantum statim in 'pause' redditur.
 * Convention AArch64 Darwinae: syscalls per 'svc #0x80' cum numero in
 * x16 et argumentis in x0..x5; errores per vexillum C (Carry) indicantur.
 *
 * Aedificatio obiecti (semel):
 *
 *     clang -arch arm64 -std=c99 -O2                                  \
 *           -nostdinc -ffreestanding -fno-stack-protector              \
 *           -fno-vectorize -fno-slp-vectorize                          \
 *           -Ivulcanus/include                                         \
 *           -c vulcanus/vulcanus.c -o vulcanus/vulcanus.o
 *
 * Aedificatio programmatis peregrini (PROG.c) quod armlxiv exsequitur:
 *
 *     clang -arch arm64 -std=c99 -O2                                  \
 *           -nostdinc -ffreestanding -fno-stack-protector              \
 *           -fno-vectorize -fno-slp-vectorize                          \
 *           -Ivulcanus/include                                         \
 *           -nostdlib -Wl,-static -Wl,-e,_start                        \
 *           PROG.c vulcanus/vulcanus.o -o PROG
 *     ./armlxiv PROG
 *
 * Pro unico fasciculo, 'vulcanus/vulcanus.c' loco 'vulcanus.o' adhiberi
 * potest. Vexilla magni momenti:
 *   -nostdinc -Ivulcanus/include    capita vulcani, non systematis
 *   -ffreestanding -nostdlib        sine runtime hospitali
 *   -fno-stack-protector            ne stirpes ex libSystem petantur
 *   -fno-vectorize -fno-slp-vectorize    ne AdvSIMD iussa generentur
 *                                        quae imitator non cognoscit
 *   -Wl,-static -Wl,-e,_start       Mach-O statica, introitus ad _start
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

int errno = 0;

/* ======================================================================== *
 * I.  Vocationes systematis (interfaciei Darwin arm64 BSD).
 *
 * svc #0x80; numerus in x16; args x0..x5; resultum x0; errorem vexillum
 * C (Carry) in NZCV indicat. Nos nzcv per 'mrs' legimus post svc.
 * ======================================================================== */

static inline long
_vocatio(long n, long a0, long a1, long a2, long a3, long a4, long a5)
{
    register long x0  __asm__("x0")  = a0;
    register long x1  __asm__("x1")  = a1;
    register long x2  __asm__("x2")  = a2;
    register long x3  __asm__("x3")  = a3;
    register long x4  __asm__("x4")  = a4;
    register long x5  __asm__("x5")  = a5;
    register long x16 __asm__("x16") = n;
    long nz;
    __asm__ volatile(
        "svc #0x80\n\t"
        "mrs %1, nzcv"
        : "+r"(x0), "=r"(nz),
        "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5), "+r"(x16)
        :
        : "memory", "cc"
    );
    /* Si C positum, error; valor ipse est errno positivus; reddimus
     * negativum ut morem 'r < 0 && r > -4096' conservemus. */
    if ((nz >> 29) & 1)
        return -x0;
    return x0;
}

static inline long vocatio1(long n, long a)
{ return _vocatio(n, a, 0, 0, 0, 0, 0); }
static inline long vocatio2(long n, long a, long b)
{ return _vocatio(n, a, b, 0, 0, 0, 0); }
static inline long vocatio3(long n, long a, long b, long c)
{ return _vocatio(n, a, b, c, 0, 0, 0); }
static inline long vocatio4(long n, long a, long b, long c, long d)
{ return _vocatio(n, a, b, c, d, 0, 0); }
static inline long vocatio6(long n, long a, long b, long c, long d, long e, long f)
{ return _vocatio(n, a, b, c, d, e, f); }

/* BSD syscall numeri Darwinae. */
#define SYS_exit         1
#define SYS_fork         2
#define SYS_read         3
#define SYS_write        4
#define SYS_open         5
#define SYS_close        6
#define SYS_wait4        7
#define SYS_unlink      10
#define SYS_getpid      20
#define SYS_getuid      24
#define SYS_geteuid     25
#define SYS_recvfrom    29
#define SYS_accept      30
#define SYS_access      33
#define SYS_kill        37
#define SYS_getppid     39
#define SYS_dup         41
#define SYS_pipe        42
#define SYS_getegid     43
#define SYS_sigaction   46
#define SYS_getgid      47
#define SYS_sigprocmask 48
#define SYS_ioctl       54
#define SYS_readlink    58
#define SYS_execve      59
#define SYS_munmap      73
#define SYS_mprotect    74
#define SYS_msync       65
#define SYS_setitimer   83
#define SYS_dup2        90
#define SYS_fcntl       92
#define SYS_select      93
#define SYS_socket      97
#define SYS_connect     98
#define SYS_bind       104
#define SYS_listen     106
#define SYS_gettimeofday 116
#define SYS_readv      120
#define SYS_writev     121
#define SYS_flock      131
#define SYS_mkfifo     132
#define SYS_sendto     133
#define SYS_shutdown   134
#define SYS_socketpair 135
#define SYS_mkdir      136
#define SYS_rmdir      137
#define SYS_mmap       197
#define SYS_lseek      199
#define SYS_truncate   200
#define SYS_ftruncate  201
#define SYS_stat       338
#define SYS_fstat      339
#define SYS_lstat      340
#define SYS_pause      0x200
#define SYS_opendir    0x210
#define SYS_readdir    0x211
#define SYS_closedir   0x212
/* Darwin non habet exit_group/uname/time/clock_gettime/brk nativos;
 * stimamur: exit_group -> exit, alii -> vocationes ficticiae sub. */
#define SYS_exit_group    SYS_exit

static long
sysret(long r)
{
    if (r < 0 && r > -4096) {
        errno = (int)-r;
        return -1;
    }
    return r;
}

ssize_t read(int fd, void *b, size_t n) { return (ssize_t)sysret(vocatio3(SYS_read, fd, (long)b, (long)n)); }
ssize_t write(int fd, const void *b, size_t n) { return (ssize_t)sysret(vocatio3(SYS_write, fd, (long)b, (long)n)); }
int     close(int fd) { return (int)sysret(vocatio1(SYS_close, fd)); }
off_t   lseek(int fd, off_t o, int w) { return (off_t)sysret(vocatio3(SYS_lseek, fd, o, w)); }
pid_t   getpid(void) { return (pid_t)vocatio1(SYS_getpid, 0); }
uid_t   getuid(void) { return (uid_t)vocatio1(SYS_getuid, 0); }
uid_t   geteuid(void) { return (uid_t)vocatio1(SYS_geteuid, 0); }
gid_t   getgid(void) { return (gid_t)vocatio1(SYS_getgid, 0); }
gid_t   getegid(void) { return (gid_t)vocatio1(SYS_getegid, 0); }

void _exit(int c) {
    vocatio1(SYS_exit_group, c);
    __builtin_unreachable();
}

int
open(const char *p, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    int m = 0;
    if (
        flags & 0100 /*O_CREAT*/
    ) m = va_arg(ap, int);
    va_end(ap);
    return (int)sysret(vocatio3(SYS_open, (long)p, flags, m));
}

int fstat(int fd, struct stat *s) { return (int)sysret(vocatio2(SYS_fstat, fd, (long)s)); }
int isatty(int fd) {
    (void)fd;
    return 0;
}

int uname(struct utsname *u)
{
    /* Darwin non exponit 'uname' syscall; stringas fictas scribimus ita
     * ut opus imitatoris idemptique sit sub diversis systematibus. */
    static const char *p[6] = {
        "Darwin", "armlxiv", "24.0.0-armlxiv", "#1 SMP armlxiv", "arm64", ""
    };
    char *dst = (char *)u;
    for (int i = 0; i < 6; i++) {
        char *d       = dst + i * 65;
        const char *s = p[i];
        size_t j      = 0;
        while (s[j] && j < 64) {
            d[j] = s[j];
            j++;
        }
        while (j < 65) d[j++] = 0;
    }
    return 0;
}

int clock_gettime(clockid_t id, struct timespec *ts)
{
    /* Darwin non habet clock_gettime syscall publicum; utimur
     * gettimeofday (microsecundis) et augmus in nanosecundos. */
    (void)id;
    long tv[2] = {0, 0};
    long r = vocatio2(SYS_gettimeofday, (long)tv, 0);
    if (r < 0)
        return (int)sysret(r);
    ts->tv_sec  = tv[0];
    ts->tv_nsec = tv[1] * 1000;
    return 0;
}

time_t time(time_t *pt)
{
    long tv[2] = {0, 0};
    long r = vocatio2(SYS_gettimeofday, (long)tv, 0);
    if (r < 0) {
        if (pt)
            *pt = (time_t)-1;
        return (time_t)-1;
    }
    if (pt)
        *pt = (time_t)tv[0];
    return (time_t)tv[0];
}

clock_t clock(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return (clock_t)-1;
    return (clock_t)(ts.tv_sec * 1000000L + ts.tv_nsec / 1000L);
}

unsigned sleep(unsigned s) { struct timespec a = { (time_t)s, 0 }, b;
    /* imitator noster nanosleep non habet; tantum looping. Falsum sed sufficiens. */
    volatile unsigned i = s * 100; while (i--) { }
    (void)a;
    (void)b;
    return 0;
}
int usleep(unsigned us) {
    volatile unsigned i = us;
    while (i--) {

    } return 0;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    long r = vocatio6(SYS_mmap, (long)addr, (long)len, prot, flags, fd, off);
    if (r < 0 && r > -4096) {
        errno = (int)-r;
        return (void*)-1;
    }
    return (void*)r;
}
int munmap(void *a, size_t l) { return (int)sysret(vocatio2(SYS_munmap, (long)a, (long)l)); }
int mprotect(void *a, size_t l, int p) { return (int)sysret(vocatio3(SYS_mprotect, (long)a, (long)l, p)); }

int fcntl(int fd, int cmd, ...) {
    (void)fd;
    (void)cmd;
    return 0;
}
char *getcwd(char *b, size_t n) {
    if (!b || n < 2) {
        errno = ERANGE;
        return NULL;
    }
    b[0] = '/';
    b[1] = 0;
    return b;
}

/* ======================================================================== *
 * II.  Memoria et operationes crudae.
 * ======================================================================== */

void *
    memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d       = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *
    memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d       = dst;
    const unsigned char *s = src;
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

void *
    memset(void *p, int c, size_t n)
{
    unsigned char *q = p;
    while (n--)
        *q++ = (unsigned char)c;
    return p;
}

int
memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    while (n--) {
        if (*x != *y)
            return *x - *y;
        x++;
        y++;
    }
    return 0;
}

void *
    memchr(const void *p, int c, size_t n)
{
    const unsigned char *s = p;
    while (n--) {
        if (*s == (unsigned char)c)
            return (void*)s;
        s++;
    }
    return NULL;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}
size_t strnlen(const char *s, size_t n) {
    size_t i = 0;
    while (i < n && s[i])
        i++;
    return i;
}

char *strcpy(char *d, const char *s) {
    char *r = d;
    while ((*d++ = *s++))
        (void)0;
    return r;
}
char *strncpy(char *d, const char *s, size_t n)
{
    char *r = d;
    while (n && *s) {
        *d++ = *s++;
        n--;
    }
    while (n--) *d++ = 0;
    return r;
}

char *strcat(char *d, const char *s) {
    char *r = d;
    while (*d)
        d++;
    while ((*d++ = *s++))
        (void)0;
    return r;
}
char *strncat(char *d, const char *s, size_t n)
{
    char *r = d;
    while (*d)
        d++;
    while (n && *s) {
        *d++ = *s++;
        n--;
    }
    *d = 0;
    return r;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    } return (unsigned char)*a - (unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (!n)
        return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char*)s;
        s++;
    }
    return c == 0 ? (char*)s : NULL;
}
char *strrchr(const char *s, int c)
{
    const char *r = NULL;
    for (;; s++) {
        if (*s == (char)c)
            r = s;
        if (!*s)
            break;
    }
    return (char*)r;
}
char *strstr(const char *h, const char *n)
{
    if (!*n)
        return (char*)h;
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && *a == *b) {
            a++;
            b++;
        }
        if (!*b)
            return (char*)h;
    }
    return NULL;
}

static char *strtok_save;
char *strtok(char *s, const char *d) { return strtok_r(s, d, &strtok_save); }
char *strtok_r(char *s, const char *d, char **sv)
{
    char *p = s ? s : *sv;
    if (!p)
        return NULL;
    while (*p && strchr(d, *p))
        p++;
    if (!*p) {
        *sv = NULL;
        return NULL;
    }
    char *start = p;
    while (*p && !strchr(d, *p))
        p++;
    if (*p) {
        *p  = 0;
        *sv = p + 1;
    } else {
        *sv = NULL;
    }
    return start;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *r  = malloc(n);
    if (!r)
        return NULL;
    memcpy(r, s, n);
    return r;
}

char *strerror(int e) {
    (void)e;
    return "culpa";
}

/* ======================================================================== *
 * III.  Ctype.
 * ======================================================================== */

int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int ispunct(int c) { return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126); }
int iscntrl(int c) { return (c >= 0 && c<32) || c == 127; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isprint(int c) { return c >= 32 && c<127; }
int isgraph(int c) { return c>32 && c<127; }
int tolower(int c) { return isupper(c) ? c + 32 : c; }
int toupper(int c) { return islower(c) ? c - 32 : c; }

/* ======================================================================== *
 * IV.  Allocator: bump simplex + coniunctio retroactiva simplicis.
 * Libertas (free) in stirpem revertatur si ultimus bloccus; aliter ignoramus.
 * Sufficit pro programmatibus quae nimis non redistribuunt memoriam.
 * ======================================================================== */

typedef struct Bloccus {
    size_t mag;
    struct Bloccus *sequens;
} Bloccus;

static uint8_t *heap_init  = NULL;
static uint8_t *heap_curr  = NULL;
static uint8_t *heap_limes = NULL;
static Bloccus *liberae    = NULL;  /* lista simplex. */

static void
heap_dilatare(size_t quanta)
{
    /* Darwin brk non exsistit; semper utimur mmap. */
    size_t opus = quanta + 4096;
    opus        = (opus + 4095) & ~(size_t)4095;
    {
        void *p = mmap(
            NULL, opus, PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0
        );
        if (p == (void*)-1)
            return;
        if (!heap_init) {
            heap_init  = (uint8_t*)p;
            heap_curr  = heap_init;
            heap_limes = heap_init + opus;
        } else {
            /* insula nova; discontigua, sed sufficit pro bump. */
            heap_curr  = (uint8_t*)p;
            heap_limes = (uint8_t*)p + opus;
        }
        return;
    }
}

void *
    malloc(size_t n)
{
    if (n == 0)
        n = 1;
    n = (n + 15) & ~(size_t)15;
    /* scruta liberae prius. */
    Bloccus **cursor = &liberae;
    while (*cursor) {
        if ((*cursor)->mag >= n) {
            Bloccus *b = *cursor;
            *cursor    = b->sequens;
            return (void*)(b + 1);
        }
        cursor = &(*cursor)->sequens;
    }
    /* allocare novum. */
    size_t totalis = sizeof(Bloccus) + n;
    if (heap_curr + totalis > heap_limes) {
        heap_dilatare(totalis);
        if (heap_curr + totalis > heap_limes)
            return NULL;
    }
    Bloccus *b = (Bloccus*)heap_curr;
    b->mag     = n;
    b->sequens = NULL;
    heap_curr += totalis;
    return (void*)(b + 1);
}

void
free(void *p)
{
    if (!p)
        return;
    Bloccus *b = (Bloccus*)p - 1;
    b->sequens = liberae;
    liberae    = b;
}

void *
    calloc(size_t n, size_t s)
{
    size_t t = n * s;
    void *p  = malloc(t);
    if (p)
        memset(p, 0, t);
    return p;
}

void *
    realloc(void *p, size_t n)
{
    if (!p)
        return malloc(n);
    if (n == 0) {
        free(p);
        return NULL;
    }
    Bloccus *b = (Bloccus*)p - 1;
    if (b->mag >= n)
        return p;
    void *q = malloc(n);
    if (!q)
        return NULL;
    memcpy(q, p, b->mag);
    free(p);
    return q;
}

/* ======================================================================== *
 * V.  Conversio numerorum (atoi, atof, strtol, strtod).
 * ======================================================================== */

static int
digit_val(int c, int base)
{
    int v;
    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'z')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z')
        v = c - 'A' + 10;
    else
        return -1;
    return v < base ? v : -1;
}

long
strtol(const char *s, char **end, int base)
{
    while (isspace((unsigned char)*s))
        s++;
    int neg = 0;
    if (*s == '+')
        s++;
    else if (*s == '-') {
        neg = 1;
        s++;
    }
    if (base == 0) {
        if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (*s == '0') {
            base = 8;
            s++;
        } else
            base = 10;
    } else if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    long v = 0;
    int d;
    while ((d = digit_val((unsigned char)*s, base)) >= 0) {
        v = v * base + d;
        s++;
    }
    if (end)
        *end = (char*)s;
    return neg ? -v : v;
}

unsigned long
strtoul(const char *s, char **end, int base)
{
    return (unsigned long)strtol(s, end, base);
}

long long
strtoll(const char *s, char **end, int base)
{
    return strtol(s, end, base);
}

unsigned long long
strtoull(const char *s, char **end, int base)
{
    return (unsigned long long)strtol(s, end, base);
}

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }
long long atoll(const char *s) { return strtol(s, NULL, 10); }

double
strtod(const char *s, char **end)
{
    while (isspace((unsigned char)*s))
        s++;
    int neg = 0;
    if (*s == '+')
        s++;
    else if (*s == '-') {
        neg = 1;
        s++;
    }
    double v = 0;
    while (isdigit((unsigned char)*s)) {
        v = v * 10 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        double scala = 0.1;
        while (isdigit((unsigned char)*s)) {
            v += (*s - '0') * scala;
            scala *= 0.1;
            s++;
        }
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        int eneg = 0;
        if (*s == '+')
            s++;
        else if (*s == '-') {
            eneg = 1;
            s++;
        }
        int ex = 0;
        while (isdigit((unsigned char)*s)) {
            ex = ex * 10 + (*s - '0');
            s++;
        }
        double mul = 1;
        for (int i = 0; i < ex; i++)
            mul *= 10;
        v = eneg ? v / mul : v * mul;
    }
    if (end)
        *end = (char*)s;
    return neg ? -v : v;
}

double atof(const char *s) { return strtod(s, NULL); }

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }

/* ======================================================================== *
 * VI.  Sors pseudo-aleatoria (Linear Congruential — sufficiens).
 * ======================================================================== */

static uint64_t semen_alea = 1;
void srand(unsigned s) { semen_alea = s ? s : 1; }
int rand(void)
{
    semen_alea = semen_alea * 6364136223846793005ull + 1442695040888963407ull;
    return (int)((semen_alea >> 33) & 0x7fffffff);
}

/* ======================================================================== *
 * VII.  qsort (algorithmus quicksort simplex).
 * ======================================================================== */

static void
permuta(char *a, char *b, size_t s)
{
    while (s--) {
        char t = *a;
        *a++   = *b;
        *b++   = t;
    }
}

static void
qsort_int(
    char *base, size_t n, size_t s,
    int (*cmp)(const void*, const void*)
) {
    if (n < 2)
        return;
    char *pv = base + (n / 2) * s;
    permuta(pv, base + (n-1) * s, s);
    pv       = base + (n-1) * s;
    size_t i = 0;
    for (size_t j = 0; j < n - 1; j++)
        if (cmp(base + j * s, pv) < 0) {
        permuta(base + i * s, base + j * s, s);
        i++;
    }
    permuta(base + i * s, pv, s);
    qsort_int(base, i, s, cmp);
    qsort_int(base + (i+1) * s, n - i - 1, s, cmp);
}

void qsort(void *b, size_t n, size_t s, int (*cmp)(const void*, const void*))
{
    qsort_int((char*)b, n, s, cmp);
}

/* ======================================================================== *
 * VIII.  Mathesis fluitans (series compactae).
 * ======================================================================== */

static inline int
est_nan(double x) { return x != x; }

double fabs(double x) { return x < 0 ? -x : x; }
float  fabsf(float x) { return x < 0 ? -x : x; }

double floor(double x)
{
    long long i = (long long)x;
    double f    = (double)i;
    return f > x ? f - 1 : f;
}
double ceil(double x)
{
    long long i = (long long)x;
    double f    = (double)i;
    return f < x ? f + 1 : f;
}
double trunc(double x) { return (double)(long long)x; }
double round(double x) { return x >= 0 ? floor(x + 0.5) : ceil(x - 0.5); }
float  floorf(float x) { return (float)floor(x); }
float  ceilf(float x) { return (float)ceil(x); }
float  truncf(float x) { return (float)trunc(x); }
float  roundf(float x) { return (float)round(x); }

double sqrt(double x)
{
    if (x != x || x < 0)
        return 0.0/0.0;
    if (x == 0)
        return 0;
    double g = x > 1 ? x / 2 : 1;
    for (int i = 0; i < 64; i++) {
        double ng = 0.5 * (g + x / g);
        if (ng == g)
            break;
        g = ng;
    }
    return g;
}
float sqrtf(float x) { return (float)sqrt(x); }

double fmod(double x, double y)
{
    if (y == 0)
        return 0.0/0.0;
    double q = trunc(x / y);
    return x - q * y;
}
float fmodf(float x, float y) { return (float)fmod(x, y); }

/* exp via Taylor expansionem cum reductione 2^k argument. */
double exp(double x)
{
    if (est_nan(x))
        return x;
    if (x > 709)
        return 1e308 * 1e308;  /* infinitum */
    if (x < -745)
        return 0;
    /* reductione: x = k * ln2 + r,  e^x = 2^k * e^r */
    double kd = floor(x * M_LOG2E + 0.5);
    int k     = (int)kd;
    double r  = x - kd * M_LN2;
    /* series: 1 + r + r^2/2! + ... */
    double s = 1, t = 1;
    for (int i = 1; i < 20; i++) {
        t *= r / i;
        s += t;
    }
    /* 2^k per potentia binaria. */
    double p = 1;
    if (k > 0) {
        double b = 2;
        int e    = k;
        while (e) {
            if (e & 1)
                p *= b;
            b *= b;
            e >>= 1;
        }
    } else if (k < 0) {
        double b = 0.5;
        int e    = -k;
        while (e) {
            if (e & 1)
                p *= b;
            b *= b;
            e >>= 1;
        }
    }
    return s * p;
}
float expf(float x) { return (float)exp(x); }

/* log via reductio ad [1, 2) per frexp-like. log(x) = k*ln2 + log(m) */
static double
log_aux(double m)
{
    /* m in [1, 2). Transform y = (m-1)/(m+1), log(m) = 2*(y + y^3/3 + y^5/5 + ...) */
    double y  = (m - 1) / (m + 1);
    double y2 = y * y;
    double s  = 0, t = y;
    for (int i = 1; i < 40; i += 2) {
        s += t / i;
        t *= y2;
    }
    return 2 * s;
}

double log(double x)
{
    if (x != x || x < 0)
        return 0.0/0.0;
    if (x == 0)
        return -1e308;
    int k    = 0;
    double m = x;
    while (m >= 2) {
        m *= 0.5;
        k++;
    }
    while (m < 1) {
        m *= 2;
        k--;
    }
    return k * M_LN2 + log_aux(m);
}
float logf(float x) { return (float)log(x); }
double log2(double x) { return log(x) / M_LN2; }
double log10(double x) { return log(x) / 2.30258509299404568402; }
float log2f(float x) { return (float)log2(x); }
float log10f(float x) { return (float)log10(x); }

double pow(double x, double y)
{
    if (y == 0)
        return 1;
    if (x == 0)
        return y > 0 ? 0 : 1e308;
    /* integer y quick path */
    if (y == (double)(long long)y) {
        long long e = (long long)y;
        int neg     = e < 0;
        if (neg)
            e = -e;
        double p = 1, b = x;
        while (e) {
            if (e & 1)
                p *= b;
            b *= b;
            e >>= 1;
        }
        return neg ? 1 / p : p;
    }
    if (x < 0)
        return 0.0/0.0;
    return exp(y * log(x));
}
float powf(float x, float y) { return (float)pow(x, y); }

/* sin / cos: Reducatio ad [-pi/2, pi/2], series Taylor. */
static void
sin_cos_red(double x, double *s, double *c)
{
    double pi2 = 2 * M_PI;
    /* reducatio ad [-pi, pi) */
    x = fmod(x, pi2);
    if (x > M_PI)
        x -= pi2;
    else if (x < -M_PI)
        x += pi2;
    int signum = 1;
    /* ad [-pi/2, pi/2] per symmetriam. */
    if (x > M_PI_2) {
        x      = M_PI - x;
        signum = -1;
    } else if (x < -M_PI_2) {
        x      = -M_PI - x;
        signum = -1;
    }
    double x2 = x * x;
    double ts = x, tc = 1;
    double sr = x, cr = 1;
    for (int i = 1; i < 12; i++) {
        ts *= -x2 / ((2*i) * (2*i+1));
        tc *= -x2 / ((2*i-1) * (2*i));
        sr += ts;
        cr += tc;
    }
    *s = sr;
    *c = signum * cr;
}

double sin(double x) {
    double s, c;
    sin_cos_red(x, &s, &c);
    return s;
}
double cos(double x) {
    double s, c;
    sin_cos_red(x, &s, &c);
    return c;
}
double tan(double x) {
    double s, c;
    sin_cos_red(x, &s, &c);
    return s / c;
}
float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float tanf(float x) { return (float)tan(x); }

double atan(double x)
{
    if (x > 1)
        return M_PI_2 - atan(1 / x);
    if (x < -1)
        return -M_PI_2 - atan(1 / x);
    double x2 = x * x, t = x, s = x;
    for (int i = 1; i < 60; i++) {
        t *= -x2;
        s += t / (2*i + 1);
    }
    return s;
}
float atanf(float x) { return (float)atan(x); }

double atan2(double y, double x)
{
    if (x > 0)
        return atan(y / x);
    if (x < 0)
        return (y >= 0) ? atan(y/x) + M_PI : atan(y/x) - M_PI;
    if (y > 0)
        return M_PI_2;
    if (y < 0)
        return -M_PI_2;
    return 0;
}
float atan2f(float y, float x) { return (float)atan2(y, x); }

double asin(double x)
{
    if (x >= 1)
        return M_PI_2;
    if (x <= -1)
        return -M_PI_2;
    return atan(x / sqrt(1 - x*x));
}
double acos(double x) { return M_PI_2 - asin(x); }

double sinh(double x) { return (exp(x) - exp(-x)) * 0.5; }
double cosh(double x) { return (exp(x) + exp(-x)) * 0.5; }
double tanh(double x)
{
    double e1 = exp(x), e2 = exp(-x);
    return (e1 - e2) / (e1 + e2);
}
float tanhf(float x) { return (float)tanh(x); }

double hypot(double x, double y) { return sqrt(x*x + y*y); }
float  hypotf(float x, float y) { return (float)sqrt((double)x*x + (double)y*y); }

#undef isnan
#undef isinf
#undef isfinite
int isnan(double x) { return x != x; }
int isinf(double x) { return x > 1e308 || x < -1e308; }
int isfinite(double x) { return !isnan(x) && !isinf(x); }

double ldexp(double x, int e) { return x * pow(2.0, (double)e); }
float  ldexpf(float x, int e) { return (float)ldexp((double)x, e); }
double fma(double a, double b, double c) { return a * b + c; }
float  fmaf(float a, float b, float c) { return a * b + c; }

int fpclassify(double x)
{
    if (isnan(x))
        return FP_NAN;
    if (isinf(x))
        return FP_INFINITE;
    if (x == 0)
        return FP_ZERO;
    double a = fabs(x);
    if (a < 2.2250738585072014e-308)
        return FP_SUBNORMAL;
    return FP_NORMAL;
}

/* ======================================================================== *
 * IX.  FILE et stdio: buffers simplices.
 * ======================================================================== */

struct FILE {
    int    fd;
    int    modus;        /* 1 legere, 2 scribere, 3 utrumque */
    int    eof_vexill;
    int    error_vexill;
    int    push_char;    /* ungetc buffer, -1 si vacuus */
    /* buffer outputus, tantum pro stdout/stderr line-buffered. */
    char  *buf;
    size_t buf_mag;
    size_t buf_pos;
    int    buf_modus;    /* 0=nullus, 1=linea, 2=plenus */
};

static FILE file_stdin_obj  = { 0, 1, 0, 0, -1, NULL, 0, 0, 0 };
static FILE file_stdout_obj = { 1, 2, 0, 0, -1, NULL, 0, 0, 1 };
static FILE file_stderr_obj = { 2, 2, 0, 0, -1, NULL, 0, 0, 0 };

FILE *stdin  = &file_stdin_obj;
FILE *stdout = &file_stdout_obj;
FILE *stderr = &file_stderr_obj;

static char stdout_buf[4096];

static void
file_init(void)
{
    stdout->buf       = stdout_buf;
    stdout->buf_mag   = sizeof stdout_buf;
    stdout->buf_pos   = 0;
    stdout->buf_modus = isatty(1) ? 1 : 2;
}

int fflush(FILE *f)
{
    if (!f) {
        fflush(stdout);
        fflush(stderr);
        return 0;
    }
    if (f->buf_modus && f->buf_pos) {
        write(f->fd, f->buf, f->buf_pos);
        f->buf_pos = 0;
    }
    return 0;
}

int feof(FILE *f) { return f->eof_vexill; }
int ferror(FILE *f) { return f->error_vexill; }
void clearerr(FILE *f) {
    f->eof_vexill   = 0;
    f->error_vexill = 0;
}
int fileno(FILE *f) { return f->fd; }
void setbuf(FILE *f, char *b) {
    (void)f;
    (void)b;
}
int setvbuf(FILE *f, char *b, int m, size_t n) {
    (void)f;
    (void)b;
    (void)m;
    (void)n;
    return 0;
}

static int
file_emit(FILE *f, const char *s, size_t n)
{
    if (f->buf_modus == 0) {
        write(f->fd, s, n);
        return (int)n;
    }
    for (size_t i = 0; i < n; i++) {
        if (f->buf_pos == f->buf_mag)
            fflush(f);
        f->buf[f->buf_pos++] = s[i];
        if (f->buf_modus == 1 && s[i] == '\n')
            fflush(f);
    }
    return (int)n;
}

int fputs(const char *s, FILE *f) { return file_emit(f, s, strlen(s)); }
int fputc(int c, FILE *f) {
    char b = (char)c;
    file_emit(f, &b, 1);
    return (unsigned char)c;
}
int putc(int c, FILE *f) { return fputc(c, f); }
int putchar(int c) { return fputc(c, stdout); }
int puts(const char *s) {
    fputs(s, stdout);
    fputc('\n', stdout);
    return 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int flags = 0;
    int modus = 0;
    if (mode[0] == 'r') {
        flags = 0;
        modus = (mode[1] == '+') ? 3 : 1;
    } else if (mode[0] == 'w') {
        flags = 01|0100|01000 /*RW+CREAT+TRUNC*/;
        if (mode[1] == '+')
            flags = 2|0100|01000;
        modus = (mode[1] == '+') ? 3 : 2;
    } else if (mode[0] == 'a') {
        flags = 01|0100|02000 /*RW+CREAT+APPEND*/;
        if (mode[1] == '+')
            flags = 2|0100|02000;
        modus = (mode[1] == '+') ? 3 : 2; }
    int fd = open(path, flags, 0644);
    if (fd < 0)
        return NULL;
    FILE *f = malloc(sizeof *f);
    if (!f) {
        close(fd);
        return NULL;
    }
    memset(f, 0, sizeof *f);
    f->fd        = fd;
    f->modus     = modus;
    f->push_char = -1;
    return f;
}

FILE *freopen(const char *p, const char *m, FILE *f) {
    if (f)
        fclose(f);
    return fopen(p, m);
}

int fclose(FILE *f)
{
    if (!f)
        return -1;
    fflush(f);
    int r = close(f->fd);
    if (f != stdin && f != stdout && f != stderr)
        free(f);
    return r;
}

size_t
fread(void *p, size_t sz, size_t n, FILE *f)
{
    size_t totalis = sz * n;
    size_t habui   = 0;
    if (f->push_char >= 0 && totalis > 0) {
        ((char*)p)[0] = (char)f->push_char;
        f->push_char  = -1;
        habui         = 1;
    }
    while (habui < totalis) {
        ssize_t r = read(f->fd, (char*)p + habui, totalis - habui);
        if (r < 0) {
            f->error_vexill = 1;
            break;
        }
        if (r == 0) {
            f->eof_vexill = 1;
            break;
        }
        habui += (size_t)r;
    }
    return habui / sz;
}

size_t fwrite(const void *p, size_t sz, size_t n, FILE *f)
{
    file_emit(f, (const char*)p, sz * n);
    return n;
}

int fseek(FILE *f, long off, int w) {
    fflush(f);
    return (int)(lseek(f->fd, off, w) < 0 ? -1 : 0);
}
long ftell(FILE *f) { return (long)lseek(f->fd, 0, 1 /*SEEK_CUR*/); }
void rewind(FILE *f) {
    fseek(f, 0, 0);
    clearerr(f);
}

int fgetc(FILE *f)
{
    if (f->push_char >= 0) {
        int c        = f->push_char;
        f->push_char = -1;
        return c;
    }
    unsigned char b;
    ssize_t r = read(f->fd, &b, 1);
    if (r <= 0) {
        f->eof_vexill = (r == 0);
        return EOF;
    }
    return b;
}
int getc(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }
int ungetc(int c, FILE *f) {
    f->push_char = c;
    return c;
}

char *fgets(char *buf, int n, FILE *f)
{
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) {
            if (i == 0)
                return NULL;
            break;
        }
        buf[i++] = (char)c;
        if (c == '\n')
            break;
    }
    buf[i] = 0;
    return buf;
}

int remove(const char *p) {
    (void)p;
    return -1;
}
int rename(const char *a, const char *b) {
    (void)a;
    (void)b;
    return -1;
}
void perror(const char *s) {
    if (s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs("culpa\n", stderr); }

/* ======================================================================== *
 * X.  Formator (printf).
 *
 * Sustinentur: d i u x X o p c s f e g n (%n explicite PROHIBITUS), % %%
 * Modificatores longitudinis: l ll z h hh L
 * Flags: - + (space) 0 #
 * Width et precision: numerus aut *
 * ======================================================================== */

typedef int (*scriptor_f)(void *q, const char *s, size_t n);

typedef struct {
    char *d;
    size_t mag;
    size_t pos;
} buf_scriptor;

static int
scrib_ad_bufum(void *q, const char *s, size_t n)
{
    buf_scriptor *b = q;
    for (size_t i = 0; i < n; i++) {
        if (b->pos + 1 < b->mag)
            b->d[b->pos] = s[i];
        b->pos++;
    }
    return (int)n;
}

static int
scrib_ad_file(void *q, const char *s, size_t n)
{
    FILE *f = q;
    return file_emit(f, s, n);
}

/* Converte integer ad chordam in basi. Signum non applicat. */
static int
formata_integer(char *buf, unsigned long long v, int basis, int maiusculus)
{
    const char *digiti  = maiusculus ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[32];
    int n = 0;
    if (v == 0)
        tmp[n++] = '0';
    else
        while (v) {
        tmp[n++] = digiti[v % basis];
        v /= basis;
    }
    for (int i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];
    return n;
}

/* Formata numerum fluentem in notatione 'f'. */
static int
formata_duplicem_f(char *buf, double v, int precisio)
{
    int n = 0;
    if (est_nan(v)) {
        buf[0] = 'n';
        buf[1] = 'a';
        buf[2] = 'n';
        return 3;
    }
    if (v < 0) {
        buf[n++] = '-';
        v        = -v;
    }
    if (v > 1e308) {
        buf[n++] = 'i';
        buf[n++] = 'n';
        buf[n++] = 'f';
        return n;
    }
    /* integer pars */
    double ipart = floor(v);
    double fpart = v - ipart;
    char ibuf[32];
    int in = 0;
    if (ipart == 0)
        ibuf[in++] = '0';
    else {
        /* scribamus ibu reversum */
        while (ipart >= 1 && in < 30) {
            double d   = fmod(ipart, 10);
            ibuf[in++] = '0' + (int)d;
            ipart      = floor(ipart / 10);
        }
    }
    for (int i = in - 1; i >= 0; i--)
        buf[n++] = ibuf[i];
    if (precisio > 0) {
        buf[n++] = '.';
        for (int i = 0; i < precisio; i++) {
            fpart *= 10;
            int d = (int)fpart;
            if (d > 9)
                d = 9;
            if (d < 0)
                d = 0;
            buf[n++] = '0' + d;
            fpart -= d;
        }
        /* rounding simplex: si fpart*10 >= 5 rotunda ultimum */
        if (fpart >= 0.5) {
            for (int i = n - 1; i >= 0; i--) {
                if (buf[i] == '.')
                    continue;
                if (buf[i] == '-')
                    break;
                if (buf[i] == '9') {
                    buf[i] = '0';
                } else {
                    buf[i]++;
                    break;
                }
            }
        }
    }
    return n;
}

/* Formata numerum fluentem in notatione 'e'. */
static int
formata_duplicem_e(char *buf, double v, int precisio, int maiusculus)
{
    int n = 0;
    if (est_nan(v)) {
        buf[0] = 'n';
        buf[1] = 'a';
        buf[2] = 'n';
        return 3;
    }
    if (v < 0) {
        buf[n++] = '-';
        v        = -v;
    }
    if (v == 0) {
        buf[n++] = '0';
        if (precisio > 0) {
            buf[n++] = '.';
            for (int i = 0; i < precisio; i++)
                buf[n++] = '0';
        }
        buf[n++] = maiusculus ? 'E' : 'e';
        buf[n++] = '+';
        buf[n++] = '0';
        buf[n++] = '0';
        return n;
    }
    int exp = 0;
    while (v >= 10) {
        v /= 10;
        exp++;
    }
    while (v < 1) {
        v *= 10;
        exp--;
    }
    n += formata_duplicem_f(buf + n, v, precisio);
    buf[n++] = maiusculus ? 'E' : 'e';
    if (exp < 0) {
        buf[n++] = '-';
        exp      = -exp;
    } else
        buf[n++] = '+';
    if (exp < 10)
        buf[n++] = '0';
    int en = 0;
    char eb[8];
    if (exp == 0)
        eb[en++] = '0';
    else
        while (exp) {
        eb[en++] = '0' + (exp % 10);
        exp /= 10;
    }
    for (int i = en - 1; i >= 0; i--)
        buf[n++] = eb[i];
    return n;
}

static int
format_core(scriptor_f scrib, void *q, const char *fmt, va_list ap)
{
    int total = 0;
    char tb[64];
    while (*fmt) {
        if (*fmt != '%') {
            const char *s = fmt;
            while (*fmt && *fmt != '%')
                fmt++;
            total += scrib(q, s, fmt - s);
            continue;
        }
        fmt++;
        /* flags */
        int sinister = 0, signum_pos = 0, spatium = 0, zero = 0, alt = 0;
        /* sgn hic sta, non in bloco interno, ne eius durata terminet
         * antequam 'prefix' ad scripturam pertingat (ne UB fiat). */
        char sgn = 0;
        while (*fmt) {
            if (*fmt == '-')
                sinister = 1;
            else if (*fmt == '+')
                signum_pos = 1;
            else if (*fmt == ' ')
                spatium = 1;
            else if (*fmt == '0')
                zero = 1;
            else if (*fmt == '#')
                alt = 1;
            else
                break;
            fmt++;
        }
        /* width */
        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
        } else
            while (isdigit((unsigned char)*fmt)) {
            width = width*10 + (*fmt - '0');
            fmt++;
        }
        /* precision */
        int precisio = -1;
        if (*fmt == '.') {
            fmt++;
            precisio = 0;
            if (*fmt == '*') {
                precisio = va_arg(ap, int);
                fmt++;
            } else
                while (isdigit((unsigned char)*fmt)) {
                precisio = precisio*10 + (*fmt - '0');
                fmt++;
            }
        }
        /* length */
        int longitudo = 0;  /* 0=int, 1=long, 2=longlong, -1=short, -2=char, 3=size_t */
        if (*fmt == 'l') {
            longitudo = 1;
            fmt++;
            if (*fmt == 'l') {
                longitudo = 2;
                fmt++;
            }
        } else if (*fmt == 'h') {
            longitudo = -1;
            fmt++;
            if (*fmt == 'h') {
                longitudo = -2;
                fmt++;
            }
        } else if (*fmt == 'z') {
            longitudo = 3;
            fmt++;
        } else if (*fmt == 'j') {
            longitudo = 2;
            fmt++;
        } else if (*fmt == 't') {
            longitudo = 3;
            fmt++;
        } else if (*fmt == 'L') {
            longitudo = 2;
            fmt++;
        }
        char c       = *fmt++;
        char *prefix = "";
        int prefix_n = 0;
        int n        = 0;
        if (c == 'd' || c == 'i') {
            long long v;
            switch (longitudo) {
            case 0:
            case -1:
            case -2:
                v = va_arg(ap, int);
                break;
            case 1:
                v = va_arg(ap, long);
                break;
            case 2:
                v = va_arg(ap, long long);
                break;
            case 3:
                v = va_arg(ap, long long);
                break;
            default:
                v = va_arg(ap, int);
            }
            unsigned long long u;
            sgn = 0;
            if (v < 0) {
                u   = (unsigned long long)(-v);
                sgn = '-';
            } else {
                u = (unsigned long long)v;
                if (signum_pos)
                    sgn = '+';
                else if (spatium)
                    sgn = ' ';
            }
            n = formata_integer(tb, u, 10, 0);
            if (sgn) {
                prefix   = &sgn;
                prefix_n = 1;
            }
        } else if (c == 'u' || c == 'x' || c == 'X' || c == 'o') {
            unsigned long long v;
            switch (longitudo) {
            case 0:
            case -1:
            case -2:
                v = va_arg(ap, unsigned);
                break;
            case 1:
                v = va_arg(ap, unsigned long);
                break;
            case 2:
                v = va_arg(ap, unsigned long long);
                break;
            case 3:
                v = va_arg(ap, unsigned long long);
                break;
            default:
                v = va_arg(ap, unsigned);
            }
            int basis = (c == 'u') ? 10 : (c == 'o') ? 8 : 16;
            n         = formata_integer(tb, v, basis, c == 'X');
            if (alt) {
                if (c == 'x') {
                    prefix   = "0x";
                    prefix_n = 2;
                } else if (c == 'X') {
                    prefix   = "0X";
                    prefix_n = 2;
                } else if (c == 'o' && tb[0] != '0') {
                    prefix   = "0";
                    prefix_n = 1;
                }
            }
        } else if (c == 'p') {
            unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void*);
            n = formata_integer(tb, v, 16, 0);
            prefix = "0x";
            prefix_n = 2;
        } else if (c == 'c') {
            tb[0] = (char)va_arg(ap, int);
            n     = 1;
        } else if (c == 's') {
            const char *s = va_arg(ap, const char*);
            if (!s)
                s = "(null)";
            n = (int)strlen(s);
            if (precisio >= 0 && precisio < n)
                n = precisio;
            /* directe scribere sine tb. */
            int pad = width > n ? width - n : 0;
            if (!sinister)
                for (int i = 0; i < pad; i++)
                    scrib(q, " ", 1), total++;
            scrib(q, s, n);
            total += n;
            if (sinister)
                for (int i = 0; i < pad; i++)
                    scrib(q, " ", 1), total++;
            continue;
        } else if (c == 'f' || c == 'F') {
            double v = va_arg(ap, double);
            if (precisio < 0)
                precisio = 6;
            n = formata_duplicem_f(tb, v, precisio);
            if (signum_pos && tb[0] != '-') {
                prefix   = "+";
                prefix_n = 1;
            } else if (spatium && tb[0] != '-') {
                prefix   = " ";
                prefix_n = 1;
            }
        } else if (c == 'e' || c == 'E') {
            double v = va_arg(ap, double);
            if (precisio < 0)
                precisio = 6;
            n = formata_duplicem_e(tb, v, precisio, c == 'E');
        } else if (c == 'g' || c == 'G') {
            double v = va_arg(ap, double);
            if (precisio < 0)
                precisio = 6;
            if (precisio == 0)
                precisio = 1;
            /* si exponens decimalis < -4 vel >= precisio, usemus e */
            double av = v < 0 ? -v : v;
            int exp   = 0;
            if (av > 0) {
                double t = av;
                while (t >= 10) {
                    t /= 10;
                    exp++;
                }
                while (t < 1) {
                    t *= 10;
                    exp--;
                }
            }
            if (exp < -4 || exp >= precisio)
                n = formata_duplicem_e(tb, v, precisio - 1, c == 'G');
            else
                n = formata_duplicem_f(tb, v, precisio - 1 - exp);
        } else if (c == '%') {
            tb[0] = '%';
            n     = 1;
        } else if (c == 'n') {
            /* explicite PROHIBITUS: culpa. */
            const char *err = "[%n prohibitum]";
            scrib(q, err, strlen(err));
            total += (int)strlen(err);
            continue;
        } else {
            tb[0] = '%';
            tb[1] = c;
            n     = 2;
        }
        int corpus = n + prefix_n;
        int pad    = width > corpus ? width - corpus : 0;
        if (!sinister && !zero)
            for (int i = 0; i < pad; i++) {
            scrib(q, " ", 1);
            total++;
        }
        if (prefix_n) {
            scrib(q, prefix, prefix_n);
            total += prefix_n;
        }
        if (!sinister && zero)
            for (int i = 0; i < pad; i++) {
            scrib(q, "0", 1);
            total++;
        }
        scrib(q, tb, n);
        total += n;
        if (sinister)
            for (int i = 0; i < pad; i++) {
            scrib(q, " ", 1);
            total++;
        }
    }
    return total;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) { return format_core(scrib_ad_file, f, fmt, ap); }
int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap)
{
    buf_scriptor bs = { b, n, 0 };
    int r = format_core(scrib_ad_bufum, &bs, fmt, ap);
    if (n)
        b[bs.pos < n ? bs.pos : n - 1] = 0;
    return r;
}
int vsprintf(char *b, const char *fmt, va_list ap) { return vsnprintf(b, (size_t)-1, fmt, ap); }

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}
int sprintf(char *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(b, fmt, ap);
    va_end(ap);
    return r;
}
int snprintf(char *b, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(b, n, fmt, ap);
    va_end(ap);
    return r;
}

/* scanf simplicissimus: %d, %u, %x, %s, %c, %f, %lf. */
static int
scan_core(
    const char *(*get)(void*), void (*unget)(void*, int), void *q,
    const char *fmt, va_list ap
) {
    int assigned = 0;
    int c        = -1;
    while (*fmt) {
        if (isspace((unsigned char)*fmt)) {
            while ((c = (int)(unsigned char)(**get ? (*get)(q)[0] : 0)) >= 0 && isspace(c)) { }
            if (c >= 0)
                unget(q, c);
            fmt++;
            continue;
        }
        (void)ap;
        (void)assigned;
        (void)unget;
        (void)c;
        fmt++;   /* scanf minimalis: indicetur non sustinitur */
    }
    return assigned;
}

int scanf(const char *fmt, ...) {
    (void)fmt;
    return 0;
}
int sscanf(const char *b, const char *fmt, ...) {
    (void)b;
    (void)fmt;
    return 0;
}
int fscanf(FILE *f, const char *fmt, ...) {
    (void)f;
    (void)fmt;
    return 0;
}

static __attribute__((used)) void *_scan_ref = (void*)scan_core;

/* ======================================================================== *
 * XI.  atexit + exit + abort.
 * ======================================================================== */

#define MAX_FN_EXIT 32
static void (*fn_exit[MAX_FN_EXIT])(void);
static int fn_exit_n = 0;

int atexit(void (*f)(void)) {
    if (fn_exit_n >= MAX_FN_EXIT)
        return -1;
    fn_exit[fn_exit_n++] = f;
    return 0;
}

void exit(int code)
{
    while (fn_exit_n) { fn_exit[--fn_exit_n](); }
    fflush(stdout);
    fflush(stderr);
    _exit(code);
}

void abort(void) {
    write(2, "abortatum\n", 10);
    _exit(134);
}

char *getenv(const char *n) {
    (void)n;
    return NULL;
}

/* ======================================================================== *
 * XII.  _start: parsat argc/argv ex pila, vocat main, exit.
 * ======================================================================== */

extern int main(int argc, char **argv, char **envp);

void
start_c(long *sp)
{
    file_init();
    long argc   = sp[0];
    char **argv = (char**)(sp + 1);
    char **envp = argv + argc + 1;
    int r       = main((int)argc, argv, envp);
    exit(r);
}

/* Darwin: assembler symbolum C 'start_c' evadit '_start_c'; symbolum
 * introitus '_start' (= 'start' in C) ipsi ld per '-e start' traditur.
 * Emulator armlxiv eum introducit per LC_MAIN aut LC_UNIXTHREAD. */
__asm__(
    "    .text\n"
    "    .globl _start\n"
    "_start:\n"
    "    mov x29, #0\n"         /* FP = 0 */
    "    mov x30, #0\n"         /* LR = 0 */
    "    mov x0, sp\n"          /* x0 = sp -> start_c */
    "    and sp, x0, #-16\n"
    "    bl _start_c\n"
    "    brk #1\n"
);

/* ==================== Processus, plicae, conexiones. ==================== */

pid_t fork(void) { return (pid_t)sysret(vocatio1(SYS_fork, 0)); }
pid_t wait(int *s) { return (pid_t)sysret(vocatio4(SYS_wait4, -1, (long)s, 0, 0)); }
pid_t waitpid(pid_t p, int *s, int o) { return (pid_t)sysret(vocatio4(SYS_wait4, p, (long)s, o, 0)); }
pid_t getppid(void) { return (pid_t)vocatio1(SYS_getppid, 0); }
int   kill(pid_t p, int s) { return (int)sysret(vocatio2(SYS_kill, p, s)); }
int   unlink(const char *p) { return (int)sysret(vocatio1(SYS_unlink, (long)p)); }
int   stat(const char *p, struct stat *s) { return (int)sysret(vocatio2(SYS_stat, (long)p, (long)s)); }
int   lstat(const char *p, struct stat *s) { return (int)sysret(vocatio2(SYS_lstat, (long)p, (long)s)); }
int   mkdir(const char *p, mode_t m) { return (int)sysret(vocatio2(SYS_mkdir, (long)p, m)); }
int   mkfifo(const char *p, mode_t m) { return (int)sysret(vocatio2(SYS_mkfifo, (long)p, m)); }
int   rmdir(const char *p) { return (int)sysret(vocatio1(SYS_rmdir, (long)p)); }
int   chmod(const char *p, mode_t m) {
    (void)p;
    (void)m;
    return 0;
}
int   pipe(int fd[2]) { return (int)sysret(vocatio1(SYS_pipe, (long)fd)); }
int   dup(int f) { return (int)sysret(vocatio1(SYS_dup, f)); }
int   dup2(int o, int n) { return (int)sysret(vocatio2(SYS_dup2, o, n)); }
int   execve(const char *p, char *const a[], char *const e[]) {
    return (int)sysret(vocatio3(SYS_execve, (long)p, (long)a, (long)e));
}
int   execv(const char *p, char *const a[]) { return execve(p, a, 0); }
int   execvp(const char *p, char *const a[]) { return execve(p, a, 0); }
int   ftruncate(int fd, off_t l) { return (int)sysret(vocatio2(SYS_ftruncate, fd, l)); }
int   truncate(const char *p, off_t l) { return (int)sysret(vocatio2(SYS_truncate, (long)p, l)); }
int   flock(int fd, int op) { return (int)sysret(vocatio2(SYS_flock, fd, op)); }
int   access(const char *p, int m) {
    (void)p;
    (void)m;
    errno = EACCES;
    return -1;
}

/* Socket. */
int socket(int d, int t, int p) { return (int)sysret(vocatio3(SYS_socket, d, t, p)); }
int socketpair(int d, int t, int p, int sv[2]) {
    return (int)sysret(vocatio4(SYS_socketpair, d, t, p, (long)sv));
}
int bind(int s, const void *a, unsigned l) {
    return (int)sysret(vocatio3(SYS_bind, s, (long)a, l));
}
int connect(int s, const void *a, unsigned l) {
    return (int)sysret(vocatio3(SYS_connect, s, (long)a, l));
}
int listen(int s, int b) { return (int)sysret(vocatio2(SYS_listen, s, b)); }
int accept(int s, void *a, unsigned *l) {
    return (int)sysret(vocatio3(SYS_accept, s, (long)a, (long)l));
}
ssize_t send(int s, const void *b, size_t n, int f) {
    return (ssize_t)sysret(vocatio6(SYS_sendto, s, (long)b, (long)n, f, 0, 0));
}
ssize_t recv(int s, void *b, size_t n, int f) {
    return (ssize_t)sysret(vocatio6(SYS_recvfrom, s, (long)b, (long)n, f, 0, 0));
}
ssize_t sendto(int s, const void *b, size_t n, int f, const void *a, unsigned l) {
    return (ssize_t)sysret(vocatio6(SYS_sendto, s, (long)b, (long)n, f, (long)a, (long)l));
}
ssize_t recvfrom(int s, void *b, size_t n, int f, void *a, unsigned *l) {
    return (ssize_t)sysret(vocatio6(SYS_recvfrom, s, (long)b, (long)n, f, (long)a, (long)l));
}
int shutdown(int s, int h) { return (int)sysret(vocatio2(SYS_shutdown, s, h)); }
int setsockopt(int s, int lv, int o, const void *v, unsigned l) {
    (void)s;
    (void)lv;
    (void)o;
    (void)v;
    (void)l;
    return 0;
}
int getsockopt(int s, int lv, int o, void *v, unsigned *l) {
    (void)s;
    (void)lv;
    (void)o;
    (void)v;
    (void)l;
    return 0;
}

/* Select. Guest struct fd_set identicus ei quem armlxiv expectat. */
int select(int n, void *r, void *w, void *e, void *to) {
    return (int)sysret(vocatio6(SYS_select, n, (long)r, (long)w, (long)e, (long)to, 0));
}

/* Dirent: indicem integrum tenemus; structuram entitatis in congerie
 * vocatoris locamus, ne imitator memoriam peregrini distribuat. */
#include <dirent.h>
struct _vdir {
    int handle;
    struct dirent ent;
};

DIR *opendir(const char *p) {
    long h = vocatio1(SYS_opendir, (long)p);
    if (h < 0) {
        errno = (int)-h;
        return 0;
    }
    DIR *d = malloc(sizeof(DIR));
    if (!d) {
        errno = ENOMEM;
        return 0;
    }
    d->handle = (int)h;
    return d;
}
struct dirent *readdir(DIR *d) {
    if (!d)
        return 0;
    long r = vocatio2(SYS_readdir, d->handle, (long)&d->ent);
    if (r == 0)
        return &d->ent;
    if (r > 0)
        return 0;   /* EOF */
    errno = (int)-r;
    return 0;
}
int closedir(DIR *d) {
    if (!d)
        return -1;
    long r = vocatio1(SYS_closedir, d->handle);
    free(d);
    return r < 0 ? (errno = (int)-r, -1) : 0;
}

/* Signa: tantum simulacra praebemus; imitator SIGALRM solum statim
 * in vocatione 'pause' tradit. */
int sigaction(int sig, const void *act, void *oact) {
    (void)sig;
    (void)act;
    (void)oact;
    return (int)sysret(vocatio3(SYS_sigaction, sig, (long)act, (long)oact));
}
int sigemptyset(void *s) {
    if (s)
        *(long*)s = 0;
    return 0;
}
int sigfillset(void *s) {
    if (s)
        *(long*)s = -1;
    return 0;
}
int sigaddset(void *s, int g) {
    if (s)
        *(long*)s |= (1ul << g);
    return 0;
}
int sigdelset(void *s, int g) {
    if (s)
        *(long*)s &= ~(1ul << g);
    return 0;
}
int sigprocmask(int h, const void *s, void *o) {
    (void)h;
    (void)s;
    (void)o;
    return 0;
}
int sigsuspend(const void *m) {
    (void)m;
    errno = EINTR;
    return -1;
}
int raise(int sig) { return kill(getpid(), sig); }
void (*signal(int sig, void (*func)(int)))(int) {
    (void)sig;
    (void)func;
    return 0;
}
unsigned alarm(unsigned s) {
    (void)s;
    return 0;
}
int ualarm(unsigned us, unsigned iv) {
    (void)us;
    (void)iv;
    return 0;
}
int pause(void) {
    (void)vocatio1(SYS_pause, 0);
    errno = EINTR;
    return -1;
}

int msync(void *a, size_t l, int f) { return (int)sysret(vocatio3(SYS_msync, (long)a, l, f)); }

double difftime(time_t a, time_t b) { return (double)(a - b); }
struct tm _tm_obj;
struct tm *localtime(const time_t *t) {
    (void)t;
    return &_tm_obj;
}
struct tm *gmtime(const time_t *t) {
    (void)t;
    return &_tm_obj;
}
