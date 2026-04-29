/* vulcanus: stdarg.h — argumenta variabilia per GCC builtins. */
#ifndef VULCANUS_STDARG_H
#define VULCANUS_STDARG_H

typedef __builtin_va_list va_list;
#define va_start(ap,last) __builtin_va_start(ap,last)
#define va_end(ap)        __builtin_va_end(ap)
#define va_arg(ap,tp)     __builtin_va_arg(ap,tp)
#define va_copy(d,s)      __builtin_va_copy(d,s)

#endif
