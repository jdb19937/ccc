/* minerva: stdint.h — integra numera fixi magnitudinis. */
#ifndef MINERVA_STDINT_H
#define MINERVA_STDINT_H

typedef __INT8_TYPE__   int8_t;
typedef __INT16_TYPE__  int16_t;
typedef __INT32_TYPE__  int32_t;
typedef __INT64_TYPE__  int64_t;
typedef __UINT8_TYPE__  uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTMAX_TYPE__  intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

#define INT8_MIN   (-128)
#define INT16_MIN  (-32767-1)
#define INT32_MIN  (-2147483647-1)
#define INT64_MIN  (-9223372036854775807LL-1)
#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  9223372036854775807LL
#define UINT8_MAX  0xff
#define UINT16_MAX 0xffff
#define UINT32_MAX 0xffffffffu
#define UINT64_MAX 0xffffffffffffffffull
#define SIZE_MAX   UINT64_MAX

#define INT8_C(v)   v
#define INT16_C(v)  v
#define INT32_C(v)  v
#define INT64_C(v)  v##LL
#define UINT8_C(v)  v
#define UINT16_C(v) v
#define UINT32_C(v) v##u
#define UINT64_C(v) v##ull

#endif
