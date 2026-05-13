#ifndef SRVROS_POSIX_MATH_H
#define SRVROS_POSIX_MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define HUGE_VALL (__builtin_huge_vall())

#define fabs(value) ((value) < 0 ? -(value) : (value))
#define floor(value) (value)
#define ceil(value) (value)
#define ldexp(value, exponent) ((void)(exponent), (value))
#define frexp(value, exponent) (*(exponent) = 0, (value))

#endif
