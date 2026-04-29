#ifndef VULCANUS_MATH_H
#define VULCANUS_MATH_H

#define M_PI    3.14159265358979323846
#define M_E     2.71828182845904523536
#define M_SQRT2 1.41421356237309504880
#define M_LN2   0.69314718055994530942
#define M_LOG2E 1.44269504088896340736
#define M_PI_2  1.57079632679489661923
#define M_PI_4  0.78539816339744830962

#define HUGE_VAL  (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define INFINITY  (__builtin_inff())
#define NAN       (__builtin_nanf(""))

double sqrt(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double trunc(double x);
double exp(double x);
double log(double x);
double log2(double x);
double log10(double x);
double pow(double x, double y);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double fmod(double x, double y);
double hypot(double x, double y);

float  sqrtf(float x);
float  fabsf(float x);
float  floorf(float x);
float  ceilf(float x);
float  expf(float x);
float  logf(float x);
float  powf(float x, float y);
float  sinf(float x);
float  cosf(float x);
float  tanf(float x);
float  tanhf(float x);
float  atanf(float x);
float  atan2f(float y, float x);
float  fmodf(float x, float y);
float  hypotf(float x, float y);
float  roundf(float x);
float  truncf(float x);
float  log2f(float x);
float  log10f(float x);

double ldexp(double x, int e);
float  ldexpf(float x, int e);
double fma(double a, double b, double c);
float  fmaf(float a, float b, float c);

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4
int fpclassify(double x);

int    isnan(double x);
int    isinf(double x);
int    isfinite(double x);

#ifndef isnan
#define isnan(x)    __builtin_isnan(x)
#endif
#ifndef isinf
#define isinf(x)    __builtin_isinf(x)
#endif
#ifndef isfinite
#define isfinite(x) __builtin_isfinite(x)
#endif

#endif
