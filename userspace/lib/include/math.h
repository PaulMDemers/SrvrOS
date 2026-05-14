#ifndef SRVROS_POSIX_MATH_H
#define SRVROS_POSIX_MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())
#define INFINITY (__builtin_huge_valf())
#define NAN (__builtin_nanf(""))

#define isnan(value) ((value) != (value))
#define isinf(value) (!isnan(value) && isnan((value) - (value)))
#define isfinite(value) (!isnan(value) && !isinf(value))

double fabs(double value);
float fabsf(float value);
long double fabsl(long double value);
double floor(double value);
float floorf(float value);
long double floorl(long double value);
double ceil(double value);
float ceilf(float value);
long double ceill(long double value);
double trunc(double value);
float truncf(float value);
long double truncl(long double value);
double fmod(double left, double right);
float fmodf(float left, float right);
long double fmodl(long double left, long double right);
double sqrt(double value);
float sqrtf(float value);
long double sqrtl(long double value);
double sin(double value);
float sinf(float value);
long double sinl(long double value);
double cos(double value);
float cosf(float value);
long double cosl(long double value);
double tan(double value);
float tanf(float value);
long double tanl(long double value);
double atan(double value);
float atanf(float value);
long double atanl(long double value);
double atan2(double y, double x);
float atan2f(float y, float x);
long double atan2l(long double y, long double x);
double asin(double value);
float asinf(float value);
long double asinl(long double value);
double acos(double value);
float acosf(float value);
long double acosl(long double value);
double exp(double value);
float expf(float value);
long double expl(long double value);
double log(double value);
float logf(float value);
long double logl(long double value);
double log2(double value);
float log2f(float value);
long double log2l(long double value);
double log10(double value);
float log10f(float value);
long double log10l(long double value);
double pow(double base, double exponent);
float powf(float base, float exponent);
long double powl(long double base, long double exponent);
double ldexp(double value, int exponent);
float ldexpf(float value, int exponent);
long double ldexpl(long double value, int exponent);
double frexp(double value, int *exponent);
float frexpf(float value, int *exponent);
long double frexpl(long double value, int *exponent);

#endif
