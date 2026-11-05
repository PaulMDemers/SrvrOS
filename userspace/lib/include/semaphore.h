#ifndef SRVROS_POSIX_SEMAPHORE_H
#define SRVROS_POSIX_SEMAPHORE_H

#include <limits.h>

typedef struct {
    unsigned int __value;
} sem_t;

#ifdef __cplusplus
extern "C" {
#endif

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#ifdef __cplusplus
}
#endif

#endif
