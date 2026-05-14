#include <math.h>
#include <stdint.h>

#define SRV_PI 3.14159265358979323846264338327950288
#define SRV_HALF_PI 1.57079632679489661923132169163975144
#define SRV_TWO_PI 6.28318530717958647692528676655900576
#define SRV_LN2 0.693147180559945309417232121458176568
#define SRV_LN10 2.30258509299404568401799145468436421
#define SRV_E 2.71828182845904523536028747135266250
#define SRV_INT_LIMIT 9223372036854775808.0

static double srv_nan(void) {
    return __builtin_nan("");
}

static double reduce_angle(double value) {
    if (!isfinite(value)) {
        return srv_nan();
    }
    while (value > SRV_PI) {
        value -= SRV_TWO_PI;
    }
    while (value < -SRV_PI) {
        value += SRV_TWO_PI;
    }
    return value;
}

double fabs(double value) {
    return value < 0.0 ? -value : value;
}

float fabsf(float value) {
    return (float)fabs(value);
}

long double fabsl(long double value) {
    return value < 0.0L ? -value : value;
}

double trunc(double value) {
    if (!isfinite(value) || value >= SRV_INT_LIMIT || value <= -SRV_INT_LIMIT) {
        return value;
    }
    return (double)(long long)value;
}

float truncf(float value) {
    return (float)trunc(value);
}

long double truncl(long double value) {
    if (value != value || value >= (long double)SRV_INT_LIMIT || value <= -(long double)SRV_INT_LIMIT) {
        return value;
    }
    return (long double)(long long)value;
}

double floor(double value) {
    if (!isfinite(value) || value >= SRV_INT_LIMIT || value <= -SRV_INT_LIMIT) {
        return value;
    }
    long long integer = (long long)value;
    if (value < 0.0 && (double)integer != value) {
        integer--;
    }
    return (double)integer;
}

float floorf(float value) {
    return (float)floor(value);
}

long double floorl(long double value) {
    return (long double)floor((double)value);
}

double ceil(double value) {
    if (!isfinite(value) || value >= SRV_INT_LIMIT || value <= -SRV_INT_LIMIT) {
        return value;
    }
    long long integer = (long long)value;
    if (value > 0.0 && (double)integer != value) {
        integer++;
    }
    return (double)integer;
}

float ceilf(float value) {
    return (float)ceil(value);
}

long double ceill(long double value) {
    return (long double)ceil((double)value);
}

double fmod(double left, double right) {
    if (right == 0.0 || !isfinite(left) || isnan(right)) {
        return srv_nan();
    }
    if (!isfinite(right)) {
        return left;
    }
    double quotient = trunc(left / right);
    return left - quotient * right;
}

float fmodf(float left, float right) {
    return (float)fmod(left, right);
}

long double fmodl(long double left, long double right) {
    return (long double)fmod((double)left, (double)right);
}

double sqrt(double value) {
    if (value < 0.0 || isnan(value)) {
        return srv_nan();
    }
    if (value == 0.0 || isinf(value)) {
        return value;
    }
    double guess = value >= 1.0 ? value : 1.0;
    for (int i = 0; i < 32; i++) {
        guess = 0.5 * (guess + value / guess);
    }
    return guess;
}

float sqrtf(float value) {
    return (float)sqrt(value);
}

long double sqrtl(long double value) {
    return (long double)sqrt((double)value);
}

double sin(double value) {
    double x = reduce_angle(value);
    if (isnan(x)) {
        return x;
    }
    double x2 = x * x;
    double term = x;
    double sum = x;
    for (int n = 1; n < 12; n++) {
        term *= -x2 / ((2.0 * n) * (2.0 * n + 1.0));
        sum += term;
    }
    return sum;
}

float sinf(float value) {
    return (float)sin(value);
}

long double sinl(long double value) {
    return (long double)sin((double)value);
}

double cos(double value) {
    double x = reduce_angle(value);
    if (isnan(x)) {
        return x;
    }
    double x2 = x * x;
    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n < 12; n++) {
        term *= -x2 / ((2.0 * n - 1.0) * (2.0 * n));
        sum += term;
    }
    return sum;
}

float cosf(float value) {
    return (float)cos(value);
}

long double cosl(long double value) {
    return (long double)cos((double)value);
}

double tan(double value) {
    return sin(value) / cos(value);
}

float tanf(float value) {
    return (float)tan(value);
}

long double tanl(long double value) {
    return (long double)tan((double)value);
}

double atan(double value) {
    if (isnan(value)) {
        return value;
    }
    if (value < 0.0) {
        return -atan(-value);
    }
    if (value > 1.0) {
        return SRV_HALF_PI - atan(1.0 / value);
    }
    double x2 = value * value;
    double term = value;
    double sum = value;
    for (int n = 1; n < 32; n++) {
        term *= -x2;
        sum += term / (2.0 * n + 1.0);
    }
    return sum;
}

float atanf(float value) {
    return (float)atan(value);
}

long double atanl(long double value) {
    return (long double)atan((double)value);
}

