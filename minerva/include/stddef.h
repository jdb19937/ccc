/* minerva: stddef.h — vacuum et typi fundamentales.
 * Nota: componitor (GCC) proprium stddef.h habet; illud praeferimus si
 * accessibile, nostrum pro completione sufficit. */
#ifndef MINERVA_STDDEF_H
#define MINERVA_STDDEF_H

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef __SIZE_TYPE__     size_t;
typedef __PTRDIFF_TYPE__  ptrdiff_t;
typedef int               wchar_t;

#ifndef offsetof
#define offsetof(tp,mb) __builtin_offsetof(tp,mb)
#endif

#endif
