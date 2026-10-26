#ifndef SRVROS_POSIX_SETJMP_H
#define SRVROS_POSIX_SETJMP_H

typedef unsigned long jmp_buf[8];

int setjmp(jmp_buf env) __attribute__((returns_twice));
void longjmp(jmp_buf env, int value) __attribute__((noreturn));
/* Neither form saves the signal mask. These are real symbol aliases so the
 * saved return address remains the caller's, without a C wrapper frame. */
int _setjmp(jmp_buf env) __attribute__((returns_twice));
void _longjmp(jmp_buf env, int value) __attribute__((noreturn));

#endif
