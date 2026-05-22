#ifndef SRVROS_POSIX_SEMAPHORE_H
#define SRVROS_POSIX_SEMAPHORE_H

typedef struct {
    unsigned int __value;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#endif
