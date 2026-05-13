#ifndef SRVROS_POSIX_SETJMP_H
#define SRVROS_POSIX_SETJMP_H

typedef unsigned long jmp_buf[8];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value) __attribute__((noreturn));

#endif
