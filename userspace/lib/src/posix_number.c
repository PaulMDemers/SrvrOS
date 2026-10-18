/* Decimal conversion used by the minimal libc. Scaling work is bounded by
 * representable double range, rather than the size of an input exponent.
 * This remains a decimal-only conversion, not a correctly-rounded strtod. */
#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include <float.h>
#include <errno.h>

static double pow10_int(int exponent) {
    double value = 1.0;
    while (exponent > 0) {
        value *= 10.0;
        exponent--;
        if (!isfinite(value)) break;
    }
    while (exponent < 0) {
        value /= 10.0;
        exponent++;
        if (value == 0.0) break;
    }
    return value;
}

double strtod(const char *text, char **endptr) {
    const char *start = text;
    double sign = 1.0;
    double value = 0.0;
    int any = 0;
    while (*text == ' ' || (*text >= '\t' && *text <= '\r')) {
        text++;
    }
    if (*text == '-') {
        sign = -1.0;
        text++;
    } else if (*text == '+') {
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10.0 + (double)(*text - '0');
        text++;
        any = 1;
    }
    if (*text == '.') {
        double scale = 0.1;
        text++;
        while (*text >= '0' && *text <= '9') {
            value += (double)(*text - '0') * scale;
            scale *= 0.1;
            text++;
            any = 1;
        }
    }
    if (any && (*text == 'e' || *text == 'E')) {
        const char *exponent_start = text;
        int exponent_sign = 1;
        int exponent = 0;
        int exponent_any = 0;
        text++;
        if (*text == '-') {
            exponent_sign = -1;
            text++;
        } else if (*text == '+') {
            text++;
        }
        while (*text >= '0' && *text <= '9') {
            /* Decimal range is tiny compared with an arbitrary input exponent.
             * Saturate while still consuming its digits, without signed overflow. */
            if (exponent < 10000) {
                exponent = exponent * 10 + (*text - '0');
                if (exponent > 10000) exponent = 10000;
            }
            text++;
            exponent_any = 1;
        }
        if (exponent_any) {
            /* Zero with any exponent is exact, including signed zero. */
            if (value != 0.0) {
                double previous = value;
                value *= pow10_int(exponent * exponent_sign);
                if (!isfinite(value) || value == 0.0 || (value < DBL_MIN && previous != 0.0)) errno = ERANGE;
            }
        } else {
            text = exponent_start;
        }
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? text : start);
    }
    if (any && !isfinite(value)) errno = ERANGE;
    return value * sign;
}

float strtof(const char *text, char **endptr) {
    double value = strtod(text, endptr);
    float result = (float)value;
    if (isfinite(value) && value != 0.0 && (!isfinite(result) || result == 0.0f || (result > -FLT_MIN && result < FLT_MIN))) errno = ERANGE;
    return result;
}

long double strtold(const char *text, char **endptr) {
    return (long double)strtod(text, endptr);
}

float strtof_l(const char *text, char **endptr, locale_t locale) {
    (void)locale;
    return strtof(text, endptr);
}

double strtod_l(const char *text, char **endptr, locale_t locale) {
    (void)locale;
    return strtod(text, endptr);
}

long double strtold_l(const char *text, char **endptr, locale_t locale) {
    (void)locale;
    return strtold(text, endptr);
}