double atan2(double y, double x) {
    if (x > 0.0) {
        return atan(y / x);
    }
    if (x < 0.0 && y >= 0.0) {
        return atan(y / x) + SRV_PI;
    }
    if (x < 0.0 && y < 0.0) {
        return atan(y / x) - SRV_PI;
    }
    if (x == 0.0 && y > 0.0) {
        return SRV_HALF_PI;
    }
    if (x == 0.0 && y < 0.0) {
        return -SRV_HALF_PI;
    }
    return 0.0;
}

float atan2f(float y, float x) {
    return (float)atan2(y, x);
}

long double atan2l(long double y, long double x) {
    return (long double)atan2((double)y, (double)x);
}

double asin(double value) {
    if (value < -1.0 || value > 1.0) {
        return srv_nan();
    }
    return atan2(value, sqrt(1.0 - value * value));
}

float asinf(float value) {
    return (float)asin(value);
}

long double asinl(long double value) {
    return (long double)asin((double)value);
}

double acos(double value) {
    if (value < -1.0 || value > 1.0) {
        return srv_nan();
    }
    return SRV_HALF_PI - asin(value);
}

float acosf(float value) {
    return (float)acos(value);
}

long double acosl(long double value) {
    return (long double)acos((double)value);
}

double ldexp(double value, int exponent) {
    if (value == 0.0 || !isfinite(value)) {
        return value;
    }
    while (exponent > 0) {
        value *= 2.0;
        exponent--;
    }
    while (exponent < 0) {
        value *= 0.5;
        exponent++;
    }
    return value;
}

float ldexpf(float value, int exponent) {
    return (float)ldexp(value, exponent);
}

long double ldexpl(long double value, int exponent) {
    return (long double)ldexp((double)value, exponent);
}

double frexp(double value, int *exponent) {
    if (exponent != 0) {
        *exponent = 0;
    }
    if (value == 0.0 || !isfinite(value)) {
        return value;
    }
    int sign = value < 0.0 ? -1 : 1;
    double x = sign < 0 ? -value : value;
    int exp = 0;
    while (x >= 1.0) {
        x *= 0.5;
        exp++;
    }
    while (x < 0.5) {
        x *= 2.0;
        exp--;
    }
    if (exponent != 0) {
        *exponent = exp;
    }
    return sign < 0 ? -x : x;
}

float frexpf(float value, int *exponent) {
    return (float)frexp(value, exponent);
}

long double frexpl(long double value, int *exponent) {
    return (long double)frexp((double)value, exponent);
}

double exp(double value) {
    if (isnan(value)) {
        return value;
    }
    if (value > 709.0) {
        return HUGE_VAL;
    }
    if (value < -745.0) {
        return 0.0;
    }
    int k = (int)(value / SRV_LN2);
    double r = value - (double)k * SRV_LN2;
    double term = 1.0;
    double sum = 1.0;
    for (int i = 1; i < 28; i++) {
        term *= r / (double)i;
        sum += term;
    }
    return ldexp(sum, k);
}

float expf(float value) {
    return (float)exp(value);
}

long double expl(long double value) {
    return (long double)exp((double)value);
}

double log(double value) {
    if (value < 0.0 || isnan(value)) {
        return srv_nan();
    }
    if (value == 0.0) {
        return -HUGE_VAL;
    }
    if (isinf(value)) {
        return value;
    }
    int exponent = 0;
    double m = frexp(value, &exponent);
    m *= 2.0;
    exponent--;
    double y = (m - 1.0) / (m + 1.0);
    double y2 = y * y;
    double term = y;
    double sum = y;
    for (int n = 1; n < 32; n++) {
        term *= y2;
        sum += term / (double)(2 * n + 1);
    }
    return 2.0 * sum + (double)exponent * SRV_LN2;
}

float logf(float value) {
    return (float)log(value);
}

long double logl(long double value) {
    return (long double)log((double)value);
}

double log2(double value) {
    return log(value) / SRV_LN2;
}

float log2f(float value) {
    return (float)log2(value);
}

long double log2l(long double value) {
    return (long double)log2((double)value);
}

double log10(double value) {
    return log(value) / SRV_LN10;
}

float log10f(float value) {
    return (float)log10(value);
}

long double log10l(long double value) {
    return (long double)log10((double)value);
}

double pow(double base, double exponent) {
    if (exponent == 0.0) {
        return 1.0;
    }
    if (base == 0.0) {
        return exponent > 0.0 ? 0.0 : HUGE_VAL;
    }
    if (isfinite(exponent) && exponent == floor(exponent) &&
        exponent > -63.0 && exponent < 63.0) {
        long long count = (long long)exponent;
        int negative = count < 0;
        if (negative) {
            count = -count;
        }
        double result = 1.0;
        double factor = base;
        while (count > 0) {
            if ((count & 1) != 0) {
                result *= factor;
            }
            factor *= factor;
            count >>= 1;
        }
        return negative ? 1.0 / result : result;
    }
    if (base < 0.0) {
        return srv_nan();
    }
    return exp(exponent * log(base));
}

float powf(float base, float exponent) {
    return (float)pow(base, exponent);
}

long double powl(long double base, long double exponent) {
    return (long double)pow((double)base, (double)exponent);
}
