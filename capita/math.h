#ifndef _MATH_H_
#define _MATH_H_

double fabs(double);
double sqrt(double);
double exp(double);
double log(double);
double log2(double);
double log10(double);
double pow(double, double);
double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double atan(double);
double atan2(double, double);
double sinh(double);
double cosh(double);
double tanh(double);
double floor(double);
double ceil(double);
double round(double);
double fmod(double, double);
double fmin(double x, double y);
double fmax(double x, double y);
double acos(double x);
double asin(double x);

float fabsf(float);
float sqrtf(float);
float expf(float);
float logf(float);
float log2f(float);
float log10f(float);
float powf(float, float);
float sinf(float);
float cosf(float);
float tanf(float);
float tanhf(float);
float floorf(float);
float ceilf(float);
float roundf(float);
float fmodf(float, float);
float fminf(float x, float y);
float fmaxf(float x, float y);
float acosf(float x);
float asinf(float x);

/* long double variants (long double tractatur ut double) */
double fabsl(double);
double sqrtl(double);
double expl(double);
double logl(double);
double log2l(double);
double log10l(double);
double powl(double, double);
double sinl(double);
double cosl(double);
double tanl(double);
double asinl(double);
double acosl(double);
double atanl(double);
double atan2l(double, double);
double sinhl(double);
double coshl(double);
double tanhl(double);
double floorl(double);
double ceill(double);
double roundl(double);
double fmodl(double, double);

#define M_PI  3.14159265358979323846
#define M_E   2.7182818284590452354

#define HUGE_VAL  1e999
#define HUGE_VALF 1e999f
#define INFINITY  (1.0f/0.0f)
#define NAN       (0.0f/0.0f)

#endif
