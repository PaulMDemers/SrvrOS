#ifndef SRVROS_POSIX_SYS_PRCTL_H
#define SRVROS_POSIX_SYS_PRCTL_H

#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0

int prctl(int option, ...);

#endif
