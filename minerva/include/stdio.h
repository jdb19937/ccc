#ifndef MINERVA_STDIO_H
#define MINERVA_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)
#define BUFSIZ 4096
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE FILE;
typedef long fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int    printf(const char *fmt, ...)  __attribute__((format(printf, 1, 2)));
int    fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int    sprintf(char *buf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int    snprintf(char *buf, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int    vprintf(const char *fmt, va_list ap);
int    vfprintf(FILE *f, const char *fmt, va_list ap);
int    vsprintf(char *buf, const char *fmt, va_list ap);
int    vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

int    puts(const char *s);
int    putchar(int c);
int    fputs(const char *s, FILE *f);
int    fputc(int c, FILE *f);
int    putc(int c, FILE *f);

int    scanf(const char *fmt, ...);
int    sscanf(const char *buf, const char *fmt, ...);
int    fscanf(FILE *f, const char *fmt, ...);
char  *fgets(char *buf, int n, FILE *f);
int    fgetc(FILE *f);
int    getchar(void);
int    getc(FILE *f);
int    ungetc(int c, FILE *f);

FILE  *fopen(const char *path, const char *mode);
FILE  *freopen(const char *path, const char *mode, FILE *f);
int    fclose(FILE *f);
size_t fread(void *p, size_t sz, size_t n, FILE *f);
size_t fwrite(const void *p, size_t sz, size_t n, FILE *f);
int    fseek(FILE *f, long off, int w);
long   ftell(FILE *f);
void   rewind(FILE *f);
int    fflush(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   clearerr(FILE *f);
int    remove(const char *p);
int    rename(const char *a, const char *b);
void   perror(const char *s);
void   setbuf(FILE *f, char *b);
int    setvbuf(FILE *f, char *b, int m, size_t n);
int    fileno(FILE *f);

#endif
