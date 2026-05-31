#ifndef SRVROS_POSIX_SYS_SYSCALL_H
#define SRVROS_POSIX_SYS_SYSCALL_H

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_munmap 11
#define SYS_getpid 39
#define SYS_gettid 186
#define SYS_futex 202
#define __NR_futex SYS_futex
#define SYS_capget 125
#define SYS_getrandom 318

#ifdef __cplusplus
extern "C" {
#endif

long syscall(long number, ...);

#ifdef __cplusplus
}
#endif

#endif
