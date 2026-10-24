#ifndef SRVROS_POSIX_SYS_TIMES_H
#define SRVROS_POSIX_SYS_TIMES_H
#include <time.h>
struct tms { clock_t tms_utime,tms_stime,tms_cutime,tms_cstime; };
#ifdef __cplusplus
extern "C" {
#endif
clock_t times(struct tms *buffer);
#ifdef __cplusplus
}
#endif
#endif
