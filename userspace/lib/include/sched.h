#ifndef SRVROS_POSIX_SCHED_H
#define SRVROS_POSIX_SCHED_H

#include <stddef.h>
#include <sys/types.h>

#define CPU_SETSIZE 64
#define __CPU_WORD_BITS (8 * sizeof(unsigned long))

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2

typedef struct {
    unsigned long bits[(CPU_SETSIZE + __CPU_WORD_BITS - 1) / __CPU_WORD_BITS];
} cpu_set_t;

struct sched_param {
    int sched_priority;
};

#define CPU_ZERO(set) do { \
    for (size_t __i = 0; __i < sizeof(*(set)) / sizeof((set)->bits[0]); __i++) { \
        (set)->bits[__i] = 0; \
    } \
} while (0)

#define CPU_SET(cpu, set) do { \
    int __cpu = (cpu); \
    if (__cpu >= 0 && __cpu < CPU_SETSIZE) { \
        (set)->bits[(size_t)__cpu / __CPU_WORD_BITS] |= \
            1ul << ((size_t)__cpu % __CPU_WORD_BITS); \
    } \
} while (0)

#define CPU_CLR(cpu, set) do { \
    int __cpu = (cpu); \
    if (__cpu >= 0 && __cpu < CPU_SETSIZE) { \
        (set)->bits[(size_t)__cpu / __CPU_WORD_BITS] &= \
            ~(1ul << ((size_t)__cpu % __CPU_WORD_BITS)); \
    } \
} while (0)

#define CPU_ISSET(cpu, set) \
    ((cpu) >= 0 && (cpu) < CPU_SETSIZE && \
        (((set)->bits[(size_t)(cpu) / __CPU_WORD_BITS] & \
            (1ul << ((size_t)(cpu) % __CPU_WORD_BITS))) != 0))

#define CPU_COUNT(set) __sched_cpu_count(set)

#ifdef __cplusplus
extern "C" {
#endif

int sched_yield(void);
int sched_getcpu(void);
int sched_get_priority_min(int policy);
int sched_get_priority_max(int policy);
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
int __sched_cpu_count(const cpu_set_t *mask);

#ifdef __cplusplus
}
#endif

#endif
